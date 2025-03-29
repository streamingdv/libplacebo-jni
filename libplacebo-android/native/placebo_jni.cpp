#include "com_grill_libplacebo_PlaceboManager.h"
#include <jni.h>
#include <android/native_window_jni.h>
#include <iostream>

#define EGL_EGLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES

#include <GLES3/gl3.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2ext.h>
#include <libplacebo/gpu.h>
#include <libplacebo/options.h>
#include <libplacebo/renderer.h>
#include <libplacebo/opengl.h>
#include <libplacebo/swapchain.h>
#include <libplacebo/log.h>
#include <libplacebo/cache.h>

#define PL_LIBAV_IMPLEMENTATION 0
#include <libplacebo/utils/libav.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavcodec/mediacodec.h>
#include <libavcodec/jni.h>
#include <libavutil/hwcontext.h>
#include <libavutil/avutil.h>
#include <libavutil/error.h>
#include <libavutil/frame.h>
#include <libavutil/hwcontext_mediacodec.h>
}

static pl_opengl placebo_gl = NULL;
static pl_renderer placebo_renderer = NULL;
static pl_swapchain placebo_swapchain = NULL;

static  AVMediaCodecContext *mMediaCodecContext = nullptr;

/*** define helper functions ***/

jobject globalCallback;
jmethodID onLogMethod;

void LogCallbackFunction(void *log_priv, enum pl_log_level level, const char *msg) {
    JNIEnv *env = (JNIEnv *)log_priv;
    if (env != nullptr && globalCallback != nullptr) {
        jstring message = env->NewStringUTF(msg);
        env->CallVoidMethod(globalCallback, onLogMethod, (jint)level, message);
        env->DeleteLocalRef(message);
    } else {
        std::cout << "Log Level " << level << ": " << msg << std::endl;
    }
}

extern "C"
JNIEXPORT jlong JNICALL Java_com_grill_placebo_PlaceboManager_plLogCreate
  (JNIEnv *env, jobject obj, jint apiVersion, jint logLevel, jobject logCallback) {
  globalCallback = env->NewGlobalRef(logCallback);
  jclass cls = env->GetObjectClass(logCallback);
  onLogMethod = env->GetMethodID(cls, "onLog", "(ILjava/lang/String;)V");
  if (!onLogMethod) {
      env->DeleteGlobalRef(globalCallback); // Cleanup on failure
      return 0; // Failed
  }

  enum pl_log_level plLevel = static_cast<enum pl_log_level>(logLevel);

  struct pl_log_params log_params = {
      .log_cb = LogCallbackFunction,
      .log_priv = env,
      .log_level = plLevel,
  };

  pl_log placebo_log = pl_log_create(apiVersion, &log_params);

  return reinterpret_cast<jlong>(placebo_log);
}

extern "C"
JNIEXPORT void JNICALL Java_com_grill_placebo_PlaceboManager_plLogDestroy
  (JNIEnv *env, jobject obj, jlong placebo_log) {
  pl_log log = reinterpret_cast<pl_log>(placebo_log);
  if (log != nullptr) {
      pl_log_destroy(&log);
  }
  if (globalCallback != nullptr) {
      env->DeleteGlobalRef(globalCallback);
      globalCallback = nullptr;
  }
}

// OpenGL ES + Swapchain Initialization
JNIEXPORT jboolean JNICALL
Java_com_grill_placebo_PlaceboManager_plOpenGLCreate(
    JNIEnv *env, jobject thiz, jlong logHandle, jlong eglDisplay, jlong eglContext,
    jint width, jint height, jint colorTransfer) {

    pl_log log = reinterpret_cast<pl_log>(logHandle);

    struct pl_opengl_params params = {
        .get_proc_addr = (pl_voidfunc_t (*)(const char *)) eglGetProcAddress,
        .egl_display = (EGLDisplay)eglDisplay,
        .egl_context = (EGLContext)eglContext
    };

    placebo_gl = pl_opengl_create(log, &params);
    if (!placebo_gl) {
        return JNI_FALSE;
    }

    placebo_renderer = pl_renderer_create(log, placebo_gl->gpu);
    if (!placebo_renderer) {
        pl_opengl_destroy(&placebo_gl);
        placebo_gl = nullptr;
        return JNI_FALSE;
    }

    struct pl_opengl_swapchain_params swapchain_params = {
        .framebuffer = {
            .flipped = false
        },
        .max_swapchain_depth = 1
    };

    placebo_swapchain = pl_opengl_create_swapchain(placebo_gl, &swapchain_params);
    if (!placebo_swapchain) {
        pl_renderer_destroy(&placebo_renderer);
        placebo_renderer = nullptr;
        pl_opengl_destroy(&placebo_gl);
        placebo_gl = nullptr;
        return JNI_FALSE;
    }

    return JNI_TRUE;
}

