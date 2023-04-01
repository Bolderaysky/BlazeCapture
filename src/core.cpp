#include "blaze/capture/core.hpp"
#include "imgui.h"

#include <GLFW/glfw3.h>

#include <bits/chrono.h>
#include <chrono>
#include <iostream>
#include <filesystem>

#include "BS_thread_pool_light.hpp"

#include "SQLiteCpp/SQLiteCpp.h"

#include "blaze/capture/misc.hpp"

namespace blaze {

    void BlazeCapture::setShortcuts() {

        glfwSetKeyCallback(window, [](GLFWwindow* window, std::int32_t key,
                                      std::int32_t, std::int32_t action,
                                      std::int32_t mods) {
            if (key == GLFW_KEY_F9 && mods == GLFW_MOD_ALT &&
                action == GLFW_PRESS) {

                const auto& self = static_cast<BlazeCapture*>(
                    glfwGetWindowUserPointer(window));

                if (self->isWindowHidden) {

                    glfwShowWindow(window);
                    self->isWindowHidden = false;

                } else {

                    glfwHideWindow(window);
                    self->isWindowHidden = true;
                }
            }
        });
    }

    BlazeCapture::BlazeCapture() : vfs(512u) {

        if (!glfwInit()) errHandler("[glfw] Cannot init glfw", -1);

        if (!vfs.readArchive("assets.asar"))
            errHandler("[blaze] Cannot read assets", -1);

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);


