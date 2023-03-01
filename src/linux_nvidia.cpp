#include "blaze/capture/linux/nvidia.hpp"

#include <cerrno>
#include <iostream>
#include <cstring>
#include <cstdlib>

#include <unistd.h>
#include <dlfcn.h>

#include "NvFBC.h"
#include "nvEncodeAPI.h"
#include "NvFBCUtils.h"

#define LIB_NVFBC_NAME "libnvidia-fbc.so.1"
#define LIB_ENCODEAPI_NAME "libnvidia-encode.so.1"

typedef NVENCSTATUS(NVENCAPI *PFNNVENCODEAPICREATEINSTANCEPROC)(
    NV_ENCODE_API_FUNCTION_LIST *);

namespace blaze::internal {

    NVENCSTATUS NvfbcCapture::validateEncodeGUID(void *encoder,
                                                 GUID encodeGuid) {
        unsigned int nGuids = 0, i, encodeGuidCount = 0, codecFound = 0;
        GUID *encodeGuidArray = NULL;
        NVENCSTATUS status = NV_ENC_SUCCESS;

        status = pEncFn.nvEncGetEncodeGUIDCount(encoder, &encodeGuidCount);
        if (status != NV_ENC_SUCCESS) {
            if (encodeGuidArray) {
                free(encodeGuidArray);
                encodeGuidArray = NULL;
            }
            errHandler("Failed to query number of supported codecs", status);
        }

        encodeGuidArray = (GUID *)malloc(sizeof(GUID) * encodeGuidCount);
        if (!encodeGuidArray) {
            if (encodeGuidArray) {
                free(encodeGuidArray);
                encodeGuidArray = NULL;
            }
            errHandler("Failed to allocate GUID array", status);
        }

        status = pEncFn.nvEncGetEncodeGUIDs(encoder, encodeGuidArray,
                                            encodeGuidCount, &nGuids);
        if (status != NV_ENC_SUCCESS) {

            if (encodeGuidArray) {
                free(encodeGuidArray);
                encodeGuidArray = NULL;
            }
            errHandler("Failed to query supported codecs", status);
        }


        for (i = 0; i < nGuids; i++) {
            if (!memcmp(&encodeGuid, &encodeGuidArray[i], sizeof(GUID))) {
                codecFound = 1;
                break;
            }
        }

        return codecFound ? NV_ENC_SUCCESS : status;
    }

