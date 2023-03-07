#ifndef BLAZE_AUDIO_H
#define BLAZE_AUDIO_H

extern "C" {

typedef void (*onErrorCallbackType)(const char *, long);
typedef void (*onNewDataType)(void *, unsigned long);

struct AudioCapture;

enum state {

    AUDIO_CAPTURE_FALSE = 0,
    AUDIO_CAPTURE_TRUE = 1

};

int audio_capture_init(struct AudioCapture *instance);
void audio_capture_deinit(struct AudioCapture *instance);

void audio_capture_on_error(struct AudioCapture *instance,
                            onErrorCallbackType callback);

void audio_capture_on_new_data(struct AudioCapture *instance,
                               onNewDataType callback);

void audio_capture_on_new_mic_data(struct AudioCapture *instance,
                                   onNewDataType callback);

void audio_capture_load(struct AudioCapture *instance);
void audio_capture_set_mic_capturing(struct AudioCapture *instance,
                                     state value);
void audio_capture_set_desktop_sound_capturing(struct AudioCapture *instance,
                                               state value);

void audio_capture_start_capture(struct AudioCapture *instance);
void audio_capture_stop_capture(struct AudioCapture *instance);
};