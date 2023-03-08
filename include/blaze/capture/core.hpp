#pragma once

#include "SQLiteCpp/Database.h"
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

    enum ACTION {

        OPEN_APP,
        CLOSE_APP,
        RECORD

    };

    class BlazeCapture {

        protected:
            GLFWwindow* window = nullptr;

            internal::NvfbcCapture videoCapturer;
            AudioCapture audioCapturer;

            FILE* videoFile = nullptr;
            FILE* audioFile = nullptr;
            FILE* micFile = nullptr;

            bool isWindowHidden = false;
            bool isMicCaptured = true;
            bool isDesktopSoundCaptured = true;

            std::unique_ptr<SQLite::Database> db;

        public:
            BlazeCapture();
            ~BlazeCapture();

            void run();

        protected:
            void errHandler(const char* err, std::int32_t c);
            void LOG(LOG_STATUS status, const char* msg, std::int32_t code = 0);
            void HISTORY(ACTION action, std::uint64_t s);
            void setShortcuts();
    };

}; // namespace blaze