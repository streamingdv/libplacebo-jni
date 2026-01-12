package com.grill.placebo;

public final class D3D11VANative {
    private D3D11VANative() {
    }

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