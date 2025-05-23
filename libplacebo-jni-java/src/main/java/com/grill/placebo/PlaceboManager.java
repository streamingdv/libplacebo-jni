package com.grill.placebo;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.nio.file.AccessDeniedException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.util.NoSuchElementException;

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

    public static class VulkanExtensions {
        public static final String VK_KHR_SURFACE_EXTENSION = "VK_KHR_SURFACE_EXTENSION_NAME";
        public static final String VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION = "VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME";
        public static final String VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION = "VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME";
        public static final String VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION = "VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION_NAME";
        public static final String VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION = "VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME";

        public static String[] getExtensions() {
            return new String[]{
                    VulkanExtensions.VK_KHR_SURFACE_EXTENSION,
                    VulkanExtensions.VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION,
                    VulkanExtensions.VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION,
                    VulkanExtensions.VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION,
                    VulkanExtensions.VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION
            };
        }
    }

    public static final int API_VERSION = 342;

    /**
     * Creates a logger with the log callback
     *
     * @param apiVersion the API version
     * @param logLevel   the log level
     * @param callback   the callback
     * @return the log handle, pl_log
     */
    public native long plLogCreate(int apiVersion, int logLevel, LogCallback callback);

    public native long plLogCreate2(int apiVersion, int logLevel);

    /**
     * Destroys a logger with the log callback
     *
     * @param logHandle the log handle to be deleted, pl_log
     */
    public native void plLogDestroy(long logHandle);

    /**
     * Get the underlying windowing system. Please note: this is only supported on Unix systems
     *
     * @return the underlying windowing system or "unknown"
     */
    public native String getWindowingSystem();

    /**
     * Creates a Vulkan instance
     *
     * @param plLog               the logHandle
     * @param windowingSystemType the windowing system type. Only used on Unix systems.
     *                            1 for xcb and 2 for wayland.
     * @return the handle to the vk instance, placebo_vk_inst
     */
    public native long plVkInstCreate(long plLog, int windowingSystemType);

    /**
     * Creates a Vulkan instance with default surface extension assuming default windowing system type.
     * On Windows it is VK_KHR_WIN32_SURFACE_EXTENSION_NAME and on Unix systems VK_KHR_XCB_SURFACE_EXTENSION_NAME
     *
     * @param plLog the logHandle
     * @return the handle to the vk instance, placebo_vk_inst
     */
    public long plVkInstCreate(final long plLog) {
        return this.plVkInstCreate(plLog, 0);
    }

    /**
     * Destroys a vl inst
     *
     * @param vkInst the vk inst to be deleted
     */
    public native void plVkInstDestroy(long vkInst);

    /**
     * Creates a vulkan device chosen by libplacebo
     *
     * @param plLog          the log handle
     * @param vkInst         the vk inst handle
     * @param surface        the surface handle
     * @param hwAccelBackend true if acting as the decoder backend; a physical device with Vulkan video support is required
     * @return the vulkan device handle or <= 0 if not successful
     */
    public native long plVulkanCreate(long plLog, long vkInst, long surface, boolean hwAccelBackend);

    /**
     * Creates a vulkan device and check if the device meets our requirements
     *
     * @param plLog          the log handle
     * @param vkInst         the vk inst handle
     * @param surface        the surface handle
     * @param decoder        0 for h264 1 for h265
     * @param hdr            true if hdr is enabled, false otherwise
     * @param hwAccelBackend true if acting as the decoder backend; a physical device with Vulkan video support is required
     * @return the vulkan device handle or <= 0 if not successful
     */
    public native long plVulkanCreateForBestDevice(long plLog, long vkInst, long surface, int decoder, boolean hdr, boolean hwAccelBackend);

    /**
     * Init the vulkan decoding queue
     *
     * @param vk the vulkan device handle
     * @return true if successful, false otherwise
     */
    public native boolean plInitQueue(long vk);

    /**
     * Destroys the vulkan device
     *
     * @param vk the vulkan device handle
     */
    public native void plVulkanDestroy(long vk);

    /**
     * Get device handle
     *
     * @param vk the vulkan device handle
     * @return the vk device handle
     */
    public native long plGetVkDevice(long vk);

    /**
     * Get Physical device handle
     *
     * @param vk the vulkan device handle
     * @return the vk physical device handle
     */
    public native long plGetVkPhysicalDevice(long vk);

    /**
     * After initialization fill in all necessary function pointers
     *
     * @param vkInst the vk inst handle
     * @return true if all function pointers could be initialized correctly, false otherwise
     */
    public native boolean plInitFunctionPointers(long vkInst);

    /**
     * Create placebo cache
     *
     * @param plLog   the log handle
     * @param maxSize the max cache size. Negative number are not allowed
     * @return the cache handle
     */
    public native long plCacheCreate(long plLog, int maxSize);

    /**
     * Destroys the cache
     *
     * @param plCache the cache handle
     */
    public native void plCacheDestroy(long plCache);

    /**
     * Sets the gpu cache
     *
     * @param vk    the vulkan device handle
     * @param cache the cache handle
     */
    public native void plGpuSetCache(long vk, long cache);

    /**
     * Loads the shader cache file
     *
     * @param cache    the cache handle
     * @param filePath the file path to the shader cache file
     */
    public native void plCacheLoadFile(long cache, String filePath);

    /**
     * Saved the shader cache file
     *
     * @param cache    the cache handle
     * @param filePath the file path to the shader cache file
     */
    public native void plCacheSaveFile(long cache, String filePath);

    /**
     * Gets the handle to the vulkan instance
     *
     * @param vkInst the vk inst handle
     * @return the vulkan instance
     */
    public native long plGetVkInstance(long vkInst);

    /**
     * Gets the handle to the vkCreateWin32SurfaceKHR function
     *
     * @return the handle to the vkCreateWin32SurfaceKHR function
     */
    public native long plGetWin32SurfaceFunctionPointer();

    /**
     * Gets the handle to the vkCreateMetalSurfaceEXT function
     *
     * @return the handle to the vkCreateMetalSurfaceEXT function
     */
    public native long plGetMetalSurfaceEXT();

    /**
     * Gets the handle to the vkCreateXcbSurfaceKHR function
     *
     * @return the handle to the vkCreateXcbSurfaceKHR function
     */
    public native long plGetXcbSurfaceFunctionPointer();

    /**
     * Gets the handle to the vkCreateWaylandSurfaceKHR function
     *
     * @return the handle to the vkCreateWaylandSurfaceKHR function
     */
    public native long plGetWaylandSurfaceFunctionPointer();

    /**
     * Destroys and releases the native surface
     *
     * @param vkInst  the vk inst handle
     * @param surface the surface handle
     */
    public native void plDestroySurface(long vkInst, long surface);

    /**
     * Creates the swapchain and returns the handle
     *
     * @param vk               the vulkan device handle
     * @param surface          the surface handle
     * @param vkPresentModeKHR the presentation mode supported for a surface (must be either be a value from 0-3)
     * @return the swapchain handle
     */
    public native long plCreateSwapchain(long vk, long surface, int vkPresentModeKHR);

    /**
     * Creates the swapchain with the best most suitable present mode and returns the handle
     *
     * @param vk           the vulkan device handle
     * @param surface      the surface handle
     * @param vsyncEnabled true if vsync is enabled
     * @return the swapchain handle
     */
    public native long plCreateSwapchainWithBestPresentMode(long vk, long surface, boolean vsyncEnabled);

    /**
     * Destroys and releases the swapchain
     *
     * @param swapchain the swapchain handle
     */
    public native void plDestroySwapchain(long swapchain);

    /**
     * Creates the renderer and retunrs the handle
     *
     * @param vk    the vulkan device handle
     * @param plLog the log handle
     * @return the renderer handle
     */
    public native long plCreateRenderer(long vk, long plLog);

    /**
     * Destroys and releases the renderer
     *
     * @param renderer the renderer handle
     */
    public native void plDestroyRenderer(long renderer);

    /**
     * Resizes the swapchain
     *
     * @param swapchain the swapchain handle
     * @param width     the new width
     * @param height    the new height
     * @return true if successful, false otherwise
     */
    public native boolean plSwapchainResize(long swapchain, int width, int height);

    /**
     * Resizes the swapchain
     *
     * @param swapchain       the swapchain handle
     * @param intBufferWidth  the int buffer handle for width
     * @param intBufferHeight the int buffer handle for height
     * @return true if successful, false otherwise
     */
    public native boolean plSwapchainResizeWithBuffer(long swapchain, long intBufferWidth, long intBufferHeight);

    /**
     * Waits until the renderer is ready to start rendering the next frame.
     * <p>
     * This should be called before rendering to ensure that the swapchain is ready,
     * especially when using frame pacing or adaptive frame scheduling.
     * </p>
     * On non-Windows platforms, this also waits for any queued frame presentations
     * to complete before proceeding.
     *
     * @param vk        Pointer to the initialized {@code pl_vulkan} instance.
     * @param swapchain Pointer to the {@code pl_swapchain}.
     * @param width     The drawable width of the window in pixels.
     * @param height    The drawable height of the window in pixels.
     * @return {@code true} if rendering can proceed, {@code false} if the GPU is in a failed state
     * or the swapchain could not be resized.
     */
    public native boolean plWaitToRender(long vk, long swapchain, int width, int height);

    /**
     * Sets the vulkan context from libplacebo to the hardware vulkan context of ffmpeg
     *
     * @param vulkan_hw_dev_ctx_handle the handle to the AVBufferRef
     * @param vkInst                   the vk inst handle
     * @return true when the libplacebo context could be set successfully
     */
    public native boolean plSetHwDeviceCtx(long vulkan_hw_dev_ctx_handle, long vk, long vkInst);

    /**
     * Activates fast rendering
     */
    public native void plActivateFastRendering();

    /**
     * Activates high quality rendering
     */
    public native void plActivateHighQualityRendering();

    /**
     * Activate default rendering
     */
    public native void plActivateDefaultRendering();

    /**
     * Set the rendering format
     *
     * @param format 0 for Normal, 1 for Stretched, 2 for Zoomed. (other values will be ignored)
     */
    public native void plSetRenderingFormat(int format);

    /**
     * Renders an avframe
     *
     * @param avframe   the handle to the avframe
     * @param vk        the vulkan device handle
     * @param swapchain the swapchain handle
     * @param renderer  the renderer handle
     * @return true if successfully rendered, false otherwise
     */
    public native boolean plRenderAvFrame(long avframe, long vk, long swapchain, long renderer);

    public native boolean plRenderAvFrame1(long avframe, long vk, long swapchain, long renderer);

    public native boolean plRenderAvFrame2(long avframe, long vk, long swapchain, long renderer);

    public native boolean plRenderAvFrame3(long avframe, long vk, long swapchain, long renderer);

    public native boolean plRenderAvFrame4(long avframe, long vk, long swapchain, long renderer);

    public native boolean plRenderAvFrame5(long avframe, long vk, long swapchain, long renderer);

    public native long getVkGetInstanceProcAddr();

    /**
     * Renders an avframe with UI overlay
     *
     * @param avframe   the handle to the avframe
     * @param vk        the vulkan device handle
     * @param swapchain the swapchain handle
     * @param renderer  the renderer handle
     * @param ui        the ui handle
     * @param width     the current window width
     * @param height    the current window height
     * @return true if successfully rendered, false otherwise
     */
    public native boolean plRenderAvFrameWithUi(long avframe, long vk, long swapchain, long renderer, long ui, int width, int height);


    public native boolean plRenderUiOnly(long vk, long swapchain, long renderer, long ui, int width, int height);

    /**
     * Cleanup swapchain context after rendering
     *
     * @param swapchain the swapchain handle
     */
    public native void plCleanupRendererContext(long swapchain);

    /**
     * Destroy the global saved textures
     *
     * @param vk the vulkan device handle
     */
    public native void plTextDestroy(long vk);

    /**
     * Creates and inits the nuklear ui stuff
     *
     * @param vk the vulkan device handle
     * @param locale the locale of how the font glyphs which should be used for the UI
     * @return the ui handle
     */
    public native long nkCreateUI(long vk, String locale);


    /**
     * Updated the UI state with the given values which should be taken into consideration the next rendering update
     */
    public native void nkUpdateUIState(
            boolean showTouchpad, boolean showPanel, boolean showPopup,
            boolean touchpadPressed, boolean panelPressed, boolean panelShowMicButton,
            boolean panelShowFullscreenButton, boolean panelMicButtonPressed, boolean panelMicButtonActive,
            boolean panelShareButtonPressed, boolean panelPsButtonPressed, boolean panelOptionsButtonPressed,
            boolean panelFullscreenButtonPressed, boolean panelFullscreenButtonActive, boolean panelCloseButtonPressed,
            String popupHeaderText, String popupPopupText, boolean popupShowCheckbox,
            String popupButtonLeft, String popupButtonRight, String popupCheckboxText,
            boolean popupCheckboxChecked, boolean popupCheckboxFocused, boolean popupLeftButtonPressed,
            boolean popupLeftButtonFocused, boolean popupRightButtonPressed, boolean popupRightButtonFocused,
            String contentNotStreamableText, boolean showContentNotStreamable
    );

    /**
     * Destroys the nuklear ui stuff
     *
     * @param ui the ui handle
     */
    public native void nkDestroyUI(long ui);

    /************************/
    /*** load lib methods ***/
    /************************/

    public static void loadNative(final File directory) throws IOException {
        PlaceboManager.loadNative(directory, true);
    }

    public static void loadNative(final File directory, final boolean allowArm) throws IOException {
        final String nativeLibraryName = PlaceboManager.getNativeLibraryName(allowArm);
        final InputStream source = PlaceboManager.class.getResourceAsStream("/native-binaries/" + nativeLibraryName);
        if (source == null) {
            throw new IOException("Could not find native library " + nativeLibraryName);
        }

        final Path destination = directory.toPath().resolve(nativeLibraryName);
        try {
            Files.copy(source, destination, StandardCopyOption.REPLACE_EXISTING);
        } catch (final AccessDeniedException ignored) {
            // The file already exists, or we don't have permission to write to the directory
        }
        System.load(new File(directory, nativeLibraryName).getAbsolutePath());
    }

    /**
     * Extract the native library and load it
     *
     * @throws IOException          In case an error occurs while extracting the native library
     * @throws UnsatisfiedLinkError In case the native libraries fail to load
     */
    public static void setupWithTemporaryFolder() throws IOException {
        final File temporaryDir = Files.createTempDirectory("placebo-jni").toFile();
        temporaryDir.deleteOnExit();

        try {
            PlaceboManager.loadNative(temporaryDir);
        } catch (final UnsatisfiedLinkError e) {
            e.printStackTrace();

            // Try without ARM support
            PlaceboManager.loadNative(temporaryDir, false);
        }
    }

    /***********************/
    /*** private methods ***/
    /***********************/

    private static String getNativeLibraryName(final boolean allowArm) {
        final String bitnessArch = System.getProperty("os.arch").toLowerCase();
        final String bitnessDataModel = System.getProperty("sun.arch.data.model", null);

        final boolean is64bit = bitnessArch.contains("64") || (bitnessDataModel != null && bitnessDataModel.contains("64"));
        final String arch = bitnessArch.startsWith("aarch") && allowArm ? "arm" : "";

        if (is64bit) {
            final String library64 = PlaceboManager.processLibraryName("libplacebo-jni-native-" + arch + "64");
            if (PlaceboManager.hasResource("/native-binaries/" + library64)) {
                return library64;
            }
        } else {
            final String library32 = PlaceboManager.processLibraryName("libplacebo-jni-native-" + arch + "32");
            if (PlaceboManager.hasResource("/native-binaries/" + library32)) {
                return library32;
            }
        }

        final String library = PlaceboManager.processLibraryName("libplacebo-jni-native");
        if (!PlaceboManager.hasResource("/native-binaries/" + library)) {
            throw new NoSuchElementException("No binary for the current system found, even after trying bit neutral names");
        } else {
            return library;
        }
    }

    private static String processLibraryName(final String library) {
        final String systemName = System.getProperty("os.name", "bare-metal?").toLowerCase();

        if (systemName.contains("nux") || systemName.contains("nix")) {
            return library + ".so";
        } else if (systemName.contains("mac")) {
            return library + ".dylib";
        } else if (systemName.contains("windows")) {
            return library + ".dll";
        } else {
            throw new NoSuchElementException("No native library for system " + systemName);
        }
    }

    private static boolean hasResource(final String resource) {
        return PlaceboManager.class.getResource(resource) != null;
    }
}
