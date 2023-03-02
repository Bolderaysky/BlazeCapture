#pragma once

#include <atomic>
#include <functional>
#include <cstdint>

#include <xcb/dri3.h>
#include <xcb/randr.h>
#include <xcb/xcb.h>
#include <xcb/xcb_image.h>

#include "blaze/capture/linux/misc.hpp"

#include "tsl/bhopscotch_map.h"

namespace blaze::internal {

    class X11Capture {

        protected:
            std::function<void(const char *, std::int32_t)> errHandler;
            std::function<void(void *, std::uint64_t)> newFrameHandler;
            std::uint16_t refreshRate = 60u;

            std::shared_ptr<xcb_randr_get_crtc_info_reply_t> selectedCrtc;
            std::uint16_t dstWidth, dstHeight;

            std::atomic<bool> isScreenCaptured = false;
            bool isInitialized = false;
            bool isResolutionSet = false;

            xcb_connection_t *conn = nullptr;
            xcb_screen_t *screen;

            std::int32_t shmid;
            xcb_shm_seg_t seg;

            blaze::format format = yuv420p;

            tsl::bhopscotch_map<
                const char *, std::shared_ptr<xcb_randr_get_crtc_info_reply_t>>
                windows;

        public:
            X11Capture();
            ~X11Capture();

            void load();
            void setRefreshRate(std::uint16_t fps);
            void setResolution(std::uint16_t width, std::uint16_t height);
            void startCapture();
            void stopCapture();
            void onErrorCallback(
                std::function<void(const char *, std::int32_t)> callback);
            void
                onNewFrame(std::function<void(void *, std::uint64_t)> callback);

            void selectWindow(const char *window);
            std::vector<const char *> listWindows();


            void setBufferFormat(blaze::format type);
    };

    class WaylandCapture {

        protected:
            std::function<void(const char *, std::int32_t)> errHandler;
            std::function<void(void *, std::uint64_t)> newFrameHandler;
            std::uint16_t refreshRate = 60u;

            std::uint16_t scWidth, scHeight;
            std::uint16_t dstWidth, dstHeight;

            std::atomic<bool> isScreenCaptured = false;
            bool isInitialized = false;
            bool isResolutionSet = false;

        public:
            WaylandCapture();
            ~WaylandCapture();

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


}; // namespace blaze::internal