extern "C"
JNIEXPORT jboolean JNICALL Java_com_grill_placebo_PlaceboManager_plSwapchainResize
  (JNIEnv *env, jobject obj, jint width, jint height) {
    if (!placebo_swapchain)
        return JNI_FALSE;

    int w = static_cast<int>(width);
    int h = static_cast<int>(height);
    bool success = pl_swapchain_resize(placebo_swapchain, &w, &h);
    return static_cast<jboolean>(success);
}

// Rendering external texture via libplacebo
JNIEXPORT jboolean JNICALL
Java_com_grill_placebo_PlaceboManager_renderEGLImage(
    JNIEnv *env, jobject thiz, jlong textureHandle,
    jint width, jint height, jint colorTransfer, jint colorRange) {

    if (!placebo_renderer) {
        LogCallbackFunction(nullptr, PL_LOG_ERR, "Renderer is not initialized!");
        return JNI_FALSE;
    }

    GLuint texture = (GLuint)(uintptr_t)textureHandle;

    struct pl_opengl_wrap_params wrap_params = {
        .iformat = (colorTransfer == 6) ? GL_RGBA16F : GL_RGB8, // Not sure...
        .width = width,
        .height = height,
        .depth = 1,
        .texture = texture,
        .target = GL_TEXTURE_EXTERNAL_OES,
    };

    pl_tex ext_tex = pl_opengl_wrap(placebo_gl->gpu, &wrap_params);
    if (!ext_tex) {
        LogCallbackFunction(nullptr, PL_LOG_ERR, "Failed to wrap external texture as pl_tex!");
        return JNI_FALSE;
    }

    struct pl_frame placebo_frame = {0};
    struct pl_frame target_frame = {0};

    placebo_frame.num_planes = 1;
    placebo_frame.planes[0] = (struct pl_plane) {
        .texture = ext_tex,
        .components = 3,
        .component_mapping = {0, 1, 2, -1},  // YUV mapping for EGLImage
    };
    //placebo_frame.color = pl_color_space_unknown;
    // Manage color space (SDR/HDR)
    placebo_frame.color = (struct pl_color_space) {
        .primaries = (colorTransfer == 6) ? PL_COLOR_PRIM_BT_2020 : PL_COLOR_PRIM_BT_709,
        .transfer = (colorTransfer == 6) ? PL_COLOR_TRC_PQ : PL_COLOR_TRC_GAMMA22,
        //.hdr = (colorTransfer == 6) ? PL_HDR_METADATA_HDR10 : PL_HDR_METADATA_NONE // ToDo check
    };
    //placebo_frame.repr = pl_color_repr_unknown;
    placebo_frame.repr = (struct pl_color_repr) {
        .sys = (colorTransfer == 6) ? PL_COLOR_SYSTEM_BT_2020_NC : PL_COLOR_SYSTEM_BT_709,
        .levels = (colorRange == 2) ? PL_COLOR_LEVELS_LIMITED : PL_COLOR_LEVELS_FULL,
        .alpha = PL_ALPHA_INDEPENDENT
    };
    placebo_frame.crop = (pl_rect2df){ 0.0f, 0.0f, (float)width, (float)height };

    // Crop Handling
    pl_rect2df crop = {
        .x0 = 0.0f,
        .y0 = 0.0f,
        .x1 = 1.0f,
        .y1 = 1.0f
    };
    pl_rect2df_aspect_copy(&target_frame.crop, &crop, 0.0);

    struct pl_swapchain_frame sw_frame = {0};
    if (!pl_swapchain_start_frame(placebo_swapchain, &sw_frame)) {
        LogCallbackFunction(nullptr, PL_LOG_ERR, "Failed to start swapchain frame!");
        pl_tex_destroy(placebo_gl->gpu, &ext_tex);
        return JNI_FALSE;
    }
    pl_frame_from_swapchain(&target_frame, &sw_frame);

    if (!pl_render_image(placebo_renderer, &placebo_frame, &target_frame, NULL)) {
        LogCallbackFunction(nullptr, PL_LOG_ERR, "Failed to render Placebo frame!");
        pl_tex_destroy(placebo_gl->gpu, &ext_tex);
        return JNI_FALSE;
    }

    if (!pl_swapchain_submit_frame(placebo_swapchain)) {
        LogCallbackFunction(nullptr, PL_LOG_ERR, "Failed to submit swapchain frame!");
        pl_tex_destroy(placebo_gl->gpu, &ext_tex);
        return JNI_FALSE;
    }

    pl_swapchain_swap_buffers(placebo_swapchain);
    pl_tex_destroy(placebo_gl->gpu, &ext_tex);

    return JNI_TRUE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_grill_placebo_PlaceboManager_bindSurfaceToHwDevice(JNIEnv* env, jclass clazz,
                                                             jlong avCodecContexHandle, jobject javaSurface) {
    JavaVM* javaVm = nullptr;
    env->GetJavaVM(&javaVm);
    av_jni_set_java_vm(javaVm, nullptr);

    LogCallbackFunction(env, PL_LOG_INFO, "0");

    if (avCodecContexHandle == 0) {
        LogCallbackFunction(env, PL_LOG_ERR, "avCodecContexHandle is null");
        return JNI_FALSE;
    }

    AVCodecContext* mCodecContext = reinterpret_cast<AVCodecContext*>(avCodecContexHandle);
    if (!mCodecContext) {
        LogCallbackFunction(env, PL_LOG_ERR, "mCodecContext is null");
        return JNI_FALSE;
    }

    LogCallbackFunction(env, PL_LOG_INFO, "1");

    if (javaSurface == nullptr) {
        LogCallbackFunction(env, PL_LOG_ERR, "javaSurface is null");
        return JNI_FALSE;
    }

    LogCallbackFunction(env, PL_LOG_INFO, "2");

    mMediaCodecContext = av_mediacodec_alloc_context();
    if (!mMediaCodecContext) {
        LogCallbackFunction(env, PL_LOG_ERR, "Failed to allocate AVMediaCodecContext");
        return JNI_FALSE;
    }
    LogCallbackFunction(env, PL_LOG_INFO, "3");

    int ret = av_mediacodec_default_init(mCodecContext, mMediaCodecContext, javaSurface);
    if (ret < 0) {
        if (mMediaCodecContext != nullptr) {
            av_mediacodec_default_free(mCodecContext);
            mMediaCodecContext = nullptr;
        }
        char errbuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errbuf, sizeof(errbuf));
        char buf[512];
        snprintf(buf, sizeof(buf), "av_mediacodec_default_init() failed: %s (%d)", errbuf, ret);
        LogCallbackFunction(env, PL_LOG_ERR, buf);
        return JNI_FALSE;
    }

    LogCallbackFunction(env, PL_LOG_INFO, "4");
    return JNI_TRUE;
}

