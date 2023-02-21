#include "NvFBC.h"
#include "blaze/capture/nvidia.hpp"

#define LIB_NVFBC_NAME "libnvidia-fbc.so.1"
#define LIB_CUDA_NAME "libcuda.so.1"

#include <iostream>
#include <cstring>
#include <cstdlib>

#include <unistd.h>
#include <dlfcn.h>

/*
 * CUDA entry points
 */
typedef CUresult (*CUINITPROC)(unsigned int Flags);
typedef CUresult (*CUDEVICEGETPROC)(CUdevice *device, int ordinal);
typedef CUresult (*CUCTXCREATEV2PROC)(CUcontext *pctx, unsigned int flags,
                                      CUdevice dev);
typedef CUresult (*CUMEMCPYDTOHV2PROC)(void *dstHost, CUdeviceptr srcDevice,
                                       size_t ByteCount);

inline CUINITPROC cuInit_ptr = nullptr;
inline CUDEVICEGETPROC cuDeviceGet_ptr = nullptr;
inline CUCTXCREATEV2PROC cuCtxCreate_v2_ptr = nullptr;
inline CUMEMCPYDTOHV2PROC cuMemcpyDtoH_v2_ptr = nullptr;


namespace blaze::internal {

    void NvfbcCapture::load() {

        CUcontext cuCtx;
        NVFBC_CREATE_HANDLE_PARAMS createHandleParams;

        NVFBC_GET_STATUS_PARAMS statusParams;
        NVFBC_CREATE_CAPTURE_SESSION_PARAMS createCaptureParams;
        NVFBC_TOCUDA_SETUP_PARAMS setupParams;

        const auto &bufferFormat = NVFBC_BUFFER_FORMAT_YUV420P;

        NVFBC_TRACKING_TYPE trackingType = NVFBC_TRACKING_DEFAULT;


        libCUDA = dlopen(LIB_CUDA_NAME, RTLD_NOW);
        if (libCUDA == nullptr)
            errHandler("Unable to open '" LIB_CUDA_NAME "'", -1);

        cuInit_ptr = (CUINITPROC)dlsym(libCUDA, "cuInit");
        if (cuInit_ptr == nullptr)
            errHandler("Unable to resolve symbol 'cuInit'", -1);

        cuDeviceGet_ptr = (CUDEVICEGETPROC)dlsym(libCUDA, "cuDeviceGet");
        if (cuDeviceGet_ptr == nullptr)
            errHandler("Unable to resolve symbol 'cuDeviceGet'", -1);

        cuCtxCreate_v2_ptr = (CUCTXCREATEV2PROC)dlsym(libCUDA,
                                                      "cuCtxCreate_v2");
        if (cuCtxCreate_v2_ptr == nullptr)
            errHandler("Unable to resolve symbol 'cuCtxCreate_v2'", -1);

        cuMemcpyDtoH_v2_ptr = (CUMEMCPYDTOHV2PROC)dlsym(libCUDA,
                                                        "cuMemcpyDtoH_v2");
        if (cuMemcpyDtoH_v2_ptr == nullptr)
            errHandler("Unable to resolve symbol 'cuMemcpyDtoH_v2'", -1);

        CUresult cuRes;
        CUdevice cuDev;

        cuRes = cuInit_ptr(0);
        if (cuRes != CUDA_SUCCESS)
            errHandler("Unable to initialize CUDA", cuRes);

        cuRes = cuDeviceGet_ptr(&cuDev, 0);
        if (cuRes != CUDA_SUCCESS) errHandler("Unable to get CUDA", cuRes);

        cuRes = cuCtxCreate_v2_ptr(&cuCtx, CU_CTX_SCHED_AUTO, cuDev);
        if (cuRes != CUDA_SUCCESS)
            errHandler("Unable to create CUDA context", cuRes);

        libNVFBC = dlopen(LIB_NVFBC_NAME, RTLD_NOW);
        if (libNVFBC == nullptr)
            errHandler("Unable to open '" LIB_NVFBC_NAME "'", cuRes);

        const auto NvFBCCreateInstance_ptr = (PNVFBCCREATEINSTANCE)dlsym(
            libNVFBC, "NvFBCCreateInstance");
        if (NvFBCCreateInstance_ptr == nullptr)
            errHandler("Unable to resolve symbol 'NvFBCCreateInstance'", -1);

        memset(&pFn, 0, sizeof(pFn));

        pFn.dwVersion = NVFBC_VERSION;

        fbcStatus = NvFBCCreateInstance_ptr(&pFn);
        if (fbcStatus != NVFBC_SUCCESS)
            errHandler("Unable to create NvFBC instance", fbcStatus);

        memset(&createHandleParams, 0, sizeof(createHandleParams));

        createHandleParams.dwVersion = NVFBC_CREATE_HANDLE_PARAMS_VER;

        fbcStatus = pFn.nvFBCCreateHandle(&fbcHandle, &createHandleParams);
        if (fbcStatus != NVFBC_SUCCESS)
            errHandler(pFn.nvFBCGetLastErrorStr(fbcHandle), -1);

        memset(&statusParams, 0, sizeof(statusParams));

        statusParams.dwVersion = NVFBC_GET_STATUS_PARAMS_VER;

        fbcStatus = pFn.nvFBCGetStatus(fbcHandle, &statusParams);
        if (fbcStatus != NVFBC_SUCCESS)
            errHandler(pFn.nvFBCGetLastErrorStr(fbcHandle), -1);

        if (statusParams.bCanCreateNow == NVFBC_FALSE)
            errHandler("It is not possible to create a capture session on this "
                       "system.\n",
                       -1);

        memset(&createCaptureParams, 0, sizeof(createCaptureParams));

        createCaptureParams.dwVersion = NVFBC_CREATE_CAPTURE_SESSION_PARAMS_VER;
        createCaptureParams.eCaptureType = NVFBC_CAPTURE_SHARED_CUDA;
        createCaptureParams.bWithCursor = NVFBC_TRUE;
        createCaptureParams.frameSize = frameSize;
        createCaptureParams.eTrackingType = trackingType;

        fbcStatus = pFn.nvFBCCreateCaptureSession(fbcHandle,
                                                  &createCaptureParams);
        if (fbcStatus != NVFBC_SUCCESS)
            errHandler(pFn.nvFBCGetLastErrorStr(fbcHandle), -1);

        memset(&setupParams, 0, sizeof(setupParams));

        setupParams.dwVersion = NVFBC_TOCUDA_SETUP_PARAMS_VER;
        setupParams.eBufferFormat = bufferFormat;

        fbcStatus = pFn.nvFBCToCudaSetUp(fbcHandle, &setupParams);
        if (fbcStatus != NVFBC_SUCCESS)
            errHandler(pFn.nvFBCGetLastErrorStr(fbcHandle), -1);

        isInitialized = true;
    }

