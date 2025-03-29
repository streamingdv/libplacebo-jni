#include "com_grill_placebo_FFmpegManager.h"
#include <iostream>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_mediacodec.h>
}
#include <android/native_window_jni.h>

// Global static decoder state and Java callback references
static AVCodecContext* codecCtx       = nullptr;
static AVFrame*        frame          = nullptr;
static AVPacket*       packet         = nullptr;
static AVBufferRef*    hwDeviceCtx    = nullptr;
static uint8_t*        inputBuffer    = nullptr;   // Pointer to input buffer (BytePointer memory)
static jobject         globalDecoderListener = nullptr;
static jobject         globalLogCallback    = nullptr;
static jobject         globalSurfaceRef     = nullptr;
static jmethodID       midOnFirstFrame = nullptr;
static jmethodID       midOnIDRNeeded  = nullptr;
static jmethodID       midOnLog        = nullptr;
static bool            firstFrameDecoded = false;
static int             failCount         = 0;
static JavaVM*         globalVm          = nullptr;

// Logging callback function for FFmpeg to forward logs to Java LogCallback
static void LogCallbackFunction(void *log_priv, int level, const char *msg) {
    JNIEnv *env = reinterpret_cast<JNIEnv*>(log_priv);
    if (env != nullptr && globalLogCallback != nullptr && midOnLog != nullptr) {
        // Convert the log message to a Java string and call the onLog callback
        jstring jmsg = env->NewStringUTF(msg);
        env->CallVoidMethod(globalLogCallback, midOnLog, (jint)level, jmsg);
        env->DeleteLocalRef(jmsg);
    } else {
        // Fallback: print to console if no Java callback available
        std::cerr << "FFmpeg [level " << level << "]: " << msg;
    }
}

// Helper: choose HW pixel format (MediaCodec) when offered by decoder
static enum AVPixelFormat getHWPixelFormat(AVCodecContext *ctx, const enum AVPixelFormat *pix_fmts) {
    // Iterate through the pixel formats proposed by the decoder.
    for (const enum AVPixelFormat *p = pix_fmts; *p != AV_PIX_FMT_NONE; ++p) {
        if (*p == AV_PIX_FMT_MEDIACODEC) {
            return AV_PIX_FMT_MEDIACODEC;
        }
    }
    // If MediaCodec format not found, use the first available format.
    return pix_fmts[0];
}

