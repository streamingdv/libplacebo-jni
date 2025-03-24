package com.grill.libplacebo;

import android.opengl.EGLContext;
import android.opengl.EGLDisplay;
import android.view.Surface;

public class PlaceboManager {

    public interface LogCallback {
        void onLog(int level, String message);
    }

    static {
        System.loadLibrary("placebo"); // Load your JNI library
    }

    /**
     * Create EGL Image from SurfaceTexture
     */
    public long createEGLImage(final EGLDisplay eglDisplay, final EGLContext eglContext, final Surface surface) {
        return this.createEGLImageFromSurface(
                eglDisplay.getNativeHandle(),
                eglContext.getNativeHandle(),
                surface
        );
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
     * @return A native handle (as {@code long}) to the initialized {@code pl_renderer}, or {@code 0} on failure.
     */
    public long openGLCreate(final long logHandle, final long eglDisplay, final long eglContext, final int width, final int height, final boolean useHDR) {
        final int colorTransfer = useHDR ? 6 : 1; // 6 = ST2084 (PQ) for HDR, 1 = Gamma 2.2 for SDR
        return this.plOpenGLCreate(logHandle, eglDisplay, eglContext, width, height, colorTransfer);
    }

    /**
     * Renders a frame using the provided EGLImage and libplacebo renderer.
     *
     * @param rendererHandle A native handle to the {@code pl_renderer} instance.
     * @param eglImageHandle The EGLImage texture handle to be rendered.
     * @param width          The width of the frame to render.
     * @param height         The height of the frame to render.
     * @param useHDR         {@code true} to render in HDR mode (ST2084/PQ), {@code false} for SDR (Gamma 2.2).
     * @return {@code true} if the frame was rendered and submitted successfully, {@code false} otherwise.
     */
    public boolean renderFrame(final long rendererHandle, final long eglImageHandle, final int width, final int height, final boolean useHDR) {
        final int colorTransfer = useHDR ? 6 : 1; // 6 = ST2084 (PQ) for HDR, 1 = Gamma 2.2 for SDR
        final int colorRange = 2; // Limited range by default
        return this.renderEGLImage(rendererHandle, eglImageHandle, width, height, colorTransfer, colorRange);
    }

    /**
     * Resizes the current libplacebo OpenGL swapchain to the specified width and height.
     *
     * @param swapchainHandle A native handle (jlong) to the {@code pl_swapchain} instance.
     * @param width           The new width of the swapchain.
     * @param height          The new height of the swapchain.
     * @return {@code true} if the resize succeeded, {@code false} otherwise.
     */
    public native boolean plSwapchainResize(long swapchainHandle, int width, int height);


    /**********************/
    /*** native methods ***/
    /**********************/

    private native long createEGLImageFromSurface(long eglDisplay, long eglContext, Surface surface);

    private native long plOpenGLCreate(long logHandle, long eglDisplay, long eglContext, int width, int height, int colorTransfer);

    private native boolean renderEGLImage(long rendererHandle, long eglImageHandle, int width, int height, int colorTransfer, int colorRange);

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
     * Destroys a logger with the log callback
     *
     * @param logHandle the log handle to be deleted, pl_log
     */
    public native void plLogDestroy(long logHandle);

    private native void cleanupResources();
}
