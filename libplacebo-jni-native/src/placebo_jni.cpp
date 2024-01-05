#include "com_grill_placebo_PlaceboManager.h"

#include <cstdlib>
#include <string>
#include <vector>
#include <map>
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

/*** imports related to UI stuff ***/

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_BOOL
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_UINT_DRAW_INDEX
#define NK_IMPLEMENTATION
#include <nuklear.h>

#include <libplacebo/dispatch.h>
#include <libplacebo/shaders/custom.h>
#include <vk_mem_alloc.h>

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

void render_ui(struct ui *ui);
bool ui_draw(struct ui *ui, const struct pl_swapchain_frame *frame);

std::map<ButtonType, VkImageView> imageViewMap;

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
JNIEXPORT jlong JNICALL Java_com_grill_placebo_PlaceboManager_plGetVkDevice
  (JNIEnv *env, jobject obj, jlong placebo_vulkan) {
  pl_vulkan vulkan = reinterpret_cast<pl_vulkan>(placebo_vulkan);
  VkDevice device = vulkan->device;
  return reinterpret_cast<jlong>(device);
}

extern "C"
JNIEXPORT jlong JNICALL Java_com_grill_placebo_PlaceboManager_plGetVkPhysicalDevice
  (JNIEnv *env, jobject obj, jlong placebo_vulkan) {
  pl_vulkan vulkan = reinterpret_cast<pl_vulkan>(placebo_vulkan);
  VkPhysicalDevice phys_device = vulkan->phys_device;
  return reinterpret_cast<jlong>(phys_device);
}

