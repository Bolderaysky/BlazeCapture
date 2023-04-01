#pragma once

#ifdef _WIN32

#include "blaze/capture/win32/nvidia.hpp"
#include "blaze/capture/win32/amd.hpp"
#include "blaze/capture/win32/intel.hpp"
#include "blaze/capture/win32/generic.hpp"

#else

#include "blaze/capture/linux/nvidia.hpp"
#include "blaze/capture/linux/amd.hpp"
#include "blaze/capture/linux/intel.hpp"
#include "blaze/capture/linux/generic.hpp"

#endif

#include <memory>
#include <variant>

#include "tsl/bhopscotch_map.h"

namespace blaze {

    class VideoCapture {

        protected:
            std::function<void(const char*, std::int32_t)> errHandler;
            std::function<void(void*, std::uint64_t)> newFrameHandler;

            // tsl::bhopscotch_map<
            //     const char*,
            //     std::variant<internal::NvfbcCapture, internal::X11Capture>>
            //     objects;

        public:
            VideoCapture();
            ~VideoCapture();

            void selectBackend(const char* backendName);
            const char* getSelectedBackendName();

            void setRefreshRate(std::uint16_t fps);
            void setResolution(std::uint16_t width, std::uint16_t height);
            void startCapture();
            void stopCapture();
            void onErrorCallback(
                std::function<void(const char*, std::int32_t)> callback);
            void onNewFrame(std::function<void(void*, std::uint64_t)> callback);
    };

}; // namespace blaze