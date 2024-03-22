# Rooftop ice buildup detection using Edge Impulse with synthetic data created with NVIDIA Omniverse Replicator

## Intro
This portable device monitors buildings and warns when potential hazardous icicles are formed. In ideal conditions icicles can form at a rate of [more than 1 cm (0.39 in) per minute](https://en.wikipedia.org/wiki/Icicle). As numerous people are injured and killed by these solid projectiles each year, responsible building owners often close sidewalks in the spring to minimize risk. This project demonstrates how an extra set of digital eyes can notify property owners icicles are forming and need to be removed before they can cause harm.

## Hardware used:
* [Arduino Portena H7](https://docs.arduino.cc/hardware/portenta-h7/)
* [Arduino Portena Vision Shield w/LoRa Connectivity](https://docs.arduino.cc/hardware/portenta-vision-shield/)
* NVIDIA GeForce RTX
* Formlabs Form 2 3D printer

## Software used:
* [Edge Impulse Studio](https://studio.edgeimpulse.com/studio)
* [NVIDIA Omniverse Code](https://www.nvidia.com/en-us/omniverse/) with [Replicator](https://developer.nvidia.com/omniverse/replicator)
* [Visual Studio Code](https://code.visualstudio.com/)
* [Blender](https://www.blender.org/)
* [Autodesk Fusion 360](https://www.autodesk.no/products/fusion-360/)

## Code and machine learning repository
Project [Impulse](https://studio.edgeimpulse.com/public/332581/latest) and [code repository](https://github.com/eivholt/icicle-monitor).

## Working principle
Forming icicles are detected using a neural network with an architecture aimed at detecting objects in images from the on-board camera. The NN is trained and tested exclusively on synthesized images. The images are generated with realistic simulated lighting conditions. A small amount of real images are used to verify the model.

## Challenges
The main challenge of detecting forming icicles is the transparent nature of ice. Because of this we need a great number of images to train a model that captures enough features of the ice with varying lighting conditions. We can mitigate this problem by synthesizing a lot of images, but we need to be able to vary lighting conditions in a realistic manner.

## Mobility
A powerful platform combined with a high resolution camera with fish-eye lense would increase the ability to detect icicles. However, by implementing the object detection model on a small, power-efficient, but highly constrained device, options for deployment increase. Properly protected against moisture this device can be mounted outdoors on poles facing roofs in question. LoRaWAN communication enables low battery consumption and long transmission range.

## Limitations
### Weatherproofing
The device enclosure is not properly sealed for permanent outdoor installation. The camera is mounted on the shield PCB and will need some engineering to be able to see through the enclosure while remaining water tight. For inspiration on how to create weather-proof enclosures that allow sensors and antennas outside access, [see this project](https://www.hackster.io/eivholt/low-power-snow-depth-sensor-using-lora-e5-b8e7b8) on friction fitting and use of rubber washers. The project in question also proves that battery operated sensors can work with no noticible degradation in winter conditions (to at least -15 degrees Celcius).

### Obscured view
The project has no safe-guard against false negatives. The device will not report if it's view is blocked. This could be resolved by placing static markers on both sides of an area to monitor and included in synthetic training data. Absence of at least one marker could trigger a notification that the view is obscured.

### Object scale
Due to optimization techniques in Faster Objects - More Objects (FoMo) determining relative sizes of the icicles is not feasible. As even icicles with small mass can be harmful at moderate elevation this is not a crucial feature.

### Exact number of icicles
The object detection model has not been trained to give an exact number of icicles in view. This has no practical implication other than the model verification results appearing worse than practical performance.

### Grayscale
To be able to compile a representation of our neural network and have it run on the severely limited amount of RAM available on the Arduino Portena H7, pixel representation has been limited to a single channel - grayscale. Colors are not needed to detect icicles so this will not affect the results.

## Object detection using neural networks
[FOMO (Faster Objects, More Objects)](https://docs.edgeimpulse.com/docs/edge-impulse-studio/learning-blocks/object-detection/fomo-object-detection-for-constrained-devices) is a novel machine learning algorithm that allows for visual object detection on highly constrained devices through training of a neural network with a number of convolutional layers.

![](img/EILogo.svg "Edge Impulse")

### Capturing training data and labeling objects
One of the most labor intensive aspects of building any machine learning model is gathering the training data and to label it. For an object detection model this requires taking hundreds or thousands of images of the objects to detect, drawing rectangles around them and choosing the correct label for each class. Recently generating pre-labeled images has become feasible and has proven great results. This is referred to as synthetic data generation with domain randomization. In this project a model will be trained exclusively on synthetic data and we will see how it can detect the real life counterparts.

### Domain randomization using NVIDIA Omniverse Replicator
NVIDIA Omniverse Code is an IDE that allows us to compose 3D scenes and to write simple Python code to capture images. Further, the extention Replicator is a toolkit that allows us to label the objects in the images and to simplify common domain randomization tasks, such as scattering objects between images. For an in-depth walkthrough on getting started with Omniverse and Replicator [see this article](https://docs.edgeimpulse.com/experts/featured-machine-learning-projects/surgery-inventory-synthetic-data).

### Making a scene
It's possible to create an empty scene in Omniverse and add content programmatically. However, composing initial objects by hand serves as a practical starting point. In this project a royalty free 3D model of a house was used as a basis.

### Icicles

### Randomizing colors
The surface behind the icicles may vary greatly, both in color and texture. Using Replicator randomizing the color of an objects material is easy.

In the scene in Omniverse either manually create a plane behind the icicles, or create one programmatically.

In code, define a function that takes in a reference to the plane we want to randomize the color of and use one of the distribution functions with min and max value span:

```python
def randomize_screen(screen):
		with screen:
			# Randomize each RGB channel for the whole color spectrum.
            rep.randomizer.color(colors=rep.distribution.uniform((0, 0, 0), (1, 1, 1)))
		return screen.node
```

Then get a reference to the plane:

```python
screen = rep.get.prims(path_pattern='/World/Screen')
```

Lastly register the function and trigger it on each new frame:

```python
rep.randomizer.register(randomize_screen)
with rep.trigger.on_frame(num_frames=2000, rt_subframes=50):  # rt_subframes=50
        # Other randomization functions...
		rep.randomizer.randomize_screen(screen)
```

Now each image will have a background with random (deterministic, same starting seed) RGB color. Replicator takes care of creating a material with a shader for us. As you might remember, in an effort to reduce RAM usage our neural network reduces RGB color channels to grayscale. In this project we could simplify the color randomization to only pick grayscale colors. The example has been included as it would benefit in projects where color information is not reduced. To only randomize in grayscale, we could change the code in the randomization function to use the same value for R, G and B as follows:

```python
def randomize_screen(screen):
		with screen:
			# Generate a single random value for grayscale
			gray_value = rep.distribution.uniform(0, 1)
			# Apply this value across all RGB channels to ensure the color is grayscale
			rep.randomizer.color(colors=gray_value)
		return screen.node
```

### Randomizing textures
To further steer training of the object detection model in capturing features of the desired class, the icicles, and not features that appear due to short commings in the domain randomization, we can create images with the icicles in front of a large variety of background images. A simple way of achieving this is to use a large dataset of random images and randomly assigning one of them to a background plane for each image generated.


```python
import os

def randomize_screen(screen, texture_files):
		with screen:
			# Let Replicator pick a random texture from list of .jpg-files
			rep.randomizer.texture(textures=texture_files)
		return screen.node

# Define what folder to look for .jpg files in
folder_path = 'C:/Users/eivho/source/repos/icicle-monitor/val2017/testing/'
# Create a list of strings with complete path and .jpg file names
texture_files = [os.path.join(folder_path, f) for f in os.listdir(folder_path) if f.endswith('.jpg')]

# Register randomizer
rep.randomizer.register(randomize_screen)

# For each frame, call randomization function
with rep.trigger.on_frame(num_frames=2000, rt_subframes=50):
    # Other randomization functions...
    rep.randomizer.randomize_screen(screen, texture_files)
```

We could instead generate textures with random shapes and colors. Either way, the resulting renders will look weird, but help the model training process weight features that are relevant for the icicles, not the background.

These are rather unsofisticated approaches. More realistic results would be achieved by changing the [materials](https://docs.omniverse.nvidia.com/materials-and-rendering/latest/materials.html) of the actual walls of the house used as background. Omniverse has a large selection of available materials available in the NVIDIA Assets browser, allowing us to randomize a [much wider range of aspects](https://docs.omniverse.nvidia.com/extensions/latest/ext_replicator/randomizer_details.html) of the rendered results.

## Deployment to device and LoRaWAN
### Testing model on device using OpenMV
To get visual verification our model works as intended we can go to Deployment in Edge Impulse Studio, select **OpenMV Firmware** as target and build. 

![](img/OpenMV_deployment.png "Edge Impulse Studio Deployment OpenMV Firmware")

Follow the [documentation](https://docs.edgeimpulse.com/docs/run-inference/running-your-impulse-openmv) on how to flash the device and to modify the ei_object_detection.py code. Remember to change: sensor.set_pixformat(sensor.GRAYSCALE)! The file edge_impulse_firmware_arduino_portenta.bin is our firmware for the Arduino Portenta H7 with Vision shield.

![](img/OpenMV-testing.png "Testing model on device with OpenMV")

### Deploy model as Arduino compatible library and send inference results to The Things Network with LoRaWAN
Start by selecting Arduino library as Deployment target.

![](img/EI-arduino-library.png "Deploy model as Arduino compatible library")

Once built and downloaded, open Arduino IDE, go to **Sketch> Include Library> Add .zip Library ...** and locate the downloaded library. Next go to **File> Examples> [name of project]_inferencing> portenta_h7> portenat_h7_camera** to open a generic sketch template using our model. To test the model continuously and print the results to console this sketch is ready to go. The code might appear daunting, but we really only need to focus on the loop() function.

![](img/EI-arduino-library-example.png "Arduino compatible library example sketch")

### Transmit results to The Things Stack sandbox using LoRaWAN
Using The Things Stack sandbox (formely known as The Things Network) we can create a low-power sensor network that allows transmitting device data with minimal energy consumption, long range without network fees. Your area might already be covered by a crowd funded network, or you can [initiate your own](https://www.thethingsnetwork.org/community/bodo/). [Getting started with LoRaWAN](https://www.thethingsindustries.com/docs/getting-started/) is really fun!

![](img/ttn-map.png "The Things Network")

Following the [Arduino guide](https://docs.arduino.cc/tutorials/portenta-vision-shield/connecting-to-ttn/) we create an application in The Things Stack sandbox and register our first device.

![](img/ttn-app.png "The Things Stack application")

![](img/ttn-device.png "The Things Stack device")

Next we will simplify things by merging an example Arduino sketch for transmitting a LoRaWAN-message with the Edge Impulse generated object detection model code. Open the example sketch called LoraSendAndReceive included with the MKRWAN(v2) library mentioned in the [Arduino guide](https://docs.arduino.cc/tutorials/portenta-vision-shield/connecting-to-ttn/). In the [project code repository](https://github.com/eivholt/icicle-monitor/tree/main/portenta-h7/portenta_h7_camera_lora) we can find an Arduino sketch witht the merged code.

![](img/arduino-lora.png "Arduino transmitting inference results over LoRaWAN")

In short we perform inference evary 10 seconds. If any icicles are detected we simply transmit a binary 1 to the The Things Stack application.

```python
if(bb_found) {
    int lora_err;
    modem.setPort(1);
    modem.beginPacket();
    modem.write((uint8_t)1); // This sends the binary value 0x01
    lora_err = modem.endPacket(true);
```

In the The Things Stack application we need to define a function that will be used to decode the byte into a JSON structure that is easier to interpet when we pass the message further up the chain of services. The function can be found in the [project code repository](https://github.com/eivholt/icicle-monitor/tree/main/portenta-h7/portenta_h7_camera_lora).

```javascript
function Decoder(bytes, port) {
    // Initialize the result object
    var result = {
        detected: false
    };

    // Check if the first byte is non-zero
    if(bytes[0] !== 0) {
        result.detected = true;
    }

    // Return the result
    return result;
}
```

* Integration with dashboards
* Sun studies
* Power profiling
* Basic Writer to pascal voc
* Markers, false negatives
* Compile time... 61736466



TinyML project to detect dangerous ice build-up on buildings.

https://www.youtube.com/watch?v=9H1gRQ6S7gg&t=23s
https://www.insidescience.org/news/riddles-rippled-icicle
https://link.brightcove.com/services/player/bcpid106573614001?bckey=AQ~~,AAAAGKlf6FE~,iSMGT5PckNvcgUb_ru5CAy2Tyv4G5OW3&bctid=2732728840001

3D:
Snow, particle effect
Building, random
Skybox
    https://youtu.be/MRD-oAxaV8w
    https://www.nvidia.com/en-us/on-demand/session/omniverse2020-om1417/

Icicle

## Sun studies
Window->Sun Study

## Semantics Schema Editor
Replicator->Semantics Schema Editor

Snow on roof
Passer-byes

Blender export:
Selection only
Convert Orientation:
Forward Axis: X
Up Axis: Y

Select vertex on model (Edit Mode), Shift+S-> Cursor to selected
(Object Mode) Select Hierarchy, Object>Set Origin\Origin to 3D Cursor
(Object Mode) Shift+S\Cursor to World Origin

