#include "blaze/capture/linux/generic.hpp"
#include "blaze/capture/linux/nvidia.hpp"
#include "blaze/capture/video.hpp"


namespace blaze {

    VideoCapture::VideoCapture() {

        // objects.emplace("nvfbc", internal::NvfbcCapture());
        // objects.emplace("generic", internal::X11Capture());
    }

}; // namespace blaze