pl_tex placebo_tex_global[4] = {nullptr, nullptr, nullptr, nullptr};

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_grill_placebo_PlaceboManager_plRenderAvFrame(
    JNIEnv *env, jobject obj,
    jlong avframeHandle) {

    LogCallbackFunction(env, PL_LOG_INFO, "plRenderAvFrame() called");

    AVFrame *frame = reinterpret_cast<AVFrame *>(avframeHandle);
    if (!frame) {
        LogCallbackFunction(env, PL_LOG_ERR, "AVFrame is null!");
        return JNI_FALSE;
    }

    if (!placebo_gl) {
        LogCallbackFunction(env, PL_LOG_ERR, "placebo_gl is null!");
        return JNI_FALSE;
    }

    if (!placebo_renderer) {
        LogCallbackFunction(env, PL_LOG_ERR, "placebo_renderer is null!");
        return JNI_FALSE;
    }

    if (!placebo_swapchain) {
        LogCallbackFunction(env, PL_LOG_ERR, "placebo_swapchain is null!");
        return JNI_FALSE;
    }

    if (!frame->buf[0]) {
        LogCallbackFunction(env, PL_LOG_ERR, "AVFrame buf[0] is null! Likely not backed.");
        return JNI_FALSE;
    }

    if (!frame->data[3]) {
        LogCallbackFunction(env, PL_LOG_WARN, "AVFrame data[3] is null. Possibly no hardware buffer.");
    } else {
        uintptr_t handle = reinterpret_cast<uintptr_t>(frame->data[3]);
        char msg[128];
        snprintf(msg, sizeof(msg), "AVFrame data[3] (likely AHardwareBuffer*) = 0x%" PRIxPTR, handle);
        LogCallbackFunction(env, PL_LOG_INFO, msg);
    }

    struct pl_frame placebo_frame = {0};
    struct pl_frame target_frame = {0};
    struct pl_swapchain_frame sw_frame = {0};

    struct pl_avframe_params avparams = {
        .frame = frame,
        .tex = placebo_tex_global,
    };

    LogCallbackFunction(env, PL_LOG_INFO, "Calling pl_map_avframe_ex...");
    if (!pl_map_avframe_ex(placebo_gl->gpu, &placebo_frame, &avparams)) {
        LogCallbackFunction(env, PL_LOG_ERR, "Failed to map AVFrame!");
        return JNI_FALSE;
    }

    LogCallbackFunction(env, PL_LOG_INFO, "Mapped AVFrame, updating colorspace hint");
    pl_swapchain_colorspace_hint(placebo_swapchain, &placebo_frame.color);

    LogCallbackFunction(env, PL_LOG_INFO, "Starting swapchain frame...");
    if (!pl_swapchain_start_frame(placebo_swapchain, &sw_frame)) {
        LogCallbackFunction(env, PL_LOG_ERR, "Failed to start swapchain frame!");
        pl_unmap_avframe(placebo_gl->gpu, &placebo_frame);
        return JNI_FALSE;
    }

    pl_frame_from_swapchain(&target_frame, &sw_frame);
    pl_rect2df_aspect_copy(&target_frame.crop, &placebo_frame.crop, 0.0f);

    LogCallbackFunction(env, PL_LOG_INFO, "Calling pl_render_image...");
    if (!pl_render_image(placebo_renderer, &placebo_frame, &target_frame, nullptr)) {
        LogCallbackFunction(env, PL_LOG_ERR, "Failed to render image!");
        pl_unmap_avframe(placebo_gl->gpu, &placebo_frame);
        return JNI_FALSE;
    }

    LogCallbackFunction(env, PL_LOG_INFO, "Submitting swapchain frame...");
    if (!pl_swapchain_submit_frame(placebo_swapchain)) {
        LogCallbackFunction(env, PL_LOG_ERR, "Failed to submit swapchain frame!");
        pl_unmap_avframe(placebo_gl->gpu, &placebo_frame);
        return JNI_FALSE;
    }

    LogCallbackFunction(env, PL_LOG_INFO, "Swapping buffers...");
    pl_swapchain_swap_buffers(placebo_swapchain);

    LogCallbackFunction(env, PL_LOG_INFO, "Unmapping AVFrame...");
    pl_unmap_avframe(placebo_gl->gpu, &placebo_frame);

    LogCallbackFunction(env, PL_LOG_INFO, "plRenderAvFrame() completed successfully");
    return JNI_TRUE;
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_grill_placebo_PlaceboManager_plRenderAvFrameDirect(
    JNIEnv *env, jobject obj,
    jlong avframeHandle) {
    AVFrame *frame = reinterpret_cast<AVFrame *>(avframeHandle);
    if (!frame) {
        LogCallbackFunction(env, PL_LOG_ERR, "AVFrame is null!");
        return JNI_FALSE;
    }

    av_mediacodec_release_buffer((AVMediaCodecBuffer *)frame->data[3], 1);
}

extern "C" JNIEXPORT void JNICALL
Java_com_grill_placebo_PlaceboManager_releaseMediaCodecContext(JNIEnv *env, jclass clazz,
                                                                jlong codecCtxHandle) {
    AVCodecContext* codecCtx = reinterpret_cast<AVCodecContext*>(codecCtxHandle);
    if (codecCtx != nullptr && mMediaCodecContext != nullptr) {
        av_mediacodec_default_free(codecCtx);
        mMediaCodecContext = nullptr;
    }
}

extern "C"
JNIEXPORT jlong JNICALL
Java_com_grill_placebo_PlaceboManager_createMediaCodecHwDevice(JNIEnv *env, jclass clazz, jobject javaSurface) {
    if (!javaSurface) {
        LogCallbackFunction(env, PL_LOG_ERR, "Java Surface is null");
        return -1;
    }

    JavaVM* javaVm = nullptr;
    if (env->GetJavaVM(&javaVm) != JNI_OK) {
        LogCallbackFunction(env, PL_LOG_ERR, "Failed to get JavaVM");
        return -1;
    }
    av_jni_set_java_vm(javaVm, nullptr);

    AVBufferRef *device_ref = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_MEDIACODEC);
    if (!device_ref) {
        LogCallbackFunction(env, PL_LOG_ERR, "Failed to allocate device_ref");
        return -1;
    }

    AVHWDeviceContext *ctx = reinterpret_cast<AVHWDeviceContext *>(device_ref->data);
    if (!ctx || !ctx->hwctx) {
        LogCallbackFunction(env, PL_LOG_ERR, "Invalid AVHWDeviceContext or hwctx");
        av_buffer_unref(&device_ref);
        return -1;
    }

    AVMediaCodecDeviceContext *hwctx = static_cast<AVMediaCodecDeviceContext *>(ctx->hwctx);
    hwctx->surface = javaSurface;

    if (av_hwdevice_ctx_init(device_ref) < 0) {
        LogCallbackFunction(env, PL_LOG_ERR, "av_hwdevice_ctx_init failed");
        av_buffer_unref(&device_ref);
        return -1;
    }

    LogCallbackFunction(env, PL_LOG_INFO, "MediaCodec device initialized successfully");
    return reinterpret_cast<jlong>(device_ref);
}

