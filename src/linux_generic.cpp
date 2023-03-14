#include "blaze/capture/linux/generic.hpp"

#include <cstring>
#include <ctime>
#include <memory>
#include <cmath>
#include <chrono>

#include <xcb/shm.h>
#include <xcb/xcb.h>
#include <xcb/xcb_image.h>
#include <xcb/randr.h>

#include <sys/resource.h>
#include <sys/shm.h>
#include <unistd.h>
#include <fcntl.h>

#include <libyuv/scale.h>
#include <libyuv/convert.h>

#include "BS_thread_pool_light.hpp"

#include <iostream>

namespace blaze::internal {

    X11Capture::X11Capture() {
    }

    X11Capture::~X11Capture() {

        xcb_disconnect(conn);
    }

    void X11Capture::onErrorCallback(
        std::function<void(const char *, std::int32_t)> callback) {

        errHandler = callback;
    }

    void X11Capture::onNewFrame(
        std::function<void(void *, std::uint64_t)> callback) {

        newFrameHandler = callback;
    }

    void X11Capture::setRefreshRate(std::uint16_t fps) {

        refreshRate = fps;
    }

    void X11Capture::setResolution(std::uint16_t width, std::uint16_t height) {

        isResolutionSet = true;
        dstWidth = width;
        dstHeight = height;
    }

    void X11Capture::selectScreen(const std::string &screen) {

        auto it = screens.find(screen);
        if (it != screens.end()) {

            selectedCrtc = screens.at(screen);

        } else errHandler("Selected screen does not exist", -1);
    }

    std::vector<std::string> X11Capture::listScreen() {

        if (conn == nullptr && screens.size() == 0) {

            this->load();

            xcb_disconnect(conn);
            conn = nullptr;

            isInitialized = false;
        }

        std::vector<std::string> vec;
        vec.reserve(screens.size());

        for (const auto &[key, val] : screens) vec.emplace_back(key);

        return vec;
    }

    void X11Capture::updateScreenList() {

        if (screens.size() != 0) screens.clear();

        std::shared_ptr<xcb_randr_get_screen_resources_current_reply_t> reply(
            xcb_randr_get_screen_resources_current_reply(
                conn,
                xcb_randr_get_screen_resources_current(conn, screen->root),
                nullptr),
            free);

        xcb_timestamp_t timestamp = reply->config_timestamp;
        int len = xcb_randr_get_screen_resources_current_outputs_length(
            reply.get());
        xcb_randr_output_t *randr_outputs =
            xcb_randr_get_screen_resources_current_outputs(reply.get());

        screens.reserve(len);

        for (std::uint16_t i = 0; i < len; i++) {

            std::shared_ptr<xcb_randr_get_output_info_reply_t> output(
                xcb_randr_get_output_info_reply(
                    conn,
                    xcb_randr_get_output_info(conn, randr_outputs[i],
                                              timestamp),
                    nullptr),
                free);
            if (output == nullptr) continue;

            if (output->crtc == XCB_NONE ||
                output->connection == XCB_RANDR_CONNECTION_DISCONNECTED)
                continue;

            std::shared_ptr<xcb_randr_get_crtc_info_reply_t> crtc(
                xcb_randr_get_crtc_info_reply(
                    conn,
                    xcb_randr_get_crtc_info(conn, output->crtc, timestamp),
                    nullptr),
                free);


            const auto &name = "Screen " + std::to_string(screens.size());
            screens.emplace(name, crtc);
        }

        if (screens.size() > 0 && selectedCrtc == nullptr)
            selectedCrtc = screens.begin()->second;
        else if (screens.size() > 0 && selectedCrtc != nullptr) return;
        else errHandler("Cannot find connected monitor", -1);
    }

    void X11Capture::load() {

        conn = xcb_connect(nullptr, nullptr);

        if (xcb_connection_has_error(conn))
            errHandler("Failed to connect to X server", -1);

        const auto setup = xcb_get_setup(conn);

        screen = xcb_setup_roots_iterator(setup).data;

        this->updateScreenList();

        isInitialized = true;
    }

