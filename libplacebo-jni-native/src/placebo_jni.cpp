#include "com_grill_placebo_PlaceboManager.h"

#include <cstdlib>
#include <string>
#include <vector>
#include <map>
#include <array>
#include <iterator>
#include <iostream>
#include <algorithm>
#include <set>
#include <cstring>
#include <string.h>
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
#elif defined(__APPLE__)
    #include <vulkan/vulkan_metal.h>
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
#include <roboto_font.h>
#include <roboto_bold_font.h>
#include <gui_font.h>
#include <ui_consts.h>
#include <ui_state.h>

#include <libavutil/buffer.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_vulkan.h>

/*** global color space variable ***/

pl_color_space m_LastColorspace = {};

/*** define helper functions ***/

JNIEnv *globalEnv;
jobject globalCallback;
jmethodID onLogMethod;

void LogCallbackFunction(void *log_priv, enum pl_log_level level, const char *msg) {
  if (globalEnv != nullptr && globalCallback != nullptr) {
      jstring message = globalEnv->NewStringUTF(msg);
      globalEnv->CallVoidMethod(globalCallback, onLogMethod, (jint)level, message);
      globalEnv->DeleteLocalRef(message);
  } else {
      std::cout << "Log Level " << level << ": " << msg << std::endl;
  }
}

struct nk_image globalBtnImage;

void render_ui(struct ui *ui, int width, int height);
bool ui_draw(struct ui *ui, const struct pl_swapchain_frame *frame);

bool m_HasPendingSwapchainFrame = false;
int vk_decode_queue_index = -1;

struct {
#ifdef _WIN32
    PFN_vkCreateWin32SurfaceKHR vkCreateWin32SurfaceKHR;
#elif defined(__APPLE__)
    PFN_vkCreateMetalSurfaceEXT vkCreateMetalSurfaceEXT;
#else
    PFN_vkCreateXcbSurfaceKHR vkCreateXcbSurfaceKHR;
    PFN_vkCreateWaylandSurfaceKHR vkCreateWaylandSurfaceKHR;
#endif
    PFN_vkDestroySurfaceKHR vkDestroySurfaceKHR;
    PFN_vkGetPhysicalDeviceQueueFamilyProperties vkGetPhysicalDeviceQueueFamilyProperties;
    /** device check methods **/
    PFN_vkGetPhysicalDeviceSurfacePresentModesKHR vkGetPhysicalDeviceSurfacePresentModesKHR;
    PFN_vkGetPhysicalDeviceSurfaceFormatsKHR vkGetPhysicalDeviceSurfaceFormatsKHR;
    PFN_vkEnumeratePhysicalDevices vkEnumeratePhysicalDevices;
    PFN_vkGetPhysicalDeviceProperties vkGetPhysicalDeviceProperties;
    PFN_vkGetPhysicalDeviceSurfaceSupportKHR vkGetPhysicalDeviceSurfaceSupportKHR;
    PFN_vkEnumerateDeviceExtensionProperties vkEnumerateDeviceExtensionProperties;

} vk_funcs;

   // Keep these in sync with hwcontext_vulkan.c
static const char *opt_dev_extensions[] = {
    /* Misc or required by other extensions */
    VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,
    VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME,
    VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME,
    VK_EXT_PHYSICAL_DEVICE_DRM_EXTENSION_NAME,
    VK_EXT_SHADER_ATOMIC_FLOAT_EXTENSION_NAME,
    VK_KHR_COOPERATIVE_MATRIX_EXTENSION_NAME,

    /* Imports/exports */
    VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
    VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME,
    VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME,
    VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME,
    VK_EXT_EXTERNAL_MEMORY_HOST_EXTENSION_NAME,

#ifdef _WIN32
    VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME,
    VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME,
#endif

    VK_KHR_VIDEO_QUEUE_EXTENSION_NAME,
    VK_KHR_VIDEO_DECODE_QUEUE_EXTENSION_NAME,
    VK_KHR_VIDEO_DECODE_H264_EXTENSION_NAME,
    VK_KHR_VIDEO_DECODE_H265_EXTENSION_NAME,
};

// minimum required extensions
static const char *opt_dev_extensions_min[] = {
    VK_KHR_VIDEO_QUEUE_EXTENSION_NAME,
    VK_KHR_VIDEO_DECODE_QUEUE_EXTENSION_NAME,
    VK_KHR_VIDEO_DECODE_H264_EXTENSION_NAME,
    VK_KHR_VIDEO_DECODE_H265_EXTENSION_NAME,
};

bool isExtensionSupportedByPhysicalDevice(VkPhysicalDevice device, const char *extensionName)
{
    uint32_t extensionCount = 0;
    vk_funcs.vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> extensions(extensionCount);
    vk_funcs.vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, extensions.data());

    for (const VkExtensionProperties& extension : extensions) {
        if (strcmp(extension.extensionName, extensionName) == 0) {
            return true;
        }
    }

    return false;
}

bool isSurfacePresentationSupportedByPhysicalDevice(VkPhysicalDevice device, VkSurfaceKHR vkSurfaceKHR)
{
    uint32_t queueFamilyCount = 0;
    vk_funcs.vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        VkBool32 supported = VK_FALSE;
        if (vk_funcs.vkGetPhysicalDeviceSurfaceSupportKHR(device, i, vkSurfaceKHR, &supported) == VK_SUCCESS && supported == VK_TRUE) {
            return true;
        }
    }

    return false;
}

bool isColorSpaceSupportedByPhysicalDevice(VkPhysicalDevice device, VkColorSpaceKHR colorSpace, VkSurfaceKHR vkSurfaceKHR)
{
    uint32_t formatCount = 0;
    vk_funcs.vkGetPhysicalDeviceSurfaceFormatsKHR(device, vkSurfaceKHR, &formatCount, nullptr);

    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vk_funcs.vkGetPhysicalDeviceSurfaceFormatsKHR(device, vkSurfaceKHR, &formatCount, formats.data());

    for (uint32_t i = 0; i < formatCount; i++) {
        if (formats[i].colorSpace == colorSpace) {
            return true;
        }
    }

    return false;
}

bool isPresentModeSupportedByPhysicalDevice(VkPhysicalDevice device, VkPresentModeKHR presentMode, VkSurfaceKHR vkSurfaceKHR)
{
    uint32_t presentModeCount = 0;
    vk_funcs.vkGetPhysicalDeviceSurfacePresentModesKHR(device, vkSurfaceKHR, &presentModeCount, nullptr);

    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    vk_funcs.vkGetPhysicalDeviceSurfacePresentModesKHR(device, vkSurfaceKHR, &presentModeCount, presentModes.data());

    for (uint32_t i = 0; i < presentModeCount; i++) {
        if (presentModes[i] == presentMode) {
            return true;
        }
    }

    return false;
}