JNIEXPORT jboolean JNICALL Java_com_grill_placebo_FFmpegManager_init
  (JNIEnv *env, jobject thiz, jobject surface, jlong bufferAddr,
   jobject listener, jint codecType, jint width, jint height,
   jboolean useSoftware, jboolean enableFallback, jobject logCb) {
    // Prevent double-initialization: if already initialized, return false.
    if (codecCtx != nullptr) {
        return JNI_FALSE;
    }

    // Store the Java VM pointer for potential thread attachment (for logging)
    env->GetJavaVM(&globalVm);

    // Save the input buffer address for packet data reuse
    inputBuffer = reinterpret_cast<uint8_t*>(bufferAddr);

    // Prepare global references and method IDs for the Java callbacks
    if (listener != nullptr) {
        jclass listenerCls = env->GetObjectClass(listener);
        midOnFirstFrame = env->GetMethodID(listenerCls, "onFirstFrameDecoded", "()V");
        midOnIDRNeeded  = env->GetMethodID(listenerCls, "onIDRFrameNeeded", "()V");
        globalDecoderListener = env->NewGlobalRef(listener);
    }
    if (logCb != nullptr) {
        jclass logCls = env->GetObjectClass(logCb);
        midOnLog = env->GetMethodID(logCls, "onLog", "(ILjava/lang/String;)V");
        globalLogCallback = env->NewGlobalRef(logCb);
    }
    if (surface != nullptr) {
        globalSurfaceRef = env->NewGlobalRef(surface);
    }

    // Register all codecs (necessary for older FFmpeg; no-op on newer versions)
    avcodec_register_all();

    // Select the appropriate codec (hardware or software) based on parameters
    const AVCodec* decoder = nullptr;
    bool useHW = (!useSoftware && surface != nullptr);  // true if we attempt hardware decoding
    if (codecType == FFmpegManager::CODEC_H264 /*0*/ || codecType == 0) {
        if (useHW) {
            decoder = avcodec_find_decoder_by_name("h264_mediacodec");
            if (!decoder && enableFallback) {
                decoder = avcodec_find_decoder(AV_CODEC_ID_H264);
                useHW = false;
            }
        } else {
            decoder = avcodec_find_decoder(AV_CODEC_ID_H264);
        }
    } else if (codecType == FFmpegManager::CODEC_HEVC /*1*/ || codecType == 1) {
        if (useHW) {
            decoder = avcodec_find_decoder_by_name("hevc_mediacodec");
            if (!decoder && enableFallback) {
                decoder = avcodec_find_decoder(AV_CODEC_ID_HEVC);
                useHW = false;
            }
        } else {
            decoder = avcodec_find_decoder(AV_CODEC_ID_HEVC);
        }
    }
    if (!decoder) {
        // Codec not found (unsupported codec or missing FFmpeg components)
        return JNI_FALSE;
    }

    // Allocate the codec context
    codecCtx = avcodec_alloc_context3(decoder);
    if (!codecCtx) {
        return JNI_FALSE;
    }
    // Set known stream parameters (if available)
    codecCtx->width  = width;
    codecCtx->height = height;
    // (Additional parameters like extradata could be set here if needed)

    // If using hardware acceleration, create the MediaCodec device context
    if (useHW) { // ToDo check
        if (av_hwdevice_ctx_create(&hwDeviceCtx, AV_HWDEVICE_TYPE_MEDIACODEC,
                                   NULL, NULL, 0) < 0) {
            // Failed to create a MediaCodec device – fallback to software if allowed
            if (enableFallback) {
                useHW = false;
            } else {
                // No fallback allowed: cleanup and return failure
                avcodec_free_context(&codecCtx);
                if (globalSurfaceRef) { env->DeleteGlobalRef(globalSurfaceRef); globalSurfaceRef = nullptr; }
                if (globalDecoderListener) { env->DeleteGlobalRef(globalDecoderListener); globalDecoderListener = nullptr; }
                if (globalLogCallback) { env->DeleteGlobalRef(globalLogCallback); globalLogCallback = nullptr; }
                return JNI_FALSE;
            }
        }
        if (useHW && hwDeviceCtx) {
            // Attach the Surface to the HW device context for MediaCodec output
            AVHWDeviceContext *deviceCtx = (AVHWDeviceContext*) hwDeviceCtx->data;
            AVMediaCodecDeviceContext *mcCtx = (AVMediaCodecDeviceContext*) deviceCtx->hwctx;
            mcCtx->surface = globalSurfaceRef;  // set the Java Surface handle for decoder&#8203;:contentReference[oaicite:0]{index=0}
            // Use the hardware device context for the codec
            codecCtx->hw_device_ctx = av_buffer_ref(hwDeviceCtx);
            // Ensure the decoder selects AV_PIX_FMT_MEDIACODEC for output frames
            codecCtx->get_format = getHWPixelFormat;
        }
    }

    // Open the codec (with or without hardware acceleration)
    if (avcodec_open2(codecCtx, decoder, NULL) < 0) {
        // If opening the hardware decoder failed and fallback is enabled, retry with software decoder
        if (useHW && enableFallback) {
            if (codecType == 0) decoder = avcodec_find_decoder(AV_CODEC_ID_H264);
            else                decoder = avcodec_find_decoder(AV_CODEC_ID_HEVC);
            // Release hardware resources
            if (codecCtx->hw_device_ctx) {
                av_buffer_unref(&codecCtx->hw_device_ctx);
            }
            if (hwDeviceCtx) {
                av_buffer_unref(&hwDeviceCtx);
                hwDeviceCtx = nullptr;
            }
            useHW = false;
            if (!decoder || avcodec_open2(codecCtx, decoder, NULL) < 0) {
                // Software decoder also failed
                avcodec_free_context(&codecCtx);
                // Clean up global refs
                if (globalSurfaceRef) { env->DeleteGlobalRef(globalSurfaceRef); globalSurfaceRef = nullptr; }
                if (globalDecoderListener) { env->DeleteGlobalRef(globalDecoderListener); globalDecoderListener = nullptr; }
                if (globalLogCallback) { env->DeleteGlobalRef(globalLogCallback); globalLogCallback = nullptr; }
                return JNI_FALSE;
            }
        } else {
            // Decoder open failed (no fallback or not a recoverable error)
            avcodec_free_context(&codecCtx);
            if (hwDeviceCtx) { av_buffer_unref(&hwDeviceCtx); hwDeviceCtx = nullptr; }
            if (globalSurfaceRef) { env->DeleteGlobalRef(globalSurfaceRef); globalSurfaceRef = nullptr; }
            if (globalDecoderListener) { env->DeleteGlobalRef(globalDecoderListener); globalDecoderListener = nullptr; }
            if (globalLogCallback) { env->DeleteGlobalRef(globalLogCallback); globalLogCallback = nullptr; }
            return JNI_FALSE;
        }
    }

    // Allocate an AVPacket and AVFrame for reuse during decoding
    packet = av_packet_alloc();
    frame  = av_frame_alloc();
    if (!packet || !frame) {
        // Allocation failed; release any resources and return false
        if (packet) { av_packet_free(&packet); }
        if (frame)  { av_frame_free(&frame); }
        // Clean up FFmpeg context and global references
        avcodec_free_context(&codecCtx);
        if (hwDeviceCtx) { av_buffer_unref(&hwDeviceCtx); hwDeviceCtx = nullptr; }
        if (globalSurfaceRef) { env->DeleteGlobalRef(globalSurfaceRef); globalSurfaceRef = nullptr; }
        if (globalDecoderListener) { env->DeleteGlobalRef(globalDecoderListener); globalDecoderListener = nullptr; }
        if (globalLogCallback) { env->DeleteGlobalRef(globalLogCallback); globalLogCallback = nullptr; }
        return JNI_FALSE;
    }

    // Set up FFmpeg logging callback (if provided by user)
    if (globalLogCallback != nullptr && midOnLog != nullptr) {
        av_log_set_callback(LogCallbackFunction);
        // We will pass JNIEnv* as the log context pointer when calling av_log manually.
        // (FFmpeg internal logs may pass different context; this simplistic approach
        // covers logs we explicitly send. For full thread-safe logging, a JavaVM attach
        // would be used, omitted here for brevity.)
        av_log_set_level(AV_LOG_INFO);  // Set desired log level (INFO or verbose as needed)
    }

    // Initialize state flags
    firstFrameDecoded = false;
    failCount = 0;
    return JNI_TRUE;
}