    void NvfbcCapture::load() {

        PNVFBCCREATEINSTANCE NvFBCCreateInstance_ptr = NULL;
        PFNNVENCODEAPICREATEINSTANCEPROC NvEncodeAPICreateInstance = NULL;

        NVFBC_CREATE_HANDLE_PARAMS createHandleParams;

        Display *dpy = None;
        Pixmap pixmap = None;
        GLXPixmap glxPixmap = None;
        GLXFBConfig *fbConfigs;
        Bool res;
        int n;

        int attribs[] = {GLX_DRAWABLE_TYPE,
                         GLX_PIXMAP_BIT | GLX_WINDOW_BIT,
                         GLX_BIND_TO_TEXTURE_RGBA_EXT,
                         1,
                         GLX_BIND_TO_TEXTURE_TARGETS_EXT,
                         GLX_TEXTURE_2D_BIT_EXT,
                         None};

        dpy = XOpenDisplay(NULL);
        if (dpy == None) errHandler("Unable to open display", -1);

        fbConfigs = glXChooseFBConfig(dpy, DefaultScreen(dpy), attribs, &n);
        if (!fbConfigs) errHandler("Unable to find FB configs", -1);

        glxCtx = glXCreateNewContext(dpy, fbConfigs[0], GLX_RGBA_TYPE, None,
                                     True);
        if (glxCtx == None) errHandler("Unable to create GL context", -1);

        pixmap = XCreatePixmap(dpy, XDefaultRootWindow(dpy), 1, 1,
                               DisplayPlanes(dpy, XDefaultScreen(dpy)));

        if (pixmap == None) errHandler("Unable to create pixmap", -1);

        glxPixmap = glXCreatePixmap(dpy, fbConfigs[0], pixmap, NULL);
        if (glxPixmap == None) errHandler("Unable to create GLX pixmap", -1);

        res = glXMakeCurrent(dpy, glxPixmap, glxCtx);
        if (!res) errHandler("Unable to make context current", -1);

        glxFBConfig = fbConfigs[0];

        XFree(fbConfigs);

        libNVFBC = dlopen(LIB_NVFBC_NAME, RTLD_NOW);
        if (libNVFBC == NULL)
            errHandler("Unable to open '" LIB_NVFBC_NAME "'", -1);

        libEnc = dlopen(LIB_ENCODEAPI_NAME, RTLD_NOW);
        if (libNVFBC == NULL)
            errHandler("Unable to open '" LIB_ENCODEAPI_NAME "'", -1);

        NvFBCCreateInstance_ptr = (PNVFBCCREATEINSTANCE)dlsym(
            libNVFBC, "NvFBCCreateInstance");
        if (NvFBCCreateInstance_ptr == NULL)
            errHandler("Unable to resolve symbol 'NvFBCCreateInstance'", -1);

        memset(&pFn, 0, sizeof(pFn));

        pFn.dwVersion = NVFBC_VERSION;

        fbcStatus = NvFBCCreateInstance_ptr(&pFn);
        if (fbcStatus != NVFBC_SUCCESS)
            errHandler("Unable to create NvFBC instance", fbcStatus);

        NvEncodeAPICreateInstance = (PFNNVENCODEAPICREATEINSTANCEPROC)dlsym(
            libEnc, "NvEncodeAPICreateInstance");
        if (NvEncodeAPICreateInstance == NULL)
            errHandler("Unable to resolve symbol 'NvEncodeAPICreateInstance'",
                       -1);

        memset(&pEncFn, 0, sizeof(pEncFn));

        pEncFn.version = NV_ENCODE_API_FUNCTION_LIST_VER;

        encStatus = NvEncodeAPICreateInstance(&pEncFn);
        if (encStatus != NV_ENC_SUCCESS)
            errHandler("Unable to create NvEncodeAPI instance", encStatus);

        memset(&createHandleParams, 0, sizeof(createHandleParams));

        createHandleParams.dwVersion = NVFBC_CREATE_HANDLE_PARAMS_VER;
        createHandleParams.bExternallyManagedContext = NVFBC_TRUE;
        createHandleParams.glxCtx = glxCtx;
        createHandleParams.glxFBConfig = glxFBConfig;

        fbcStatus = pFn.nvFBCCreateHandle(&fbcHandle, &createHandleParams);
        if (fbcStatus != NVFBC_SUCCESS)
            errHandler(pFn.nvFBCGetLastErrorStr(fbcHandle), -1);

        NVFBC_GET_STATUS_PARAMS statusParams;
        NVFBC_CREATE_CAPTURE_SESSION_PARAMS createCaptureParams;
        NVFBC_TOGL_SETUP_PARAMS setupParams;

        NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS encodeSessionParams;
        NV_ENC_PRESET_CONFIG presetConfig;
        NV_ENC_INITIALIZE_PARAMS initParams;

        memset(&statusParams, 0, sizeof(statusParams));

        statusParams.dwVersion = NVFBC_GET_STATUS_PARAMS_VER;

        fbcStatus = pFn.nvFBCGetStatus(fbcHandle, &statusParams);
        if (fbcStatus != NVFBC_SUCCESS)
            errHandler(pFn.nvFBCGetLastErrorStr(fbcHandle), -1);

        if (statusParams.bCanCreateNow == NVFBC_FALSE)
            errHandler(
                "It is not possible to create a capture session on this system",
                -1);

        /*
         * Create a capture session.
         */
        memset(&createCaptureParams, 0, sizeof(createCaptureParams));

        createCaptureParams.dwVersion = NVFBC_CREATE_CAPTURE_SESSION_PARAMS_VER;
        createCaptureParams.eCaptureType = NVFBC_CAPTURE_TO_GL;
        createCaptureParams.bWithCursor = NVFBC_TRUE;
        createCaptureParams.frameSize = frameSize;
        createCaptureParams.eTrackingType = NVFBC_TRACKING_DEFAULT;
        createCaptureParams.bDisableAutoModesetRecovery = NVFBC_TRUE;

        fbcStatus = pFn.nvFBCCreateCaptureSession(fbcHandle,
                                                  &createCaptureParams);
        if (fbcStatus != NVFBC_SUCCESS)
            errHandler(pFn.nvFBCGetLastErrorStr(fbcHandle), -1);

        /*
         * Set up the capture session.
         */
        memset(&setupParams, 0, sizeof(setupParams));

        setupParams.dwVersion = NVFBC_TOGL_SETUP_PARAMS_VER;
        setupParams.eBufferFormat = bufferFormat;

        fbcStatus = pFn.nvFBCToGLSetUp(fbcHandle, &setupParams);
        if (fbcStatus != NVFBC_SUCCESS)
            errHandler(pFn.nvFBCGetLastErrorStr(fbcHandle), -1);

        /*
         * Create an encoder session
         */
        memset(&encodeSessionParams, 0, sizeof(encodeSessionParams));

        encodeSessionParams.version = NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS_VER;
        encodeSessionParams.apiVersion = NVENCAPI_VERSION;
        encodeSessionParams.deviceType = NV_ENC_DEVICE_TYPE_OPENGL;

        encStatus = pEncFn.nvEncOpenEncodeSessionEx(&encodeSessionParams,
                                                    &encoder);
        if (encStatus != NV_ENC_SUCCESS)
            errHandler("Failed to open an encoder session", encStatus);

        /*
         * Validate the codec requested
         */
        GUID encodeGuid = NV_ENC_CODEC_HEVC_GUID;
        encStatus = validateEncodeGUID(encoder, encodeGuid);
        if (encStatus != NV_ENC_SUCCESS)
            errHandler("Encode GUID cannot be validated", -1);

        memset(&presetConfig, 0, sizeof(presetConfig));

        presetConfig.version = NV_ENC_PRESET_CONFIG_VER;
        presetConfig.presetCfg.version = NV_ENC_CONFIG_VER;
        encStatus = pEncFn.nvEncGetEncodePresetConfig(
            encoder, encodeGuid, NV_ENC_PRESET_LOW_LATENCY_HQ_GUID,
            &presetConfig);
        if (encStatus != NV_ENC_SUCCESS)
            errHandler("Failed to obtain preset settings", encStatus);

        presetConfig.presetCfg.rcParams.averageBitRate = 5 * 1024 * 1024;
        presetConfig.presetCfg.rcParams.maxBitRate = 8 * 1024 * 1024;
        presetConfig.presetCfg.rcParams.vbvBufferSize =
            87382; /* single frame */


        /*
         * Initialize the encode session
         */
        memset(&initParams, 0, sizeof(initParams));

        initParams.version = NV_ENC_INITIALIZE_PARAMS_VER;
        initParams.encodeGUID = encodeGuid;
        initParams.presetGUID = NV_ENC_PRESET_LOW_LATENCY_HQ_GUID;
        initParams.encodeConfig = &presetConfig.presetCfg;
        initParams.encodeWidth = frameSize.w;
        initParams.encodeHeight = frameSize.h;
        initParams.frameRateNum = refreshRate;
        initParams.frameRateDen = 1;
        initParams.enablePTD = 1;

        encStatus = pEncFn.nvEncInitializeEncoder(encoder, &initParams);
        if (encStatus != NV_ENC_SUCCESS)
            errHandler("Failed to initialize the encode session", encStatus);


        /*
         * Register the textures received from NvFBC for use with NvEncodeAPI
         */
        for (std::uint32_t i = 0; i < NVFBC_TOGL_TEXTURES_MAX; i++) {
            NV_ENC_REGISTER_RESOURCE registerParams;
            NV_ENC_INPUT_RESOURCE_OPENGL_TEX texParams;

            if (!setupParams.dwTextures[i]) { break; }

            memset(&registerParams, 0, sizeof(registerParams));

            texParams.texture = setupParams.dwTextures[i];
            texParams.target = setupParams.dwTexTarget;

            registerParams.version = NV_ENC_REGISTER_RESOURCE_VER;
            registerParams.resourceType = NV_ENC_INPUT_RESOURCE_TYPE_OPENGL_TEX;
            registerParams.width = frameSize.w;
            registerParams.height = frameSize.h;
            registerParams.pitch = frameSize.w;
            registerParams.resourceToRegister = &texParams;
            registerParams.bufferFormat = NV_ENC_BUFFER_FORMAT_NV12;

            encStatus = pEncFn.nvEncRegisterResource(encoder, &registerParams);
            if (encStatus != NV_ENC_SUCCESS)
                errHandler("Failed to register texture", encStatus);

            registeredResources[i] = registerParams.registeredResource;
        }


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

        NV_ENC_CREATE_BITSTREAM_BUFFER bitstreamBufferParams;
        NV_ENC_MAP_INPUT_RESOURCE mapParams;
        NV_ENC_LOCK_BITSTREAM lockParams;

        NV_ENC_INPUT_PTR inputBuffer = nullptr;

        int bufferSize = 0;

        /*
         * Create a bitstream buffer to hold the output
         */
        memset(&bitstreamBufferParams, 0, sizeof(bitstreamBufferParams));
        bitstreamBufferParams.version = NV_ENC_CREATE_BITSTREAM_BUFFER_VER;

        encStatus = pEncFn.nvEncCreateBitstreamBuffer(encoder,
                                                      &bitstreamBufferParams);
        if (encStatus != NV_ENC_SUCCESS)
            errHandler("Failed to create a bitstream buffer", encStatus);

        outputBuffer = bitstreamBufferParams.bitstreamBuffer;

        /*
         * Pre-fill mapping information
         */
        memset(&mapParams, 0, sizeof(mapParams));

        mapParams.version = NV_ENC_MAP_INPUT_RESOURCE_VER;

        /*
         * Pre-fill frame encoding information
         */
        memset(&encParams, 0, sizeof(encParams));

        encParams.version = NV_ENC_PIC_PARAMS_VER;
        encParams.inputWidth = frameSize.w;
        encParams.inputHeight = frameSize.h;
        encParams.inputPitch = frameSize.w;
        encParams.pictureStruct = NV_ENC_PIC_STRUCT_FRAME;
        encParams.outputBitstream = outputBuffer;


        /*
         * Start capturing and encoding frames.
         */

        isScreenCaptured.store(true);

        while (isScreenCaptured) {
            NVFBC_TOGL_GRAB_FRAME_PARAMS grabParams;

            memset(&grabParams, 0, sizeof(grabParams));

            grabParams.dwVersion = NVFBC_TOGL_GRAB_FRAME_PARAMS_VER;

            /*
             * Capture a frame.
             */
            fbcStatus = pFn.nvFBCToGLGrabFrame(fbcHandle, &grabParams);
            if (fbcStatus == NVFBC_ERR_MUST_RECREATE)
                errHandler("Capture session must be recreated", -1);
            else if (fbcStatus != NVFBC_SUCCESS)
                errHandler(pFn.nvFBCGetLastErrorStr(fbcHandle), -1);

            /*
             * Map the frame for use by the encoder.
             */
            mapParams.registeredResource =
                registeredResources[grabParams.dwTextureIndex];
            encStatus = pEncFn.nvEncMapInputResource(encoder, &mapParams);
            if (encStatus != NV_ENC_SUCCESS)
                errHandler("Failed to map the resource", encStatus);

            encParams.inputBuffer = inputBuffer = mapParams.mappedResource;
            encParams.bufferFmt = mapParams.mappedBufferFmt;
            encParams.frameIdx = encParams.inputTimeStamp;

            /*
             * Encode the frame.
             */
            encStatus = pEncFn.nvEncEncodePicture(encoder, &encParams);
            if (encStatus != NV_ENC_SUCCESS)
                errHandler("Failed to encode frame", encStatus);
            else {
                /*
                 * Get the bitstream and dump to file.
                 */
                memset(&lockParams, 0, sizeof(lockParams));

                lockParams.version = NV_ENC_LOCK_BITSTREAM_VER;
                lockParams.outputBitstream = outputBuffer;

                encStatus = pEncFn.nvEncLockBitstream(encoder, &lockParams);
                if (encStatus == NV_ENC_SUCCESS) {
                    bufferSize = lockParams.bitstreamSizeInBytes;

                    newFrameHandler(lockParams.bitstreamBufferPtr, bufferSize);

                    encStatus = pEncFn.nvEncUnlockBitstream(encoder,
                                                            outputBuffer);
                    if (encStatus != NV_ENC_SUCCESS)
                        errHandler("Failed to unlock bitstream buffer",
                                   encStatus);
                } else errHandler("Failed to lock bitstream buffer", encStatus);
            }

            /*
             * Unmap the frame.
             */
            encStatus = pEncFn.nvEncUnmapInputResource(encoder, inputBuffer);
            if (encStatus != NV_ENC_SUCCESS)
                errHandler("Failed to unmap the resource", encStatus);

            if (bufferSize == 0)
                errHandler("Failed to obtain the bitstream", -1);
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
        std::function<void(void *, std::uint64_t)> callback) {

        newFrameHandler = callback;
    }

    void NvfbcCapture::stopCapture() {

        isScreenCaptured.store(false);

        this->clear();
    }

    void NvfbcCapture::clear() {

        NVFBC_DESTROY_HANDLE_PARAMS destroyHandleParams;
        NVFBC_DESTROY_CAPTURE_SESSION_PARAMS destroyCaptureParams;

        memset(&encParams, 0, sizeof(encParams));
        encParams.version = NV_ENC_PIC_PARAMS_VER;
        encParams.encodePicFlags = NV_ENC_PIC_FLAG_EOS;

        encStatus = pEncFn.nvEncEncodePicture(encoder, &encParams);
        if (encStatus != NV_ENC_SUCCESS)
            errHandler("Failed to flush the encoder", encStatus);

        if (outputBuffer) {
            encStatus = pEncFn.nvEncDestroyBitstreamBuffer(encoder,
                                                           outputBuffer);
            if (encStatus != NV_ENC_SUCCESS)
                errHandler("Failed to destroy buffer", encStatus);

            outputBuffer = NULL;
        }

        for (std::uint32_t i = 0; i < NVFBC_TOGL_TEXTURES_MAX; i++) {
            if (registeredResources[i]) {
                encStatus = pEncFn.nvEncUnregisterResource(
                    encoder, registeredResources[i]);
                if (encStatus != NV_ENC_SUCCESS)
                    errHandler("Failed to unregister resource", encStatus);
                registeredResources[i] = NULL;
            }
        }

        encStatus = pEncFn.nvEncDestroyEncoder(encoder);
        if (encStatus != NV_ENC_SUCCESS)
            errHandler("Failed to destroy encoder", encStatus);

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

    void NvfbcCapture::setBufferFormat(blaze::format type) {

        switch (type) {

            case (blaze::format::argb):
                bufferFormat = NVFBC_BUFFER_FORMAT_ARGB;
                break;
            case (blaze::format::rgb):
                bufferFormat = NVFBC_BUFFER_FORMAT_RGB;
                break;
            case (blaze::format::rgba):
                bufferFormat = NVFBC_BUFFER_FORMAT_RGBA;
                break;
            case (blaze::format::bgra):
                bufferFormat = NVFBC_BUFFER_FORMAT_BGRA;
                break;
            case (blaze::format::nv12):
                bufferFormat = NVFBC_BUFFER_FORMAT_NV12;
                break;
            case (blaze::format::yuv420p):
                bufferFormat = NVFBC_BUFFER_FORMAT_YUV420P;
                break;
            case (blaze::format::yuv444p):
                bufferFormat = NVFBC_BUFFER_FORMAT_YUV444P;
                break;
        }
    }

}; // namespace blaze::internal