    NvfbcCapture::NvfbcCapture() {
    }

    NvfbcCapture::~NvfbcCapture() {
    }

    void NvfbcCapture::startCapture() {

        if (!isInitialized)
            errHandler("NvfbcCapture::load() were not called or was executed "
                       "with errors",
                       -1);

        isScreenCaptured.store(true);

        CUdeviceptr cuDevicePtr;
        std::uint32_t lastByteSize = 0;

        CUresult cuRes;

        NVFBC_TOCUDA_GRAB_FRAME_PARAMS grabParams;

        NVFBC_FRAME_GRAB_INFO frameInfo;

        memset(&grabParams, 0, sizeof(grabParams));
        memset(&frameInfo, 0, sizeof(frameInfo));

        grabParams.dwVersion = NVFBC_TOCUDA_GRAB_FRAME_PARAMS_VER;

        grabParams.dwFlags = NVFBC_TOCUDA_GRAB_FLAGS_NOWAIT;

        grabParams.pFrameGrabInfo = &frameInfo;

        grabParams.pCUDADeviceBuffer = &cuDevicePtr;

        while (isScreenCaptured.load()) {

            fbcStatus = pFn.nvFBCToCudaGrabFrame(fbcHandle, &grabParams);
            if (fbcStatus != NVFBC_SUCCESS)
                errHandler(pFn.nvFBCGetLastErrorStr(fbcHandle), -1);

            if (lastByteSize < frameInfo.dwByteSize) {
                buffer = (std::uint8_t *)realloc(buffer, frameInfo.dwByteSize);
                if (buffer == nullptr)
                    errHandler("Unable to allocate system memory\n", -1);

                lastByteSize = frameInfo.dwByteSize;
            }

            cuRes = cuMemcpyDtoH_v2_ptr((void *)buffer, cuDevicePtr,
                                        frameInfo.dwByteSize);
            if (cuRes != CUDA_SUCCESS) errHandler("CUDA memcpy failure", cuRes);

            newFrameHandler(buffer, frameInfo.dwByteSize);

            // usleep(sleepTime);
        }
    }

    void NvfbcCapture::onErrorCallback(
        std::function<void(const char *, std::int32_t)> callback) {

        errHandler = callback;
    }

    void NvfbcCapture::setRefreshRate(std::uint16_t fps) {

        refreshRate = fps;
    }

    void NvfbcCapture::setResolution(std::uint16_t width,
                                     std::uint16_t height) {

        frameSize.w = width;
        frameSize.h = height;
    }

    void NvfbcCapture::onNewFrame(
        std::function<void(std::uint8_t *, std::uint64_t)> callback) {

        newFrameHandler = callback;
    }

    void NvfbcCapture::stopCapture() {

        NVFBC_DESTROY_CAPTURE_SESSION_PARAMS destroyCaptureParams;
        NVFBC_DESTROY_HANDLE_PARAMS destroyHandleParams;


        isScreenCaptured.store(false);

        /*
         * Destroy capture session, tear down resources.
         */
        memset(&destroyCaptureParams, 0, sizeof(destroyCaptureParams));

        destroyCaptureParams.dwVersion =
            NVFBC_DESTROY_CAPTURE_SESSION_PARAMS_VER;

        fbcStatus = pFn.nvFBCDestroyCaptureSession(fbcHandle,
                                                   &destroyCaptureParams);
        if (fbcStatus != NVFBC_SUCCESS)
            errHandler(pFn.nvFBCGetLastErrorStr(fbcHandle), -1);

        /*
         * Destroy session handle, tear down more resources.
         */
        memset(&destroyHandleParams, 0, sizeof(destroyHandleParams));

        destroyHandleParams.dwVersion = NVFBC_DESTROY_HANDLE_PARAMS_VER;

        fbcStatus = pFn.nvFBCDestroyHandle(fbcHandle, &destroyHandleParams);
        if (fbcStatus != NVFBC_SUCCESS)
            errHandler(pFn.nvFBCGetLastErrorStr(fbcHandle), -1);
    }

}; // namespace blaze::internal