JNIEXPORT jlong JNICALL Java_com_grill_placebo_FFmpegManager_decodeFrame
  (JNIEnv *env, jobject thiz, jboolean isKeyFrame, jint limit) {
    if (codecCtx == nullptr) {
        // Decoder not initialized or already disposed
        return 0;
    }

    // Prepare the AVPacket with input data from the shared BytePointer buffer.
    // The `limit` parameter is used here as the number of bytes of data in the buffer.
    int dataSize = limit;
    av_packet_unref(packet);  // reset packet to blank state
    packet->data = inputBuffer;
    packet->size = dataSize;
    packet->flags = 0;
    if (isKeyFrame) {
        packet->flags |= AV_PKT_FLAG_KEY;
    }

    // Send the packet to the decoder
    int ret = avcodec_send_packet(codecCtx, packet);
    if (ret == AVERROR(EAGAIN)) {
        // The decoder's internal buffers are full and require reading output first.
        // Retrieve one frame from the decoder (from a previous packet) before sending new data.
        ret = avcodec_receive_frame(codecCtx, frame);
        if (ret == 0) {
            // We got a frame from previous input – output it before sending new packet.
            jlong framePtr = reinterpret_cast<jlong>(frame);
            if (!firstFrameDecoded) {
                firstFrameDecoded = true;
                if (globalDecoderListener != nullptr && midOnFirstFrame != nullptr) {
                    env->CallVoidMethod(globalDecoderListener, midOnFirstFrame);
                }
            }
            // We have not sent the current packet yet; try again on next call.
            // (The current input packet remains to be sent in the next decode call.)
            return framePtr;
        }
        // If no frame was available (ret == AVERROR(EAGAIN) again), we proceed to send the packet.
        ret = avcodec_send_packet(codecCtx, packet);
    }
    if (ret < 0) {
        // Failed to send packet (decoder in bad state or other error)
        return 0;
    }

    // Try to receive a decoded frame from the decoder
    ret = avcodec_receive_frame(codecCtx, frame);
    if (ret < 0) {
        if (ret == AVERROR(EAGAIN)) {
            // No frame available yet (needs more data for output)
            if (!firstFrameDecoded && !isKeyFrame) {
                // If we haven't decoded anything yet and this packet was not a key frame,
                // increase the failure count and possibly signal the need for a key frame.
                failCount++;
                if (globalDecoderListener != nullptr && midOnIDRNeeded != nullptr && failCount >= limit) {
                    env->CallVoidMethod(globalDecoderListener, midOnIDRNeeded);
                    failCount = 0;  // reset the counter after notifying
                }
            }
            return 0;  // No frame output for this call
        } else {
            // A decoding error occurred (ret is AVERROR(EINVAL) or other fatal error)
            return 0;
        }
    }

    // We have successfully received a frame
    firstFrameDecoded = true;
    failCount = 0;
    if (globalDecoderListener != nullptr && midOnFirstFrame != nullptr) {
        // If this was the first decoded frame, notify the listener (only once)
        static bool notifiedFirst = false;
        if (!notifiedFirst) {
            env->CallVoidMethod(globalDecoderListener, midOnFirstFrame);
            notifiedFirst = true;
        }
    }
    // Return the native handle (pointer) to the AVFrame
    // (The frame is owned by the decoder; use or copy its data before next decode call.)
    return reinterpret_cast<jlong>(frame);
}

