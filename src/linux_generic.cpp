#include "blaze/capture/linux/generic.hpp"

#include <cstring>
#include <ctime>
#include <memory>

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

    void X11Capture::selectScreen(const char *screen) {

        auto it = screens.find(screen);
        if (it != screens.end()) selectedWindow = screens.at(screen);
        else errHandler("Selected screen does not exist", -1);
    }

    std::vector<const char *> X11Capture::listScreen() {

        if (conn == nullptr && screens.size() == 0) {

            this->load();

            xcb_disconnect(conn);
            conn = nullptr;

            isLoaded = false;
            isInitialized = false;
        }

        std::vector<const char *> vec;
        vec.reserve(screens.size());

        for (const auto &[key, val] : screens) vec.emplace_back(key);

        return vec;
    }

    /*

        // Connect to the X server
        xcb_connection_t* conn = xcb_connect(nullptr, nullptr);

        // Get the root window
        xcb_screen_t* screen =
       xcb_setup_roots_iterator(xcb_get_setup(conn)).data; xcb_window_t root =
       screen->root;

        // Get the _NET_CLIENT_LIST_STACKING property
        xcb_atom_t net_client_list =
            xcb_intern_atom_reply(
                conn,
                xcb_intern_atom(conn, 0, strlen("_NET_CLIENT_LIST_STACKING"),
                                "_NET_CLIENT_LIST_STACKING"),
                nullptr)
                ->atom;
        xcb_get_property_cookie_t cookie = xcb_get_property(
            conn, 0, root, net_client_list, XCB_ATOM_WINDOW, 0, 1024);
        xcb_get_property_reply_t* reply = xcb_get_property_reply(conn, cookie,
                                                                 nullptr);

        // Iterate over the list of windows and add visible ones to a vector
        std::vector<xcb_window_t> visible_windows;
        xcb_window_t* windows = (xcb_window_t*)xcb_get_property_value(reply);
        int num_windows = xcb_get_property_value_length(reply) /
                          sizeof(xcb_window_t);
        for (int i = 0; i < num_windows; i++) {
            xcb_window_t window = windows[i];

            // Get the attributes of the window
            xcb_get_window_attributes_cookie_t attr_cookie =
                xcb_get_window_attributes(conn, window);
            xcb_get_window_attributes_reply_t* attr_reply =
                xcb_get_window_attributes_reply(conn, attr_cookie, nullptr);

            // Check if the window is visible
            if (attr_reply->map_state == XCB_MAP_STATE_VIEWABLE &&
                attr_reply->override_redirect == 0) {
                visible_windows.push_back(window);
            }

            free(attr_reply);
        }

        // Print the list of visible windows
        std::cout << "Visible windows:" << std::endl;
        for (auto window : visible_windows) {

            xcb_get_property_cookie_t cookie = xcb_get_property(
                conn, 0, window, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 0, 1024);
            xcb_get_property_reply_t* reply = xcb_get_property_reply(conn,
       cookie, nullptr);

            screens.emplace((const char*)xcb_get_property_value(reply),
       windows[i]);
        }

        // Clean up
        free(reply);
        xcb_disconnect(conn);

    */

    void X11Capture::updateScreenList() {

        xcb_atom_t net_client_list =
            xcb_intern_atom_reply(
                conn,
                xcb_intern_atom(conn, 0, strlen("_NET_CLIENT_LIST_STACKING"),
                                "_NET_CLIENT_LIST_STACKING"),
                nullptr)
                ->atom;
        xcb_get_property_cookie_t cookie = xcb_get_property(
            conn, 0, screen->root, net_client_list, XCB_ATOM_WINDOW, 0, 1024);
        std::shared_ptr<xcb_get_property_reply_t> reply(
            xcb_get_property_reply(conn, cookie, nullptr), free);

        xcb_window_t *windows = (xcb_window_t *)xcb_get_property_value(
            reply.get());
        const auto &num_windows = xcb_get_property_value_length(reply.get()) /
                                  sizeof(xcb_window_t);

        screens.clear();
        screens.reserve(num_windows);

        for (std::uint32_t i = 0; i < num_windows; ++i) {
            xcb_window_t window = windows[i];

            // Get the attributes of the window
            xcb_get_window_attributes_cookie_t attr_cookie =
                xcb_get_window_attributes(conn, window);
            std::shared_ptr<xcb_get_window_attributes_reply_t> attr_reply(
                xcb_get_window_attributes_reply(conn, attr_cookie, nullptr),
                free);

            // Check if the window is visible
            if (attr_reply->map_state == XCB_MAP_STATE_VIEWABLE &&
                attr_reply->override_redirect == 0) {

                xcb_get_property_cookie_t cookie = xcb_get_property(
                    conn, 0, window, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 0,
                    1024);
                xcb_get_property_reply_t *reply = xcb_get_property_reply(
                    conn, cookie, nullptr);

                auto name = (const char *)xcb_get_property_value(reply);

                if (strlen(name) == 0) continue;

                xcb_get_geometry_cookie_t geometry_cookie = xcb_get_geometry(
                    conn, windows[i]);
                std::shared_ptr<xcb_get_geometry_reply_t> geometry_reply(
                    xcb_get_geometry_reply(conn, geometry_cookie, nullptr),
                    free);
                if (!geometry_reply)
                    errHandler("Failed to get window geometry", -1);

                X11Window windowData;

                windowData.window = windows[i];
                windowData.geometry = geometry_reply;

                screens.emplace(name, windowData);
            }
        }
    }

    void X11Capture::load() {

        if (isLoaded) return;

        conn = xcb_connect(nullptr, nullptr);

        if (xcb_connection_has_error(conn))
            errHandler("Failed to connect to X server", -1);

        const auto setup = xcb_get_setup(conn);

        screen = xcb_setup_roots_iterator(setup).data;

        this->updateScreenList();

        if (screens.size() > 0) {

            selectedWindow = screens.begin()->second;

        } else errHandler("Cannot find connected monitor", -1);

        /* std::shared_ptr<xcb_randr_get_screen_resources_current_reply_t>
         reply( xcb_randr_get_screen_resources_current_reply( conn,
                 xcb_randr_get_screen_resources_current(conn, screen->root),
                 nullptr),
             free);

         xcb_timestamp_t timestamp = reply->config_timestamp;
         int len = xcb_randr_get_screen_resources_current_outputs_length(
             reply.get());
         xcb_randr_output_t *randr_outputs =
             xcb_randr_get_screen_resources_current_outputs(reply.get());

         for (int i = 0; i < len; i++) {

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

             screens.emplace(("screen-" + std::to_string(i)).c_str(), crtc);
         }

         if (screens.size() > 0) selectedCrtc = screens.at(0);
         else errHandler("Cannot find connected monitor", -1);*/

        isLoaded = true;
        isInitialized = true;
    }

    void X11Capture::startCapture() {

        if (!isInitialized)
            errHandler("X11Capture::load() were not called or was executed "
                       "with errors",
                       -1);

        isScreenCaptured.store(true);

        seg = xcb_generate_id(conn);
        shmid = shmget(IPC_PRIVATE,
                       selectedWindow.geometry->width *
                           selectedWindow.geometry->height * 4,
                       IPC_CREAT | 0777);

        if (shmid == -1) errHandler("Cannot allocate shared memory", -1);

        xcb_shm_attach(conn, seg, shmid, false);

        constexpr std::uint16_t ms = 1'000.0f;
        const std::uint16_t timeBetweenFrames = ms / refreshRate;
        const std::clock_t ticksBetweenFrames = timeBetweenFrames * 1'000u;

        std::clock_t before, after, execTime;

        std::uint8_t *buffer = static_cast<std::uint8_t *>(
            shmat(shmid, nullptr, 0));

        xcb_shm_get_image_cookie_t cookie;

        std::uint32_t yuv420bufLength = (selectedWindow.geometry->width *
                                         selectedWindow.geometry->height * 3u) /
                                        2u;

        std::uint8_t *yuv420buffer = static_cast<std::uint8_t *>(
            malloc(yuv420bufLength));

        BS::thread_pool_light pool(2u);

        std::atomic<std::uint16_t> fps = 0u;

        pool.push_task([&]() {
            for (;;) {

                sleep(1);
                printf("%d fps\n", fps.load());
                fps.store(0u);
            }
        });

        std::atomic<bool> isFrameHandled = true;

        const auto src_stride_argb = selectedWindow.geometry->width * 4u;
        const auto u = yuv420buffer + selectedWindow.geometry->width *
                                          selectedWindow.geometry->height;
        const auto stride_u = selectedWindow.geometry->width / 2u;
        const auto v =
            yuv420buffer +
            selectedWindow.geometry->width * selectedWindow.geometry->height +
            (selectedWindow.geometry->width * selectedWindow.geometry->height) /
                4u;

        bool scale = isResolutionSet &&
                     (selectedWindow.geometry->width != dstWidth &&
                      selectedWindow.geometry->height != dstHeight);

        std::uint8_t *scaledBuf = nullptr;

        const auto scaledBufSize = (dstWidth * dstHeight * 3u) / 2u;

        const auto src_u = yuv420buffer + selectedWindow.geometry->width *
                                              selectedWindow.geometry->height;
        const auto src_v = yuv420buffer + selectedWindow.geometry->width *
                                              selectedWindow.geometry->height *
                                              5 / 4;
        const auto dst_u = scaledBuf + dstWidth * dstHeight;
        const auto dst_stride_u = dstWidth / 2;
        const auto dst_v = scaledBuf + dstWidth * dstHeight * 5 / 4;

        if (scale) {

            scaledBuf = static_cast<std::uint8_t *>(malloc(scaledBufSize));
        }

        while (isScreenCaptured.load()) {

            before = clock();

            cookie = xcb_shm_get_image_unchecked(
                conn, selectedWindow.window, selectedWindow.geometry->x,
                selectedWindow.geometry->y, selectedWindow.geometry->width,
                selectedWindow.geometry->height, ~0, XCB_IMAGE_FORMAT_Z_PIXMAP,
                seg, 0);

            free(xcb_shm_get_image_reply(conn, cookie, nullptr));

            while (!isFrameHandled.load()) { usleep(500); }

            libyuv::ARGBToI420(buffer, src_stride_argb, yuv420buffer,
                               selectedWindow.geometry->width, u, stride_u, v,
                               stride_u, selectedWindow.geometry->width,
                               selectedWindow.geometry->height);

            if (scale) {

                libyuv::I420Scale(
                    yuv420buffer, selectedWindow.geometry->width, src_u,
                    stride_u, src_v, stride_u, selectedWindow.geometry->width,
                    selectedWindow.geometry->height, scaledBuf, dstWidth, dst_u,
                    dst_stride_u, dst_v, dst_stride_u, dstWidth, dstHeight,
                    libyuv::kFilterNone);

                pool.push_task([&]() {
                    isFrameHandled.store(false);
                    newFrameHandler(scaledBuf, scaledBufSize);
                    isFrameHandled.store(true);
                });
            } else {

                pool.push_task([&]() {
                    isFrameHandled.store(false);
                    newFrameHandler(yuv420buffer, yuv420bufLength);
                    isFrameHandled.store(true);
                });
            }

            after = clock();

            execTime = after - before;

            if (execTime < ticksBetweenFrames)
                usleep(ticksBetweenFrames - execTime);

            fps.store(fps.load() + 1u);
        }

        if (scale) free(scaledBuf);

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