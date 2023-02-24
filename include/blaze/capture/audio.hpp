#pragma once

#include <cstdint>
#include <functional>

namespace blaze {

    enum audioChannel : std::uint8_t {

        mono = 1u,
        stereo = 2u

    };

};

namespace blaze::internal {

    class AudioCapture {

        protected:
            std::function<void(const char *, std::int32_t)> errHandler;
            std::function<void(void *, std::uint64_t)> audioStreamHandler;

            std::uint32_t bufferLength = 4096;
            bool micCapture = true;
            bool desktopSoundCapture = true;

            std::uint32_t sampleRate = 44100u;

            blaze::audioChannel channelCount = stereo;


        public:
            AudioCapture();
            ~AudioCapture();

            void setAudioChannel(blaze::audioChannel channelType);
            void setSampleRate(std::uint32_t rate);

            void load();
            void startCapture();
            void stopCapture();

            void setBufferLength(std::uint32_t len);

            void setMicCapturing(bool state);
            void setDesktopSoundCapturing(bool state);

            void onErrorCallback(
                std::function<void(const char *, std::int32_t)> callback);
            void
                onNewChunk(std::function<void(void *, std::uint64_t)> callback);
    };

}; // namespace blaze::internal