    void X11Capture::startCapture() {

        if (!isInitialized)
            errHandler("X11Capture::load() were not called or was executed "
                       "with errors",
                       -1);

        isScreenCaptured.store(true);

        const bool scale = isResolutionSet &&
                           (selectedCrtc->width != dstWidth &&
                            selectedCrtc->height != dstHeight);

        seg = xcb_generate_id(conn);
        shmid = shmget(IPC_PRIVATE,
                       selectedCrtc->width * selectedCrtc->height * 4,
                       IPC_CREAT | 0777);

        if (shmid == -1) errHandler("Cannot allocate shared memory", -1);

        xcb_shm_attach(conn, seg, shmid, false);


        const auto yuv420bufLength =
            (selectedCrtc->width * selectedCrtc->height * 3u) / 2u;
        const auto scaledBufSize = (dstWidth * dstHeight * 3u) / 2u;


        std::uint8_t *buffer = static_cast<std::uint8_t *>(
            shmat(shmid, nullptr, 0));

        std::uint8_t *yuv420buffer = nullptr;
        std::uint8_t *scaledBuf = nullptr;

        if (scale) {

            yuv420buffer = static_cast<std::uint8_t *>(
                malloc(yuv420bufLength + scaledBufSize));
            scaledBuf = yuv420buffer + yuv420bufLength;
        } else
            yuv420buffer = static_cast<std::uint8_t *>(malloc(yuv420bufLength));


        xcb_shm_get_image_cookie_t cookie;

        std::atomic<std::uint16_t> fps = 0u;
        std::atomic<bool> isFrameHandled = true;

        BS::thread_pool_light pool(2u);

        pool.push_task([&]() {
            for (;;) {

                std::this_thread::sleep_for(std::chrono::seconds(1));
                std::cout << '\r' << fps.load() << " fps" << std::flush;
                fps.store(0u);
            }
        });

        const auto stride_argb = selectedCrtc->width * 4u;

        const auto yuv420_u = yuv420buffer +
                              selectedCrtc->width * selectedCrtc->height;
        const auto yuv420_v = yuv420_u +
                              (selectedCrtc->width * selectedCrtc->height) / 4u;

        const auto stride_u = std::ceil(selectedCrtc->width / 2u / 16u) * 16u;

        const auto scaled_u = scaledBuf + dstWidth * dstHeight;
        const auto scaled_stride_u = dstWidth / 2;
        const auto scaled_v = scaled_u + (dstWidth * dstHeight) / 4u;

        constexpr std::uint16_t ms = 1'000.0f;
        const std::uint16_t timeBetweenFrames = ms / refreshRate;

        void *end_buffer = nullptr;
        auto end_length = 0;

        if (scale) {

            end_buffer = scaledBuf;
            end_length = scaledBufSize;

        } else {

            end_buffer = yuv420buffer;
            end_length = yuv420bufLength;
        }

        while (isScreenCaptured.load()) {

            auto startTime = std::chrono::high_resolution_clock::now();

            cookie = xcb_shm_get_image_unchecked(
                conn, screen->root, selectedCrtc->x, selectedCrtc->y,
                selectedCrtc->width, selectedCrtc->height, ~0,
                XCB_IMAGE_FORMAT_Z_PIXMAP, seg, 0);

            free(xcb_shm_get_image_reply(conn, cookie, nullptr));

            while (!isFrameHandled.load()) {
                std::this_thread::sleep_for(std::chrono::microseconds(500));
            }

            libyuv::ARGBToI420(buffer, stride_argb, yuv420buffer,
                               selectedCrtc->width, yuv420_u, stride_u,
                               yuv420_v, stride_u, selectedCrtc->width,
                               selectedCrtc->height);

            if (scale) {

                libyuv::I420Scale(yuv420buffer, selectedCrtc->width, yuv420_u,
                                  stride_u, yuv420_v, stride_u,
                                  selectedCrtc->width, selectedCrtc->height,
                                  scaledBuf, dstWidth, scaled_u,
                                  scaled_stride_u, scaled_v, scaled_stride_u,
                                  dstWidth, dstHeight, libyuv::kFilterBox);
            }

            pool.push_task([&]() {
                isFrameHandled.store(false);
                newFrameHandler(end_buffer, end_length);
                isFrameHandled.store(true);
            });

            auto elapsedTime =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::high_resolution_clock::now() - startTime)
                    .count();

            auto sleepTime = timeBetweenFrames - elapsedTime;
            if (sleepTime > 0)
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(sleepTime));

            fps.store(fps.load() + 1u);
        }

        free(yuv420buffer);
        shmdt(buffer);
    }

    void X11Capture::stopCapture() {

        isScreenCaptured.store(false);
    }


}; // namespace blaze::internal