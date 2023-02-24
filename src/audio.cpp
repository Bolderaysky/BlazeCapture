#include "blaze/capture/audio.hpp"

namespace blaze::internal {


    AudioCapture::AudioCapture() {
    }
    AudioCapture::~AudioCapture() {
    }

    void AudioCapture::setAudioChannel(blaze::audioChannel channelType) {

        channelCount = channelType;
    }

    void AudioCapture::onErrorCallback(
        std::function<void(const char *, std::int32_t)> callback) {

        errHandler = callback;
    }

    void AudioCapture::onNewChunk(
        std::function<void(void *, std::uint64_t)> callback) {

        audioStreamHandler = callback;
    }

    void AudioCapture::setBufferLength(std::uint32_t len) {

        bufferLength = len;
    }

    void AudioCapture::setDesktopSoundCapturing(bool state) {

        desktopSoundCapture = state;
    }

    void AudioCapture::setMicCapturing(bool state) {

        micCapture = state;
    }

    void AudioCapture::setSampleRate(std::uint32_t rate) {

        sampleRate = rate;
    }

    void AudioCapture::load() {
    }


}; // namespace blaze::internal