JNIEXPORT void JNICALL Java_com_grill_placebo_FFmpegManager_disposeDecoder
  (JNIEnv *env, jobject thiz) {
    if (codecCtx == nullptr) {
        // Decoder already disposed or not initialized
        return;
    }
    // Close the codec and free the codec context
    avcodec_free_context(&codecCtx);
    codecCtx = nullptr;
    // Free the AVFrame and AVPacket
    if (frame) {
        av_frame_free(&frame);
        frame = nullptr;
    }
    if (packet) {
        av_packet_free(&packet);
        packet = nullptr;
    }
    // Free the hardware device context (if any)
    if (hwDeviceCtx) {
        av_buffer_unref(&hwDeviceCtx);
        hwDeviceCtx = nullptr;
    }
    // Release global references to Java objects to avoid memory leaks
    if (globalSurfaceRef) {
        env->DeleteGlobalRef(globalSurfaceRef);
        globalSurfaceRef = nullptr;
    }
    if (globalDecoderListener) {
        env->DeleteGlobalRef(globalDecoderListener);
        globalDecoderListener = nullptr;
    }
    if (globalLogCallback) {
        env->DeleteGlobalRef(globalLogCallback);
        globalLogCallback = nullptr;
    }
    // (Optional) Restore FFmpeg log callback to default if it was overridden
    av_log_set_callback(av_log_default_callback);  // reset to default logging
    // Reset state flags
    firstFrameDecoded = false;
    failCount = 0;
}
