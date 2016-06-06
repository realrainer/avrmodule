# avrmodule
Node.js multithread C++ module for capture and decode video streams. 
Only HTTP and RTSP input video streams are supported, only h264 and mjpeg input codecs are supported. Output stream may saved or encoded to mpeg1 and used to display in browser with [https://github.com/phoboslab/jsmpeg](jsmpeg) library

### Required:
* [http://www.live555.com](live555) with headers and shared libraries
* [https://ffmpeg.org](ffmpeg) with headers and shared libraries
* libcurl with headers and shared libraries

### Example usage:
Capture RTSP video stream from url rtsp://10.0.10.2:554/h264 with resolution 1280x720 and save to separate files: raw stream, jpeg thumbnail (320x180) and mpeg1 encoded stream
```
var avr = require('./avrmodule/build/Release/avrmodule');
var fs = require('fs');

var camCount = 1;
process.env.UV_THREADPOOL_SIZE = 5 + camCount * 2;

h264stream = fs.createWriteStream("./stream.h264");
mpeg1stream = fs.createWriteStream("./stream.mpeg1video");

cam01 = new avr.aRTSPInput("rtsp://10.0.10.2:554/h264", "h264");
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
