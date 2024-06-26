/* Edge Impulse ingestion SDK
 * Copyright (c) 2022 EdgeImpulse Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

/* Includes ---------------------------------------------------------------- */
#include <icicle-monitor_inferencing.h>
#include "camera.h"
#include "himax.h"
#include "edge-impulse-sdk/dsp/image/image.hpp"
#include <MKRWAN_v2.h>
#include "arduino_secrets.h"

/* Constant defines -------------------------------------------------------- */
// Please enter your sensitive data in the Secret tab or arduino_secrets.h
String appEui = SECRET_APP_EUI;
String appKey = SECRET_APP_KEY;

#define EI_CAMERA_RAW_FRAME_BUFFER_COLS           320
#define EI_CAMERA_RAW_FRAME_BUFFER_ROWS           240

// frame buffer allocation options:
//    - static (default if none below is chosen)
//    - heap or
//    - SDRAM
#define EI_CAMERA_FRAME_BUFFER_SDRAM
//#define EI_CAMERA_FRAME_BUFFER_HEAP

#ifdef EI_CAMERA_FRAME_BUFFER_SDRAM
#include "SDRAM.h"
#endif

/*
 ** NOTE: If you run into TFLite arena allocation issue.
 **
 ** This may be due to may dynamic memory fragmentation.
 ** Try defining "-DEI_CLASSIFIER_ALLOCATION_STATIC" in boards.local.txt (create
 ** if it doesn't exist) and copy this file to
 ** `<ARDUINO_CORE_INSTALL_PATH>/arduino/hardware/<mbed_core>/<core_version>/`.
 **
 ** See
 ** (https://support.arduino.cc/hc/en-us/articles/360012076960-Where-are-the-installed-cores-located-)
 ** to find where Arduino installs cores on your machine.
 **
 ** If the problem persists then there's not enough memory for this model and application.
 */

#define ALIGN_PTR(p,a)   ((p & (a-1)) ?(((uintptr_t)p + a) & ~(uintptr_t)(a-1)) : p)

/* Edge Impulse ------------------------------------------------------------- */

typedef struct {
    size_t width;
    size_t height;
} ei_device_resize_resolutions_t;

/**
 * @brief      Check if new serial data is available
 *
 * @return     Returns number of available bytes
 */
int ei_get_serial_available(void) {
    return Serial.available();
}

/**
 * @brief      Get next available byte
 *
 * @return     byte
 */
char ei_get_serial_byte(void) {
    return Serial.read();
}

/* Private variables ------------------------------------------------------- */
LoRaModem modem;
static bool debug_nn = false; // Set this to true to see e.g. features generated from the raw signal
static bool is_initialised = false;
static bool is_ll_initialised = false;
HM01B0 himax;
static Camera cam(himax);
FrameBuffer fb;


/*
** @brief points to the output of the capture
*/
static uint8_t *ei_camera_capture_out = NULL;

/*
** @brief used to store the raw frame
*/
#if defined(EI_CAMERA_FRAME_BUFFER_SDRAM) || defined(EI_CAMERA_FRAME_BUFFER_HEAP)
static uint8_t *ei_camera_frame_mem;
static uint8_t *ei_camera_frame_buffer; // 32-byte aligned
#else
static uint8_t ei_camera_frame_buffer[EI_CAMERA_RAW_FRAME_BUFFER_COLS * EI_CAMERA_RAW_FRAME_BUFFER_ROWS] __attribute__((aligned(32)));
#endif

/* Function definitions ------------------------------------------------------- */
bool ei_camera_init(void);
void ei_camera_deinit(void);
bool ei_camera_capture(uint32_t img_width, uint32_t img_height, uint8_t *out_buf) ;
int calculate_resize_dimensions(uint32_t out_width, uint32_t out_height, uint32_t *resize_col_sz, uint32_t *resize_row_sz, bool *do_resize);

void Debugln(String message);
void Debugln(String message, String info);
void Debug(String message);
String EI_IMPULSE_ERROR_ToString(EI_IMPULSE_ERROR error);

