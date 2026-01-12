package com.grill.placebo;

public final class FFmpegD3D11VAFrames {

    static {
        // load your JNI library here
        // System.loadLibrary("psplay_native");
    }

    private FFmpegD3D11VAFrames() {
    }

    /**
     * Creates an AVHWFramesContext (AVBufferRef*) for D3D11VA whose decoded textures
     * are allocated with D3D11_BIND_SHADER_RESOURCE, so you can CreateShaderResourceView on them.
     *
     * @param hwDeviceCtxPtr  pointer to AVBufferRef (hw_device_ctx) (i.e. AVBufferRef*)
     * @param swFormat        AVPixelFormat (e.g. AV_PIX_FMT_NV12 / AV_PIX_FMT_P010)
     * @param width           coded width (you can pass aligned or unaligned; see notes below)
     * @param height          coded height
     * @param initialPoolSize decoder surface pool size (e.g. 20 or 32)
     * @param forceSrvBind    true -> add D3D11_BIND_SHADER_RESOURCE
     * @return pointer to AVBufferRef (AVBufferRef*) of the created hw_frames_ctx, or 0 on failure/non-windows
     */
    public static native long nativeCreateD3D11VAFramesCtx(
            long hwDeviceCtxPtr,
            int swFormat,
            int width,
            int height,
            int initialPoolSize,
            boolean forceSrvBind
    );

    /**
     * av_buffer_unref() for an AVBufferRef* pointer returned above.
     */
    public static native void nativeUnrefBuffer(long avBufferRefPtr);
}

