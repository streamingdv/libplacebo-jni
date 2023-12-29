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

#include <libplacebo/options.h>
#include <libplacebo/vulkan.h>
#include <libplacebo/renderer.h>
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

extern "C"
JNIEXPORT jlong JNICALL Java_com_grill_placebo_PlaceboManager_plGetVkInstance
  (JNIEnv *env, jobject obj, jlong placebo_vk_inst) {
  pl_vk_inst instance = reinterpret_cast<pl_vk_inst>(placebo_vk_inst);
  VkInstance vk_instance = instance->instance;
  return reinterpret_cast<jlong>(vk_instance);
}

extern "C"
JNIEXPORT jlong JNICALL Java_com_grill_placebo_PlaceboManager_plGetWin32SurfaceFunctionPointer
  (JNIEnv *env, jobject obj, jlong placebo_vk_inst) {
  #ifdef _WIN32
       pl_vk_inst instance = reinterpret_cast<pl_vk_inst>(placebo_vk_inst);
       PFN_vkCreateWin32SurfaceKHR createSurface = reinterpret_cast<PFN_vkCreateWin32SurfaceKHR>(
               instance->get_proc_addr(instance->instance, "vkCreateWin32SurfaceKHR"));
       return reinterpret_cast<jlong>(createSurface);
  #endif

  return 0;
}

extern "C"
JNIEXPORT void JNICALL Java_com_grill_placebo_PlaceboManager_plDestroySurface
  (JNIEnv *env, jobject obj, jlong placebo_vk_inst, jlong surface) {
  pl_vk_inst instance = reinterpret_cast<pl_vk_inst>(placebo_vk_inst);
  VkSurfaceKHR vkSurfaceKHR = reinterpret_cast<VkSurfaceKHR>(static_cast<uint64_t>(surface));
  PFN_vkDestroySurfaceKHR destroySurface = reinterpret_cast<PFN_vkDestroySurfaceKHR>(
          instance->get_proc_addr(instance->instance, "vkDestroySurfaceKHR"));
  destroySurface(instance->instance, vkSurfaceKHR, nullptr);
}

extern "C"
JNIEXPORT jlong JNICALL Java_com_grill_placebo_PlaceboManager_plCreateSwapchain
  (JNIEnv *env, jobject obj, jlong placebo_vulkan, jlong surface) {
  pl_vulkan vulkan = reinterpret_cast<pl_vulkan>(placebo_vulkan);
  VkSurfaceKHR vkSurfaceKHR = reinterpret_cast<VkSurfaceKHR>(static_cast<uint64_t>(surface));

  struct pl_vulkan_swapchain_params swapchain_params = {
      .surface = vkSurfaceKHR,
      .present_mode = VK_PRESENT_MODE_FIFO_KHR,
  };

  pl_swapchain placebo_swapchain = pl_vulkan_create_swapchain(vulkan, &swapchain_params);
  return reinterpret_cast<jlong>(placebo_swapchain);
}

extern "C"
JNIEXPORT void JNICALL Java_com_grill_placebo_PlaceboManager_plDestroySwapchain
  (JNIEnv *env, jobject obj, jlong swapchain) {
  pl_swapchain placebo_swapchain = reinterpret_cast<pl_swapchain>(swapchain);
  pl_swapchain_destroy(&placebo_swapchain);
}

extern "C"
JNIEXPORT jlong JNICALL Java_com_grill_placebo_PlaceboManager_plCreateRenderer
  (JNIEnv *env, jobject obj, jlong placebo_vulkan, jlong placebo_log) {
  pl_vulkan vulkan = reinterpret_cast<pl_vulkan>(placebo_vulkan);
  pl_log log = reinterpret_cast<pl_log>(placebo_log);
  pl_renderer placebo_renderer = pl_renderer_create(log, vulkan->gpu);
  return reinterpret_cast<jlong>(placebo_renderer);
}

extern "C"
JNIEXPORT void JNICALL Java_com_grill_placebo_PlaceboManager_plDestroyRenderer
  (JNIEnv *env, jobject obj, jlong renderer) {
  pl_renderer placebo_renderer = reinterpret_cast<pl_renderer>(renderer);
  pl_renderer_destroy(&placebo_renderer);
}