        // Create window with graphics context
        glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
        glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);

        videoCapturer.onErrorCallback([&](const char* err, std::int32_t c) {
            errHandler(err, c);
        });

        videoCapturer.onNewFrame([&](void* buffer, std::uint64_t size) {
            fwrite(buffer, size, 1, videoFile);
        });

        audioCapturer.onErrorCallback([&](const char* err, std::int32_t c) {
            errHandler(err, c);
        });

        audioCapturer.onNewMicData([&](float* buffer, std::uint64_t size) {
            if (!isMicCaptured) return;
            fwrite(buffer, size, sizeof(float), micFile);
        });

        audioCapturer.onNewDesktopData([&](float* buffer, std::uint64_t size) {
            if (!isDesktopSoundCaptured) return;
            fwrite(buffer, size, sizeof(float), audioFile);
        });

        if (!std::filesystem::exists("data"))
            std::filesystem::create_directory("data");

        videoFile = fopen("data/nvidia.hevc", "wb");
        if (videoFile == nullptr)
            errHandler("[blaze] Cannot open file for writing", -1);

        audioFile = fopen("data/audio.raw", "wb");
        if (audioFile == nullptr)
            errHandler("[blaze] Cannot open file for writing", -1);

        micFile = fopen("data/mic.raw", "wb");
        if (micFile == nullptr)
            errHandler("[blaze] Cannot open file for writing", -1);

        db = std::make_unique<SQLite::Database>(
            "data/log.db", SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);

        db->exec("CREATE TABLE IF NOT EXISTS log (status, msg, code)");
        db->exec("CREATE TABLE IF NOT EXISTS history (action, elapsedTime)");
    }

    BlazeCapture::~BlazeCapture() {

        if (videoFile != nullptr) fclose(videoFile);
        if (audioFile != nullptr) fclose(audioFile);
        if (micFile != nullptr) fclose(micFile);

        // Cleanup
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        glfwDestroyWindow(window);
        glfwTerminate();
    }

    void BlazeCapture::run() {

        HISTORY(ACTION::OPEN_APP, 0u);

        const auto& APP_START_TIME = std::chrono::system_clock::now();
        std::chrono::time_point<std::chrono::system_clock> RECORD_START_TIME,
            RECORD_STOP_TIME;

        const auto& monitor = glfwGetPrimaryMonitor();
        const auto& mode = glfwGetVideoMode(monitor);

        videoCapturer.setResolution(mode->width, mode->height);
        videoCapturer.setRefreshRate(mode->refreshRate);

        videoCapturer.load();

        videoCapturer.selectScreen(videoCapturer.listScreen()[1]);

        audioCapturer.load();

        window = glfwCreateWindow(mode->width, mode->height, "BlazeCapture",
                                  monitor, nullptr);

        if (window == nullptr) errHandler("[glfw] Cannot create window", -1);

        glfwMakeContextCurrent(window);
        glfwSwapInterval(1); // Enable vsync

        glfwSetWindowUserPointer(window, this);
        this->setShortcuts();

        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        (void)io;

        // Setup Dear ImGui style
        ImGui::StyleColorsDark();

        // double free or corruption (!prev) for now, use files instead

        // auto redhatFontTTF = vfs.read<std::string>(
        //     "fonts/RedHatDisplay-Regular.ttf");

        // const auto& RedhatDisplayLarge = io.Fonts->AddFontFromMemoryTTF(
        //     static_cast<void*>(redhatFontTTF.data()), 24u, 24u);
        // const auto& RedhatDisplaySmall = io.Fonts->AddFontFromMemoryTTF(
        //     static_cast<void*>(redhatFontTTF.data()), 18u, 18u);

        const auto& RedhatDisplayLarge = io.Fonts->AddFontFromFileTTF(
            "../assets/fonts/RedHatDisplay-Regular.ttf", 24.0f);
        const auto& RedhatDisplaySmall = io.Fonts->AddFontFromFileTTF(
            "../assets/fonts/RedHatDisplay-Regular.ttf", 18.0f);


        // Setup Platform/Renderer backends
        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init("#version 130");

        // Our state
        std::int32_t backgroundAlpha = 50;
        ImVec4 clearColor = ImVec4(0.0f, 0.0f, 0.00f, backgroundAlpha / 100.0f);

        bool isRecorded = false;
        bool areSettingsOpened = false;
        bool isThemeDark = true;
        bool isReplayEnabled = false;
        bool overlayOpened = true;

        BS::thread_pool_light thread_pool(4u);

        while (!glfwWindowShouldClose(window)) {

            glfwPollEvents();

            // Start the Dear ImGui frame
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            if (!overlayOpened) goto end;

            {

                ImGui::SetNextWindowPos(
                    ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
                    ImGuiCond_Always, ImVec2(0.5f, 0.5f));

                ImGui::SetNextWindowSize(
                    ImVec2(io.DisplaySize.x * 0.2f, io.DisplaySize.y * 0.2f));

                ImGui::SetNextWindowBgAlpha(1.0f);

                ImGui::Begin("BlazeCapture", &overlayOpened,
                             ImGuiWindowFlags_NoMove |
                                 ImGuiWindowFlags_NoResize |
                                 ImGuiWindowFlags_NoCollapse);

                ImGui::PushFont(RedhatDisplayLarge);

                if (ImGui::Button(isRecorded ? "Stop recording" :
                                               "Start recording")) {
                    isRecorded = !isRecorded;
                    if (isRecorded) {

                        RECORD_START_TIME = std::chrono::system_clock::now();

                        thread_pool.push_task([&]() {
                            videoCapturer.startCapture();
                        });

                        if (isMicCaptured || isDesktopSoundCaptured) {

                            thread_pool.push_task([&]() {
                                audioCapturer.startCapture();
                            });
                        }
                    }


                    else {

                        RECORD_STOP_TIME = std::chrono::system_clock::now();

                        HISTORY(
                            ACTION::RECORD,
                            std::chrono::duration_cast<std::chrono::seconds>(
                                RECORD_STOP_TIME - RECORD_START_TIME)
                                .count());

                        videoCapturer.stopCapture();

                        if (isMicCaptured || isDesktopSoundCaptured) {

                            audioCapturer.stopCapture();
                        }

                        // thread_pool.push_task([&]() {
                        //     videoCapturer.stopCapture();

                        //     if (isMicCaptured || isDesktopSoundCaptured) {

                        //         audioCapturer.stopCapture();
                        //     }
                        // });
                    }

                    ImGui::SetItemDefaultFocus();
                }

                ImGui::Checkbox("Capture microphone sound", &isMicCaptured);

                ImGui::Checkbox("Capture desktop sound",
                                &isDesktopSoundCaptured);

                ImGui::Checkbox("Enable replay", &isReplayEnabled);

                ImGui::SetCursorPos(ImVec2(io.DisplaySize.x * 0.167f,
                                           io.DisplaySize.y * 0.1721f));

                if (ImGui::Button("Settings")) {

                    areSettingsOpened = !areSettingsOpened;
                }

                ImGui::PopFont();

                ImGui::End();

                if (areSettingsOpened) {

                    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f,
                                                   io.DisplaySize.y * 0.5f),
                                            ImGuiCond_Always,
                                            ImVec2(0.5f, 0.5f));

                    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x * 0.2f,
                                                    io.DisplaySize.y * 0.2f));

                    ImGui::SetNextWindowBgAlpha(1.0f);

                    ImGui::Begin("Settings", &areSettingsOpened,
                                 ImGuiWindowFlags_NoMove |
                                     ImGuiWindowFlags_NoResize |
                                     ImGuiWindowFlags_NoCollapse);

                    if (ImGui::Button("Switch theme")) {

                        if (isThemeDark) {

                            ImGui::StyleColorsLight();

                        } else ImGui::StyleColorsDark();

                        isThemeDark = !isThemeDark;
                    }

                    ImGui::Text("Background opacity:");
                    ImGui::SliderInt("##", &backgroundAlpha, 0u, 100u, "%d%%");

                    ImGui::SetCursorPosY(io.DisplaySize.y * 0.167f);

                    ImGui::PushFont(RedhatDisplaySmall);

                    ImGui::Text("Selected backend: Nvfbc");
                    ImGui::Text("OS: %s", OSName());

                    ImGui::PopFont();

                    ImGui::End();
                }
            }

            // Rendering

            {

                clearColor.w = backgroundAlpha / 100.0f;

                ImGui::Render();
                int display_w, display_h;
                glfwGetFramebufferSize(window, &display_w, &display_h);
                glViewport(0, 0, display_w, display_h);
                glClearColor(clearColor.x * clearColor.w,
                             clearColor.y * clearColor.w,
                             clearColor.z * clearColor.w, clearColor.w);
                glClear(GL_COLOR_BUFFER_BIT);
                ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

                glfwSwapBuffers(window);
            }
        }

    end:

        const auto& APP_END_TIME = std::chrono::system_clock::now();

        HISTORY(ACTION::CLOSE_APP,
                std::chrono::duration_cast<std::chrono::seconds>(APP_END_TIME -
                                                                 APP_START_TIME)
                    .count());
    }

    void BlazeCapture::errHandler(const char* err, std::int32_t c) {

        std::cerr << err << "\nStatus code: " << c << std::endl;
        std::abort();
    }

    void BlazeCapture::LOG(LOG_STATUS status, const char* msg,
                           std::int32_t code) {

        const auto& req = "INSERT INTO log VALUES(" + std::to_string(status) +
                          ",'" + msg + "'," + std::to_string(code) + ")";

        db->exec(req);
    }

    void BlazeCapture::HISTORY(ACTION action, std::uint64_t s) {

        std::string actionStr;

        if (action == ACTION::OPEN_APP) actionStr = "OPEN_APP";
        else if (action == ACTION::CLOSE_APP) actionStr = "CLOSE_APP";
        else actionStr = "RECORD";

        const auto& req = "INSERT INTO history VALUES('" + actionStr + "'," +
                          std::to_string(s) + ")";

        db->exec(req);
    }

}; // namespace blaze