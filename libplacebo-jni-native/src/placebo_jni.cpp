#include "com_grill_placebo_PlaceboManager.h"

#include <cstdlib>
#include <string>
#include <vector>
#include <jni.h>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <xcb/xcb.h>
#endif

#include <libplacebo/vulkan.h>
#include <libplacebo/log.h>
#include <libplacebo/cache.h>

#include <vulkan/vulkan.h>
#ifdef _WIN32
    #include <vulkan/vulkan_win32.h>
#else
    #include <vulkan/vulkan_xcb.h>
    #include <vulkan/vulkan_wayland.h>
#endif

/*** define helper functions ***/

JNIEnv *globalEnv;
jobject globalCallback;
jmethodID onLogMethod;

void LogCallbackFunction(void *log_priv, enum pl_log_level level, const char *msg) {
    if (globalEnv != nullptr && globalCallback != nullptr) {
        jstring message = globalEnv->NewStringUTF(msg);
        globalEnv->CallVoidMethod(globalCallback, onLogMethod, (jint)level, message);
        globalEnv->DeleteLocalRef(message);
    }
}

/*** define JNI methods ***/

extern "C"
JNIEXPORT jstring JNICALL Java_com_grill_placebo_PlaceboManager_getWindowingSystem(JNIEnv* env, jobject obj) {
    const char* sessionType = std::getenv("XDG_SESSION_TYPE");
    if (sessionType != nullptr && sessionType[0] != '\0') {
        return env->NewStringUTF(sessionType);
    } else {
        const char* waylandDisplay = std::getenv("WAYLAND_DISPLAY");
        if (waylandDisplay != nullptr && waylandDisplay[0] != '\0') {
            return env->NewStringUTF("wayland");
        }

        const char* x11Display = std::getenv("DISPLAY");
        if (x11Display != nullptr && x11Display[0] != '\0') {
            return env->NewStringUTF("x11");
        }
    }

    return env->NewStringUTF("unknown");
}

extern "C"
JNIEXPORT jlong JNICALL Java_com_grill_placebo_PlaceboManager_createLog
  (JNIEnv *env, jobject obj, jint apiVersion, jint logLevel, jobject logCallback) {

    globalEnv = env;
    globalCallback = env->NewGlobalRef(logCallback);
    jclass cls = env->GetObjectClass(logCallback);
    onLogMethod = env->GetMethodID(cls, "onLog", "(ILjava/lang/String;)V");

    enum pl_log_level plLevel = static_cast<enum pl_log_level>(logLevel);

    struct pl_log_params log_params = {
        .log_cb = LogCallbackFunction,
        .log_priv = nullptr,
        .log_level = plLevel,
    };

    pl_log placebo_log = pl_log_create(apiVersion, &log_params);

    return reinterpret_cast<jlong>(&placebo_log);
}

extern "C"
JNIEXPORT void JNICALL Java_com_grill_placebo_PlaceboManager_destroyLog
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

JNIEXPORT jlong JNICALL Java_com_grill_placebo_PlaceboManager_plVkInstCreate(JNIEnv *env, jobject obj, jlong placebo_log) {
    #ifdef _WIN32
        const char *vk_exts[] = {
            VK_KHR_SURFACE_EXTENSION_NAME,
            VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
        };
    #else
        // ToDo allow somehow the detection if wayland or not
        const char *vk_exts[] = {
            VK_KHR_SURFACE_EXTENSION_NAME,
            VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME,
        };
    #endif

    const char *opt_extensions[] = {
        VK_EXT_HDR_METADATA_EXTENSION_NAME,
    };

    struct pl_vk_inst_params vk_inst_params = {
        .extensions = vk_exts,
        .num_extensions = 2,
        .opt_extensions = opt_extensions,
        .num_opt_extensions = 1,
    };

    pl_log log = reinterpret_cast<pl_log>(placebo_log);
    pl_vk_inst instance = pl_vk_inst_create(log, &vk_inst_params);

    return reinterpret_cast<jlong>(instance);
}

extern "C"
JNIEXPORT void JNICALL Java_com_grill_placebo_PlaceboManager_plVkInstDestroy
  (JNIEnv *env, jobject obj, jlong placebo_vk_inst) {
     pl_vk_inst instance = reinterpret_cast<pl_vk_inst>(placebo_vk_inst);
     if (instance != nullptr) {
         pl_vk_inst_destroy(&instance);
     }
}

extern "C"
JNIEXPORT jlong JNICALL Java_com_grill_placebo_PlaceboManager_plVulkanCreate
  (JNIEnv *env, jobject obj, jlong placebo_log, jlong placebo_vk_inst) {
     pl_log log = reinterpret_cast<pl_log>(placebo_log);
     pl_vk_inst instance = reinterpret_cast<pl_vk_inst>(placebo_vk_inst);

     struct pl_vulkan_params vulkan_params = {
         .instance = instance->instance,
         .get_proc_addr = instance->get_proc_addr,
         PL_VULKAN_DEFAULTS
     };

     pl_vulkan placebo_vulkan = pl_vulkan_create(log, &vulkan_params);
     return reinterpret_cast<jlong>(placebo_vulkan);
}

extern "C"
JNIEXPORT void JNICALL Java_com_grill_placebo_PlaceboManager_plVulkanDestroy
  (JNIEnv *env, jobject obj, jlong placebo_vulkan) {
     pl_vulkan vulkan = reinterpret_cast<pl_vulkan>(placebo_vulkan);
     if (vulkan != nullptr) {
         pl_vulkan_destroy(&vulkan);
     }
}

// ToDo next ->
/*
     struct pl_cache_params cache_params = {
         .log = placebo_log,
         .max_total_size = 10 << 20, // 10 MB
     };
     placebo_cache = pl_cache_create(&cache_params);
     pl_gpu_set_cache(placebo_vulkan->gpu, placebo_cache);
     FILE *file = fopen(qPrintable(GetShaderCacheFile()), "rb");
     if (file) {
         pl_cache_load_file(placebo_cache, file);
         fclose(file);
     }
*/
