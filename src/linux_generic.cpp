#include "blaze/capture/linux/generic.hpp"

#include <ctime>
#include <xcb/present.h>
#include <xcb/shm.h>
#include <xcb/xcb.h>
#include <xcb/xcb_image.h>

#include <memory>
#include <xcb/xproto.h>

#include <unistd.h>
#include <fcntl.h>

#include <iostream>

#include "BS_thread_pool.hpp"

#include <sys/shm.h>

#include <libyuv/convert.h>

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
        scWidth = width;
        scHeight = height;
    }

    void X11Capture::load() {

        conn = xcb_connect(nullptr, nullptr);

        if (xcb_connection_has_error(conn))
            errHandler("Failed to connect to X server", -1);

        const auto setup = xcb_get_setup(conn);

        screen = xcb_setup_roots_iterator(setup).data;

        if (!isResolutionSet) {

            scWidth = screen->width_in_pixels;
            scHeight = screen->height_in_pixels;
        }

        seg = xcb_generate_id(conn);
        shmid = shmget(IPC_PRIVATE, scWidth * scHeight * 4, IPC_CREAT | 0777);

        if (shmid == -1) errHandler("Cannot allocate shared memory", -1);

        xcb_shm_attach(conn, seg, shmid, false);

        isInitialized = true;
    }

    void X11Capture::startCapture() {

        if (!isInitialized)
            errHandler("X11Capture::load() were not called or was executed "
                       "with errors",
                       -1);

        isScreenCaptured.store(true);

        constexpr std::uint16_t ms = 1'000.0f;
        const std::uint16_t timeBetweenFrames = ms / refreshRate;
        std::uint16_t execTime = 0u;

        std::clock_t before, after;

        std::uint8_t *buffer = static_cast<std::uint8_t *>(
            shmat(shmid, nullptr, 0));

        xcb_shm_get_image_cookie_t cookie;

        std::uint32_t yuv420bufLength = (scWidth * scHeight * 3u) / 2u;

        std::uint8_t *yuv420buffer = static_cast<std::uint8_t *>(
            malloc(yuv420bufLength));

        while (isScreenCaptured.load()) {

            before = clock();

            cookie = xcb_shm_get_image_unchecked(
                conn, screen->root, 0, 0, scWidth, scHeight, ~0,
                XCB_IMAGE_FORMAT_Z_PIXMAP, seg, 0);

            free(xcb_shm_get_image_reply(conn, cookie, nullptr));

            libyuv::ARGBToI420(buffer, scWidth * 4u, yuv420buffer, scWidth,
                               yuv420buffer + scWidth * scHeight, scWidth / 2u,
                               yuv420buffer + scWidth * scHeight +
                                   (scWidth * scHeight) / 4u,
                               scWidth / 2u, scWidth, scHeight);


            newFrameHandler(yuv420buffer, yuv420bufLength);

            after = clock();

            execTime = (after - before) / 1'000;

            if (execTime < timeBetweenFrames)
                usleep((timeBetweenFrames - execTime) * 1'000);
        }

        free(yuv420buffer);
        shmdt(buffer);
    }

    void X11Capture::stopCapture() {

        isScreenCaptured.store(false);
    }

    void X11Capture::setBufferFormat(blaze::format type) {

        format = type;
    }


}; // namespace blaze::internal