#include "blaze/capture/linux/generic.hpp"

#include <ctime>
#include <xcb/present.h>
#include <xcb/xcb.h>
#include <xcb/xcb_image.h>
#include <xcb/dri3.h>

#include <memory>
#include <xcb/xproto.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

#include <unistd.h>
#include <fcntl.h>

#include <iostream>

#include "BS_thread_pool.hpp"

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

        isInitialized = true;
    }

    void X11Capture::startCapture() {

        if (!isInitialized)
            errHandler("X11Capture::load() were not called or was executed "
                       "with errors",
                       -1);

        isScreenCaptured.store(true);

        constexpr float ms = 1'000.0f;
        const float timeBetweenFrames = ms / refreshRate;
        std::uint16_t sleepTime = 0u;

        std::size_t before, after;

        /*BS::thread_pool pool;

        std::atomic<std::uint16_t> fps = 0u;

        pool.push_task([&]() {
            while (true) {

                sleep(1);
                std::cout << fps.load() << " fps\n";
                fps.store(0u);
            }
        });*/

        while (isScreenCaptured.load()) {

            before = clock();

            std::shared_ptr<xcb_get_image_reply_t> image(xcb_get_image_reply(
                conn,
                xcb_get_image_unchecked(conn, XCB_IMAGE_FORMAT_Z_PIXMAP,
                                        screen->root, 0, 0, scWidth, scHeight,
                                        ~0),
                nullptr));

            const auto frame = xcb_get_image_data(image.get());
            const auto &length = xcb_get_image_data_length(image.get());

            newFrameHandler(static_cast<void *>(frame), length);

            // fps.store(fps.load() + 1);

            after = clock();

            sleepTime = timeBetweenFrames -
                        (float(after - before) / CLOCKS_PER_SEC) * 1'000.0f;

            // std::cout << sleepTime << '\n';

            if (sleepTime < timeBetweenFrames) usleep(sleepTime * 1'000);
        }
    }

    void X11Capture::stopCapture() {

        isScreenCaptured.store(false);
    }


}; // namespace blaze::internal