#pragma once

#include <cstdint>

#include "imgui.h"
#include "imgui_impl_vulkan.h"
#include "imgui_impl_glfw.h"

#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

namespace blaze {

    struct AppInfo {

            const char* title;
            std::uint32_t width;
            std::uint32_t height;
    };

    class BlazeCapture {

        protected:
            GLFWwindow* window;
            ImGui_ImplVulkanH_Window* vulkanWindow;

            VkAllocationCallbacks* g_Allocator = nullptr;
            VkInstance g_Instance = nullptr;
            VkPhysicalDevice g_PhysicalDevice = nullptr;
            VkDevice g_Device = nullptr;
            std::uint32_t g_QueueFamily = (std::uint32_t)-1;
            VkQueue g_Queue = nullptr;
            VkDebugReportCallbackEXT g_DebugReport = nullptr;
            VkPipelineCache g_PipelineCache = nullptr;
            VkDescriptorPool g_DescriptorPool = nullptr;

            std::int32_t g_MinImageCount = 2;
            bool g_SwapChainRebuild = false;

            AppInfo Info;

        public:
            BlazeCapture(AppInfo app_info);
            ~BlazeCapture();
            void run();
            void runAsync();

        protected:
            void setupVulkan(const char** extensions,
                             std::uint32_t extensions_count);
            void setupVulkanWindow(ImGui_ImplVulkanH_Window* wd,
                                   VkSurfaceKHR surface, std::int32_t width,
                                   std::int32_t height);
            void cleanupVulkanWindow();
            void cleanupVulkan();
            void frameRender(ImGui_ImplVulkanH_Window* wd,
                             ImDrawData* draw_data);
            void framePresent(ImGui_ImplVulkanH_Window* wd);
            static void checkVkResult(VkResult err);
            static void glfwErrCallback(std::int32_t error,
                                        const char* description);
    };

}; // namespace blaze