JNIEXPORT jboolean JNICALL
Java_com_grill_placebo_PlaceboManager_assignHwDeviceToCodecContext(
        JNIEnv *env, jclass clazz,
        jlong codecCtxHandle, jlong deviceRefHandle) {

    if (!codecCtxHandle || !deviceRefHandle) {
        LogCallbackFunction(env, PL_LOG_ERR, "Invalid codecCtx or deviceRef handle");
        return JNI_FALSE;
    }

    AVCodecContext *ctx = reinterpret_cast<AVCodecContext *>(codecCtxHandle);
    AVBufferRef *device_ref = reinterpret_cast<AVBufferRef *>(deviceRefHandle);

    ctx->hw_device_ctx = av_buffer_ref(device_ref);
    if (!ctx->hw_device_ctx) {
        LogCallbackFunction(env, PL_LOG_ERR, "Failed to ref and assign hw_device_ctx");
        return JNI_FALSE;
    }

    return JNI_TRUE;
}

JNIEXPORT void JNICALL
Java_com_grill_placebo_PlaceboManager_cleanupResources(JNIEnv *env, jobject thiz) {
    for (int i = 0; i < 4; i++) {
        if (placebo_tex_global[i])
            pl_tex_destroy(placebo_gl->gpu, &placebo_tex_global[i]);
    }

    if (placebo_swapchain) {
        pl_swapchain_destroy(&placebo_swapchain);
        placebo_swapchain = nullptr;
    }
    if (placebo_renderer) {
        pl_renderer_destroy(&placebo_renderer);
        placebo_renderer = nullptr;
    }
    if (placebo_gl) {
        pl_opengl_destroy(&placebo_gl);
        placebo_gl = nullptr;
    }
}
