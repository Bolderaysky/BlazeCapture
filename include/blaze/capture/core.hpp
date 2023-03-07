#pragma once

#include "blaze/capture/linux/nvidia.hpp"
#include <memory>

#define GL_SILENCE_DEPRECATION

#include <GLFW/glfw3.h>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "blaze/capture/video.hpp"
#include "blaze/capture/audio.hpp"

namespace blaze {

    enum LOG_STATUS {

        INFO,
        WARNING,
        ERROR,
        CRITICAL

    };

    class BlazeCapture {

        protected:
            GLFWwindow* window = nullptr;
            internal::NvfbcCapture Capturer;
            FILE* videoFile = nullptr;

            bool isWindowHidden = false;

        public:
            BlazeCapture();
            ~BlazeCapture();

            void run();

        protected:
            void errHandler(const char* err, std::int32_t c);
            void LOG(LOG_STATUS status, const char* msg, std::int32_t code = 0);
            void setShortcuts();
    };

}; // namespace blaze