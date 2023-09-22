# adv-stream

ADV/VN streaming server using [mongoose](https://github.com/cesanta/mongoose).

Streams MJPEG animation and static images on custom endpoints.

Along with image frames in JPEG, the server reads script file that is parsed by the client:
```
@src /red
Sample text.
@src /grey
Sample text.
```

[C++ client](client/main.cpp) uses SDL2 and [cpr](https://github.com/libcpr/cpr) and is made that it barely works.

JS client can be found in server [web_root](server/web_root/script.js).
