#pragma once

#include <atomic>
#include <functional>
#include <cstdint>

#include <GL/gl.h>
#include <GL/glx.h>

#include "NvFBC.h"
#include "nvEncodeAPI.h"
#include "NvFBCUtils.h"

namespace blaze {

    enum format : std::uint8_t {

        rgb,
        rgba,
        argb,
        bgra,
        yuv420p,
        yuv444p,
        nv12

    };
};

namespace blaze::internal {

    class NvfbcCapture {

        protected:
            void *libNVFBC = nullptr, *libEnc = nullptr;
            void *encoder = nullptr;

            NVFBC_API_FUNCTION_LIST pFn;
            NV_ENCODE_API_FUNCTION_LIST pEncFn;

            GLXContext glxCtx = None;
            GLXFBConfig glxFBConfig = None;

            NVFBCSTATUS fbcStatus;
            NVENCSTATUS encStatus;

            NVFBC_SESSION_HANDLE fbcHandle;

            _NVFBC_BUFFER_FORMAT bufferFormat = NVFBC_BUFFER_FORMAT_YUV420P;

            NV_ENC_OUTPUT_PTR outputBuffer = nullptr;
            NV_ENC_PIC_PARAMS encParams;
            NV_ENC_REGISTERED_PTR registeredResources[NVFBC_TOGL_TEXTURES_MAX] =
                {NULL};

            std::function<void(const char *, std::int32_t)> errHandler;
            std::function<void(void *, std::uint64_t)> newFrameHandler;
            std::uint16_t refreshRate = 60u;
            NVFBC_SIZE frameSize = {0u, 0u};

            std::atomic<bool> isScreenCaptured = false;
            bool isInitialized = false;

        public:
            NvfbcCapture();
            ~NvfbcCapture();

            void clear();
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

        protected:
            NVENCSTATUS validateEncodeGUID(void *encoder, GUID encodeGuid);
    };


}; // namespace blaze::internal