extern "C"
JNIEXPORT void JNICALL Java_com_grill_placebo_PlaceboManager_plSwapchainResize
  (JNIEnv *env, jobject obj, jlong swapchain, jint width, jint height) {
  pl_swapchain placebo_swapchain = reinterpret_cast<pl_swapchain>(swapchain);
  int int_width = static_cast<int>(width);
  int int_height = static_cast<int>(height);
  pl_swapchain_resize(placebo_swapchain, &int_width, &int_height);
}

extern "C"
JNIEXPORT jboolean JNICALL Java_com_grill_placebo_PlaceboManager_plRenderAvFrameTest
  (JNIEnv *env, jobject obj, jlong avframe, jlong placebo_vulkan, jlong swapchain, jlong renderer) {
  AVFrame *frame = reinterpret_cast<AVFrame*>(avframe);
  pl_vulkan vulkan = reinterpret_cast<pl_vulkan>(placebo_vulkan);
  pl_swapchain placebo_swapchain = reinterpret_cast<pl_swapchain>(swapchain);
  pl_renderer placebo_renderer = reinterpret_cast<pl_renderer>(renderer);

  if (frame == nullptr) {
      return false;
  }
  return true;
}



extern "C"
JNIEXPORT jboolean JNICALL Java_com_grill_placebo_PlaceboManager_plRenderAvFrame
  (JNIEnv *env, jobject obj, jlong avframe, jlong placebo_vulkan, jlong swapchain, jlong renderer) {
  AVFrame *frame = reinterpret_cast<AVFrame*>(avframe);
  pl_vulkan vulkan = reinterpret_cast<pl_vulkan>(placebo_vulkan);
  pl_swapchain placebo_swapchain = reinterpret_cast<pl_swapchain>(swapchain);
  pl_renderer placebo_renderer = reinterpret_cast<pl_renderer>(renderer);

  pl_tex placebo_tex[4] = {nullptr, nullptr, nullptr, nullptr};
  struct pl_swapchain_frame sw_frame = {0};
  struct pl_frame placebo_frame = {0};
  struct pl_frame target_frame = {0};

  struct pl_avframe_params avparams = {
      .frame = frame,
      .tex = placebo_tex,
      .map_dovi = false,
  };
  bool mapped = pl_map_avframe_ex(vulkan->gpu, &placebo_frame, &avparams);
  LogCallbackFunction(nullptr, PL_LOG_ERR, "DEBUG LOG 1!");

  if (!mapped) {
      LogCallbackFunction(nullptr, PL_LOG_ERR, "Failed to map AVFrame to Placebo frame!");
      return false;
  }
  av_frame_free(&frame);
  LogCallbackFunction(nullptr, PL_LOG_ERR, "DEBUG LOG 1.1!");
  // set colorspace hint
  struct pl_color_space hint = placebo_frame.color;
  pl_swapchain_colorspace_hint(placebo_swapchain, &hint);

  pl_rect2df crop;
  LogCallbackFunction(nullptr, PL_LOG_ERR, "DEBUG LOG 1.2!");
  bool ret = false;
  pl_render_params render_params = pl_render_fast_params; // pl_render_high_quality_params, pl_render_default_params

  LogCallbackFunction(nullptr, PL_LOG_ERR, "DEBUG LOG 2!");
  if (!pl_swapchain_start_frame(placebo_swapchain, &sw_frame)) {
      LogCallbackFunction(nullptr, PL_LOG_ERR, "Failed to start Placebo frame!");
      goto cleanup;
  }

  LogCallbackFunction(nullptr, PL_LOG_ERR, "DEBUG LOG 3!");
  pl_frame_from_swapchain(&target_frame, &sw_frame);

  crop = placebo_frame.crop;
  /*switch (resolution_mode) {
      case ResolutionMode::Normal:
          pl_rect2df_aspect_copy(&target_frame.crop, &crop, 0.0);
          break;
      case ResolutionMode::Stretch:
          // Nothing to do, target.crop already covers the full image
          break;
      case ResolutionMode::Zoom:
          pl_rect2df_aspect_copy(&target_frame.crop, &crop, 1.0);
          break;
  }*/

  LogCallbackFunction(nullptr, PL_LOG_ERR, "DEBUG LOG 4!");
  if (!pl_render_image(placebo_renderer, &placebo_frame, &target_frame, &render_params)) {
      LogCallbackFunction(nullptr, PL_LOG_ERR, "Failed to render Placebo frame!");
      goto cleanup;
  }
  LogCallbackFunction(nullptr, PL_LOG_ERR, "DEBUG LOG 5!");
  if (!pl_swapchain_submit_frame(placebo_swapchain)) {
      LogCallbackFunction(nullptr, PL_LOG_ERR, "Failed to submit Placebo frame!");
      goto cleanup;
  }
  LogCallbackFunction(nullptr, PL_LOG_ERR, "DEBUG LOG 6!");
  pl_swapchain_swap_buffers(placebo_swapchain);
  ret = true;

cleanup:
  LogCallbackFunction(nullptr, PL_LOG_ERR, "DEBUG LOG 7!");
  pl_unmap_avframe(vulkan->gpu, &placebo_frame);

  return ret;
}

