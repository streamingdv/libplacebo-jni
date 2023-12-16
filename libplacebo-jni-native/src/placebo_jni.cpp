#include "com_grill_placebo_PlaceboManager.h"

#include <jni.h>
#include <libplacebo/log.h>

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

extern "C"
JNIEXPORT jlong JNICALL Java_PlaceboWrapper_createLog
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
JNIEXPORT void JNICALL Java_PlaceboWrapper_cleanupLog
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