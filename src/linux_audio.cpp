#include "blaze/capture/audio.hpp"
#include "blaze/capture/linux/audio.hpp"

namespace blaze {

    AudioCapture::AudioCapture(int argc, char *argv[]) {

        pw_init(&argc, &argv);
    }

    AudioCapture::AudioCapture() {

        pw_init(nullptr, nullptr);
    }

    AudioCapture::~AudioCapture() {

        if (micCapture) pw_stream_destroy(data.micStream);
        if (desktopSoundCapture) pw_stream_destroy(data.desktopSoundStream);

        pw_main_loop_destroy(data.loop);
        pw_deinit();
    }

    void AudioCapture::onErrorCallback(
        std::function<void(const char *, std::int32_t)> callback) {

        data.errHandler = callback;
    }

    void AudioCapture::onNewDesktopData(
        std::function<void(float *, std::uint32_t)> callback) {

        if (!desktopSoundCapture)
            data.errHandler("AudioCapture::setDesktopSoundCapturing() were not "
                            "set to 'true'",
                            -1);

        data.audioStreamHandler = callback;
    }

    void AudioCapture::onNewMicData(
        std::function<void(float *, std::uint32_t)> callback) {

        if (!desktopSoundCapture)
            data.errHandler("AudioCapture::setMicCapturing() were not "
                            "set to 'true'",
                            -1);

        data.micStreamHandler = callback;
    }

    void AudioCapture::setDesktopSoundCapturing(bool state) {

        desktopSoundCapture = state;
    }

    void AudioCapture::setMicCapturing(bool state) {

        micCapture = state;
    }

    void AudioCapture::load() {

        /* make a main loop. If you already have another main loop, you can
         * add the fd of this pipewire mainloop to it. */
        data.loop = pw_main_loop_new(nullptr);

        const auto &doQuit = [](void *userdata, std::int32_t) {
            const auto data = static_cast<struct audioData *>(userdata);
            pw_main_loop_quit(data->loop);
        };

        pw_loop_add_signal(pw_main_loop_get_loop(data.loop), SIGINT, doQuit,
                           &data);
        pw_loop_add_signal(pw_main_loop_get_loop(data.loop), SIGTERM, doQuit,
                           &data);

        if (desktopSoundCapture) {


            struct pw_properties *props;
            const struct spa_pod *params[1];

            uint8_t buffer[1024];
            struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer,
                                                            sizeof(buffer));

            struct spa_audio_info_raw infoRawInit =
                SPA_AUDIO_INFO_RAW_INIT(.format = SPA_AUDIO_FORMAT_F32);

            const auto &onProcess = [](void *userdata) {
                const auto data = static_cast<struct audioData *>(userdata);

                struct pw_buffer *b;
                struct spa_buffer *buf;
                float *samples;

                if ((b = pw_stream_dequeue_buffer(data->desktopSoundStream)) ==
                    nullptr)
                    data->errHandler("Out of buffers", -1);

                buf = b->buffer;
                if ((samples = static_cast<float *>(buf->datas[0].data)) ==
                    nullptr)
                    return;

                const auto &n_samples = buf->datas[0].chunk->size /
                                        sizeof(float);

                data->audioStreamHandler(samples, n_samples);

                pw_stream_queue_buffer(data->desktopSoundStream, b);
            };

            static const struct pw_stream_events desktopSoundEvents = {
                PW_VERSION_STREAM_EVENTS,
                .process = onProcess,
            };


            /* Create a simple stream, the simple stream manages the core and
             * remote objects for you if you don't need to deal with them.
             *
             * If you plan to autoconnect your stream, you need to provide at
             * least media, category and role properties.
             *
             * Pass your events and a user_data pointer as the last arguments.
             * This will inform you about the stream state. The most important
             * event you need to listen to is the process event where you need
             * to produce the data.
             */
            props = pw_properties_new(PW_KEY_MEDIA_TYPE, "Audio",
                                      PW_KEY_MEDIA_CATEGORY, "Capture",
                                      PW_KEY_MEDIA_ROLE, "Music", NULL);


            /* uncomment if you want to capture from the sink monitor ports */
            pw_properties_set(props, PW_KEY_STREAM_CAPTURE_SINK, "true");

            data.desktopSoundStream = pw_stream_new_simple(
                pw_main_loop_get_loop(data.loop), "desktop-sound-capture",
                props, &desktopSoundEvents, &data);

            /* Make one parameter with the supported formats. The
             * SPA_PARAM_EnumFormat id means that this is a format enumeration
             * (of 1 value). We leave the channels and rate empty to accept the
             * native graph rate and channels. */
            params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat,
                                                   &infoRawInit);


