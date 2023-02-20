#include "blaze/capture/core.hpp"

#include <cstdlib>
#include <iostream>

namespace blaze {

    void BlazeCapture::run() {

        window = glfwCreateWindow(Info.width, Info.height, Info.title, nullptr,
                                  nullptr);

        // Create Window Surface
        VkSurfaceKHR surface;
        VkResult err = glfwCreateWindowSurface(g_Instance, window, g_Allocator,
                                               &surface);
        checkVkResult(err);

        // Create Framebuffers
        std::int32_t w, h;
        glfwGetFramebufferSize(window, &w, &h);
        this->setupVulkanWindow(vulkanWindow, surface, w, h);

        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        (void)io;
        // io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable
        // Keyboard Controls io.ConfigFlags |=

        // Setup Dear ImGui style
        ImGui::StyleColorsDark();

        // Setup Platform/Renderer backends
        ImGui_ImplGlfw_InitForVulkan(window, true);
        ImGui_ImplVulkan_InitInfo init_info = {};
        init_info.Instance = g_Instance;
        init_info.PhysicalDevice = g_PhysicalDevice;
        init_info.Device = g_Device;
        init_info.QueueFamily = g_QueueFamily;
        init_info.Queue = g_Queue;
        init_info.PipelineCache = g_PipelineCache;
        init_info.DescriptorPool = g_DescriptorPool;
        init_info.Subpass = 0;
        init_info.MinImageCount = g_MinImageCount;
        init_info.ImageCount = (vulkanWindow)->ImageCount;
        init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
        init_info.Allocator = g_Allocator;
        init_info.CheckVkResultFn = checkVkResult;
        ImGui_ImplVulkan_Init(&init_info, (vulkanWindow)->RenderPass);

        // Upload Fonts
        {
            // Use any command queue
            VkCommandPool command_pool =
                vulkanWindow->Frames[vulkanWindow->FrameIndex].CommandPool;
            VkCommandBuffer command_buffer =
                vulkanWindow->Frames[vulkanWindow->FrameIndex].CommandBuffer;

            err = vkResetCommandPool(g_Device, command_pool, 0);
            checkVkResult(err);
            VkCommandBufferBeginInfo begin_info = {};
            begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            err = vkBeginCommandBuffer(command_buffer, &begin_info);
            checkVkResult(err);

            ImGui_ImplVulkan_CreateFontsTexture(command_buffer);

            VkSubmitInfo end_info = {};
            end_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            end_info.commandBufferCount = 1;
            end_info.pCommandBuffers = &command_buffer;
            err = vkEndCommandBuffer(command_buffer);
            checkVkResult(err);
            err = vkQueueSubmit(g_Queue, 1, &end_info, VK_NULL_HANDLE);
            checkVkResult(err);

            err = vkDeviceWaitIdle(g_Device);
            checkVkResult(err);
            ImGui_ImplVulkan_DestroyFontUploadObjects();
        }

        // Our state
        bool show_demo_window = true;
        bool show_another_window = false;
        ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

        // Main loop
        while (!glfwWindowShouldClose(window)) {
            // Poll and handle events (inputs, window resize, etc.)
            // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard
            // flags to tell if dear imgui wants to use your inputs.
            // - When io.WantCaptureMouse is true, do not dispatch mouse input
            // data to your main application, or clear/overwrite your copy of
            // the mouse data.
            // - When io.WantCaptureKeyboard is true, do not dispatch keyboard
            // input data to your main application, or clear/overwrite your copy
            // of the keyboard data. Generally you may always pass all inputs to
            // dear imgui, and hide them from your application based on those
            // two flags.
            glfwPollEvents();

            // Resize swap chain?
            if (g_SwapChainRebuild) {
                int width, height;
                glfwGetFramebufferSize(window, &width, &height);
                if (width > 0 && height > 0) {
                    ImGui_ImplVulkan_SetMinImageCount(g_MinImageCount);
                    ImGui_ImplVulkanH_CreateOrResizeWindow(
                        g_Instance, g_PhysicalDevice, g_Device, vulkanWindow,
                        g_QueueFamily, g_Allocator, width, height,
                        g_MinImageCount);
                    vulkanWindow->FrameIndex = 0;
                    g_SwapChainRebuild = false;
                }
            }

            // Start the Dear ImGui frame
            ImGui_ImplVulkan_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            // 1. Show the big demo window (Most of the sample code is in
            // ImGui::ShowDemoWindow()! You can browse its code to learn more
            // about Dear ImGui!).
            if (show_demo_window) ImGui::ShowDemoWindow(&show_demo_window);

            // 2. Show a simple window that we create ourselves. We use a
            // Begin/End pair to create a named window.
            {
                static float f = 0.0f;
                static int counter = 0;

                ImGui::Begin("Hello, world!"); // Create a window called "Hello,
                                               // world!" and append into it.

                ImGui::Text(
                    "This is some useful text."); // Display some text (you can
                                                  // use a format strings too)
                ImGui::Checkbox("Demo Window",
                                &show_demo_window); // Edit bools storing our
                                                    // window open/close state
                ImGui::Checkbox("Another Window", &show_another_window);

                ImGui::SliderFloat(
                    "float", &f, 0.0f,
                    1.0f); // Edit 1 float using a slider from 0.0f to 1.0f
                ImGui::ColorEdit3(
                    "clear color",
                    (float*)&clear_color); // Edit 3 floats representing a color

                if (ImGui::Button(
                        "Button")) // Buttons return true when clicked (most
                                   // widgets return true when edited/activated)
                    counter++;
                ImGui::SameLine();
                ImGui::Text("counter = %d", counter);

                ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
                            1000.0f / ImGui::GetIO().Framerate,
                            ImGui::GetIO().Framerate);
                ImGui::End();
            }

            // 3. Show another simple window.
            if (show_another_window) {
                ImGui::Begin(
                    "Another Window",
                    &show_another_window); // Pass a pointer to our bool
                                           // variable (the window will have a
                                           // closing button that will clear the
                                           // bool when clicked)
                ImGui::Text("Hello from another window!");
                if (ImGui::Button("Close Me")) show_another_window = false;
                ImGui::End();
            }

            // Rendering
            ImGui::Render();
            ImDrawData* draw_data = ImGui::GetDrawData();
            const bool is_minimized = (draw_data->DisplaySize.x <= 0.0f ||
                                       draw_data->DisplaySize.y <= 0.0f);
            if (!is_minimized) {
                vulkanWindow->ClearValue.color.float32[0] = clear_color.x *
                                                            clear_color.w;
                vulkanWindow->ClearValue.color.float32[1] = clear_color.y *
                                                            clear_color.w;
                vulkanWindow->ClearValue.color.float32[2] = clear_color.z *
                                                            clear_color.w;
                vulkanWindow->ClearValue.color.float32[3] = clear_color.w;
                this->frameRender(vulkanWindow, draw_data);
                this->framePresent(vulkanWindow);
            }
        }
    }

    void BlazeCapture::runAsync() {
    }

    BlazeCapture::BlazeCapture(AppInfo app_info) : Info(app_info) {

        vulkanWindow = new ImGui_ImplVulkanH_Window;

        glfwSetErrorCallback(glfwErrCallback);

        if (!glfwInit()) {

            std::cerr << "[glfw] Init error: -1\n";
            std::abort();
        }

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        if (!glfwVulkanSupported()) {

            std::cerr << "GLFW: Vulkan Not Supported\n";
            std::abort();
        }

        std::uint32_t extensions_count = 0;
        const char** extensions = glfwGetRequiredInstanceExtensions(
            &extensions_count);
        this->setupVulkan(extensions, extensions_count);
    }

    BlazeCapture::~BlazeCapture() {

        // Cleanup
        const auto& err = vkDeviceWaitIdle(g_Device);
        checkVkResult(err);
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        this->cleanupVulkanWindow();
        this->cleanupVulkan();

        glfwDestroyWindow(window);
        glfwTerminate();
    }

}; // namespace blaze