package com.grill.placebo;

import android.view.Surface;

public class FFmpegManager {
    // Codec type constants for H.264 and HEVC
    public static final int CODEC_H264 = 0;
    public static final int CODEC_HEVC = 1;

    private boolean initialized = false;  // Tracks if decoder is initialized

    /**
     * Listener for decoder events (to be implemented by caller).
     */
    public interface DecoderListener {
        /**
         * Called when the first frame has been decoded successfully.
         */
        void onFirstFrameDecoded();

        /**
         * Called when the decoder requires an IDR (key) frame to continue.
         */
        void onIDRFrameNeeded();
    }

    /**
     * Callback interface for forwarding FFmpeg log messages to Java.
     */
    public interface LogCallback {
        /**
         * Receives log messages from native decoder (level follows AV log levels).
         */
        void onLog(int level, String message);
    }

    // Native method declarations
    private native boolean init(Surface surface,
                                long bufferAddr,
                                DecoderListener listener,
                                int codecType, int width, int height,
                                boolean useSoftware, boolean enableFallback,
                                LogCallback logCb, int cpuCount);

    private native long decodeFrame(boolean isKeyFrame, int limit);

    private native void disposeDecoder();

    // Optionally, we can wrap native calls to enforce lifecycle checks in Java as well:
    public boolean initializeDecoder(final Surface surface, final long bufferAddr,
                                     final DecoderListener listener, final int codecType,
                                     final int width, final int height,
                                     final boolean useSoftware, final boolean enableFallback,
                                     final LogCallback logCb, final int cpuCount) {
        if (this.initialized) {
            this.disposeDecoder();
        }
        final boolean ok = this.init(surface, bufferAddr, listener, codecType, width, height,
                useSoftware, enableFallback, logCb, cpuCount);
        this.initialized = ok;
        return ok;
    }


    public long decodeNextFrame(final boolean isKeyFrame, final int limit) {
        if (!this.initialized) {
            throw new IllegalStateException("Decoder not initialized");
        }
        return this.decodeFrame(isKeyFrame, limit);
    }

    public void releaseDecoder() {
        if (!this.initialized) {
            return;
        }
        this.disposeDecoder();
        this.initialized = false;
    }
}