/**
* @brief      Arduino setup function
*/
void setup()
{
    // put your setup code here, to run once:
    Serial.begin(115200);
    Serial1.begin(115200);
    // comment out the below line to cancel the wait for USB connection (needed for native USB)
    // while (!Serial);
    Debugln("Edge Impulse Inferencing - Icicle Monitor\r\n");

#ifdef EI_CAMERA_FRAME_BUFFER_SDRAM
    // initialise the SDRAM
    SDRAM.begin(SDRAM_START_ADDRESS);
#endif

    if (ei_camera_init() == false) {
        Debugln("Failed to initialize Camera!");
    }
    else {
        Debugln("Camera initialized");
    }

    for (size_t ix = 0; ix < ei_dsp_blocks_size; ix++) {
        ei_model_dsp_t block = ei_dsp_blocks[ix];
        if (block.extract_fn == &extract_image_features) {
            ei_dsp_config_image_t config = *((ei_dsp_config_image_t*)block.config);
            int16_t channel_count = strcmp(config.channels, "Grayscale") == 0 ? 1 : 3;
            if (channel_count == 3) {
                Debugln("WARN: You've deployed a color model, but the Arduino Portenta H7 only has a monochrome image sensor. Set your DSP block to 'Grayscale' for best performance.");
                break; // only print this once
            }
        }
    }

    // change this to your regional band (eg. US915, AS923, ...)
    if (!modem.begin(EU868)) {
      Serial.println("Failed to start module");
      while (1) {}
    };
    Debugln("Your module version is: ");
    Debugln(modem.version().toString());
    Debugln("Your device EUI is: ");
    Debugln(modem.deviceEUI());

    int connected = modem.joinOTAA(appEui, appKey);
    if (!connected) {
      Debugln("Something went wrong; are you indoor? Move near a window and retry");
      while (1) {}
    }

    // Set poll interval to 60 secs.
    modem.minPollInterval(60);
    // NOTE: independently by this setting the modem will
    // not allow to send more than one message every 2 minutes,
    // this is enforced by firmware and can not be changed.
}

/**
* @brief      Get data and run inferencing
*
* @param[in]  debug  Get debug info if true
*/
void loop()
{
    Debugln("\nStarting inferencing in 10 seconds...\n");

    // instead of wait_ms, we'll wait on the signal, this allows threads to cancel us...
    if (ei_sleep(10000) != EI_IMPULSE_OK) {
        return;
    }

    Debugln("Taking photo...\n");

    ei::signal_t signal;
    signal.total_length = EI_CLASSIFIER_INPUT_WIDTH * EI_CLASSIFIER_INPUT_HEIGHT;
    signal.get_data = &ei_camera_cutout_get_data;

    if (ei_camera_capture((size_t)EI_CLASSIFIER_INPUT_WIDTH, (size_t)EI_CLASSIFIER_INPUT_HEIGHT, NULL) == false) {
        Debugln("Failed to capture image");
        return;
    }

    // Run the classifier
    ei_impulse_result_t result = { 0 };

    EI_IMPULSE_ERROR err = run_classifier(&signal, &result, debug_nn);
    if (err != EI_IMPULSE_OK) {
        Debugln("ERR: Failed to run classifier (%d)", EI_IMPULSE_ERROR_ToString(err));
        return;
    }

    // print the predictions
    String prediction = "Predictions (DSP: " + String(result.timing.dsp) +
                 " ms., Classification: " + String(result.timing.classification) +
                 " ms., Anomaly: " + String(result.timing.anomaly) + " ms.): ";
    Debugln(prediction);
#if EI_CLASSIFIER_OBJECT_DETECTION == 1
    bool bb_found = result.bounding_boxes[0].value > 0;
    for (size_t ix = 0; ix < result.bounding_boxes_count; ix++) {
        auto bb = result.bounding_boxes[ix];
        if (bb.value == 0) {
            continue;
        }

        String prediction = "     " + String(bb.label) + "(" + String(bb.value, 2) + ") [ x: " + String(bb.x) + ", y: " + String(bb.y) + ", width: " + String(bb.width) + ", height: " + String(bb.height) + " ]";
        Debugln(prediction);
    }

    if(bb_found) {
      int lora_err;
      modem.setPort(1);
      modem.beginPacket();
      modem.write((uint8_t)1); // This sends the binary value 0x01
      lora_err = modem.endPacket(true);
      if (lora_err > 0) {
        Debugln("Message sent correctly!");
      } else {
        Debugln("Error sending message: ", String(lora_err));
        Debugln("(you may send a limited amount of messages per minute, depending on the signal strength");
        Debugln("it may vary from 1 message every couple of seconds to 1 message every minute)");
      }
    }

    if (!bb_found) {
        Debugln("    No objects found");
    }
#else
    for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
        Debug(String("    %s: " + result.classification[ix].label);
        Debugln(String(result.classification[ix].value));
    }
#if EI_CLASSIFIER_HAS_ANOMALY == 1
    Debug("    anomaly score: ");
    Debugln(String(result.anomaly));
#endif
#endif
}

