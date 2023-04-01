#pragma once


namespace blaze {

    constexpr const char* OSName() {

#ifdef _WIN32
        return "Windows";
#elif __linux__
        return "Linux";
#elif __APPLE__ || __MACH__
        return "MacOS";
#elif __FreeBSD__
        return "FreeBSD";
#elif __ANDROID__
        return "Android";
#endif
    }

}; // namespace blaze