package com.grill.placebo;

public final class D3D11VANative {

    static {
        // Make sure this matches your built JNI library name
        System.loadLibrary("grill_placebo");
    }

    private D3D11VANative() {
    }

    /**
     * Fill the AVD3D11VADeviceContext inside hwdev with real struct assignments in native code.
     *
     * @param hwdev        AVBufferRef returned by av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_D3D11VA)
     * @param lockCtxPtr   your contextLockPointer
     * @param d3dDevicePtr ID3D11Device*
     * @param immCtxPtr    ID3D11DeviceContext* (immediate context)
     * @param lockFnPtr    function pointer returned by your nativeGetLockFnPtr()
     * @param unlockFnPtr  function pointer returned by your nativeGetUnlockFnPtr()
     * @return 0 on success, negative on error
     */
    /*public static int fillHwDeviceContextPointers(
            AVBufferRef hwdev,
            long lockCtxPtr,
            long d3dDevicePtr,
            long immCtxPtr,
            long lockFnPtr,
            long unlockFnPtr
    ) {
        if (hwdev == null || hwdev.address() == 0) return -1;

        // This ensures hwdev.data() is valid and points to AVHWDeviceContext
        AVHWDeviceContext base = new AVHWDeviceContext(hwdev.data());
        Pointer hwctx = base.hwctx();
        if (hwctx == null || hwctx.address() == 0) return -2;

        return nativeFillD3D11VADeviceContext(
                hwdev.address(),
                lockCtxPtr,
                d3dDevicePtr,
                immCtxPtr,
                lockFnPtr,
                unlockFnPtr
        );
    }*/

    /**
     * Fill AVD3D11VAFramesContext.BindFlags/MiscFlags (and texture/texture_infos null)
     * via real FFmpeg struct in native code.
     * <p>
     * Call this after you set up AVHWFramesContext basics in Java, before av_hwframe_ctx_init().
     *
     * @param framesRef AVBufferRef returned by av_hwframe_ctx_alloc(hwdev)
     * @param bindFlags e.g. D3D11_BIND_DECODER | D3D11_BIND_SHADER_RESOURCE
     * @param miscFlags usually 0
     * @return 0 on success, negative on error
     */
    /*public static int fillHwFramesContextPointers(
            AVBufferRef framesRef,
            int bindFlags,
            int miscFlags
    ) {
        if (framesRef == null || framesRef.address() == 0) return -1;

        AVHWFramesContext frames = new AVHWFramesContext(framesRef.data());
        Pointer hwctx = frames.hwctx();
        if (hwctx == null || hwctx.address() == 0) return -2;

        return nativeFillD3D11VAFramesContext(framesRef.address(), bindFlags, miscFlags);
    }*/
    public static native int nativeFillD3D11VADeviceContext(
            long hwdevBufRefPtr,
            long lockCtxPtr,
            long d3dDevicePtr,
            long immCtxPtr,
            long lockFnPtr,
            long unlockFnPtr
    );

    public static native int nativeFillD3D11VAFramesContext(
            long framesBufRefPtr,
            int bindFlags,
            int miscFlags
    );
}