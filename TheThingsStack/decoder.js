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