bool tryInitializeDevice(pl_log log,
                       pl_vk_inst instance,
                       VkPhysicalDevice device,
                       VkPhysicalDeviceProperties* deviceProps,
                       VkSurfaceKHR vkSurfaceKHR,
                       int decoderType,
                       bool hdr,
                       pl_vulkan& placebo_vulkan) {
  // Check the Vulkan API version first to ensure it meets libplacebo's minimum
  if (deviceProps->apiVersion < PL_VK_MIN_VERSION) {
      LogCallbackFunction(nullptr, PL_LOG_ERR, "Vulkan device does not meet minimum Vulkan version!");
      return false;
  }

  const char* videoDecodeExtension = nullptr; // Initialize to nullptr to avoid uninitialized usage

  if (decoderType == 0) { // h264
      videoDecodeExtension = VK_KHR_VIDEO_DECODE_H264_EXTENSION_NAME;
  }
  else if (decoderType == 1) { // h265
      videoDecodeExtension = VK_KHR_VIDEO_DECODE_H265_EXTENSION_NAME;
  } else {
      LogCallbackFunction(nullptr, PL_LOG_ERR, "Unsupported video decoder type format!");
      return false;
  }

  if (!isExtensionSupportedByPhysicalDevice(device, videoDecodeExtension)) {
      LogCallbackFunction(nullptr, PL_LOG_ERR, "Vulkan device does not support videoDecodeExtension!");
      return false;
  }

  if (!isSurfacePresentationSupportedByPhysicalDevice(device, vkSurfaceKHR)) {
      LogCallbackFunction(nullptr, PL_LOG_ERR, "Vulkan device does not support presenting on window surface!");
      return false;
  }

  if(hdr && !isColorSpaceSupportedByPhysicalDevice(device, VK_COLOR_SPACE_HDR10_ST2084_EXT, vkSurfaceKHR)){
      LogCallbackFunction(nullptr, PL_LOG_ERR, "Vulkan device does not support HDR10 (ST.2084 PQ)!");
      return false;
  }

  // Avoid software GPUs
  if (deviceProps->deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU) {
      LogCallbackFunction(nullptr, PL_LOG_ERR, "Vulkan device is a (probably slow) software renderer.");
      return false;
  }

  struct pl_vulkan_params vulkan_params = {
      .instance = instance->instance,
      .get_proc_addr = instance->get_proc_addr,
      .surface = vkSurfaceKHR,
      .device = device,
      .extra_queues = VK_QUEUE_VIDEO_DECODE_BIT_KHR,
      .opt_extensions = opt_dev_extensions,
      .num_opt_extensions = std::size(opt_dev_extensions),
  };

  placebo_vulkan = pl_vulkan_create(log, &vulkan_params);
  if (placebo_vulkan == nullptr) {
     LogCallbackFunction(nullptr, PL_LOG_ERR, "Vulkan device could not be created.");
     return false;
  }

  return true;
}

// Helper function to safely allocate and copy strings
char* copyString(JNIEnv *env, jstring jstr) {
    if (jstr == nullptr) {
        char* emptyStr = new char[1];
        emptyStr[0] = '\0'; // Allocate and return an empty string
        return emptyStr;
    }

    const char* tempStr = env->GetStringUTFChars(jstr, nullptr);
    size_t len = std::strlen(tempStr);
    char* newStr = new char[len + 1]; // Allocate new memory
    std::strcpy(newStr, tempStr); // Copy the string
    env->ReleaseStringUTFChars(jstr, tempStr); // Release JNI string
    return newStr;
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
      .log_level = plLevel,
  };

  pl_log placebo_log = pl_log_create(apiVersion, &log_params);

  return reinterpret_cast<jlong>(placebo_log);
}

