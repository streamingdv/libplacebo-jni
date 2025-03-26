package com.grill.placebo;

import android.view.Surface;

public class PlaceboManager {

    public interface LogCallback {
        void onLog(int level, String message);
    }

    public static final class LogLevel {
        public final static int PL_LOG_NONE = 0;
        public final static int PL_LOG_FATAL = 1;   // results in total loss of function of a major component
        public final static int PL_LOG_ERR = 2;     // serious error; may result in degraded function
        public final static int PL_LOG_WARN = 3;    // warning; potentially bad, probably user-relevant
        public final static int PL_LOG_INFO = 4;    // informational message, also potentially harmless errors
        public final static int PL_LOG_DEBUG = 5;   // verbose debug message, informational
        public final static int PL_LOG_TRACE = 6;   // very noisy trace of activity, usually benign
        public final static int PL_LOG_ALL = LogLevel.PL_LOG_TRACE;
    }

    public static final int API_VERSION = 342;

    static {
        System.loadLibrary("placebo"); // Load your JNI library
    }

    /**
     * Initializes the OpenGL renderer and swapchain using the specified EGL context and display.
     *
     * @param logHandle  A native handle to the {@code pl_log} instance.
     * @param eglDisplay The EGL display handle used for rendering.
     * @param eglContext The EGL context handle associated with the OpenGL ES environment.
     * @param width      The desired initial width of the rendering surface.
     * @param height     The desired initial height of the rendering surface.
     * @param useHDR     {@code true} to initialize the renderer with HDR (ST2084/PQ), {@code false} for SDR (Gamma 2.2).
     * @return {@code true} if the renderer and swapchain were successfully initialized, {@code false} otherwise.
     */
    public boolean openGLCreate(final long logHandle, final long eglDisplay, final long eglContext, final int width, final int height, final boolean useHDR) {
        final int colorTransfer = useHDR ? 6 : 1; // 6 = ST2084 (PQ) for HDR, 1 = Gamma 2.2 for SDR
        return this.plOpenGLCreate(logHandle, eglDisplay, eglContext, width, height, colorTransfer);
    }

    /**
     * Renders a frame using the provided EGLImage and libplacebo renderer.
     *
     * @param eglImageHandle The EGLImage texture handle to be rendered.
     * @param width          The width of the frame to render.
     * @param height         The height of the frame to render.
     * @param useHDR         {@code true} to render in HDR mode (ST2084/PQ), {@code false} for SDR (Gamma 2.2).
     * @return {@code true} if the frame was rendered and submitted successfully, {@code false} otherwise.
     */
    public boolean renderFrame(final long eglImageHandle, final int width, final int height, final boolean useHDR) {
        final int colorTransfer = useHDR ? 6 : 1; // 6 = ST2084 (PQ) for HDR, 1 = Gamma 2.2 for SDR
        final int colorRange = 2; // Limited range by default
        return this.renderEGLImage(eglImageHandle, width, height, colorTransfer, colorRange);
    }

    /**
     * Resizes the current libplacebo OpenGL swapchain to the specified width and height.
     *
     * @param width  The new width of the swapchain.
     * @param height The new height of the swapchain.
     * @return {@code true} if the resize succeeded, {@code false} otherwise.
     */
    public native boolean plSwapchainResize(int width, int height);


    /**********************/
    /*** native methods ***/
    /**********************/

    private native boolean plOpenGLCreate(long logHandle, long eglDisplay, long eglContext, int width, int height, int colorTransfer);

    private native boolean renderEGLImage(long eglImageHandle, int width, int height, int colorTransfer, int colorRange);

    /**
     * Creates a logger with the log callback
     *
     * @param apiVersion the API version
     * @param logLevel   the log level
     * @param callback   the callback
     * @return the log handle, pl_log
     */
    public native long plLogCreate(int apiVersion, int logLevel, LogCallback callback);

    /**
     * Binds an Android {@link android.view.Surface} to a given FFmpeg {@link org.bytedeco.ffmpeg.avcodec.AVCodecContext}
     * using the MediaCodec hardware acceleration backend.
     *
     * <p>This method allocates an internal {@link org.bytedeco.ffmpeg.avutil.AVMediaCodecContext} and binds the provided
     * {@link android.view.Surface} to it. Then, {@code av_mediacodec_default_init()} is called to initialize the surface
     * binding inside FFmpeg. This is required to enable zero-copy MediaCodec decoding via hardware acceleration.</p>
     *
     * <p>This should be called before {@code avcodec_open2()} and only once per decoder instance. The native surface
     * must remain valid and unchanged during decoding.</p>
     *
     * @param avCodecContextPtr the native pointer (as a long) to the {@link org.bytedeco.ffmpeg.avcodec.AVCodecContext}
     *                          used for the decoder instance.
     * @param surface           the {@link android.view.Surface} to bind to the MediaCodec hardware context.
     * @return {@code true} if the surface was successfully bound to the codec context, {@code false} otherwise.
     * @see org.bytedeco.ffmpeg.global.avcodec#av_mediacodec_alloc_context
     * @see org.bytedeco.ffmpeg.global.avcodec#av_mediacodec_default_init
     * @see org.bytedeco.ffmpeg.global.avcodec#avcodec_open2
     */
    public static native boolean bindSurfaceToHwDevice(long avCodecContextPtr, Surface surface);

    /**
     * Renders the given AVFrame using libplacebo with the globally initialized renderer and swapchain.
     *
     * @param avframeHandle native pointer to the AVFrame
     * @return true if rendering succeeded, false otherwise
     */
    public static native boolean plRenderAvFrame(long avframeHandle);

    /**
     * Releases the native MediaCodec context associated with the provided FFmpeg {@link AVCodecContext}.
     *
     * <p>This method should be called after hardware decoding is complete, to free resources
     * initialized with {@code av_mediacodec_default_init()}.</p>
     *
     * @param codecContextHandle the pointer to the native {@link org.bytedeco.ffmpeg.avcodec.AVCodecContext}
     *                           instance that was initialized with MediaCodec support.
     * @see org.bytedeco.ffmpeg.global.avcodec#av_mediacodec_default_free
     */
    public static native void releaseMediaCodecContext(long codecContextHandle);

    /**
     * Renders a decoded AVFrame directly to the screen and releases the MediaCodec buffer.
     * <p>
     * This method assumes the frame was decoded using MediaCodec and the buffer reference is
     * stored in {@code AVFrame.data[3]}. It releases the buffer with render=true to display it.
     * </p>
     *
     * @param avframeHandle native pointer to an {@code AVFrame} structure
     * @return {@code true} if the frame was valid and the buffer was released, {@code false} otherwise
     */
    public native boolean plRenderAvFrameDirect(long avframeHandle);

    /**
     * Binds the given Java Surface to an existing AVBufferRef for MediaCodec (AV_HWDEVICE_TYPE_MEDIACODEC).
     * You must call av_hwdevice_ctx_init() afterward.
     *
     * @param deviceRefHandle native pointer to AVBufferRef (as long)
     * @param surface         Java Surface to be bound to MediaCodec device
     * @return true if surface was set successfully, false on error
     */
    public static native boolean bindSurfaceToDeviceRef(long deviceRefHandle, Surface surface);

    /**
     * Destroys a logger with the log callback
     *
     * @param logHandle the log handle to be deleted, pl_log
     */
    public native void plLogDestroy(long logHandle);

    public native void cleanupResources();
}
