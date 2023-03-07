# BlazeCapture
## NOT FOR PRODUCTION USE YET
This is dirty and test project for now, so expect frequent changes of api, abi, things will be deleted a new added. For now, the most I can call this, is MVP.
## About Project
Welcome to the github page of **blazingly fast** screen recorder. It's screen and sound capturing overlay like `ShadowPlay` on Windows from Nvidia, but works on linux and not only on nvidia cards.
## What's done?
- Support for capturing last N minutes
- Start and stop capturing
- Resolution scaling
- Mic, desktop sound capturing
- API for using in your projects
## Support table
|   | Capturing tech. | Encoding tech. | Notes |
|---|------------|-------|----|
| Nvidia | Nvfbc | NvEnc/NvDec | On consumer-grade gpu's nvfbc needs to be unlocked using nvidia-patch |
| Generic (x11) | xcb + shm | VAAPI | Can be used on any linux system that is using x11
| Audio | Pipewire | libopus | Can be used for mic and desktop sound capturing together, only mic or only desktop sound

In general, generic capturing is slower than vendor-specific implementations. For example, Capturing using `Nvfbc` around 30-100 times faster and more efficient than using `xcb + shm`, so I strongly suggest using vendor-specific implementations for developers if possible.

## How it's developed?
Every implementation for capturing video/sound is treated as backend. For example, `Nvfbc` and `xcb + shm` is 2 separate backends, and `VideoCapture` class can automatically select which backend will be better to use.
This also means that if you want to contribute and develop new backend, maybe even for another OS, you need to only develop implementation by this template:
```cpp

    class backendName {

        protected:
            std::function<void(const char *, std::int32_t)> errHandler;
            std::function<void(void *, std::uint64_t)> newFrameHandler;
            std::uint16_t refreshRate = 60u;

            std::uint16_t scWidth, scHeight;

            std::atomic<bool> isScreenCaptured = false;
            bool isInitialized = false;
            bool isResolutionSet = false;

        public:
            backendName();
            ~backendName();

            void load();
            void setRefreshRate(std::uint16_t fps);
            void setResolution(std::uint16_t width, std::uint16_t height);
            void startCapture();
            void stopCapture();
            void onErrorCallback(
                std::function<void(const char *, std::int32_t)> callback);
            void
                onNewFrame(std::function<void(void *, std::uint64_t)> callback);
            void setBufferFormat(blaze::format type);
    };
```
This is example backend of video capturing.
`VideoCapture` will pass error callback and set callback for video frame.
