#pragma once

#include <cstdint>
#include <functional>

#include <spa/param/audio/format-utils.h>

#include <pipewire/pipewire.h>

namespace blaze {

    struct audioData {
            struct pw_main_loop *loop;
            struct pw_stream *micStream;
            struct pw_stream *desktopSoundStream;
            struct spa_audio_info format;

            std::function<void(const char *, std::int32_t)> errHandler;
            std::function<void(float *, std::uint32_t)> audioStreamHandler;
            std::function<void(float *, std::uint32_t)> micStreamHandler;

            bool isMicSound;
    };

    class AudioCapture {

        protected:
            bool micCapture = true;
            bool desktopSoundCapture = true;

            audioData data = {};

            bool isLoaded = false;

        public:
            AudioCapture();
            explicit AudioCapture(int argc, char *argv[]);

            ~AudioCapture();

            void load();
            void startCapture();
            void stopCapture();

            void setMicCapturing(bool state);
            void setDesktopSoundCapturing(bool state);

            void onErrorCallback(
                std::function<void(const char *, std::int32_t)> callback);
            void onNewDesktopData(
                std::function<void(float *, std::uint32_t)> callback);
            void onNewMicData(
                std::function<void(float *, std::uint32_t)> callback);
    };

}; // namespace blaze