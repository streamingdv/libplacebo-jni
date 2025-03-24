#include "com_grill_libplacebo_PlaceboManager.h"
#include <jni.h>
#include <iostream>
#include <GLES3/gl3.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2ext.h>
#include <android/native_window_jni.h>
#include <libplacebo/gpu.h>
#include <libplacebo/options.h>
#include <libplacebo/renderer.h>
#include <libplacebo/opengl.h>
#include <libplacebo/log.h>
#include <libplacebo/cache.h>

#define PL_LIBAV_IMPLEMENTATION 0
#include <libplacebo/utils/libav.h>

static pl_opengl *placebo_gl = NULL;
static pl_renderer *placebo_renderer = NULL;
static pl_swapchain *placebo_swapchain = NULL;

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

JNIEXPORT jlong JNICALL
Java_com_grill_placebo_PlaceboManager_createEGLImageFromSurface(
    JNIEnv *env, jobject thiz, jlong eglDisplay, jlong eglContext, jobject surface) {

    EGLDisplay display = (EGLDisplay) eglDisplay;
    EGLContext context = (EGLContext) eglContext;

    ANativeWindow *nativeWindow = ANativeWindow_fromSurface(env, surface);
    if (!nativeWindow) {
        return 0; // Failed to retrieve ANativeWindow
    }

    EGLClientBuffer clientBuffer = eglGetNativeClientBufferANDROID(nativeWindow);
    if (!clientBuffer) {
        LogCallbackFunction(nullptr, PL_LOG_ERR, "Failed to create native client buffer.");
        ANativeWindow_release(nativeWindow);
        return 0; // Failed to get EGLClientBuffer
    }

    EGLint eglImageAttribs[] = {
        EGL_IMAGE_PRESERVED_KHR, EGL_TRUE,
        EGL_NONE
    };

    EGLImageKHR eglImage = eglCreateImageKHR(
        display,
        context,
        EGL_NATIVE_BUFFER_ANDROID,
        clientBuffer,
        eglImageAttribs
    );

    if (eglImage == EGL_NO_IMAGE_KHR) {
        LogCallbackFunction(nullptr, PL_LOG_ERR, "Failed to create EGLImage.");
        ANativeWindow_release(nativeWindow);
        return 0; // Failed
    }

    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, eglImage);

    if (!texture) {
        LogCallbackFunction(nullptr, PL_LOG_ERR, "Failed to create Texture2DOES.");
        eglDestroyImageKHR(display, eglImage); // Cleanup EGLImage if texture creation fails
        ANativeWindow_release(nativeWindow);
        return 0;
    }

    ANativeWindow_release(nativeWindow);
    return (jlong)(intptr_t)texture;
}

// OpenGL ES + Swapchain Initialization
JNIEXPORT jlong JNICALL
Java_com_grill_placebo_PlaceboManager_plOpenGLCreate(
    JNIEnv *env, jobject thiz, jlong logHandle, jlong eglDisplay, jlong eglContext,
    jint width, jint height, jint colorTransfer) {

    struct pl_opengl_params params = {
        .get_proc_address = (pl_get_proc_address)eglGetProcAddress,
        .egl_display = (EGLDisplay)eglDisplay,
        .egl_context = (EGLContext)eglContext
    };

    pl_log *log = (pl_log *)logHandle;

    placebo_gl = pl_opengl_create(log, &params);
    if (!placebo_gl) {
        return 0; // Failed
    }

    placebo_renderer = pl_renderer_create(log, placebo_gl->gpu);
    if (!placebo_renderer) {
        pl_opengl_destroy(&placebo_gl);
        placebo_gl = nullptr;
        return 0; // Failed
    }

    // Create the swapchain for optimal presentation
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
        return 0; // Failed
    }


    return (jlong)placebo_renderer;
}

extern "C"
JNIEXPORT jboolean JNICALL Java_com_grill_placebo_PlaceboManager_plSwapchainResize
  (JNIEnv *env, jobject obj, jlong swapchainHandle, jint width, jint height) {
    pl_swapchain swapchain = reinterpret_cast<pl_swapchain>(swapchainHandle);
    int w = static_cast<int>(width);
    int h = static_cast<int>(height);
    bool success = pl_swapchain_resize(swapchain, &w, &h);
    return static_cast<jboolean>(success);
}