extern "C"
JNIEXPORT jboolean JNICALL Java_com_grill_placebo_PlaceboManager_plRenderAvFrame2
  (JNIEnv *env, jobject obj, jlong avframe, jlong placebo_vulkan, jlong swapchain, jlong renderer) {
  AVFrame *frame = reinterpret_cast<AVFrame*>(avframe);
  pl_vulkan vulkan = reinterpret_cast<pl_vulkan>(placebo_vulkan);
  pl_swapchain placebo_swapchain = reinterpret_cast<pl_swapchain>(swapchain);
  pl_renderer placebo_renderer = reinterpret_cast<pl_renderer>(renderer);

  pl_tex placebo_tex[4] = {nullptr, nullptr, nullptr, nullptr};
  struct pl_swapchain_frame sw_frame = {0};
  struct pl_frame placebo_frame = {0};
  struct pl_frame target_frame = {0};

  struct pl_avframe_params avparams = {
      .frame = frame,
      .tex = placebo_tex,
      .map_dovi = false,
  };
  bool mapped = pl_map_avframe_ex(vulkan->gpu, &placebo_frame, &avparams);
  LogCallbackFunction(nullptr, PL_LOG_ERR, "DEBUG LOG 1!");

  if (!mapped) {
      LogCallbackFunction(nullptr, PL_LOG_ERR, "Failed to map AVFrame to Placebo frame!");
      return false;
  }
  LogCallbackFunction(nullptr, PL_LOG_ERR, "DEBUG LOG 1.1!");
  // set colorspace hint
  struct pl_color_space hint = placebo_frame.color;
  pl_swapchain_colorspace_hint(placebo_swapchain, &hint);

  pl_rect2df crop;
  LogCallbackFunction(nullptr, PL_LOG_ERR, "DEBUG LOG 1.2!");
  bool ret = false;
  pl_render_params render_params = pl_render_fast_params; // pl_render_high_quality_params, pl_render_default_params

  LogCallbackFunction(nullptr, PL_LOG_ERR, "DEBUG LOG 2!");
  if (!pl_swapchain_start_frame(placebo_swapchain, &sw_frame)) {
      LogCallbackFunction(nullptr, PL_LOG_ERR, "Failed to start Placebo frame!");
      goto cleanup;
  }

  LogCallbackFunction(nullptr, PL_LOG_ERR, "DEBUG LOG 3!");
  pl_frame_from_swapchain(&target_frame, &sw_frame);

  crop = placebo_frame.crop;
  /*switch (resolution_mode) {
      case ResolutionMode::Normal:
          pl_rect2df_aspect_copy(&target_frame.crop, &crop, 0.0);
          break;
      case ResolutionMode::Stretch:
          // Nothing to do, target.crop already covers the full image
          break;
      case ResolutionMode::Zoom:
          pl_rect2df_aspect_copy(&target_frame.crop, &crop, 1.0);
          break;
  }*/

  LogCallbackFunction(nullptr, PL_LOG_ERR, "DEBUG LOG 4!");
  if (!pl_render_image(placebo_renderer, &placebo_frame, &target_frame, &render_params)) {
      LogCallbackFunction(nullptr, PL_LOG_ERR, "Failed to render Placebo frame!");
      goto cleanup;
  }
  LogCallbackFunction(nullptr, PL_LOG_ERR, "DEBUG LOG 5!");
  if (!pl_swapchain_submit_frame(placebo_swapchain)) {
      LogCallbackFunction(nullptr, PL_LOG_ERR, "Failed to submit Placebo frame!");
      goto cleanup;
  }
  LogCallbackFunction(nullptr, PL_LOG_ERR, "DEBUG LOG 6!");
  pl_swapchain_swap_buffers(placebo_swapchain);
  ret = true;

cleanup:
  LogCallbackFunction(nullptr, PL_LOG_ERR, "DEBUG LOG 7!");
  pl_unmap_avframe(vulkan->gpu, &placebo_frame);

  return ret;
}