void Debugln(String message) {
  Serial.println(message);
  Serial1.println(message);
}

void Debugln(String message, String info) {
  String combinedMessage = message + " " + info;
  Serial.println(combinedMessage);
  Serial1.println(combinedMessage);
}

void Debug(String message) {
  Serial.print(message);
  Serial1.print(message);
}

String EI_IMPULSE_ERROR_ToString(EI_IMPULSE_ERROR error) {
    switch (error) {
        case EI_IMPULSE_OK: return "OK";
        case EI_IMPULSE_ERROR_SHAPES_DONT_MATCH: return "Error: Shapes don't match";
        case EI_IMPULSE_CANCELED: return "Canceled";
        case EI_IMPULSE_TFLITE_ERROR: return "TFLite error";
        case EI_IMPULSE_DSP_ERROR: return "DSP error";
        case EI_IMPULSE_TFLITE_ARENA_ALLOC_FAILED: return "TFLite arena alloc failed";
        case EI_IMPULSE_CUBEAI_ERROR: return "CubeAI error";
        case EI_IMPULSE_ALLOC_FAILED: return "Alloc failed";
        case EI_IMPULSE_ONLY_SUPPORTED_FOR_IMAGES: return "Only supported for images";
        case EI_IMPULSE_UNSUPPORTED_INFERENCING_ENGINE: return "Unsupported inferencing engine";
        case EI_IMPULSE_OUT_OF_MEMORY: return "Out of memory";
        case EI_IMPULSE_INPUT_TENSOR_WAS_NULL: return "Input tensor was null";
        case EI_IMPULSE_OUTPUT_TENSOR_WAS_NULL: return "Output tensor was null";
        case EI_IMPULSE_SCORE_TENSOR_WAS_NULL: return "Score tensor was null";
        case EI_IMPULSE_LABEL_TENSOR_WAS_NULL: return "Label tensor was null";
        case EI_IMPULSE_TENSORRT_INIT_FAILED: return "TensorRT init failed";
        case EI_IMPULSE_DRPAI_INIT_FAILED: return "DRPAI init failed";
        case EI_IMPULSE_DRPAI_RUNTIME_FAILED: return "DRPAI runtime failed";
        case EI_IMPULSE_DEPRECATED_MODEL: return "Deprecated model";
        case EI_IMPULSE_LAST_LAYER_NOT_AVAILABLE: return "Last layer not available";
        case EI_IMPULSE_INFERENCE_ERROR: return "Inference error";
        case EI_IMPULSE_AKIDA_ERROR: return "Akida error";
        case EI_IMPULSE_INVALID_SIZE: return "Invalid size";
        case EI_IMPULSE_ONNX_ERROR: return "ONNX error";
        case EI_IMPULSE_MEMRYX_ERROR: return "MemryX error";
        default: return "Unknown error";
    }
}

