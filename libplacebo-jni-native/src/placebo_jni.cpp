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
#include <libplacebo/gpu.h>

#define PL_LIBAV_IMPLEMENTATION 0
#include <libplacebo/utils/libav.h>

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
JNIEXPORT jlong JNICALL Java_com_grill_placebo_PlaceboManager_plLogCreate
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


extern "C"
JNIEXPORT jlong JNICALL Java_com_grill_placebo_PlaceboManager_plCacheCreate
  (JNIEnv *env, jobject obj, jlong placebo_log, jint max_size) {
  pl_log log = reinterpret_cast<pl_log>(placebo_log);
  size_t max_cache_size;
  if (max_size >= 0) {
      max_cache_size = static_cast<size_t>(max_size);
  } else {
      max_cache_size = 10 << 20; // default use 10 MB
  }
  struct pl_cache_params cache_params = {
      .log = log,
      .max_total_size = max_cache_size,
  };

  pl_cache placebo_cache = pl_cache_create(&cache_params);
  return reinterpret_cast<jlong>(placebo_cache);
}

extern "C"
JNIEXPORT void JNICALL Java_com_grill_placebo_PlaceboManager_plCacheDestroy
  (JNIEnv *env, jobject obj, jlong placebo_cache) {
     pl_cache cache = reinterpret_cast<pl_cache>(placebo_cache);
     if (cache != nullptr) {
         pl_cache_destroy(&cache);
     }
}

extern "C"
JNIEXPORT void JNICALL Java_com_grill_placebo_PlaceboManager_plGpuSetCache
  (JNIEnv *env, jobject obj, jlong placebo_vulkan, jlong placebo_cache) {
       pl_vulkan vulkan = reinterpret_cast<pl_vulkan>(placebo_vulkan);
       pl_cache cache = reinterpret_cast<pl_cache>(placebo_cache);
       pl_gpu_set_cache(vulkan->gpu, cache);
}

extern "C"
JNIEXPORT void JNICALL Java_com_grill_placebo_PlaceboManager_plCacheLoadFile
  (JNIEnv *env, jobject obj, jlong placebo_cache, jstring cache_filePath) {
  const char *path = env->GetStringUTFChars(cache_filePath, nullptr);
  if (path == nullptr) {
      return;
  }
  pl_cache cache = reinterpret_cast<pl_cache>(placebo_cache);
  FILE *file = fopen(path, "rb");
  if (file) {
      pl_cache_load_file(cache, file);
      fclose(file);
  }

  env->ReleaseStringUTFChars(cache_filePath, path);
}

extern "C"
JNIEXPORT void JNICALL Java_com_grill_placebo_PlaceboManager_plCacheSaveFile
  (JNIEnv *env, jobject obj, jlong placebo_cache, jstring cache_filePath) {
  const char *path = env->GetStringUTFChars(cache_filePath, nullptr);
  if (path == nullptr) {
      return;
  }
  pl_cache cache = reinterpret_cast<pl_cache>(placebo_cache);
  FILE *file = fopen(path, "wb");
  if (file) {
      pl_cache_save_file(cache, file);
      fclose(file);
  }

  env->ReleaseStringUTFChars(cache_filePath, path);
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