extern "C"
JNIEXPORT void JNICALL Java_com_grill_placebo_PlaceboManager_plSetVmaVulkanFunctions
  (JNIEnv *env, jobject obj, jlong placebo_vk_inst, jlong placebo_vulkan, jlong vulkanFunctions) {
    pl_vk_inst instance = reinterpret_cast<pl_vk_inst>(placebo_vk_inst);
    pl_vulkan vulkan = reinterpret_cast<pl_vulkan>(placebo_vulkan);
    VmaVulkanFunctions* vmaVulkanFunctions = reinterpret_cast<VmaVulkanFunctions*>(vulkanFunctions);

    VkInstance vk_instance = instance->instance;
    VkDevice device = vulkan->device;

    // Assigning function pointers
    vmaVulkanFunctions->vkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(vkGetInstanceProcAddr(vk_instance, "vkGetInstanceProcAddr"));
    vmaVulkanFunctions->vkGetDeviceProcAddr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(vkGetDeviceProcAddr(device, "vkGetDeviceProcAddr"));
    vmaVulkanFunctions->vkGetPhysicalDeviceProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(vkGetInstanceProcAddr(vk_instance, "vkGetPhysicalDeviceProperties"));
    vmaVulkanFunctions->vkGetPhysicalDeviceMemoryProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(vkGetInstanceProcAddr(vk_instance, "vkGetPhysicalDeviceMemoryProperties"));
    vmaVulkanFunctions->vkAllocateMemory = reinterpret_cast<PFN_vkAllocateMemory>(vkGetDeviceProcAddr(device, "vkAllocateMemory"));
    vmaVulkanFunctions->vkFreeMemory = reinterpret_cast<PFN_vkFreeMemory>(vkGetDeviceProcAddr(device, "vkFreeMemory"));
    vmaVulkanFunctions->vkMapMemory = reinterpret_cast<PFN_vkMapMemory>(vkGetDeviceProcAddr(device, "vkMapMemory"));
    vmaVulkanFunctions->vkUnmapMemory = reinterpret_cast<PFN_vkUnmapMemory>(vkGetDeviceProcAddr(device, "vkUnmapMemory"));
    vmaVulkanFunctions->vkFlushMappedMemoryRanges = reinterpret_cast<PFN_vkFlushMappedMemoryRanges>(vkGetDeviceProcAddr(device, "vkFlushMappedMemoryRanges"));
    vmaVulkanFunctions->vkInvalidateMappedMemoryRanges = reinterpret_cast<PFN_vkInvalidateMappedMemoryRanges>(vkGetDeviceProcAddr(device, "vkInvalidateMappedMemoryRanges"));
    vmaVulkanFunctions->vkBindBufferMemory = reinterpret_cast<PFN_vkBindBufferMemory>(vkGetDeviceProcAddr(device, "vkBindBufferMemory"));
    vmaVulkanFunctions->vkBindImageMemory = reinterpret_cast<PFN_vkBindImageMemory>(vkGetDeviceProcAddr(device, "vkBindImageMemory"));
    vmaVulkanFunctions->vkGetBufferMemoryRequirements = reinterpret_cast<PFN_vkGetBufferMemoryRequirements>(vkGetDeviceProcAddr(device, "vkGetBufferMemoryRequirements"));
    vmaVulkanFunctions->vkGetImageMemoryRequirements = reinterpret_cast<PFN_vkGetImageMemoryRequirements>(vkGetDeviceProcAddr(device, "vkGetImageMemoryRequirements"));
    vmaVulkanFunctions->vkCreateBuffer = reinterpret_cast<PFN_vkCreateBuffer>(vkGetDeviceProcAddr(device, "vkCreateBuffer"));
    vmaVulkanFunctions->vkDestroyBuffer = reinterpret_cast<PFN_vkDestroyBuffer>(vkGetDeviceProcAddr(device, "vkDestroyBuffer"));
    vmaVulkanFunctions->vkCreateImage = reinterpret_cast<PFN_vkCreateImage>(vkGetDeviceProcAddr(device, "vkCreateImage"));
    vmaVulkanFunctions->vkDestroyImage = reinterpret_cast<PFN_vkDestroyImage>(vkGetDeviceProcAddr(device, "vkDestroyImage"));
    vmaVulkanFunctions->vkCmdCopyBuffer = reinterpret_cast<PFN_vkCmdCopyBuffer>(vkGetDeviceProcAddr(device, "vkCmdCopyBuffer"));

    // Extension functions with fallback
    auto vkGetBufferMemoryRequirements2KHR = reinterpret_cast<PFN_vkGetBufferMemoryRequirements2KHR>(vkGetDeviceProcAddr(device, "vkGetBufferMemoryRequirements2"));
    vmaVulkanFunctions->vkGetBufferMemoryRequirements2KHR = vkGetBufferMemoryRequirements2KHR ? vkGetBufferMemoryRequirements2KHR : reinterpret_cast<PFN_vkGetBufferMemoryRequirements2KHR>(vkGetDeviceProcAddr(device, "vkGetBufferMemoryRequirements2KHR"));

    auto vkGetImageMemoryRequirements2KHR = reinterpret_cast<PFN_vkGetImageMemoryRequirements2KHR>(vkGetDeviceProcAddr(device, "vkGetImageMemoryRequirements2"));
    vmaVulkanFunctions->vkGetImageMemoryRequirements2KHR = vkGetImageMemoryRequirements2KHR ? vkGetImageMemoryRequirements2KHR : reinterpret_cast<PFN_vkGetImageMemoryRequirements2KHR>(vkGetDeviceProcAddr(device, "vkGetImageMemoryRequirements2KHR"));

    auto vkBindBufferMemory2KHR = reinterpret_cast<PFN_vkBindBufferMemory2KHR>(vkGetDeviceProcAddr(device, "vkBindBufferMemory2"));
    vmaVulkanFunctions->vkBindBufferMemory2KHR = vkBindBufferMemory2KHR ? vkBindBufferMemory2KHR : reinterpret_cast<PFN_vkBindBufferMemory2KHR>(vkGetDeviceProcAddr(device, "vkBindBufferMemory2KHR"));

    auto vkBindImageMemory2KHR = reinterpret_cast<PFN_vkBindImageMemory2KHR>(vkGetDeviceProcAddr(device, "vkBindImageMemory2"));
    vmaVulkanFunctions->vkBindImageMemory2KHR = vkBindImageMemory2KHR ? vkBindImageMemory2KHR : reinterpret_cast<PFN_vkBindImageMemory2KHR>(vkGetDeviceProcAddr(device, "vkBindImageMemory2KHR"));

    auto vkGetPhysicalDeviceMemoryProperties2KHR = reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties2KHR>(vkGetInstanceProcAddr(vk_instance, "vkGetPhysicalDeviceMemoryProperties2"));
    vmaVulkanFunctions->vkGetPhysicalDeviceMemoryProperties2KHR = vkGetPhysicalDeviceMemoryProperties2KHR ? vkGetPhysicalDeviceMemoryProperties2KHR : reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties2KHR>(vkGetInstanceProcAddr(vk_instance, "vkGetPhysicalDeviceMemoryProperties2KHR"));

    auto vkGetDeviceBufferMemoryRequirements = reinterpret_cast<PFN_vkGetDeviceBufferMemoryRequirements>(vkGetDeviceProcAddr(device, "vkGetDeviceBufferMemoryRequirements"));
    vmaVulkanFunctions->vkGetDeviceBufferMemoryRequirements = vkGetDeviceBufferMemoryRequirements ? vkGetDeviceBufferMemoryRequirements : reinterpret_cast<PFN_vkGetDeviceBufferMemoryRequirements>(vkGetDeviceProcAddr(device, "vkGetDeviceBufferMemoryRequirementsKHR"));

    auto vkGetDeviceImageMemoryRequirements = reinterpret_cast<PFN_vkGetDeviceImageMemoryRequirements>(vkGetDeviceProcAddr(device, "vkGetDeviceImageMemoryRequirements"));
    vmaVulkanFunctions->vkGetDeviceImageMemoryRequirements = vkGetDeviceImageMemoryRequirements ? vkGetDeviceImageMemoryRequirements : reinterpret_cast<PFN_vkGetDeviceImageMemoryRequirements>(vkGetDeviceProcAddr(device, "vkGetDeviceImageMemoryRequirementsKHR"));
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
JNIEXPORT jlong JNICALL Java_com_grill_placebo_PlaceboManager_plGetVkCreateImageViewFunctionPointer
  (JNIEnv *env, jobject obj, jlong placebo_vk_inst) {
  pl_vk_inst instance = reinterpret_cast<pl_vk_inst>(placebo_vk_inst);
  PFN_vkCreateImageView vkCreateImageView = reinterpret_cast<PFN_vkCreateImageView>(
          instance->get_proc_addr(instance->instance, "vkCreateImageView"));

  return reinterpret_cast<jlong>(vkCreateImageView);
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
  (JNIEnv *env, jobject obj, jlong placebo_vulkan, jlong surface, jint vkPresentModeKHR ) {
  pl_vulkan vulkan = reinterpret_cast<pl_vulkan>(placebo_vulkan);
  VkSurfaceKHR vkSurfaceKHR = reinterpret_cast<VkSurfaceKHR>(static_cast<uint64_t>(surface));
  VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR; // Default mode

  switch (vkPresentModeKHR) {
      case 0: // VK_PRESENT_MODE_IMMEDIATE_KHR
          present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
          break;
      case 1: // VK_PRESENT_MODE_MAILBOX_KHR
          present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
          break;
      case 2: // VK_PRESENT_MODE_FIFO_KHR
          present_mode = VK_PRESENT_MODE_FIFO_KHR;
          break;
      case 3: // VK_PRESENT_MODE_FIFO_RELAXED_KHR
          present_mode = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
          break;
  }

  struct pl_vulkan_swapchain_params swapchain_params = {
      .surface = vkSurfaceKHR,
      .present_mode = present_mode,
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

pl_render_params render_params = pl_render_fast_params; // default params, others -> pl_render_high_quality_params, pl_render_default_params

extern "C"
JNIEXPORT void JNICALL Java_com_grill_placebo_PlaceboManager_plActivateFastRendering
  (JNIEnv *env, jobject obj) {
  render_params = pl_render_fast_params;
}

extern "C"
JNIEXPORT void JNICALL Java_com_grill_placebo_PlaceboManager_plActivateHighQualityRendering
  (JNIEnv *env, jobject obj) {
  render_params = pl_render_high_quality_params;
}

extern "C"
JNIEXPORT void JNICALL Java_com_grill_placebo_PlaceboManager_plActivateDefaultRendering
  (JNIEnv *env, jobject obj) {
  render_params = pl_render_default_params;
}

int renderingFormat = 0;

extern "C"
JNIEXPORT void JNICALL Java_com_grill_placebo_PlaceboManager_plSetRenderingFormat
  (JNIEnv *env, jobject obj, jint format) {
  if(format >= 0 && format < 3){
    renderingFormat = format;
  }
}

pl_tex placebo_tex_global[4] = {nullptr, nullptr, nullptr, nullptr};

extern "C"
JNIEXPORT jboolean JNICALL Java_com_grill_placebo_PlaceboManager_plRenderAvFrame
  (JNIEnv *env, jobject obj, jlong avframe, jlong placebo_vulkan, jlong swapchain, jlong renderer, jlong ui) {
  AVFrame *frame = reinterpret_cast<AVFrame*>(avframe);
  pl_vulkan vulkan = reinterpret_cast<pl_vulkan>(placebo_vulkan);
  pl_swapchain placebo_swapchain = reinterpret_cast<pl_swapchain>(swapchain);
  pl_renderer placebo_renderer = reinterpret_cast<pl_renderer>(renderer);
  bool ret = false;

  struct pl_swapchain_frame sw_frame = {0};
  struct pl_frame placebo_frame = {0};
  struct pl_frame target_frame = {0};

  struct pl_avframe_params avparams = {
      .frame = frame,
      .tex = placebo_tex_global,
      .map_dovi = false,
  };
  bool mapped = pl_map_avframe_ex(vulkan->gpu, &placebo_frame, &avparams);
  if (!mapped) {
      LogCallbackFunction(nullptr, PL_LOG_ERR, "Failed to map AVFrame to Placebo frame!");
      av_frame_free(&frame);
      return ret;
  }
  // set colorspace hint
  struct pl_color_space hint = placebo_frame.color;
  pl_swapchain_colorspace_hint(placebo_swapchain, &hint);
  pl_rect2df crop;

  if (!pl_swapchain_start_frame(placebo_swapchain, &sw_frame)) {
      LogCallbackFunction(nullptr, PL_LOG_ERR, "Failed to start Placebo frame!");
      goto cleanup;
  }
  pl_frame_from_swapchain(&target_frame, &sw_frame);

  if(ui != 0){
      struct ui *ui_instance = reinterpret_cast<struct ui *>(ui);
      render_ui(ui_instance);
  }

  crop = placebo_frame.crop;
  switch (renderingFormat) {
      case 0: // normal
          pl_rect2df_aspect_copy(&target_frame.crop, &crop, 0.0);
          break;
      case 1: // stretched
          // Nothing to do, target.crop already covers the full image
          break;
      case 2: // zoomed
          pl_rect2df_aspect_copy(&target_frame.crop, &crop, 1.0);
          break;
  }

  if (!pl_render_image(placebo_renderer, &placebo_frame, &target_frame, &render_params)) {
      LogCallbackFunction(nullptr, PL_LOG_ERR, "Failed to render Placebo frame!");
      goto cleanup;
  }
  if (ui != 0) {
     struct ui *ui_instance = reinterpret_cast<struct ui *>(ui);
     if (!ui_draw(ui_instance, &sw_frame)) {
        LogCallbackFunction(nullptr, PL_LOG_ERR, "Could not draw UI!");
     }
  }
  if (!pl_swapchain_submit_frame(placebo_swapchain)) {
      LogCallbackFunction(nullptr, PL_LOG_ERR, "Failed to submit Placebo frame!");
      goto cleanup;
  }

  pl_swapchain_swap_buffers(placebo_swapchain);
  ret = true;

cleanup:
  pl_unmap_avframe(vulkan->gpu, &placebo_frame);

  return ret;
}

extern "C"
JNIEXPORT void JNICALL Java_com_grill_placebo_PlaceboManager_plTextDestroy
  (JNIEnv *env, jobject obj, jlong placebo_vulkan) {
    pl_vulkan vulkan = reinterpret_cast<pl_vulkan>(placebo_vulkan);
    for (int i = 0; i < 4; i++) {
        if (placebo_tex_global[i])
            pl_tex_destroy(vulkan->gpu, &placebo_tex_global[i]);
    }
}

/**** create UI methods ****/

struct ui_vertex {
    float pos[2];
    float coord[2];
    uint8_t color[4];
};

#define NUM_VERTEX_ATTRIBS 3

struct ui {
    pl_gpu gpu;
    pl_dispatch dp;
    struct nk_context nk;
    struct nk_font_atlas atlas;
    struct nk_buffer cmds, verts, idx;
    pl_tex font_tex;
    struct pl_vertex_attrib attribs_pl[NUM_VERTEX_ATTRIBS];
    struct nk_draw_vertex_layout_element attribs_nk[NUM_VERTEX_ATTRIBS+1];
    struct nk_convert_config convert_cfg;
};

void ui_destroy(struct ui *ui)
{
    if (!ui)
        return;

    nk_buffer_free(&ui->cmds);
    nk_buffer_free(&ui->verts);
    nk_buffer_free(&ui->idx);
    nk_free(&ui->nk);
    nk_font_atlas_clear(&ui->atlas);
    pl_tex_destroy(ui->gpu, &ui->font_tex);
    pl_dispatch_destroy(&ui->dp);

    delete ui;
}

struct ui *ui_create(pl_gpu gpu)
{
    struct ui *ui = new struct ui;
    if (!ui)
        return NULL;

    *ui = (struct ui) {
        .gpu = gpu,
        .dp = pl_dispatch_create(gpu->log, gpu),
        .attribs_pl = {
            {
                .name = "pos",
                .fmt = pl_find_vertex_fmt(gpu, PL_FMT_FLOAT, 2),
                .offset = offsetof(struct ui_vertex, pos),
            }, {
                .name = "coord",
                .fmt = pl_find_vertex_fmt(gpu, PL_FMT_FLOAT, 2),
                .offset = offsetof(struct ui_vertex, coord),
            }, {
                .name = "vcolor",
                .fmt = pl_find_named_fmt(gpu, "rgba8"),
                .offset = offsetof(struct ui_vertex, color),
            }
        },
        .attribs_nk = {
            {NK_VERTEX_POSITION, NK_FORMAT_FLOAT, offsetof(struct ui_vertex, pos)},
            {NK_VERTEX_TEXCOORD, NK_FORMAT_FLOAT, offsetof(struct ui_vertex, coord)},
            {NK_VERTEX_COLOR, NK_FORMAT_R8G8B8A8, offsetof(struct ui_vertex, color)},
            {NK_VERTEX_LAYOUT_END}
        },
        .convert_cfg = {
            .global_alpha = 1.0f,
            .line_AA = NK_ANTI_ALIASING_ON,
            .shape_AA = NK_ANTI_ALIASING_ON,
            .circle_segment_count = 22,
            .arc_segment_count = 22,
            .curve_segment_count = 22,
            .vertex_layout = ui->attribs_nk,
            .vertex_size = sizeof(struct ui_vertex),
            .vertex_alignment = NK_ALIGNOF(struct ui_vertex),
        }
,
    };

    // Initialize font atlas using built-in font
    nk_font_atlas_init_default(&ui->atlas);
    nk_font_atlas_begin(&ui->atlas);
    struct nk_font *font = nk_font_atlas_add_default(&ui->atlas, 20, NULL);
    struct pl_tex_params tparams = {
        .format = pl_find_named_fmt(gpu, "r8"),
        .sampleable = true,
        .initial_data = nk_font_atlas_bake(&ui->atlas, &tparams.w, &tparams.h,
                                           NK_FONT_ATLAS_ALPHA8),
    };
    ui->font_tex = pl_tex_create(gpu, &tparams);
    nk_font_atlas_end(&ui->atlas, nk_handle_ptr((void *) ui->font_tex),
                      &ui->convert_cfg.tex_null);
    nk_font_atlas_cleanup(&ui->atlas);

    if (!ui->font_tex)
        goto error;

    // Initialize nuklear state
    if (!nk_init_default(&ui->nk, &font->handle)) {
        LogCallbackFunction(nullptr, PL_LOG_ERR, "NK: failed initializing UI!");
        goto error;
    }

    nk_buffer_init_default(&ui->cmds);
    nk_buffer_init_default(&ui->verts);
    nk_buffer_init_default(&ui->idx);

    return ui;

error:
    ui_destroy(ui);
    return NULL;
}

bool ui_draw(struct ui *ui, const struct pl_swapchain_frame *frame)
{
    if (nk_convert(&ui->nk, &ui->cmds, &ui->verts, &ui->idx, &ui->convert_cfg) != NK_CONVERT_SUCCESS) {
        return false;
    }

    const struct nk_draw_command *cmd = NULL;
    const uint8_t* vertices = reinterpret_cast<const uint8_t*>(nk_buffer_memory(&ui->verts));
    const nk_draw_index* indices = reinterpret_cast<const nk_draw_index*>(nk_buffer_memory(&ui->idx));
    nk_draw_foreach(cmd, &ui->nk, &ui->cmds) {
        if (!cmd->elem_count)
            continue;

        pl_shader sh = pl_dispatch_begin(ui->dp);
        struct pl_shader_desc shader_desc = {
            .desc = {
                .name = "ui_tex",
                .type = PL_DESC_SAMPLED_TEX,
            },
            .binding = {
                .object = cmd->texture.ptr,
                .sample_mode = PL_TEX_SAMPLE_NEAREST,
            },
        };
        struct pl_custom_shader custom_shader = {
            .description = "nuklear UI",
            .body = "color = textureLod(ui_tex, coord, 0.0).r * vcolor;",
            .output = PL_SHADER_SIG_COLOR,
            .descriptors = &shader_desc,
            .num_descriptors = 1,
        };
        pl_shader_custom(sh, &custom_shader);


        struct pl_color_repr repr = frame->color_repr;
        struct pl_color_map_args cmap_args = {
            .src = pl_color_space_srgb,
            .dst = frame->color_space
        };
        pl_shader_color_map_ex(sh, NULL, &cmap_args);
        pl_shader_encode_color(sh, &repr);

        struct pl_dispatch_vertex_params vertex_params = {
            .shader = &sh,
            .target = frame->fbo,
            .scissors = {
                .x0 = static_cast<int>(cmd->clip_rect.x),
                .y0 = static_cast<int>(cmd->clip_rect.y),
                .x1 = static_cast<int>(cmd->clip_rect.x + cmd->clip_rect.w),
                .y1 = static_cast<int>(cmd->clip_rect.y + cmd->clip_rect.h),
            },
            .blend_params = &pl_alpha_overlay,
            .vertex_attribs = ui->attribs_pl,
            .num_vertex_attribs = NUM_VERTEX_ATTRIBS,
            .vertex_stride = sizeof(struct ui_vertex),
            .vertex_position_idx = 0,
            .vertex_coords = PL_COORDS_ABSOLUTE,
            .vertex_flipped = frame->flipped,
            .vertex_type = PL_PRIM_TRIANGLE_LIST,
            .vertex_count = static_cast<int>(cmd->elem_count),
            .vertex_data = vertices,
            .index_data = indices,
            .index_fmt = PL_INDEX_UINT32,
        };
        bool ok = pl_dispatch_vertex(ui->dp, &vertex_params);
        if (!ok) {
            return false;
        }

        indices += cmd->elem_count;
    }

    nk_clear(&ui->nk);
    nk_buffer_clear(&ui->cmds);
    nk_buffer_clear(&ui->verts);
    nk_buffer_clear(&ui->idx);
    return true;
}

struct nk_context *ui_get_context(struct ui *ui)
{
    return &ui->nk;
}

void render_ui(struct ui *ui) {
    if (!ui)
        return;

    struct nk_context *ctx = ui_get_context(ui);
    //const struct nk_rect bounds = nk_rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    const struct nk_rect bounds = nk_rect(0, 0, 1920, 1080); // ToDo get real window size
    // hide background
    nk_style_push_style_item(ctx, &ctx->style.window.fixed_background, nk_style_item_hide());
    if (nk_begin(ctx, "FULLSCREEN", bounds, NK_WINDOW_NO_SCROLLBAR))
    {
       nk_layout_space_begin(ctx, NK_DYNAMIC, bounds.w, bounds.h );
       nk_layout_space_push(ctx, nk_rect(100, 0, 100, 30));
       // draw in screen coordinates
       if (nk_button_label(ctx, "PS")) {
           // event handling
       }

       nk_layout_space_end(ctx);
    }
    nk_end(ctx);
    // restore background
    nk_style_pop_style_item(ctx);
}

extern "C"
JNIEXPORT jlong JNICALL Java_com_grill_placebo_PlaceboManager_nkCreateUI
  (JNIEnv *env, jobject obj, jlong placebo_vulkan) {
  pl_vulkan vulkan = reinterpret_cast<pl_vulkan>(placebo_vulkan);
  struct ui *ui_instance = ui_create(vulkan->gpu);
  if (ui_instance == NULL) {
      return 0L;
  }
  return reinterpret_cast<jlong>(ui_instance);
}

enum class ButtonType {
  MIC,
  OPTIONS,
  PS,
  SHARE,
  FULLSCREEN,
  CLOSE
};

extern "C"
JNIEXPORT void JNICALL Java_com_grill_placebo_PlaceboManager_nkStoreImageView
  (JNIEnv *env, jobject obj, jlong imageView, jint btnType, jlong placebo_vulkan) {
  VkImageView vkImageView = reinterpret_cast<VkImageView>(imageView);
  ButtonType buttonType = static_cast<ButtonType>(btnType);
  pl_vulkan vulkan = reinterpret_cast<pl_vulkan>(placebo_vulkan);

  auto it = imageViewMap.find(buttonType);
  if (it != imageViewMap.end()) {
      // If an entry exists, destroy the old VkImageView
      vkDestroyImageView(vulkan->device, it->second, nullptr);
  }
  imageViewMap[buttonType] = vkImageView;
}

extern "C"
JNIEXPORT void JNICALL Java_com_grill_placebo_PlaceboManager_nkDestroyStoredImageViews
  (JNIEnv *env, jobject obj, jlong placebo_vulkan) {
  pl_vulkan vulkan = reinterpret_cast<pl_vulkan>(placebo_vulkan);
  for (const auto& pair : imageViewMap) {
      if (pair.second != VK_NULL_HANDLE) {
          vkDestroyImageView(vulkan->device, pair.second, nullptr);
      }
  }
  imageViewMap.clear();
}

extern "C"
JNIEXPORT void JNICALL Java_com_grill_placebo_PlaceboManager_nkDestroyUI
  (JNIEnv *env, jobject obj, jlong ui) {
  struct ui *ui_instance = reinterpret_cast<struct ui *>(ui);
  ui_destroy(ui_instance);
}