/**
 * @brief   Setup image sensor & start streaming
 *
 * @retval  false if initialisation failed
 */
bool ei_camera_init(void) {
    if (is_initialised) return true;

    if (is_ll_initialised == false) {
        if (!cam.begin(CAMERA_R320x240, CAMERA_GRAYSCALE, 30)) {
            Debugln("ERR: Failed to initialise camera");
            return false;
        }

    #ifdef EI_CAMERA_FRAME_BUFFER_SDRAM
        ei_camera_frame_mem = (uint8_t *) SDRAM.malloc(EI_CAMERA_RAW_FRAME_BUFFER_COLS * EI_CAMERA_RAW_FRAME_BUFFER_ROWS + 32 /*alignment*/);
        if(ei_camera_frame_mem == NULL) {
            Debugln("failed to create ei_camera_frame_mem");
            return false;
        }
        ei_camera_frame_buffer = (uint8_t *)ALIGN_PTR((uintptr_t)ei_camera_frame_mem, 32);
    #endif

        is_ll_initialised = true;
    }

    // initialize frame buffer
#if defined(EI_CAMERA_FRAME_BUFFER_HEAP)
    ei_camera_frame_mem = (uint8_t *) ei_malloc(EI_CAMERA_RAW_FRAME_BUFFER_COLS * EI_CAMERA_RAW_FRAME_BUFFER_ROWS + 32 /*alignment*/);
    if(ei_camera_frame_mem == NULL) {
        Debugln("failed to create ei_camera_frame_mem");
        return false;
    }
    ei_camera_frame_buffer = (uint8_t *)ALIGN_PTR((uintptr_t)ei_camera_frame_mem, 32);
#endif

    fb.setBuffer(ei_camera_frame_buffer);
    is_initialised = true;

    return true;
}

/**
 * @brief      Stop streaming of sensor data
 */
void ei_camera_deinit(void) {

#if defined(EI_CAMERA_FRAME_BUFFER_HEAP)
    ei_free(ei_camera_frame_mem);
    ei_camera_frame_mem = NULL;
    ei_camera_frame_buffer = NULL;
#endif

    is_initialised = false;
}

/**
 * @brief      Capture, rescale and crop image
 *
 * @param[in]  img_width     width of output image
 * @param[in]  img_height    height of output image
 * @param[in]  out_buf       pointer to store output image, NULL may be used
 *                           if ei_camera_frame_buffer is to be used for capture and resize/cropping.
 *
 * @retval     false if not initialised, image captured, rescaled or cropped failed
 *
 */
bool ei_camera_capture(uint32_t img_width, uint32_t img_height, uint8_t *out_buf) {
    bool do_resize = false;
    bool do_crop = false;

    if (!is_initialised) {
        Debugln("ERR: Camera is not initialized");
        return false;
    }

    int snapshot_response = cam.grabFrame(fb, 3000);
    if (snapshot_response != 0) {
        Debugln("ERR: Failed to get snapshot (%d)", String(snapshot_response));
        return false;
    }

    uint32_t resize_col_sz;
    uint32_t resize_row_sz;
    // choose resize dimensions
    int res = calculate_resize_dimensions(img_width, img_height, &resize_col_sz, &resize_row_sz, &do_resize);
    if (res) {
        Debugln(String("ERR: Failed to calculate resize dimensions ") + String(res));
        return false;
    }

    if ((img_width != resize_col_sz)
        || (img_height != resize_row_sz)) {
        do_crop = true;
    }

    // The following variables should always be assigned
    // if this routine is to return true
    // cutout values
    ei_camera_capture_out = ei_camera_frame_buffer;

    if (do_resize) {

        // if only resizing then and out_buf provided then use itinstead.
        if (out_buf && !do_crop) ei_camera_capture_out = out_buf;

        //ei_printf("resize cols: %d, rows: %d\r\n", resize_col_sz,resize_row_sz);
        ei::image::processing::resize_image(
            ei_camera_frame_buffer,
            EI_CAMERA_RAW_FRAME_BUFFER_COLS,
            EI_CAMERA_RAW_FRAME_BUFFER_ROWS,
            ei_camera_capture_out,
            resize_col_sz,
            resize_row_sz,
            1); // bytes per pixel
    }

    if (do_crop) {
        uint32_t crop_col_sz;
        uint32_t crop_row_sz;
        uint32_t crop_col_start;
        uint32_t crop_row_start;
        crop_row_start = (resize_row_sz - img_height) / 2;
        crop_col_start = (resize_col_sz - img_width) / 2;
        crop_col_sz = img_width;
        crop_row_sz = img_height;

        // if (also) cropping and out_buf provided then use it instead.
        if (out_buf) ei_camera_capture_out = out_buf;

        //ei_printf("crop cols: %d, rows: %d\r\n", crop_col_sz,crop_row_sz);
        ei::image::processing::cropImage(
            ei_camera_frame_buffer,
            resize_col_sz,
            resize_row_sz,
            crop_col_start,
            crop_row_start,
            ei_camera_capture_out,
            crop_col_sz,
            crop_row_sz,
            8); // bits per pixel
    }

    return true;
}

