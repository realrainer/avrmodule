# avrmodule
Node.js multithread module for capture and decode video streams. Only http and rtsp video streams are supported, only h264 and mjpeg codecs are supported. 

### Required
* live555 installed with headers and shared libraries
* ffmpeg
* libcurl 

### Example usage:
```
var avr = require('./avrmodule/build/Release/avrmodule');
var fs = require('fs');

var camCount = 1;
process.env.UV_THREADPOOL_SIZE = 5 + camCount * 2;

h264stream = fs.createWriteStream("./stream.h264");
mpeg1stream = fs.createWriteStream("./stream.mpeg1video");

cam01 = new avr.aRTSPInput("rtsp://10.16.8.48:554/h264", "h264");
cam01decoder = new avr.aStreamDecode(1280, 720, 320, 180, "h264", 16, 8);
cam01decoder.needVideo(1);

cam01.start(function(buf, keyFrame) {
    if (keyFrame) {
        console.log("Got keyframe\n");
    }
    h264stream.write(buf);
    cam01decoder.decode(buf);
});

cam01decoder.onPreview(function(buf) {
    thumb = fs.writeFile("./thumb.jpeg", buf);
});

cam01decoder.onVideo(function(buf) {
    mpeg1stream.write(buf);
});
```
