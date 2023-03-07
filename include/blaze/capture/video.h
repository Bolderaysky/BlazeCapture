#ifndef BLAZE_VIDEO
#define BLAZE_VIDEO

extern "C" {

typedef void (*onErrorCallbackType)(const char *, long);
typedef void (*onNewFrameType)(void *, unsigned long);

struct VideoCapture;

int video_capture_init(struct VideoCapture *instance);
void video_capture_deinit(struct VideoCapture *instance);

void video_capture_on_error(struct VideoCapture *instance,
                            onErrorCallbackType callback);

void video_capture_on_new_frame(struct VideoCapture *instance,
                                onNewFrameType callback);

void video_capture_load(struct VideoCapture *instance);

void video_capture_start_capture(struct VideoCapture *instance);
void video_capture_stop_capture(struct VideoCapture *instance);

void video_capture_set_refresh_rate(struct VideoCapture *instance,
                                    unsigned short fps);
void video_capture_set_resolution(struct VideoCapture *instance,
                                  unsigned short width, unsigned short height);

const char **video_capture_list_screen(struct VideoCapture *instance);
void video_capture_update_screen_list(struct VideoCapture *instance);
void video_capture_select_screen(struct VideoCapture *instance,
                                 const char *screen);
};

#endif
