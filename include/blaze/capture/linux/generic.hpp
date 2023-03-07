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

            std::shared_ptr<xcb_randr_get_crtc_info_reply_t> selectedCrtc =
                nullptr;
            std::uint16_t dstWidth, dstHeight;

            std::atomic<bool> isScreenCaptured = false;
            bool isInitialized = false;
            bool isResolutionSet = false;

            xcb_connection_t *conn = nullptr;
            xcb_screen_t *screen;

            std::int32_t shmid;
            xcb_shm_seg_t seg;

            tsl::bhopscotch_map<
                std::string, std::shared_ptr<xcb_randr_get_crtc_info_reply_t>>
                screens;

        public:
            X11Capture();
            ~X11Capture();

            // Initialize xcb connection and retrieve screen list. Must be
            // called before startCapture()
            void load();

            // Set frame rate of capturing. 0 means no limit
            void setRefreshRate(std::uint16_t fps);

            // Set resolution. Frame capturing is done in native resolution and
            // then scaled to provided resolution
            void setResolution(std::uint16_t width, std::uint16_t height);

            // Start frame capturing. Function is blocking
            void startCapture();

            // Stop frame capturing. Can be called from any thread
            void stopCapture();

            // Provide callback which will be called on any error. Must be set
            // as early as possible
            void onErrorCallback(
                std::function<void(const char *, std::int32_t)> callback);

            // Provide callback which will be called inside startCapture() when
            // there will be frame
            void
                onNewFrame(std::function<void(void *, std::uint64_t)> callback);

            // Allow to select screen which will be captured
            void selectScreen(const std::string &screen);

            // Return list of all available screens
            std::vector<std::string> listScreen();

            // Update screen list. If there's no screen connected, calls
            // user-provided error handler
            void updateScreenList();
    };

}; // namespace blaze::internal