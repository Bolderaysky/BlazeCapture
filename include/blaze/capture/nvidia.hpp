#pragma once

#include <atomic>
#include <functional>
#include <cstdint>

#include <NvFBC.h>
#include "cuda.h"

#include "blaze/capture/nvidia/NvFBCUtils.h"

namespace blaze::internal {


    class NvfbcCapture {

        protected:
            void *libNVFBC = nullptr, *libCUDA = nullptr;
            NVFBC_API_FUNCTION_LIST pFn;
            NVFBCSTATUS fbcStatus;
            NVFBC_SESSION_HANDLE fbcHandle;

            std::uint8_t *buffer = nullptr;
            std::function<void(const char *, std::int32_t)> errHandler;
            std::function<void(std::uint8_t *, std::uint64_t)> newFrameHandler;
            std::uint16_t refreshRate = 60u;
            NVFBC_SIZE frameSize = {0u, 0u};

            std::atomic<bool> isScreenCaptured = false;
            bool isInitialized = false;

        public:
            NvfbcCapture();
            ~NvfbcCapture();

            void load();
            void setRefreshRate(std::uint16_t fps);
            void setResolution(std::uint16_t width, std::uint16_t height);
            void startCapture();
            void stopCapture();
            void onErrorCallback(
                std::function<void(const char *, std::int32_t)> callback);
            void onNewFrame(
                std::function<void(std::uint8_t *, std::uint64_t)> callback);
    };


}; // namespace blaze::internal