extern "C"
JNIEXPORT jlong JNICALL Java_com_grill_placebo_PlaceboManager_plLogCreate2
  (JNIEnv *env, jobject obj, jint apiVersion, jint logLevel) {
  enum pl_log_level plLevel = static_cast<enum pl_log_level>(logLevel);

  struct pl_log_params log_params = {
      .log_cb = LogCallbackFunction,
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

JNIEXPORT jlong JNICALL Java_com_grill_placebo_PlaceboManager_plVkInstCreate(JNIEnv *env, jobject obj, jlong placebo_log, jint windowingSystemType) {
  const char *vk_exts[2];  // Array to hold Vulkan extensions
  vk_exts[0] = VK_KHR_SURFACE_EXTENSION_NAME;

  #ifdef _WIN32
      vk_exts[1] = VK_KHR_WIN32_SURFACE_EXTENSION_NAME;
  #elif defined(__APPLE__)
      vk_exts[1] = VK_EXT_METAL_SURFACE_EXTENSION_NAME;
  #else
      if (windowingSystemType == 2) {
          // Wayland surface extension
          vk_exts[1] = VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME;
      } else {
          // XCB surface extension (default for other values)
          vk_exts[1] = VK_KHR_XCB_SURFACE_EXTENSION_NAME;
      }
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

  // reset stored color space
  m_LastColorspace = {};
}

extern "C"
JNIEXPORT jlong JNICALL Java_com_grill_placebo_PlaceboManager_plVulkanCreate
  (JNIEnv *env, jobject obj, jlong placebo_log, jlong placebo_vk_inst, jlong surface) {
  pl_log log = reinterpret_cast<pl_log>(placebo_log);
  pl_vk_inst instance = reinterpret_cast<pl_vk_inst>(placebo_vk_inst);
  VkSurfaceKHR vkSurfaceKHR = reinterpret_cast<VkSurfaceKHR>(static_cast<uint64_t>(surface));

  struct pl_vulkan_params vulkan_params = {
      .instance = instance->instance,
      .get_proc_addr = instance->get_proc_addr,
      .surface = vkSurfaceKHR,
      .allow_software = true,
      PL_VULKAN_DEFAULTS
      .extra_queues = VK_QUEUE_VIDEO_DECODE_BIT_KHR,
      .opt_extensions = opt_dev_extensions_min,
      .num_opt_extensions = std::size(opt_dev_extensions_min),
  };

  pl_vulkan placebo_vulkan = pl_vulkan_create(log, &vulkan_params);
  return reinterpret_cast<jlong>(placebo_vulkan);
}

extern "C"
JNIEXPORT jlong JNICALL Java_com_grill_placebo_PlaceboManager_plVulkanCreateForBestDevice
  (JNIEnv *env, jobject obj, jlong placebo_log, jlong placebo_vk_inst, jlong surface, jint decoderType, jboolean hdr) {
  pl_log log = reinterpret_cast<pl_log>(placebo_log);
  pl_vk_inst instance = reinterpret_cast<pl_vk_inst>(placebo_vk_inst);
  VkSurfaceKHR vkSurfaceKHR = reinterpret_cast<VkSurfaceKHR>(static_cast<uint64_t>(surface));

  uint32_t physicalDeviceCount = 0;
  vk_funcs.vkEnumeratePhysicalDevices(instance->instance, &physicalDeviceCount, nullptr);
  std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
  vk_funcs.vkEnumeratePhysicalDevices(instance->instance, &physicalDeviceCount, physicalDevices.data());

  std::set<uint32_t> devicesTried;
  VkPhysicalDeviceProperties deviceProps;

  vk_funcs.vkGetPhysicalDeviceProperties(physicalDevices[0], &deviceProps);

  pl_vulkan placebo_vulkan = nullptr;
  if (tryInitializeDevice(log, instance, physicalDevices[0], &deviceProps, vkSurfaceKHR, decoderType, hdr, placebo_vulkan)) {
      return reinterpret_cast<jlong>(placebo_vulkan);
  }
  devicesTried.emplace(0);

  // Next, we'll try to match an integrated GPU, since we want to minimize
  // power consumption and inter-GPU copies.
  for (uint32_t i = 0; i < physicalDeviceCount; i++) {
    // Skip devices we've already tried
    if (devicesTried.find(i) != devicesTried.end()) {
        continue;
    }

    VkPhysicalDeviceProperties deviceProps;
    vk_funcs.vkGetPhysicalDeviceProperties(physicalDevices[i], &deviceProps);
    if (deviceProps.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
        if (tryInitializeDevice(log, instance, physicalDevices[i], &deviceProps, vkSurfaceKHR, decoderType, hdr, placebo_vulkan)) {
            return reinterpret_cast<jlong>(placebo_vulkan);
        }
        devicesTried.emplace(i);
    }
  }

  // Next, we'll try to match a discrete GPU.
  for (uint32_t i = 0; i < physicalDeviceCount; i++) {
    // Skip devices we've already tried
    if (devicesTried.find(i) != devicesTried.end()) {
        continue;
    }

    VkPhysicalDeviceProperties deviceProps;
    vk_funcs.vkGetPhysicalDeviceProperties(physicalDevices[i], &deviceProps);
    if (deviceProps.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        if (tryInitializeDevice(log, instance, physicalDevices[i], &deviceProps, vkSurfaceKHR, decoderType, hdr, placebo_vulkan)) {
            return reinterpret_cast<jlong>(placebo_vulkan);
        }
        devicesTried.emplace(i);
    }
  }

  // Finally, we'll try matching any non-software device.
  for (uint32_t i = 0; i < physicalDeviceCount; i++) {
    // Skip devices we've already tried
    if (devicesTried.find(i) != devicesTried.end()) {
        continue;
    }

    VkPhysicalDeviceProperties deviceProps;
    vk_funcs.vkGetPhysicalDeviceProperties(physicalDevices[i], &deviceProps);
    if (tryInitializeDevice(log, instance, physicalDevices[i], &deviceProps, vkSurfaceKHR, decoderType, hdr, placebo_vulkan)) {
        return reinterpret_cast<jlong>(placebo_vulkan);
    }
    devicesTried.emplace(i);
  }

  return 0;
}

extern "C"
JNIEXPORT jboolean JNICALL Java_com_grill_placebo_PlaceboManager_plInitQueue
  (JNIEnv *env, jobject obj, jlong placebo_vulkan) {
  pl_vulkan vulkan = reinterpret_cast<pl_vulkan>(placebo_vulkan);

  uint32_t queueFamilyCount = 0;
  vk_funcs.vkGetPhysicalDeviceQueueFamilyProperties(vulkan->phys_device, &queueFamilyCount, nullptr);
  std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyCount);
  vk_funcs.vkGetPhysicalDeviceQueueFamilyProperties(vulkan->phys_device, &queueFamilyCount, queueFamilyProperties.data());
  auto queue_it = std::find_if(queueFamilyProperties.begin(), queueFamilyProperties.end(), [](VkQueueFamilyProperties prop) {
      return prop.queueFlags & VK_QUEUE_VIDEO_DECODE_BIT_KHR;
  });

  if (queue_it != queueFamilyProperties.end()) {
      vk_decode_queue_index = std::distance(queueFamilyProperties.begin(), queue_it);
  }

  return vk_decode_queue_index != 0;
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
JNIEXPORT jboolean JNICALL Java_com_grill_placebo_PlaceboManager_plInitFunctionPointers
  (JNIEnv *env, jobject obj, jlong placebo_vk_inst) {
  pl_vk_inst instance = reinterpret_cast<pl_vk_inst>(placebo_vk_inst);
  #ifdef _WIN32
       vk_funcs.vkCreateWin32SurfaceKHR = reinterpret_cast<PFN_vkCreateWin32SurfaceKHR>(
               instance->get_proc_addr(instance->instance, "vkCreateWin32SurfaceKHR"));
       if(!vk_funcs.vkCreateWin32SurfaceKHR) {
           LogCallbackFunction(nullptr, PL_LOG_ERR, "Failed to resolve vkCreateWin32SurfaceKHR!");
           return static_cast<jboolean>(false);
       }
  #elif defined(__APPLE__)
      vk_funcs.vkCreateMetalSurfaceEXT = reinterpret_cast<PFN_vkCreateMetalSurfaceEXT>(
               instance->get_proc_addr(instance->instance, "vkCreateMetalSurfaceEXT"));
      if(!vk_funcs.vkCreateMetalSurfaceEXT) {
          LogCallbackFunction(nullptr, PL_LOG_ERR, "Failed to resolve vkCreateMetalSurfaceEXT!");
          return static_cast<jboolean>(false);
      }
  #else
       bool xcbSurface = true;
       vk_funcs.vkCreateXcbSurfaceKHR = reinterpret_cast<PFN_vkCreateXcbSurfaceKHR>(
               instance->get_proc_addr(instance->instance, "vkCreateXcbSurfaceKHR"));
       if(!vk_funcs.vkCreateXcbSurfaceKHR) {
           LogCallbackFunction(nullptr, PL_LOG_ERR, "Failed to resolve vkCreateXcbSurfaceKHR!");
           xcbSurface = false;
       }
       bool waylandSurface = true;
       vk_funcs.vkCreateWaylandSurfaceKHR = reinterpret_cast<PFN_vkCreateWaylandSurfaceKHR>(
               instance->get_proc_addr(instance->instance, "vkCreateWaylandSurfaceKHR"));
       if(!vk_funcs.vkCreateWaylandSurfaceKHR) {
           LogCallbackFunction(nullptr, PL_LOG_ERR, "Failed to resolve vkCreateWaylandSurfaceKHR!");
           waylandSurface = false;
       }

       if(!xcbSurface && !waylandSurface){
           return static_cast<jboolean>(false);
       }
  #endif
  vk_funcs.vkDestroySurfaceKHR = reinterpret_cast<PFN_vkDestroySurfaceKHR>(
             instance->get_proc_addr(instance->instance, "vkDestroySurfaceKHR"));
  if(!vk_funcs.vkDestroySurfaceKHR) {
      LogCallbackFunction(nullptr, PL_LOG_ERR, "Failed to resolve vkDestroySurfaceKHR!");
      return static_cast<jboolean>(false);
  }

  vk_funcs.vkGetPhysicalDeviceQueueFamilyProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(
             instance->get_proc_addr(instance->instance, "vkGetPhysicalDeviceQueueFamilyProperties"));
  if(!vk_funcs.vkGetPhysicalDeviceQueueFamilyProperties) {
      LogCallbackFunction(nullptr, PL_LOG_ERR, "Failed to resolve vkGetPhysicalDeviceQueueFamilyProperties!");
      return static_cast<jboolean>(false);
  }

  vk_funcs.vkGetPhysicalDeviceSurfacePresentModesKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfacePresentModesKHR>(
             instance->get_proc_addr(instance->instance, "vkGetPhysicalDeviceSurfacePresentModesKHR"));
  if(!vk_funcs.vkGetPhysicalDeviceSurfacePresentModesKHR) {
      LogCallbackFunction(nullptr, PL_LOG_ERR, "Failed to resolve vkGetPhysicalDeviceSurfacePresentModesKHR!");
      return static_cast<jboolean>(false);
  }

  vk_funcs.vkGetPhysicalDeviceSurfaceFormatsKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceFormatsKHR>(
             instance->get_proc_addr(instance->instance, "vkGetPhysicalDeviceSurfaceFormatsKHR"));
  if(!vk_funcs.vkGetPhysicalDeviceSurfaceFormatsKHR) {
      LogCallbackFunction(nullptr, PL_LOG_ERR, "Failed to resolve vkGetPhysicalDeviceSurfaceFormatsKHR!");
      return static_cast<jboolean>(false);
  }

  vk_funcs.vkEnumeratePhysicalDevices = reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(
             instance->get_proc_addr(instance->instance, "vkEnumeratePhysicalDevices"));
  if(!vk_funcs.vkEnumeratePhysicalDevices) {
      LogCallbackFunction(nullptr, PL_LOG_ERR, "Failed to resolve vkEnumeratePhysicalDevices!");
      return static_cast<jboolean>(false);
  }

  vk_funcs.vkGetPhysicalDeviceProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(
             instance->get_proc_addr(instance->instance, "vkGetPhysicalDeviceProperties"));
  if(!vk_funcs.vkGetPhysicalDeviceProperties) {
      LogCallbackFunction(nullptr, PL_LOG_ERR, "Failed to resolve vkGetPhysicalDeviceProperties!");
      return static_cast<jboolean>(false);
  }

  vk_funcs.vkGetPhysicalDeviceSurfaceSupportKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceSupportKHR>(
             instance->get_proc_addr(instance->instance, "vkGetPhysicalDeviceSurfaceSupportKHR"));
  if(!vk_funcs.vkGetPhysicalDeviceSurfaceSupportKHR) {
      LogCallbackFunction(nullptr, PL_LOG_ERR, "Failed to resolve vkGetPhysicalDeviceSurfaceSupportKHR!");
      return static_cast<jboolean>(false);
  }

  vk_funcs.vkEnumerateDeviceExtensionProperties = reinterpret_cast<PFN_vkEnumerateDeviceExtensionProperties>(
             instance->get_proc_addr(instance->instance, "vkEnumerateDeviceExtensionProperties"));
  if(!vk_funcs.vkEnumerateDeviceExtensionProperties) {
      LogCallbackFunction(nullptr, PL_LOG_ERR, "Failed to resolve vkEnumerateDeviceExtensionProperties!");
      return static_cast<jboolean>(false);
  }

  return static_cast<jboolean>(true);
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
  (JNIEnv *env, jobject obj) {
  #ifdef _WIN32
       return reinterpret_cast<jlong>(vk_funcs.vkCreateWin32SurfaceKHR);
  #endif

  return 0;
}

extern "C"
JNIEXPORT jlong JNICALL Java_com_grill_placebo_PlaceboManager_plGetMetalSurfaceEXT
  (JNIEnv *env, jobject obj) {
  #ifdef __APPLE__
       return reinterpret_cast<jlong>(vk_funcs.vkCreateMetalSurfaceEXT);
  #endif

  return 0;
}

extern "C"
JNIEXPORT jlong JNICALL Java_com_grill_placebo_PlaceboManager_plGetXcbSurfaceFunctionPointer
  (JNIEnv *env, jobject obj) {
#if !defined(_WIN32) && !defined(__APPLE__)
  if (vk_funcs.vkCreateXcbSurfaceKHR != nullptr) {
       return reinterpret_cast<jlong>(vk_funcs.vkCreateXcbSurfaceKHR);
  }
#endif

  return 0;
}

extern "C"
JNIEXPORT jlong JNICALL Java_com_grill_placebo_PlaceboManager_plGetWaylandSurfaceFunctionPointer
  (JNIEnv *env, jobject obj) {
#if !defined(_WIN32) && !defined(__APPLE__)
  if (vk_funcs.vkCreateWaylandSurfaceKHR != nullptr) {
       return reinterpret_cast<jlong>(vk_funcs.vkCreateWaylandSurfaceKHR);
  }
#endif

  return 0;
}

extern "C"
JNIEXPORT void JNICALL Java_com_grill_placebo_PlaceboManager_plDestroySurface
  (JNIEnv *env, jobject obj, jlong placebo_vk_inst, jlong surface) {
  pl_vk_inst instance = reinterpret_cast<pl_vk_inst>(placebo_vk_inst);
  VkSurfaceKHR vkSurfaceKHR = reinterpret_cast<VkSurfaceKHR>(static_cast<uint64_t>(surface));
  vk_funcs.vkDestroySurfaceKHR(instance->instance, vkSurfaceKHR, nullptr);
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
      .swapchain_depth = 1,
  };

  pl_swapchain placebo_swapchain = pl_vulkan_create_swapchain(vulkan, &swapchain_params);
  return reinterpret_cast<jlong>(placebo_swapchain);
}

extern "C"
JNIEXPORT jlong JNICALL Java_com_grill_placebo_PlaceboManager_plCreateSwapchainWithBestPresentMode
  (JNIEnv *env, jobject obj, jlong placebo_vulkan, jlong surface, jboolean vsync) {
  pl_vulkan vulkan = reinterpret_cast<pl_vulkan>(placebo_vulkan);
  VkSurfaceKHR vkSurfaceKHR = reinterpret_cast<VkSurfaceKHR>(static_cast<uint64_t>(surface));
  VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR; // Default mode

  if(vsync) {
      LogCallbackFunction(nullptr, PL_LOG_INFO, "Using VK_PRESENT_MODE_MAILBOX_KHR present mode with V-Sync enabled");
      present_mode = VK_PRESENT_MODE_FIFO_KHR;
  } else {
      if (isPresentModeSupportedByPhysicalDevice(vulkan->phys_device, VK_PRESENT_MODE_IMMEDIATE_KHR, vkSurfaceKHR)) {
          LogCallbackFunction(nullptr, PL_LOG_INFO, "Using Immediate present mode with V-Sync disabled");
          present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
      } else {
          LogCallbackFunction(nullptr, PL_LOG_INFO, "Immediate present mode is not supported.");

          // FIFO Relaxed can tear if the frame is running late
          if (isPresentModeSupportedByPhysicalDevice(vulkan->phys_device, VK_PRESENT_MODE_FIFO_RELAXED_KHR, vkSurfaceKHR)) {
              LogCallbackFunction(nullptr, PL_LOG_INFO, "Using VK_PRESENT_MODE_FIFO_RELAXED_KHR present mode with V-Sync disabled");
              present_mode = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
          }
          // Mailbox at least provides non-blocking behavior
          else if (isPresentModeSupportedByPhysicalDevice(vulkan->phys_device, VK_PRESENT_MODE_MAILBOX_KHR, vkSurfaceKHR)) {
              LogCallbackFunction(nullptr, PL_LOG_INFO, "Using VK_PRESENT_MODE_MAILBOX_KHR present mode with V-Sync disabled");
              present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
          }
          // FIFO is always supported
          else {
              LogCallbackFunction(nullptr, PL_LOG_INFO, "Using VK_PRESENT_MODE_FIFO_KHR present mode with V-Sync disabled");
              present_mode = VK_PRESENT_MODE_FIFO_KHR;
          }
      }
  }

  struct pl_vulkan_swapchain_params swapchain_params = {
      .surface = vkSurfaceKHR,
      .present_mode = present_mode,
      .swapchain_depth = 1, // do not queue frames
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
JNIEXPORT jboolean JNICALL Java_com_grill_placebo_PlaceboManager_plSwapchainResize
  (JNIEnv *env, jobject obj, jlong swapchain, jint width, jint height) {
  pl_swapchain placebo_swapchain = reinterpret_cast<pl_swapchain>(swapchain);
  int int_width = static_cast<int>(width);
  int int_height = static_cast<int>(height);
  bool result = pl_swapchain_resize(placebo_swapchain, &int_width, &int_height);
  return static_cast<jboolean>(result);
}

extern "C"
JNIEXPORT jboolean JNICALL Java_com_grill_placebo_PlaceboManager_plSwapchainResizeWithBuffer
  (JNIEnv *env, jobject obj, jlong swapchain, jlong widthBuffer, jlong heightBuffer) {
  pl_swapchain placebo_swapchain = reinterpret_cast<pl_swapchain>(swapchain);
  int* intWidth = reinterpret_cast<int*>(widthBuffer);
  int* intHeight = reinterpret_cast<int*>(heightBuffer);
  bool result = pl_swapchain_resize(placebo_swapchain, intWidth, intHeight);
  return static_cast<jboolean>(result);
}

extern "C"
JNIEXPORT jboolean JNICALL Java_com_grill_placebo_PlaceboManager_plSetHwDeviceCtx
  (JNIEnv *env, jobject obj, jlong vulkan_hw_dev_ctx_handle, jlong placebo_vulkan, jlong placebo_vk_inst) {
  if (vk_decode_queue_index < 0) {
    LogCallbackFunction(nullptr, PL_LOG_ERR, "Can not configure vulkan hardware device context!");
    return static_cast<jboolean>(false); // not possible
  }

  AVBufferRef *vulkan_hw_dev_ctx = reinterpret_cast<AVBufferRef *>(vulkan_hw_dev_ctx_handle);
  pl_vulkan vulkan = reinterpret_cast<pl_vulkan>(placebo_vulkan);
  pl_vk_inst instance = reinterpret_cast<pl_vk_inst>(placebo_vk_inst);

  AVHWDeviceContext *hwctx = reinterpret_cast<AVHWDeviceContext*>(vulkan_hw_dev_ctx->data);
  hwctx->user_opaque = const_cast<void*>(reinterpret_cast<const void*>(vulkan));
  AVVulkanDeviceContext *vkctx = reinterpret_cast<AVVulkanDeviceContext*>(hwctx->hwctx);
  vkctx->get_proc_addr = vulkan->get_proc_addr;
  vkctx->inst = vulkan->instance;
  vkctx->phys_dev = vulkan->phys_device;
  vkctx->act_dev = vulkan->device;
  vkctx->device_features = *vulkan->features;

  vkctx->enabled_inst_extensions = instance->extensions;
  vkctx->nb_enabled_inst_extensions = instance->num_extensions;

  vkctx->enabled_dev_extensions = vulkan->extensions;
  vkctx->nb_enabled_dev_extensions = vulkan->num_extensions;
  vkctx->queue_family_index = vulkan->queue_graphics.index;
  vkctx->nb_graphics_queues = vulkan->queue_graphics.count;
  vkctx->queue_family_tx_index = vulkan->queue_transfer.index;
  vkctx->nb_tx_queues = vulkan->queue_transfer.count;
  vkctx->queue_family_comp_index = vulkan->queue_compute.index;
  vkctx->nb_comp_queues = vulkan->queue_compute.count;

  vkctx->queue_family_decode_index = vk_decode_queue_index;
  vkctx->nb_decode_queues = 1;
  vkctx->lock_queue = [](struct AVHWDeviceContext *dev_ctx, uint32_t queue_family, uint32_t index) {
      auto vk = reinterpret_cast<pl_vulkan>(dev_ctx->user_opaque);
      vk->lock_queue(vk, queue_family, index);
  };
  vkctx->unlock_queue = [](struct AVHWDeviceContext *dev_ctx, uint32_t queue_family, uint32_t index) {
      auto vk = reinterpret_cast<pl_vulkan>(dev_ctx->user_opaque);
      vk->unlock_queue(vk, queue_family, index);
  };

  return static_cast<jboolean>(true);
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
  (JNIEnv *env, jobject obj, jlong avframe, jlong placebo_vulkan, jlong swapchain, jlong renderer) {
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
  };
  bool mapped = pl_map_avframe_ex(vulkan->gpu, &placebo_frame, &avparams);
  if (!mapped) {
      LogCallbackFunction(nullptr, PL_LOG_ERR, "Failed to map AVFrame to Placebo frame!");
      av_frame_free(&frame);
      return static_cast<jboolean>(ret);
  }
  // set colorspace hint
  if (!pl_color_space_equal(&placebo_frame.color, &m_LastColorspace)) {
      m_LastColorspace = placebo_frame.color;
      pl_swapchain_colorspace_hint(placebo_swapchain, &placebo_frame.color);
  }
  pl_rect2df crop;

  if (!pl_swapchain_start_frame(placebo_swapchain, &sw_frame)) {
      LogCallbackFunction(nullptr, PL_LOG_ERR, "Failed to start Placebo frame!");
      goto cleanup;
  }
  pl_frame_from_swapchain(&target_frame, &sw_frame);

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
  if (!pl_swapchain_submit_frame(placebo_swapchain)) {
      LogCallbackFunction(nullptr, PL_LOG_ERR, "Failed to submit Placebo frame!");
      goto cleanup;
  }

  m_HasPendingSwapchainFrame = true;
  pl_swapchain_swap_buffers(placebo_swapchain);
  ret = true;

cleanup:
  pl_unmap_avframe(vulkan->gpu, &placebo_frame);

  return static_cast<jboolean>(ret);
}

extern "C"
JNIEXPORT jboolean JNICALL Java_com_grill_placebo_PlaceboManager_plRenderAvFrameWithUi
  (JNIEnv *env, jobject obj, jlong avframe, jlong placebo_vulkan, jlong swapchain, jlong renderer, jlong ui, jint width, jint height) {
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
  };
  bool mapped = pl_map_avframe_ex(vulkan->gpu, &placebo_frame, &avparams);
  if (!mapped) {
      LogCallbackFunction(nullptr, PL_LOG_ERR, "Failed to map AVFrame to Placebo frame!");
      av_frame_free(&frame);
      return static_cast<jboolean>(ret);
  }
  // set colorspace hint
  if (!pl_color_space_equal(&placebo_frame.color, &m_LastColorspace)) {
      m_LastColorspace = placebo_frame.color;
      pl_swapchain_colorspace_hint(placebo_swapchain, &placebo_frame.color);
  }
  pl_rect2df crop;
  if (!pl_swapchain_start_frame(placebo_swapchain, &sw_frame)) {
      LogCallbackFunction(nullptr, PL_LOG_ERR, "Failed to start Placebo frame!");
      goto cleanup;
  }
  pl_frame_from_swapchain(&target_frame, &sw_frame);

  if(ui != 0) {
      struct ui *ui_instance = reinterpret_cast<struct ui *>(ui);
      render_ui(ui_instance, width, height);
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

  m_HasPendingSwapchainFrame = true;
  pl_swapchain_swap_buffers(placebo_swapchain);
  ret = true;

cleanup:
  pl_unmap_avframe(vulkan->gpu, &placebo_frame);

  return static_cast<jboolean>(ret);
}

extern "C"
JNIEXPORT jboolean JNICALL Java_com_grill_placebo_PlaceboManager_plRenderUiOnly
  (JNIEnv *env, jobject obj, jlong placebo_vulkan, jlong swapchain, jlong renderer, jlong ui, jint width, jint height) {
  pl_vulkan vulkan = reinterpret_cast<pl_vulkan>(placebo_vulkan);
    pl_swapchain placebo_swapchain = reinterpret_cast<pl_swapchain>(swapchain);
    pl_renderer placebo_renderer = reinterpret_cast<pl_renderer>(renderer);
    bool ret = false;

    struct pl_swapchain_frame sw_frame = {0};
    struct pl_frame target_frame = {0};

    struct pl_color_space hint = {
        .primaries = PL_COLOR_PRIM_UNKNOWN,
        .transfer = PL_COLOR_TRC_UNKNOWN,
        .hdr = PL_HDR_METADATA_NONE
    };
    if (!pl_color_space_equal(&hint, &m_LastColorspace)) { // reset color space hint if needed
        m_LastColorspace = hint;
        pl_swapchain_colorspace_hint(placebo_swapchain, &hint);
    }

    if (!pl_swapchain_start_frame(placebo_swapchain, &sw_frame)) {
        LogCallbackFunction(nullptr, PL_LOG_ERR, "Failed to start Placebo frame!");
        goto finish;
    }
    pl_frame_from_swapchain(&target_frame, &sw_frame);

    if(ui != 0) {
        struct ui *ui_instance = reinterpret_cast<struct ui *>(ui);
        render_ui(ui_instance, width, height);
    }

    if (!pl_render_image(placebo_renderer, NULL, &target_frame, &render_params)) {
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

    m_HasPendingSwapchainFrame = true;
    pl_swapchain_swap_buffers(placebo_swapchain);
    ret = true;

  cleanup:
    pl_unmap_avframe(vulkan->gpu, &target_frame);
  finish:
    return static_cast<jboolean>(ret);
}

extern "C"
JNIEXPORT void JNICALL Java_com_grill_placebo_PlaceboManager_plCleanupRendererContext
  (JNIEnv *env, jobject obj, jlong swapchain) {
  pl_swapchain placebo_swapchain = reinterpret_cast<pl_swapchain>(swapchain);
  if (!m_HasPendingSwapchainFrame) {
      return;
  }

  struct pl_swapchain_frame sw_frame = {0};
  if (!pl_swapchain_start_frame(placebo_swapchain, &sw_frame)) {
      LogCallbackFunction(nullptr, PL_LOG_ERR, "Failed to start Placebo frame in cleanup renderer context!");
      return;
  }

  pl_swapchain_submit_frame(placebo_swapchain);
  m_HasPendingSwapchainFrame = false;
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

nk_rune ranges_icons[] = {
    0xE802, 0xE803,
    0xE804, 0xF11B,
    0xF130, 0xF131,
    0
};

struct ui_vertex {
  float pos[2];
  float coord[2];
  uint8_t color[4];
};

#define NUM_VERTEX_ATTRIBS 3

struct ui {
  pl_gpu gpu;
  pl_dispatch dp;
  struct nk_font *default_font;
  struct nk_font *default_bold_font;
  struct nk_font *default_small_font;
  struct nk_font *icon_font;
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

  delete[] globalUiState.popupState.headerText;
  delete[] globalUiState.popupState.popupText;
  delete[] globalUiState.popupState.popupButtonLeft;
  delete[] globalUiState.popupState.popupButtonRight;
  delete[] globalUiState.popupState.checkboxText;

  // After deleting, set pointers to nullptr to avoid dangling pointers
  globalUiState.popupState.headerText = nullptr;
  globalUiState.popupState.popupText = nullptr;
  globalUiState.popupState.popupButtonLeft = nullptr;
  globalUiState.popupState.popupButtonRight = nullptr;
  globalUiState.popupState.checkboxText = nullptr;

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
  };

  // Initialize font atlas using built-in font
  nk_font_atlas_init_default(&ui->atlas);
  nk_font_atlas_begin(&ui->atlas);
  struct nk_font_config robotoConfig = nk_font_config(0);
  robotoConfig.range = nk_font_default_glyph_ranges();
  robotoConfig.oversample_h = 1; robotoConfig.oversample_v = 1;
  robotoConfig.pixel_snap = true;
  ui->default_font = nk_font_atlas_add_from_memory(&ui->atlas, roboto_font, roboto_font_size, 24, &robotoConfig);
  ui->default_bold_font = nk_font_atlas_add_from_memory(&ui->atlas, roboto_bold_font, roboto_bold_font_size, 28, &robotoConfig);
  ui->default_small_font = nk_font_atlas_add_from_memory(&ui->atlas, roboto_font, roboto_font_size, 14, &robotoConfig);
  struct nk_font_config iconConfig = nk_font_config(0);
  iconConfig.range = ranges_icons;
  iconConfig.oversample_h = 1; iconConfig.oversample_v = 1;
  iconConfig.pixel_snap = true;
  ui->icon_font = nk_font_atlas_add_from_memory(&ui->atlas, gui_font, gui_font_size, 30, &iconConfig);
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
  if (!nk_init_default(&ui->nk, &ui->default_font->handle)) {
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

void render_ui(struct ui *ui, int width, int height) {
  if (!ui || (!globalUiState.showTouchpad && !globalUiState.showPanel && !globalUiState.showPopup))
      return;

  struct nk_context *ctx = &ui->nk;
  const struct nk_rect bounds = nk_rect(0, 0, width, height);

  nk_style_push_style_item(ctx, &ctx->style.window.fixed_background, nk_style_item_hide());
  if (nk_begin(ctx, "FULLSCREEN", bounds, NK_WINDOW_NO_SCROLLBAR)) {
      nk_layout_space_begin(ctx, NK_STATIC, bounds.w, bounds.h); // use whole window space
      struct nk_command_buffer* out = nk_window_get_canvas(ctx);

      // dynamic sizes
      float centerPosition = (bounds.w / 2) - 32;
      float dialogWidth = std::min(800.0f, std::max(500.0f, bounds.w * 0.60f));
      float dialogHeight = std::min(390.0f, std::max(375.0f, bounds.h * 0.50f));
      // cache button style
      struct nk_style_button cachedButtonStyle = ctx->style.button;

      if(globalUiState.showPanel) {
          // **** PS button ****

          if(globalUiState.panelState.psButtonPressed){
              ctx->style.button.normal = nk_style_item_color(pressed_dark_grey_button_color);
              ctx->style.button.hover = nk_style_item_color(pressed_dark_grey_button_color);
              ctx->style.button.active = nk_style_item_color(pressed_dark_grey_button_color);
          } else {
              ctx->style.button.normal = nk_style_item_color(dark_grey_button_color);
              ctx->style.button.hover = nk_style_item_color(dark_grey_button_color);
              ctx->style.button.active = nk_style_item_color(dark_grey_button_color);
          }
          ctx->style.button.border_color = white_button_color;
          ctx->style.button.text_background = white_button_color;
          ctx->style.button.text_normal = white_button_color;
          ctx->style.button.text_hover = white_button_color;
          ctx->style.button.text_active = white_button_color;
          ctx->style.button.rounding = 8;

          nk_layout_space_push(ctx, nk_rect(centerPosition, (bounds.h - buttonSize) - bottomPadding, buttonSize, buttonSize));
          if (nk_button_label(ctx, "PS")) {
              // event handling (ignored here)
          }

          ctx->style.button = cachedButtonStyle;

          // **** Menu buttons ****

          /*** change font to default ***/
          nk_style_set_font(ctx, &ui->default_small_font->handle);
          /*** change font to default ***/

          if(globalUiState.panelState.shareButtonPressed){
              ctx->style.button.normal = nk_style_item_color(pressed_grey_button_color);
              ctx->style.button.hover = nk_style_item_color(pressed_grey_button_color);
              ctx->style.button.active = nk_style_item_color(pressed_grey_button_color);
          } else {
              ctx->style.button.normal = nk_style_item_color(grey_button_color);
              ctx->style.button.hover = nk_style_item_color(grey_button_color);
              ctx->style.button.active = nk_style_item_color(grey_button_color);
          }
          ctx->style.button.border_color = black_button_color;
          ctx->style.button.text_background = black_button_color;
          ctx->style.button.text_normal = black_button_color;
          ctx->style.button.text_hover = black_button_color;
          ctx->style.button.text_active = black_button_color;
          ctx->style.button.rounding = 10;
          // -> Share
          nk_layout_space_push(ctx, nk_rect(centerPosition - ((buttonSize * 1.5) + 7), ((bounds.h - menuButtonHeight) - bottomPadding) - menuButtonFontSize, buttonSize, menuButtonFontSize));
          nk_label(ctx, "SHARE", NK_TEXT_ALIGN_LEFT);
          nk_layout_space_push(ctx, nk_rect(centerPosition - (buttonSize * 1.5), (bounds.h - menuButtonHeight) - bottomPadding, buttonSize * 0.5, menuButtonHeight));
          if(globalUiState.panelState.shareButtonPressed){
              nk_button_color(ctx, pressed_grey_button_color);
          } else {
              nk_button_color(ctx, grey_button_color);
          }
          // -> Options
          if(globalUiState.panelState.optionsButtonPressed){
              ctx->style.button.normal = nk_style_item_color(pressed_grey_button_color);
              ctx->style.button.hover = nk_style_item_color(pressed_grey_button_color);
              ctx->style.button.active = nk_style_item_color(pressed_grey_button_color);
          } else {
              ctx->style.button.normal = nk_style_item_color(grey_button_color);
              ctx->style.button.hover = nk_style_item_color(grey_button_color);
              ctx->style.button.active = nk_style_item_color(grey_button_color);
          }
          nk_layout_space_push(ctx, nk_rect(centerPosition + ((buttonSize * 2) - 12), ((bounds.h - menuButtonHeight) - bottomPadding) - menuButtonFontSize, buttonSize, menuButtonFontSize));
          nk_label(ctx, "OPTIONS", NK_TEXT_ALIGN_LEFT);
          nk_layout_space_push(ctx, nk_rect(centerPosition + (buttonSize * 2), (bounds.h - menuButtonHeight) - bottomPadding, buttonSize * 0.5, menuButtonHeight ));
          if(globalUiState.panelState.optionsButtonPressed){
              nk_button_color(ctx, pressed_grey_button_color);
          } else {
              nk_button_color(ctx, grey_button_color);
          }

          ctx->style.button = cachedButtonStyle;

          // **** Mic button

          /*** change font to icon ***/
          nk_style_set_font(ctx, &ui->icon_font->handle);
          /*** change font to icon ***/

          if(globalUiState.panelState.showMicButton) {

              if(globalUiState.panelState.micButtonPressed){
                  ctx->style.button.normal = nk_style_item_color(pressed_white_button_color_alpha);
                  ctx->style.button.hover = nk_style_item_color(pressed_white_button_color_alpha);
                  ctx->style.button.active = nk_style_item_color(pressed_white_button_color_alpha);
              } else {
                  ctx->style.button.normal = nk_style_item_color(white_button_color_alpha);
                  ctx->style.button.hover = nk_style_item_color(white_button_color_alpha);
                  ctx->style.button.active = nk_style_item_color(white_button_color_alpha);
              }
              ctx->style.button.border_color = black_button_color;
              ctx->style.button.text_background = black_button_color;
              ctx->style.button.text_normal = black_button_color;
              ctx->style.button.text_hover = black_button_color;
              ctx->style.button.text_active = black_button_color;
              ctx->style.button.rounding = 8;
              ctx->style.button.border = 0;

              nk_layout_space_push(ctx, nk_rect(edgePadding, (bounds.h - buttonSize) - bottomPadding, buttonSize, buttonSize));
              if(globalUiState.panelState.micButtonActive) {
                  if (nk_button_label(ctx, "\uf130")) {
                      // event handling (ignored here)
                  }
              } else {
                  if (nk_button_label(ctx, "\uf131")) {
                      // event handling (ignored here)
                  }
              }

              ctx->style.button = cachedButtonStyle;
          }

          // **** Fullscreen

          if(globalUiState.panelState.showFullscreenButton) {
              if(globalUiState.panelState.fullscreenButtonPressed){
                  ctx->style.button.normal = nk_style_item_color(pressed_white_button_color_alpha);
                  ctx->style.button.hover = nk_style_item_color(pressed_white_button_color_alpha);
                  ctx->style.button.active = nk_style_item_color(pressed_white_button_color_alpha);
              } else {
                  ctx->style.button.normal = nk_style_item_color(white_button_color_alpha);
                  ctx->style.button.hover = nk_style_item_color(white_button_color_alpha);
                  ctx->style.button.active = nk_style_item_color(white_button_color_alpha);
              }
              ctx->style.button.border_color = black_button_color;
              ctx->style.button.text_background = black_button_color;
              ctx->style.button.text_normal = black_button_color;
              ctx->style.button.text_hover = black_button_color;
              ctx->style.button.text_active = black_button_color;
              ctx->style.button.rounding = 8;
              ctx->style.button.border = 0;
              nk_layout_space_push(ctx, nk_rect((bounds.w - (buttonSize * 2)) - (edgePadding * 1.5), (bounds.h - buttonSize) - bottomPadding, buttonSize, buttonSize));
              if(globalUiState.panelState.fullscreenButtonActive) {
                  if (nk_button_label(ctx, "\ue804")) {
                      // event handling (ignored here)
                  }
              } else {
                  if (nk_button_label(ctx, "\ue802")) {
                      // event handling (ignored here)
                  }
              }

              ctx->style.button = cachedButtonStyle;
          }

          // **** Close

          if(globalUiState.panelState.closeButtonPressed){
              ctx->style.button.normal = nk_style_item_color(pressed_white_button_color_alpha);
              ctx->style.button.hover = nk_style_item_color(pressed_white_button_color_alpha);
              ctx->style.button.active = nk_style_item_color(pressed_white_button_color_alpha);
          } else {
              ctx->style.button.normal = nk_style_item_color(white_button_color_alpha);
              ctx->style.button.hover = nk_style_item_color(white_button_color_alpha);
              ctx->style.button.active = nk_style_item_color(white_button_color_alpha);
          }
          ctx->style.button.border_color = black_button_color;
          ctx->style.button.text_background = black_button_color;
          ctx->style.button.text_normal = black_button_color;
          ctx->style.button.text_hover = black_button_color;
          ctx->style.button.text_active = black_button_color;
          ctx->style.button.rounding = 8;
          ctx->style.button.border = 0;
          nk_layout_space_push(ctx, nk_rect((bounds.w - buttonSize) - edgePadding, (bounds.h - buttonSize) - bottomPadding, buttonSize, buttonSize));
          if (nk_button_label(ctx, "\ue803")) {
              // event handling (ignored here)
          }

          ctx->style.button = cachedButtonStyle;

          /*** change font to default ***/
          nk_style_set_font(ctx, &ui->default_font->handle);
          /*** change font to default ***/
      }

      // **** Touchpad

      if(globalUiState.touchpadPressed) {
          ctx->style.button.normal = nk_style_item_color(touchpad_white_background_color_alpha);
          ctx->style.button.hover = nk_style_item_color(touchpad_white_background_color_alpha);
          ctx->style.button.active = nk_style_item_color(touchpad_white_background_color_alpha);
          ctx->style.button.border_color = touchpad_white_border_color_alpha;
          ctx->style.button.text_background = touchpad_white_border_color_alpha;
          ctx->style.button.text_normal = touchpad_white_border_color_alpha;
          ctx->style.button.text_hover = touchpad_white_border_color_alpha;
          ctx->style.button.text_active = touchpad_white_border_color_alpha;
          ctx->style.button.rounding = 8;
          ctx->style.button.border = 1;
          ctx->style.button.padding = nk_vec2(touchpadPadding, touchpadPadding);
          nk_layout_space_push(ctx, nk_rect(0, 0, bounds.w - ((touchpadPadding * 0.6)), bounds.h - ((touchpadPadding * 2) + buttonSize)));
          if (nk_button_label(ctx, "")) {
              // event handling (ignored here)
          }

          ctx->style.button = cachedButtonStyle;
      }

      // **** Fullscreen popup
      if(globalUiState.showPopup) {
          struct nk_rect dialog_rect = nk_rect((bounds.w / 2) - (dialogWidth / 2), (bounds.h / 2) - (dialogHeight / 2), dialogWidth, dialogHeight); // Background rect
          nk_fill_rect(out, dialog_rect, 8.0, dialog_background);

          /*** change font to bold default ***/
          nk_style_set_font(ctx, &ui->default_bold_font->handle);
          /*** change font to bold default ***/

          nk_layout_space_push(ctx, nk_rect(dialog_rect.x + dialogPaddingRight, dialog_rect.y + dialogHeadingPaddingTop, dialogWidth - (dialogPaddingRight * 2.5), dialogHeadingHeight)); // Heading
          nk_label_colored(ctx, globalUiState.popupState.headerText, NK_TEXT_LEFT, dialog_blue);

          /*** change font to default ***/
          nk_style_set_font(ctx, &ui->default_font->handle);
          /*** change font to default ***/


          if (globalUiState.popupState.showCheckbox) { // either text or checkbox
              nk_layout_space_push(ctx, nk_rect(dialog_rect.x + dialogPaddingRight, dialog_rect.y + dialogTextContentPaddingTop, dialogWidth - (dialogPaddingRight * 2.5), dialogButtonHeight)); // checkbox
              nk_bool check = globalUiState.popupState.checkboxChecked;
              nk_checkbox_label(ctx, globalUiState.popupState.checkboxText, &check);
          } else {
              nk_layout_space_push(ctx, nk_rect(dialog_rect.x + dialogPaddingRight, dialog_rect.y + dialogTextContentPaddingTop, dialogWidth - (dialogPaddingRight * 2.5), dialogHeight - dialogTextContentPaddingTop)); // Text
              nk_label_colored_wrap(ctx, globalUiState.popupState.popupText, white_button_color);
          }

          float buttonContainerFullWidth = (dialogButtonWidth * 2) + 14;
          float buttonContainerX = ((dialog_rect.x + dialogWidth) - buttonContainerFullWidth) - 40; // - 40 padding
          nk_layout_space_push(ctx, nk_rect(buttonContainerX, dialog_rect.y + (dialogHeight * 0.80), dialogButtonWidth, dialogButtonHeight)); // Button left

          ctx->style.button.hover = nk_style_item_color(nk_rgb(255,165,0));
          ctx->style.button.active = nk_style_item_color(nk_rgba(0,0,0,0));
          ctx->style.button.rounding = 15;
          ctx->style.button.border = 4;

          /*** change font to bold default ***/
          nk_style_set_font(ctx, &ui->default_bold_font->handle);
          /*** change font to bold default ***/

          if (globalUiState.popupState.popupButtonLeft != NULL && globalUiState.popupState.popupButtonLeft[0] != '\0') {
              nk_color buttonColor = globalUiState.popupState.leftButtonFocused ? dialog_yellow : dialog_blue;
              nk_color buttonColorBackground = globalUiState.popupState.leftButtonPressed ? buttonColor : nk_rgba(0,0,0,0);
              nk_color buttonTextColor = globalUiState.popupState.leftButtonPressed ? black_button_color : buttonColor;
              ctx->style.button.border_color = buttonColor;
              ctx->style.button.text_background = buttonTextColor;
              ctx->style.button.text_normal = buttonTextColor;
              ctx->style.button.text_hover = buttonTextColor;
              ctx->style.button.text_active = buttonTextColor;
              ctx->style.button.normal = nk_style_item_color(buttonColorBackground);

              if (nk_button_label(ctx, globalUiState.popupState.popupButtonLeft)) {
                  // event handling (ignored here)
              }
          }


          nk_layout_space_push(ctx, nk_rect(buttonContainerX + (dialogButtonWidth + 14), dialog_rect.y + (dialogHeight * 0.80), dialogButtonWidth, dialogButtonHeight)); // Button right

          nk_color buttonColor = globalUiState.popupState.rightButtonFocused ? dialog_yellow : dialog_blue;
          nk_color buttonColorBackground = globalUiState.popupState.rightButtonPressed ? buttonColor : nk_rgba(0,0,0,0);
          nk_color buttonTextColor = globalUiState.popupState.rightButtonPressed ? black_button_color : buttonColor;
          ctx->style.button.border_color = buttonColor;
          ctx->style.button.text_background = buttonTextColor;
          ctx->style.button.text_normal = buttonTextColor;
          ctx->style.button.text_hover = buttonTextColor;
          ctx->style.button.text_active = buttonTextColor;
          ctx->style.button.normal = nk_style_item_color(buttonColorBackground);

          if (nk_button_label(ctx, globalUiState.popupState.popupButtonRight)) {
              // event handling (ignored here)
          }

          /*** change font to default ***/
          nk_style_set_font(ctx, &ui->default_font->handle);
          /*** change font to default ***/

          ctx->style.button = cachedButtonStyle;
      }

      // **** END

      nk_layout_space_end(ctx);
  }
  nk_end(ctx);
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

extern "C" JNIEXPORT void JNICALL
Java_com_grill_placebo_PlaceboManager_nkUpdateUIState(JNIEnv *env, jobject obj,
  jboolean showTouchpad, jboolean showPanel, jboolean showPopup,
  jboolean touchpadPressed, jboolean panelPressed, jboolean panelShowMicButton,
  jboolean panelShowFullscreenButton, jboolean panelMicButtonPressed, jboolean panelMicButtonActive,
  jboolean panelShareButtonPressed, jboolean panelPsButtonPressed, jboolean panelOptionsButtonPressed,
  jboolean panelFullscreenButtonPressed, jboolean panelFullscreenButtonActive, jboolean panelCloseButtonPressed,
  jstring popupHeaderText, jstring popupPopupText, jboolean popupShowCheckbox,
  jstring popupButtonLeft, jstring popupButtonRight, jstring popupCheckboxText,
  jboolean popupCheckboxChecked, jboolean popupCheckboxFocused, jboolean popupLeftButtonPressed,
  jboolean popupLeftButtonFocused, jboolean popupRightButtonPressed, jboolean popupRightButtonFocused ) {

  globalUiState.showTouchpad = showTouchpad;
  globalUiState.showPanel = showPanel;
  globalUiState.showPopup = showPopup;
  globalUiState.touchpadPressed = touchpadPressed;
  globalUiState.panelPressed = panelPressed;

  globalUiState.panelState.showMicButton = panelShowMicButton;
  globalUiState.panelState.showFullscreenButton = panelShowFullscreenButton;
  globalUiState.panelState.micButtonPressed = panelMicButtonPressed;
  globalUiState.panelState.micButtonActive = panelMicButtonActive;
  globalUiState.panelState.shareButtonPressed = panelShareButtonPressed;
  globalUiState.panelState.psButtonPressed = panelPsButtonPressed;
  globalUiState.panelState.optionsButtonPressed = panelOptionsButtonPressed;
  globalUiState.panelState.fullscreenButtonPressed = panelFullscreenButtonPressed;
  globalUiState.panelState.fullscreenButtonActive = panelFullscreenButtonActive;
  globalUiState.panelState.closeButtonPressed = panelCloseButtonPressed;

  delete[] globalUiState.popupState.headerText;
  globalUiState.popupState.headerText = copyString(env, popupHeaderText);
  delete[] globalUiState.popupState.popupText;
  globalUiState.popupState.popupText = copyString(env, popupPopupText);
  delete[] globalUiState.popupState.popupButtonLeft;
  globalUiState.popupState.popupButtonLeft = copyString(env, popupButtonLeft);
  delete[] globalUiState.popupState.popupButtonRight;
  globalUiState.popupState.popupButtonRight = copyString(env, popupButtonRight);
  delete[] globalUiState.popupState.checkboxText;
  globalUiState.popupState.checkboxText = copyString(env, popupCheckboxText);

  globalUiState.popupState.showCheckbox = popupShowCheckbox;
  globalUiState.popupState.checkboxChecked = popupCheckboxChecked;
  globalUiState.popupState.checkboxFocused = popupCheckboxFocused;
  globalUiState.popupState.leftButtonPressed = popupLeftButtonPressed;
  globalUiState.popupState.leftButtonFocused = popupLeftButtonFocused;
  globalUiState.popupState.rightButtonPressed = popupRightButtonPressed;
  globalUiState.popupState.rightButtonFocused = popupRightButtonFocused;
}

extern "C"
JNIEXPORT void JNICALL Java_com_grill_placebo_PlaceboManager_nkDestroyUI
  (JNIEnv *env, jobject obj, jlong ui) {
  struct ui *ui_instance = reinterpret_cast<struct ui *>(ui);
  ui_destroy(ui_instance);
}



