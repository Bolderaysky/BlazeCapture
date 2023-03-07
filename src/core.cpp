#include "blaze/capture/core.hpp"
#include "imgui.h"

#include <GLFW/glfw3.h>
#include <iostream>

#include "BS_thread_pool_light.hpp"

namespace blaze {

    void BlazeCapture::setShortcuts() {

        glfwSetKeyCallback(window, [](GLFWwindow* window, std::int32_t key,
                                      std::int32_t scancode,
                                      std::int32_t action, std::int32_t mods) {
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

    BlazeCapture::BlazeCapture() {

        if (!glfwInit()) errHandler("[glfw] Cannot init glfw", -1);

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);


        // Create window with graphics context
        glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
        glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);

        Capturer.onErrorCallback([&](const char* err, std::int32_t c) {
            errHandler(err, c);
        });

        Capturer.onNewFrame([&](void* buffer, std::uint64_t size) {
            // fwrite(buffer, size, 1, videoFile);
            std::cout << buffer << '\n';
        });

        videoFile = fopen("nvidia.hevc", "wb");
        if (videoFile == nullptr)
            errHandler("[blaze] Cannot open file for writing", -1);
    }

    BlazeCapture::~BlazeCapture() {

        if (videoFile != nullptr) fclose(videoFile);

        // Cleanup
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        glfwDestroyWindow(window);
        glfwTerminate();
    }

    void BlazeCapture::run() {

        const auto& monitor = glfwGetPrimaryMonitor();
        const auto& mode = glfwGetVideoMode(monitor);

        Capturer.setResolution(mode->width, mode->height);
        Capturer.setRefreshRate(mode->refreshRate);

        Capturer.load();

        window = glfwCreateWindow(mode->width, mode->height, "BlazeCapture",
                                  glfwGetPrimaryMonitor(), nullptr);

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

        const auto RedhatDisplayLarge = io.Fonts->AddFontFromFileTTF(
            "RedHatDisplay-Regular.ttf", 24u);
        const auto RedhatDisplaySmall = io.Fonts->AddFontFromFileTTF(
            "RedHatDisplay-Regular.ttf", 18u);

        // Setup Platform/Renderer backends
        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init("#version 130");

        // Our state
        ImVec4 clear_color = ImVec4(0.0f, 0.0f, 0.00f, 0.50f);

        bool isRecorded = false;
        bool areSettingsOpened = false;
        bool isMicCaptured = true;
        bool isDesktopSoundCaptured = true;

        BS::thread_pool_light thread_pool(2u);

        while (!glfwWindowShouldClose(window))

        {

            glfwPollEvents();

            // Start the Dear ImGui frame
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            {

                ImGui::SetNextWindowPos(
                    ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
                    ImGuiCond_Always, ImVec2(0.5f, 0.5f));

                ImGui::SetNextWindowSize(
                    ImVec2(io.DisplaySize.x * 0.2f, io.DisplaySize.y * 0.2f));

                ImGui::SetNextWindowBgAlpha(1.0f);

                ImGui::Begin("BlazeCapture", nullptr,
                             ImGuiWindowFlags_NoMove |
                                 ImGuiWindowFlags_NoResize |
                                 ImGuiWindowFlags_NoCollapse);

                ImGui::PushFont(RedhatDisplayLarge);

                if (ImGui::Button(isRecorded ? "Stop recording" :
                                               "Start recording")) {
                    isRecorded = !isRecorded;
                    if (isRecorded)
                        thread_pool.push_task([this]() {
                            Capturer.startCapture();
                        });

                    else
                        thread_pool.push_task([this]() {
                            Capturer.stopCapture();
                        });

                    ImGui::SetItemDefaultFocus();
                }

                ImGui::Checkbox("Capture microphone sound", &isMicCaptured);

                ImGui::Checkbox("Capture desktop sound",
                                &isDesktopSoundCaptured);

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

                    ImGui::PushFont(RedhatDisplaySmall);

                    ImGui::Text("Selected backend: Nvfbc");
                    ImGui::Text("OS: Linux");

                    ImGui::PopFont();

                    ImGui::End();
                }
            }

            // Rendering
            ImGui::Render();
            int display_w, display_h;
            glfwGetFramebufferSize(window, &display_w, &display_h);
            glViewport(0, 0, display_w, display_h);
            glClearColor(clear_color.x * clear_color.w,
                         clear_color.y * clear_color.w,
                         clear_color.z * clear_color.w, clear_color.w);
            glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            glfwSwapBuffers(window);
        }
    }

    void BlazeCapture::errHandler(const char* err, std::int32_t c) {

        std::cerr << err << "\nStatus code: " << c << std::endl;
        std::abort();
    }

}; // namespace blaze