            /* Now connect this stream. We ask that our process function is
             * called in a realtime thread. */

            enum pw_stream_flags flags =
                (pw_stream_flags)(PW_STREAM_FLAG_AUTOCONNECT |
                                  PW_STREAM_FLAG_MAP_BUFFERS |
                                  PW_STREAM_FLAG_RT_PROCESS);

            pw_stream_connect(data.desktopSoundStream, PW_DIRECTION_INPUT,
                              PW_ID_ANY, flags, params, 1);
        }


        if (micCapture) {


            struct pw_properties *props;
            const struct spa_pod *params[1];

            uint8_t buffer[1024];
            struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer,
                                                            sizeof(buffer));

            struct spa_audio_info_raw infoRawInit =
                SPA_AUDIO_INFO_RAW_INIT(.format = SPA_AUDIO_FORMAT_F32);

            const auto &onProcess = [](void *userdata) {
                const auto data = static_cast<struct audioData *>(userdata);

                struct pw_buffer *b;
                struct spa_buffer *buf;
                float *samples;

                if ((b = pw_stream_dequeue_buffer(data->micStream)) == nullptr)
                    data->errHandler("Out of buffers", -1);

                buf = b->buffer;
                if ((samples = static_cast<float *>(buf->datas[0].data)) ==
                    nullptr)
                    return;

                const auto &n_samples = buf->datas[0].chunk->size /
                                        sizeof(float);

                data->micStreamHandler(samples, n_samples);

                pw_stream_queue_buffer(data->micStream, b);
            };

            static const struct pw_stream_events micEvents = {
                PW_VERSION_STREAM_EVENTS,
                .process = onProcess,
            };


            /* Create a simple stream, the simple stream manages the core and
             * remote objects for you if you don't need to deal with them.
             *
             * If you plan to autoconnect your stream, you need to provide at
             * least media, category and role properties.
             *
             * Pass your events and a user_data pointer as the last arguments.
             * This will inform you about the stream state. The most important
             * event you need to listen to is the process event where you need
             * to produce the data.
             */
            props = pw_properties_new(PW_KEY_MEDIA_TYPE, "Audio",
                                      PW_KEY_MEDIA_CATEGORY, "Capture",
                                      PW_KEY_MEDIA_ROLE, "Music", NULL);


            /* uncomment if you want to capture from the sink monitor ports */
            // pw_properties_set(props, PW_KEY_STREAM_CAPTURE_SINK, "true");

            data.micStream = pw_stream_new_simple(
                pw_main_loop_get_loop(data.loop), "mic-capture", props,
                &micEvents, &data);

            /* Make one parameter with the supported formats. The
             * SPA_PARAM_EnumFormat id means that this is a format enumeration
             * (of 1 value). We leave the channels and rate empty to accept the
             * native graph rate and channels. */
            params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat,
                                                   &infoRawInit);


            /* Now connect this stream. We ask that our process function is
             * called in a realtime thread. */

            enum pw_stream_flags flags =
                (pw_stream_flags)(PW_STREAM_FLAG_AUTOCONNECT |
                                  PW_STREAM_FLAG_MAP_BUFFERS |
                                  PW_STREAM_FLAG_RT_PROCESS);

            pw_stream_connect(data.micStream, PW_DIRECTION_INPUT, PW_ID_ANY,
                              flags, params, 1);
        }

        isLoaded = true;
    }

    void AudioCapture::startCapture() {

        if (!isLoaded)
            data.errHandler("AudioCapture::load() were not called or was "
                            "executed with errors",
                            -1);

        /* and wait while we let things run */
        pw_main_loop_run(data.loop);
    }

    void AudioCapture::stopCapture() {

        pw_main_loop_quit(data.loop);
        if (micCapture) pw_stream_disconnect(data.micStream);
        if (desktopSoundCapture) pw_stream_disconnect(data.desktopSoundStream);
    }


}; // namespace blaze