// Rendering EGLImage via libplacebo
JNIEXPORT jboolean JNICALL
Java_com_grill_placebo_PlaceboManager_renderEGLImage(
    JNIEnv *env, jobject thiz, jlong rendererHandle, jlong eglImageHandle,
    jint width, jint height, jint colorTransfer, jint colorRange) {

    if (!placebo_renderer) {
        LogCallbackFunction(nullptr, PL_LOG_ERR, "Renderer is not initialized!");
        return JNI_FALSE;
    }

    pl_renderer *renderer = (pl_renderer *)rendererHandle;
    GLuint texture = (GLuint)(uintptr_t)eglImageHandle;

    struct pl_opengl_wrap_params wrap_params = {
            .texture = texture,
            .target = GL_TEXTURE_EXTERNAL_OES,   // Correct target for EGLImage textures
            .iformat = (colorTransfer == 6) ? GL_RGBA16F : GL_RGB8,                  // Not sure...
            .width = width,
            .height = height,
            .depth = 1
        };

    pl_tex *egl_tex = pl_opengl_wrap(placebo_gl->gpu, &wrap_params);
    if (!egl_tex) {
        LogCallbackFunction(nullptr, PL_LOG_ERR, "Failed to wrap EGLImage as pl_tex!");
        return JNI_FALSE;
    }

    struct pl_frame placebo_frame = {0};
    struct pl_frame target_frame = {0};

    placebo_frame.num_planes = 1;
    placebo_frame.planes[0] = (struct pl_plane) {
        .texture = egl_tex,
        .dimensions.w = width,
        .dimensions.h = height,
        .components = 3,
        .component_mapping = {0, 1, 2, -1},  // YUV mapping for EGLImage
    };
    //placebo_frame.color = pl_color_space_unknown;
    // Manage color space (SDR/HDR)
    placebo_frame.color = (struct pl_color_space) {
        .primaries = (colorTransfer == 6) ? PL_COLOR_PRIM_BT2020 : PL_COLOR_PRIM_BT709,
        .transfer = (colorTransfer == 6) ? PL_COLOR_TRC_PQ : PL_COLOR_TRC_GAMMA22,
        .hdr = (colorTransfer == 6) ? PL_HDR_METADATA_PQ : PL_HDR_METADATA_NONE
    };
    //placebo_frame.repr = pl_color_repr_unknown;
    placebo_frame.repr = (struct pl_color_repr) {
        .sys = (colorTransfer == 6) ? PL_COLOR_SYSTEM_BT2020 : PL_COLOR_SYSTEM_BT709,
        .levels = (colorRange == 2) ? PL_COLOR_LEVELS_LIMITED : PL_COLOR_LEVELS_FULL,
        .alpha = PL_ALPHA_INDEPENDENT
    };
    placebo_frame.crop = (pl_rect2df){ 0, 0, width, height };

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
        pl_tex_destroy(placebo_gl->gpu, &egl_tex);
        return JNI_FALSE;
    }
    pl_frame_from_swapchain(&target_frame, &sw_frame);

    if (!pl_render_image(renderer, &placebo_frame, &target_frame, NULL)) {
        LogCallbackFunction(nullptr, PL_LOG_ERR, "Failed to render Placebo frame!");
        pl_tex_destroy(placebo_gl->gpu, &egl_tex);
        return JNI_FALSE;
    }

    if (!pl_swapchain_submit_frame(placebo_swapchain)) {
        LogCallbackFunction(nullptr, PL_LOG_ERR, "Failed to submit swapchain frame!");
        pl_tex_destroy(placebo_gl->gpu, &egl_tex);
        return JNI_FALSE;
    }

    pl_swapchain_swap_buffers(placebo_swapchain);
    pl_tex_destroy(placebo_gl->gpu, &egl_tex);

    return JNI_TRUE;
}

JNIEXPORT void JNICALL
Java_com_grill_placebo_PlaceboManager_cleanupResources(JNIEnv *env, jobject thiz) {
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
