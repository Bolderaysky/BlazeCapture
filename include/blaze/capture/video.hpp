#pragma once

#ifdef _WIN32

#include "blaze/capture/win32/nvidia.hpp"
#include "blaze/capture/win32/amd.hpp"
#include "blaze/capture/win32/intel.hpp"
#include "blaze/capture/win32/generic.hpp"

#else

#include "blaze/capture/linux/nvidia.hpp"
#include "blaze/capture/linux/amd.hpp"
#include "blaze/capture/linux/intel.hpp"
#include "blaze/capture/linux/generic.hpp"

#endif