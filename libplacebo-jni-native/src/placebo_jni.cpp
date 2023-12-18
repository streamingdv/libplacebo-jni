#include "com_grill_placebo_PlaceboManager.h"

#include <cstdlib>
#include <string>
#include <vector>
#include <jni.h>
#include <libplacebo/log.h>
#include <libplacebo/vulkan.h>

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

// Helper function to convert a Java string array to a std::vector of std::string
std::vector<std::string> convertStringArray(JNIEnv* env, jobjectArray jArray) {
    std::vector<std::string> result;
    if (jArray != nullptr) {
        jsize arrayLength = env->GetArrayLength(jArray);
        for (jsize i = 0; i < arrayLength; i++) {
            jstring jStr = static_cast<jstring>(env->GetObjectArrayElement(jArray, i));
            const char* rawStr = env->GetStringUTFChars(jStr, nullptr);
            result.emplace_back(rawStr);
            env->ReleaseStringUTFChars(jStr, rawStr);
        }
    }
    return result;
}

// Helper function to convert a std::vector of std::string to an array of const char*
std::vector<const char*> convertToCStringArray(const std::vector<std::string>& vec) {
    std::vector<const char*> cStrings;
    for (const auto& str : vec) {
        cStrings.push_back(str.c_str());
    }
    return cStrings;
}

/*** define JNI methods ***/

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
  (JNIEnv *env, jobject obj, jlong logHandle) {
    if (globalCallback != nullptr) {
        env->DeleteGlobalRef(globalCallback);
        globalCallback = nullptr;
    }
     pl_log *log = reinterpret_cast<pl_log *>(logHandle);
     if (log != nullptr) {
         pl_log_destroy(log);
     }
}

extern "C"
JNIEXPORT jstring JNICALL Java_WindowSystemDetector_getWindowingSystem(JNIEnv* env, jclass) {
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

JNIEXPORT jlong JNICALL Java_PlaceboLibrary_plVkInstCreate(JNIEnv *env, jobject obj, jlong plLog, jobject paramsObj) {
    jclass paramsCls = env->GetObjectClass(paramsObj);

    jfieldID debugField = env->GetFieldID(paramsCls, "debug", "Z");
    jfieldID debugExtraField = env->GetFieldID(paramsCls, "debugExtra", "Z");
    jfieldID maxApiVersionField = env->GetFieldID(paramsCls, "maxApiVersion", "I");
    jfieldID extensionsField = env->GetFieldID(paramsCls, "extensions", "[Ljava/lang/String;");
    jfieldID optExtensionsField = env->GetFieldID(paramsCls, "optExtensions", "[Ljava/lang/String;");
    jfieldID layersField = env->GetFieldID(paramsCls, "layers", "[Ljava/lang/String;");
    jfieldID optLayersField = env->GetFieldID(paramsCls, "optLayers", "[Ljava/lang/String;");

    bool debug = env->GetBooleanField(paramsObj, debugField);
    bool debugExtra = env->GetBooleanField(paramsObj, debugExtraField);
    uint32_t maxApiVersion = static_cast<uint32_t>(env->GetIntField(paramsObj, maxApiVersionField));
    jobjectArray extensionsJArray = static_cast<jobjectArray>(env->GetObjectField(paramsObj, extensionsField));
    jobjectArray optExtensionsJArray = static_cast<jobjectArray>(env->GetObjectField(paramsObj, optExtensionsField));
    jobjectArray layersJArray = static_cast<jobjectArray>(env->GetObjectField(paramsObj, layersField));
    jobjectArray optLayersJArray = static_cast<jobjectArray>(env->GetObjectField(paramsObj, optLayersField));

    auto extensions = convertStringArray(env, extensionsJArray);
    auto optExtensions = convertStringArray(env, optExtensionsJArray);
    auto layers = convertStringArray(env, layersJArray);
    auto optLayers = convertStringArray(env, optLayersJArray);

    auto extensionsCStr = convertToCStringArray(extensions);
    auto optExtensionsCStr = convertToCStringArray(optExtensions);
    auto layersCStr = convertToCStringArray(layers);
    auto optLayersCStr = convertToCStringArray(optLayers);

    pl_vk_inst_params vk_inst_params = {};
    vk_inst_params.debug = debug;
    vk_inst_params.debug_extra = debugExtra;
    vk_inst_params.max_api_version = maxApiVersion;
    vk_inst_params.extensions = extensionsJArray != nullptr ? extensionsCStr.data() : nullptr;
    vk_inst_params.num_extensions = extensionsJArray != nullptr ? extensionsCStr.size() : 0;
    vk_inst_params.opt_extensions = optExtensionsJArray != nullptr ? optExtensionsCStr.data() : nullptr;
    vk_inst_params.num_opt_extensions = optExtensionsJArray != nullptr ? optExtensionsCStr.size() : 0;
    vk_inst_params.layers = layersJArray != nullptr ? layersCStr.data() : nullptr;
    vk_inst_params.num_layers = layersJArray != nullptr ? layersCStr.size() : 0;
    vk_inst_params.opt_layers = optLayersJArray != nullptr ? optLayersCStr.data() : nullptr;
    vk_inst_params.num_opt_layers = optLayersJArray != nullptr ? optLayersCStr.size() : 0;

    pl_log log = reinterpret_cast<pl_log>(plLog);
    pl_vk_inst instance = pl_vk_inst_create(log, &vk_inst_params);

    return reinterpret_cast<jlong>(instance);
}

extern "C"
JNIEXPORT void JNICALL Java_com_grill_placebo_PlaceboManager_plVkInstDestroy
  (JNIEnv *env, jobject obj, jlong placebo_vk_inst) {
     pl_vk_inst *instance = reinterpret_cast<pl_vk_inst *>(placebo_vk_inst);
     if (instance != nullptr) {
         pl_vk_inst_destroy(instance);
     }
}

// ToDo next -> pl_vulkan_create with params placebo_log and pl_vk_inst
/*
 struct pl_vulkan_params vulkan_params = {
        .instance = placebo_vk_inst->instance,
        .get_proc_addr = placebo_vk_inst->get_proc_addr,
        PL_VULKAN_DEFAULTS
    };
    placebo_vulkan = pl_vulkan_create(placebo_log, &vulkan_params);
    return placebo_vulkan
*/


/*
// Structure representing a VkInstance. Using this is not required.
typedef const struct pl_vk_inst_t {
    VkInstance instance;

    // The Vulkan API version supported by this VkInstance.
    uint32_t api_version;

    // The associated vkGetInstanceProcAddr pointer.
    PFN_vkGetInstanceProcAddr get_proc_addr;

    // The instance extensions that were successfully enabled, including
    // extensions enabled by libplacebo internally. May contain duplicates.
    const char * const *extensions;
    int num_extensions;

    // The instance layers that were successfully enabled, including
    // layers enabled by libplacebo internally. May contain duplicates.
    const char * const *layers;
    int num_layers;
} *pl_vk_inst;

*/