/**
 * @brief      Convert monochrome data to rgb values
 *
 * @param[in]  mono_data  The mono data
 * @param      r          red pixel value
 * @param      g          green pixel value
 * @param      b          blue pixel value
 */
static inline void mono_to_rgb(uint8_t mono_data, uint8_t *r, uint8_t *g, uint8_t *b) {
    uint8_t v = mono_data;
    *r = *g = *b = v;
}


int ei_camera_cutout_get_data(size_t offset, size_t length, float *out_ptr) {
    size_t bytes_left = length;
    size_t out_ptr_ix = 0;

    // read byte for byte
    while (bytes_left != 0) {

        // grab the value and convert to r/g/b
        uint8_t pixel = ei_camera_capture_out[offset];

        uint8_t r, g, b;
        mono_to_rgb(pixel, &r, &g, &b);

        // then convert to out_ptr format
        float pixel_f = (r << 16) + (g << 8) + b;
        out_ptr[out_ptr_ix] = pixel_f;

        // and go to the next pixel
        out_ptr_ix++;
        offset++;
        bytes_left--;
    }

    // and done!
    return 0;
}

/**
 * @brief      Determine whether to resize and to which dimension
 *
 * @param[in]  out_width     width of output image
 * @param[in]  out_height    height of output image
 * @param[out] resize_col_sz       pointer to frame buffer's column/width value
 * @param[out] resize_row_sz       pointer to frame buffer's rows/height value
 * @param[out] do_resize     returns whether to resize (or not)
 *
 */
int calculate_resize_dimensions(uint32_t out_width, uint32_t out_height, uint32_t *resize_col_sz, uint32_t *resize_row_sz, bool *do_resize)
{
    size_t list_size = 6;
    const ei_device_resize_resolutions_t list[list_size] = {
        {128, 96},
        {160, 120},
        {200, 150},
        {256, 192},
        {320, 240},
    };

    // (default) conditions
    *resize_col_sz = EI_CAMERA_RAW_FRAME_BUFFER_COLS;
    *resize_row_sz = EI_CAMERA_RAW_FRAME_BUFFER_ROWS;
    *do_resize = false;

    for (size_t ix = 0; ix < list_size; ix++) {
        if ((out_width <= list[ix].width) && (out_height <= list[ix].height)) {
            *resize_col_sz = list[ix].width;
            *resize_row_sz = list[ix].height;
            *do_resize = true;
            break;
        }
    }

    return 0;
}

#if !defined(EI_CLASSIFIER_SENSOR) || EI_CLASSIFIER_SENSOR != EI_CLASSIFIER_SENSOR_CAMERA
#error "Invalid model for current sensor"
#endif
