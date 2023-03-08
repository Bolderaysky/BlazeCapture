#include "blaze/capture/core.hpp"
#include "blaze/capture/linux/audio.hpp"

#include <iostream>
#include <thread>

int main(int, char **) {

    blaze::BlazeCapture app;

    app.run();

    // blaze::AudioCapture Audio;
    // Audio.onErrorCallback([](const char *err, std::int32_t c) {
    //     std::cerr << err << "\nStatus code: " << c << std::endl;
    //     std::abort();
    // });

    // FILE *f = fopen("test.raw", "wb");

    // Audio.onNewDesktopData([&](void *buffer, std::uint32_t size) {
    //     fwrite(buffer, size, sizeof(float), f);
    // });

    // Audio.setMicCapturing(false);

    // std::thread([&]() {
    //     sleep(5u);
    //     Audio.stopCapture();
    //     std::cout << "ended\n";
    // }).detach();

    // Audio.load();

    // Audio.startCapture();

    // sleep(5);
}