#include "blaze/capture/core.hpp"

#include <cstdlib>
#include <iostream>

namespace blaze {

    void BlazeCapture::glfwErrCallback(int error, const char* description) {
        std::cerr << "[glfw] Error" << error << ": " << description << '\n';
    }
    void BlazeCapture::checkVkResult(VkResult err) {
        if (err == 0) return;
        std::cerr << "[vulkan] Error: VkResult = " << err << '\n';
        if (err < 0) abort();
    }
}; // namespace blaze