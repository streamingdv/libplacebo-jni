#include "com_grill_placebo_PlaceboManager.h"

#include <cstdlib>
#include <string>
#include <vector>
#include <map>
#include <array>
#include <iterator>
#include <iostream>
#include <algorithm>
#include <set>
#include <cstring>
#include <string.h>
#include <errno.h>
#include <cmath>
#include <type_traits>
#include <utility>
#include <mutex>
#include <atomic>
#include <cstdint>
#include <jni.h>

#ifdef _WIN32
    #include <windows.h>
#elif defined(__APPLE__)
  // Nothing for now
#else
  #include <xcb/xcb.h>
#endif

#include <libplacebo/options.h>
#include <libplacebo/vulkan.h>
#include <libplacebo/renderer.h>
#include <libplacebo/log.h>
#include <libplacebo/cache.h>
#include <libplacebo/gpu.h>

#define PL_LIBAV_IMPLEMENTATION 0
#include <libplacebo/utils/libav.h>

#include <vulkan/vulkan.h>
#ifdef _WIN32
    #include <vulkan/vulkan_win32.h>
#elif defined(__APPLE__)
    #include <vulkan/vulkan_metal.h>
#else
    #include <vulkan/vulkan_xcb.h>
    #include <vulkan/vulkan_wayland.h>
#endif

/*** imports related to UI stuff ***/

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_BOOL
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_UINT_DRAW_INDEX
#define NK_IMPLEMENTATION
#include <nuklear.h>

#include <libplacebo/dispatch.h>
#include <libplacebo/shaders/custom.h>
#include <vk_mem_alloc.h>
#include <noto_sans_regular_font.h>
#include <noto_sans_hebrew_font.h>
#include <gui_font.h>
#include <ui_consts.h>
#include <bidi_text.h>
#include <dialog_ui.h>
#include <aspect_icons.h>
#include <volume_icons.h>
#include <perf_overlay.h>
#include <ui_state.h>

#include <libavutil/buffer.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_vulkan.h>

#include <sstream>
#include <iomanip>

#include "ffx_a_embedded.h"
#include "ffx_fsr1_embedded.h"
#include "ambient/ambient_consts.h"

/*** global color space variable ***/

pl_color_space m_LastColorspace = {};

/*** Global java variables ***/

static JavaVM* g_vm = nullptr;
static jobject g_callback = nullptr;
static jmethodID g_onLog = nullptr;

/*** Screenshot state (Vulkan/libplacebo) ***/

static bool g_pending_screenshot = false;
static std::string g_screenshot_dir;
static std::string g_screenshot_name;


/*** define helper functions ***/

const nk_rune* pick_glyph_range(const char* locale) {
    if (!locale) return glyph_range_latin;

    // Hebrew comes out of a font of its own, see pick_font
    if (locale_is_hebrew(locale))
        return glyph_range_hebrew;

    // Specific variants
    if (strncmp(locale, "ko-FALL", 7) == 0)
        return glyph_range_korean_fallback;
    else if (strncmp(locale, "zh-FALL", 7) == 0)
        return glyph_range_chinese_fallback; // Simplified
    else if (strncmp(locale, "zh-CN", 5) == 0 || strncmp(locale, "zh_Hans", 7) == 0)
        return glyph_range_chinese; // Simplified
    else if (strncmp(locale, "zh-TW", 5) == 0 || strncmp(locale, "zh_Hant", 7) == 0)
        return glyph_range_chinese; // Traditional

    // General language codes
    if (strncmp(locale, "ru", 2) == 0)
        return glyph_range_cyrillic;
    else if (strncmp(locale, "hi", 2) == 0)
        return glyph_range_hindi;
    else if (strncmp(locale, "ja", 2) == 0)
        return glyph_range_japanese;
    else if (strncmp(locale, "ko", 2) == 0)
        return glyph_range_korean;
    else if (strncmp(locale, "zh", 2) == 0)
        return glyph_range_chinese;

    // Default to Latin (en, de, fr, it, pt, etc.)
    return glyph_range_latin;
}

/**
 * The typeface the locale is baked from. Every language but hebrew reads out of the merged Noto Sans,
 * which carries none of the hebrew block, so that one gets the same latin glyphs with hebrew merged in.
 */
unsigned char* pick_font(const char* locale, unsigned int* size) {
    if (locale_is_hebrew(locale)) {
        *size = NotoSansHebrew_Regular_ttf_len;
        return NotoSansHebrew_Regular_ttf;
    }
    *size = NotoSans_Regular_ttf_len;
    return NotoSans_Regular_ttf;
}

#include <sys/stat.h>
#ifdef _WIN32
  #include <direct.h>
#endif

static bool ensure_directory_exists(const std::string &path) {
#ifdef _WIN32
    // Simple version: _mkdir only makes one level; you already have recursive util elsewhere if needed.
    if (_mkdir(path.c_str()) == 0 || errno == EEXIST) {
        return true;
    }
    return false;
#else
    // Same here: one level. If you want recursive, reuse your fileutil.h later.
    if (mkdir(path.c_str(), 0755) == 0 || errno == EEXIST) {
        return true;
    }
    return false;
#endif
}

static std::string join_path(const std::string &dir, const std::string &file) {
#if defined(_WIN32)
    const char sep = '\\';
#else
    const char sep = '/';
#endif
    if (dir.empty()) return file;
    if (dir.back() == '/' || dir.back() == '\\') return dir + file;
    return dir + sep + file;
}

// libplacebo logs from its own worker threads, which are not known to the JVM.
// Attaching and detaching on every message is expensive enough to dominate the
// frame budget at debug log levels, so each thread attaches at most once and
// detaches when it exits.
namespace {

struct JniAttachment {
    JNIEnv *env = nullptr;
    bool owns_attachment = false;

    ~JniAttachment() {
        if (owns_attachment && g_vm)
            g_vm->DetachCurrentThread();
    }
};

thread_local JniAttachment t_jni;

JNIEnv *jni_env_for_current_thread() {
    if (t_jni.env)
        return t_jni.env;
    if (!g_vm)
        return nullptr;

    JNIEnv *env = nullptr;
    if (g_vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6) == JNI_OK) {
        t_jni.env = env; // owned by the JVM, must not be detached here
        return env;
    }

    // Daemon attachment so a lingering libplacebo thread cannot keep the JVM alive.
    if (g_vm->AttachCurrentThreadAsDaemon(reinterpret_cast<void **>(&env), nullptr) != JNI_OK)
        return nullptr;

    t_jni.env = env;
    t_jni.owns_attachment = true;
    return env;
}

} // namespace

void LogCallbackFunction(void*, enum pl_log_level level, const char* msg) {
    if (!g_vm || !g_callback || !g_onLog) {
        std::cout << "Log Level " << level << ": " << msg << std::endl;
        return;
    }

    JNIEnv* env = jni_env_for_current_thread();
    if (!env)
        return;

    jstring jmsg = env->NewStringUTF(msg ? msg : "");
    env->CallVoidMethod(g_callback, g_onLog, (jint)level, jmsg);
    env->DeleteLocalRef(jmsg);

    if (env->ExceptionCheck()) env->ExceptionClear();
}

#include "stb_image_write.h"

static bool save_pl_frame_to_file(pl_vulkan vulkan,
                                  pl_renderer renderer,
                                  const pl_frame *src,
                                  const std::string &directory,
                                  const std::string &fileName)
{
    if (!src) return false;

    int src_w = (int)(src->crop.x1 - src->crop.x0);
    int src_h = (int)(src->crop.y1 - src->crop.y0);

    if (src_w <= 0 || src_h <= 0) {
        return false;
    }

    const int targetMaxHeight = 350;
    float scale = (src_h > targetMaxHeight)
                    ? (float) targetMaxHeight / (float) src_h
                    : 1.0f;

    int out_h = (int) std::round(src_h * scale);
    int out_w = (int) std::round(src_w * scale);
    if (out_h <= 0) out_h = 1;
    if (out_w <= 0) out_w = 1;

    if (!ensure_directory_exists(directory)) {
        LogCallbackFunction(nullptr, PL_LOG_ERR, "Failed to create screenshot directory");
        return false;
    }

    std::string fullPath = join_path(directory, fileName);

    pl_fmt out_fmt = pl_find_named_fmt(vulkan->gpu, "rgba8");
    if (!out_fmt) {
        LogCallbackFunction(nullptr, PL_LOG_ERR, "No suitable RGBA8 format for screenshot");
        return false;
    }

    pl_tex_params tparams = {};
    tparams.w             = out_w;
    tparams.h             = out_h;
    tparams.format        = out_fmt;
    tparams.sampleable    = true;
    tparams.renderable    = true;
    tparams.host_readable = true;

    pl_tex offscreen_tex = pl_tex_create(vulkan->gpu, &tparams);
    if (!offscreen_tex) {
        LogCallbackFunction(nullptr, PL_LOG_ERR, "Failed to create offscreen texture");
        return false;
    }

    pl_frame target = {};
    target.num_planes                    = 1;
    target.planes[0].texture             = offscreen_tex;
    target.planes[0].components          = 4;
    target.planes[0].component_mapping[0] = PL_CHANNEL_R;
    target.planes[0].component_mapping[1] = PL_CHANNEL_G;
    target.planes[0].component_mapping[2] = PL_CHANNEL_B;
    target.planes[0].component_mapping[3] = PL_CHANNEL_A;
    target.planes[0].address_mode        = PL_TEX_ADDRESS_CLAMP;
    target.planes[0].flipped             = false;
    target.planes[0].shift_x             = 0.0f;
    target.planes[0].shift_y             = 0.0f;

    target.color = pl_color_space_srgb;

    // Start from the built-in RGB representation preset.
    target.repr = pl_color_repr_rgb;
    target.repr.levels = PL_COLOR_LEVELS_FULL;
    target.repr.alpha  = PL_ALPHA_INDEPENDENT;

    target.crop.x0 = 0.0f;
    target.crop.y0 = 0.0f;
    target.crop.x1 = (float) out_w;
    target.crop.y1 = (float) out_h;

    pl_render_params params = pl_render_high_quality_params;

    if (!pl_render_image(renderer, src, &target, &params)) {
        LogCallbackFunction(nullptr, PL_LOG_ERR, "pl_render_image failed for screenshot");
        pl_tex_destroy(vulkan->gpu, &offscreen_tex);
        return false;
    }

    std::vector<uint8_t> pixels((size_t) out_w * (size_t) out_h * 4);

    pl_tex_transfer_params xfer = {};
    xfer.tex = offscreen_tex;
    xfer.ptr = pixels.data();

    if (!pl_tex_download(vulkan->gpu, &xfer)) {
        LogCallbackFunction(nullptr, PL_LOG_ERR, "pl_tex_download failed");
        pl_tex_destroy(vulkan->gpu, &offscreen_tex);
        return false;
    }

    pl_tex_destroy(vulkan->gpu, &offscreen_tex);

    bool ok = false;
    std::string ext;
    auto dotPos = fullPath.find_last_of('.');
    if (dotPos != std::string::npos)
        ext = fullPath.substr(dotPos + 1);

    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == "png") {
        ok = (stbi_write_png(fullPath.c_str(),
                             out_w, out_h, 4,
                             pixels.data(),
                             out_w * 4) != 0);
    } else {
        ok = (stbi_write_jpg(fullPath.c_str(),
                             out_w, out_h, 4,
                             pixels.data(),
                             90) != 0);
    }

    if (!ok) {
        LogCallbackFunction(nullptr, PL_LOG_ERR, "Failed to write screenshot image file");
    } else {
        LogCallbackFunction(nullptr, PL_LOG_INFO,
                            ("Saved screenshot to: " + fullPath).c_str());
    }

    return ok;
}

struct Fsr1State {
    bool enabled = false;
    bool enable_rcas = true;
    float rcas_sharpness = 0.2f;

    const struct pl_hook *hook = nullptr;
    int num_hooks = 0;
    std::string shader_text;

    bool dirty = true;
    bool last_enable_rcas = true;
    float last_rcas_sharpness = 0.2f;
    pl_gpu last_gpu = nullptr;
};

static Fsr1State g_fsr1;

static void fsr1_destroy_hooks()
{
    if (g_fsr1.hook) {
        pl_mpv_user_shader_destroy(&g_fsr1.hook);
        g_fsr1.hook = nullptr;
    }
    g_fsr1.num_hooks = 0;
    g_fsr1.shader_text.clear();
}

static std::string build_fsr1_hook_text(bool enable_rcas, float sharpness)
{
    std::ostringstream s;

    // -------------------------
    // EASU PASS
    // -------------------------
    s << R"(//!HOOK MAIN
//!BIND HOOKED
//!SAVE FSR1_EASU
//!WIDTH  MAIN.w
//!HEIGHT MAIN.h
//!DESC   FSR1 EASU (upscale)

#define A_GPU 1
#define A_GLSL 1
#define A_GLSL_INOUT 1

/* --- ffx_a.h --- */
)";

    s << kFfxA << "\n\n";

    s << R"(
#define FSR_EASU_F 1

// Callbacks must return gather4 per channel.
// Use libplacebo's gather wrapper (preferred) OR HOOKED_raw.
AF4 FsrEasuRF(AF2 p) { return HOOKED_gather(p, 0); }
AF4 FsrEasuGF(AF2 p) { return HOOKED_gather(p, 1); }
AF4 FsrEasuBF(AF2 p) { return HOOKED_gather(p, 2); }

/* --- ffx_fsr1.h --- */
)";

    s << kFfxFsr1 << "\n\n";

    s << R"(
vec4 hook()
{
    AU2 ip = AU2(uvec2(gl_FragCoord.xy));

    vec2 srcSize  = HOOKED_size.xy;
    vec2 dstSize  = MAIN_size.xy;
    vec2 viewSize = srcSize;

    AU4 con0, con1, con2, con3;
    FsrEasuCon(con0, con1, con2, con3,
               viewSize.x, viewSize.y,
               srcSize.x,  srcSize.y,
               dstSize.x,  dstSize.y);

    AF3 c;
    FsrEasuF(c, ip, con0, con1, con2, con3);
    return vec4(c, 1.0);
}
)";

    if (!enable_rcas)
        return s.str();

    // -------------------------
    // RCAS PASS
    // -------------------------
    s << R"(

//!HOOK MAIN
//!BIND FSR1_EASU
//!DESC   FSR1 RCAS (sharpen)

#define A_GPU 1
#define A_GLSL 1
#define A_GLSL_INOUT 1

/* --- ffx_a.h --- */
)";

    s << kFfxA << "\n\n";

    s << "\n#define FSR_RCAS_F 1\n";
    s << "\n#define FSR1_SHARPNESS " << std::fixed << std::setprecision(6) << sharpness << "\n\n";

    s << R"(

AF4 FsrRcasLoadF(ASU2 p)
{
    ivec2 ip = ivec2(p);
    return FSR1_EASU_mul * texelFetch(FSR1_EASU_raw, ip, 0);
}

void FsrRcasInputF(inout AF1 r, inout AF1 g, inout AF1 b) { }

/* --- ffx_fsr1.h --- */
)";

    s << kFfxFsr1 << "\n\n";

    s << R"(
vec4 hook()
{
    AU4 con;
    FsrRcasCon(con, FSR1_SHARPNESS);

    AU2 ip = AU2(uvec2(gl_FragCoord.xy));

    AF1 r, g, b;
    FsrRcasF(r, g, b, ip, con);

    return vec4(r, g, b, 1.0);
}
)";

    return s.str();
}

static bool fsr1_ensure_hooks(pl_vulkan vulkan)
{
    if (!g_fsr1.enabled) return true;

    bool gpu_changed = (g_fsr1.last_gpu != vulkan->gpu);
    bool cfg_changed =
        (g_fsr1.last_enable_rcas != g_fsr1.enable_rcas) ||
        (fabsf(g_fsr1.last_rcas_sharpness - g_fsr1.rcas_sharpness) > 1e-6f);

    if (g_fsr1.hook && !g_fsr1.dirty && !gpu_changed && !cfg_changed)
        return true;

    fsr1_destroy_hooks();

    g_fsr1.shader_text = build_fsr1_hook_text(g_fsr1.enable_rcas, g_fsr1.rcas_sharpness);

    const struct pl_hook *hook = pl_mpv_user_shader_parse(
        vulkan->gpu,
        g_fsr1.shader_text.c_str(),
        g_fsr1.shader_text.size()
    );

    if (!hook) {
        LogCallbackFunction(nullptr, PL_LOG_ERR, "FSR1: shader parse failed");
        return false;
    }

    g_fsr1.hook = hook;
    g_fsr1.num_hooks = 1;

    g_fsr1.last_gpu = vulkan->gpu;
    g_fsr1.last_enable_rcas = g_fsr1.enable_rcas;
    g_fsr1.last_rcas_sharpness = g_fsr1.rcas_sharpness;
    g_fsr1.dirty = false;

    return true;
}

// The gpu of the current session. libplacebo exposes no getter for it out of a
// pl_renderer, and plValidateUserShaderText is called without a handle of its
// own, so it is tracked alongside the renderer that owns it.
static std::atomic<pl_gpu> g_active_gpu{nullptr};

// Side-loaded mpv/libplacebo //!HOOK user shader. Unlike the FSR1 state above,
// this is written from whichever thread installs a shader while the render
// thread reads it every frame, so the text is mutex guarded and the parsed hook
// is published atomically. The render thread stays lock free once the hook for
// the current (gpu, version) pair exists.
struct CustomShaderState {
    std::mutex mtx;
    std::atomic<const struct pl_hook *> hook{nullptr};
    std::atomic<pl_gpu> last_gpu{nullptr};
    std::atomic<uint64_t> last_version{0};
    std::string text;
};

static CustomShaderState g_custom;
static std::atomic<bool> g_custom_text_set{false};
static std::atomic<uint64_t> g_custom_text_version{0};

// Must be called with g_custom.mtx held.
static void custom_shader_destroy_hook_locked()
{
    const struct pl_hook *hook = g_custom.hook.exchange(nullptr, std::memory_order_acq_rel);
    if (hook)
        pl_mpv_user_shader_destroy(&hook);
    g_custom.last_gpu.store(nullptr, std::memory_order_relaxed);
    g_custom.last_version.store(0, std::memory_order_relaxed);
}

// Drops the parsed hook, which belongs to the gpu and must not outlive it. The
// installed text is deliberately kept, so a shader survives a session teardown
// and is rebuilt against the next gpu instead of silently disappearing.
static void custom_shader_release_hook()
{
    std::lock_guard<std::mutex> lock(g_custom.mtx);
    custom_shader_destroy_hook_locked();
}

// Returns the hook for the installed shader, reparsing when the text or the gpu
// changed. Returns null when no shader is installed or the source was rejected.
static const struct pl_hook *custom_shader_get_or_build_hook(pl_gpu gpu)
{
    if (!gpu || !g_custom_text_set.load(std::memory_order_acquire))
        return nullptr;

    const uint64_t version = g_custom_text_version.load(std::memory_order_acquire);

    const struct pl_hook *hook = g_custom.hook.load(std::memory_order_acquire);
    if (hook && g_custom.last_gpu.load(std::memory_order_relaxed) == gpu
             && g_custom.last_version.load(std::memory_order_relaxed) == version)
        return hook;

    std::lock_guard<std::mutex> lock(g_custom.mtx);

    // Re-read under the lock: the text and its version only move together while
    // it is held, so this is the pair the parse below is allowed to record.
    const uint64_t locked_version = g_custom_text_version.load(std::memory_order_relaxed);

    hook = g_custom.hook.load(std::memory_order_relaxed);
    if (hook && g_custom.last_gpu.load(std::memory_order_relaxed) == gpu
             && g_custom.last_version.load(std::memory_order_relaxed) == locked_version)
        return hook;

    if (g_custom.text.empty())
        return nullptr;

    custom_shader_destroy_hook_locked();

    const struct pl_hook *parsed = pl_mpv_user_shader_parse(
        gpu,
        g_custom.text.c_str(),
        g_custom.text.size()
    );

    if (!parsed) {
        // Clearing the hint is what stops the render thread from reparsing the
        // same rejected source on every single frame.
        LogCallbackFunction(nullptr, PL_LOG_ERR,
            "Custom user shader rejected; disabled until another one is installed");
        g_custom.text.clear();
        g_custom_text_set.store(false, std::memory_order_release);
        return nullptr;
    }

    g_custom.last_gpu.store(gpu, std::memory_order_relaxed);
    g_custom.last_version.store(locked_version, std::memory_order_relaxed);
    g_custom.hook.store(parsed, std::memory_order_release);

    LogCallbackFunction(nullptr, PL_LOG_INFO,
        ("Custom user shader built (" + std::to_string(g_custom.text.size()) + " bytes)").c_str());

    return parsed;
}

struct nk_image globalBtnImage;

void render_ui(struct ui *ui, int width, int height);
bool ui_draw(struct ui *ui, const struct pl_swapchain_frame *frame);

pl_swapchain_frame m_SwapchainFrame = {0};
bool m_using_wait_for_rendering = false;
// True between a successful pl_swapchain_start_frame and its matching
// pl_swapchain_submit_frame. libplacebo requires that pairing, and a started
// frame must never be left unsubmitted.
bool m_HasPendingSwapchainFrame = false;
int vk_decode_queue_index = -1;

// Reused plane textures for software uploads. Owned by the pl_gpu, so they must
// be destroyed before the pl_vulkan they came from.
pl_tex placebo_tex_global[4] = {nullptr, nullptr, nullptr, nullptr};

struct {
#ifdef _WIN32
    PFN_vkCreateWin32SurfaceKHR vkCreateWin32SurfaceKHR;
#elif defined(__APPLE__)
    PFN_vkCreateMetalSurfaceEXT vkCreateMetalSurfaceEXT;
#else
    PFN_vkCreateXcbSurfaceKHR vkCreateXcbSurfaceKHR;
    PFN_vkCreateWaylandSurfaceKHR vkCreateWaylandSurfaceKHR;
#endif
    PFN_vkDestroySurfaceKHR vkDestroySurfaceKHR;
    PFN_vkGetPhysicalDeviceQueueFamilyProperties vkGetPhysicalDeviceQueueFamilyProperties;
    /** device check methods **/
    PFN_vkGetPhysicalDeviceSurfacePresentModesKHR vkGetPhysicalDeviceSurfacePresentModesKHR;
    PFN_vkGetPhysicalDeviceSurfaceFormatsKHR vkGetPhysicalDeviceSurfaceFormatsKHR;
    PFN_vkEnumeratePhysicalDevices vkEnumeratePhysicalDevices;
    PFN_vkGetPhysicalDeviceProperties vkGetPhysicalDeviceProperties;
    PFN_vkGetPhysicalDeviceSurfaceSupportKHR vkGetPhysicalDeviceSurfaceSupportKHR;
    PFN_vkEnumerateDeviceExtensionProperties vkEnumerateDeviceExtensionProperties;

} vk_funcs;

   // Keep these in sync with hwcontext_vulkan.c
static const char *opt_dev_extensions[] = {
    /* Misc or required by other extensions */
    VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,
    VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME,
    VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME,
    VK_EXT_PHYSICAL_DEVICE_DRM_EXTENSION_NAME,
    VK_EXT_SHADER_ATOMIC_FLOAT_EXTENSION_NAME,
    VK_KHR_COOPERATIVE_MATRIX_EXTENSION_NAME,

    /* Imports/exports */
    VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
    VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME,
    VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME,
    VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME,
    VK_EXT_EXTERNAL_MEMORY_HOST_EXTENSION_NAME,

#ifdef _WIN32
    VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME,
    VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME,
#endif

    VK_KHR_VIDEO_QUEUE_EXTENSION_NAME,
    VK_KHR_VIDEO_DECODE_QUEUE_EXTENSION_NAME,
    VK_KHR_VIDEO_DECODE_H264_EXTENSION_NAME,
    VK_KHR_VIDEO_DECODE_H265_EXTENSION_NAME,
};

// minimum required extensions
static const char *opt_dev_extensions_min[] = {
    VK_KHR_VIDEO_QUEUE_EXTENSION_NAME,
    VK_KHR_VIDEO_DECODE_QUEUE_EXTENSION_NAME,
    VK_KHR_VIDEO_DECODE_H264_EXTENSION_NAME,
    VK_KHR_VIDEO_DECODE_H265_EXTENSION_NAME,
};

bool isExtensionSupportedByPhysicalDevice(VkPhysicalDevice device, const char *extensionName)
{
    uint32_t extensionCount = 0;
    vk_funcs.vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> extensions(extensionCount);
    vk_funcs.vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, extensions.data());

    for (const VkExtensionProperties& extension : extensions) {
        if (strcmp(extension.extensionName, extensionName) == 0) {
            return true;
        }
    }

    return false;
}

bool isSurfacePresentationSupportedByPhysicalDevice(VkPhysicalDevice device, VkSurfaceKHR vkSurfaceKHR)
{
    uint32_t queueFamilyCount = 0;
    vk_funcs.vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        VkBool32 supported = VK_FALSE;
        if (vk_funcs.vkGetPhysicalDeviceSurfaceSupportKHR(device, i, vkSurfaceKHR, &supported) == VK_SUCCESS && supported == VK_TRUE) {
            return true;
        }
    }

    return false;
}

bool isColorSpaceSupportedByPhysicalDevice(VkPhysicalDevice device, VkColorSpaceKHR colorSpace, VkSurfaceKHR vkSurfaceKHR)
{
    uint32_t formatCount = 0;
    vk_funcs.vkGetPhysicalDeviceSurfaceFormatsKHR(device, vkSurfaceKHR, &formatCount, nullptr);

    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vk_funcs.vkGetPhysicalDeviceSurfaceFormatsKHR(device, vkSurfaceKHR, &formatCount, formats.data());

    for (uint32_t i = 0; i < formatCount; i++) {
        if (formats[i].colorSpace == colorSpace) {
            return true;
        }
    }

    return false;
}

bool isPresentModeSupportedByPhysicalDevice(VkPhysicalDevice device, VkPresentModeKHR presentMode, VkSurfaceKHR vkSurfaceKHR)
{
    uint32_t presentModeCount = 0;
    vk_funcs.vkGetPhysicalDeviceSurfacePresentModesKHR(device, vkSurfaceKHR, &presentModeCount, nullptr);

    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    vk_funcs.vkGetPhysicalDeviceSurfacePresentModesKHR(device, vkSurfaceKHR, &presentModeCount, presentModes.data());

    for (uint32_t i = 0; i < presentModeCount; i++) {
        if (presentModes[i] == presentMode) {
            return true;
        }
    }

    return false;
}

bool tryInitializeDevice(pl_log log,
                       pl_vk_inst instance,
                       VkPhysicalDevice device,
                       VkPhysicalDeviceProperties* deviceProps,
                       VkSurfaceKHR vkSurfaceKHR,
                       int decoderType,
                       bool hdr,
                       bool hwAccelBackend,
                       pl_vulkan& placebo_vulkan) {
  // Check the Vulkan API version first to ensure it meets libplacebo's minimum
  if (deviceProps->apiVersion < PL_VK_MIN_VERSION) {
      LogCallbackFunction(nullptr, PL_LOG_ERR, "Vulkan device does not meet minimum Vulkan version!");
      return false;
  }

  if (hwAccelBackend) {
    const char* videoDecodeExtension = nullptr; // Initialize to nullptr to avoid uninitialized usage

    if (decoderType == 0) { // h264
        videoDecodeExtension = VK_KHR_VIDEO_DECODE_H264_EXTENSION_NAME;
    }
    else if (decoderType == 1) { // h265
        videoDecodeExtension = VK_KHR_VIDEO_DECODE_H265_EXTENSION_NAME;
    } else {
        LogCallbackFunction(nullptr, PL_LOG_ERR, "Unsupported video decoder type format!");
        return false;
    }

    if (!isExtensionSupportedByPhysicalDevice(device, videoDecodeExtension)) {
        LogCallbackFunction(nullptr, PL_LOG_ERR, "Vulkan device does not support videoDecodeExtension!");
        return false;
    }
  }

  if (!isSurfacePresentationSupportedByPhysicalDevice(device, vkSurfaceKHR)) {
      LogCallbackFunction(nullptr, PL_LOG_ERR, "Vulkan device does not support presenting on window surface!");
      return false;
  }

  if(hdr && !isColorSpaceSupportedByPhysicalDevice(device, VK_COLOR_SPACE_HDR10_ST2084_EXT, vkSurfaceKHR)){
      LogCallbackFunction(nullptr, PL_LOG_ERR, "Vulkan device does not support HDR10 (ST.2084 PQ)!");
      return false;
  }

  // Avoid software GPUs
  if (deviceProps->deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU) {
      LogCallbackFunction(nullptr, PL_LOG_ERR, "Vulkan device is a (probably slow) software renderer.");
      return false;
  }

  struct pl_vulkan_params vulkan_params = {
      .instance = instance->instance,
      .get_proc_addr = instance->get_proc_addr,
      .surface = vkSurfaceKHR,
      .device = device,
      .extra_queues = hwAccelBackend ? static_cast<VkQueueFlags>(VK_QUEUE_VIDEO_DECODE_BIT_KHR) : static_cast<VkQueueFlags>(0),
      .opt_extensions = opt_dev_extensions,
      .num_opt_extensions = std::size(opt_dev_extensions),
  };

  placebo_vulkan = pl_vulkan_create(log, &vulkan_params);
  if (placebo_vulkan == nullptr) {
     LogCallbackFunction(nullptr, PL_LOG_ERR, "Vulkan device could not be created.");
     return false;
  }

  return true;
}

// Helper function to safely allocate and copy strings
char* copyString(JNIEnv *env, jstring jstr) {
    if (jstr == nullptr) {
        char* emptyStr = new char[1];
        emptyStr[0] = '\0'; // Allocate and return an empty string
        return emptyStr;
    }

    const char* tempStr = env->GetStringUTFChars(jstr, nullptr);
    size_t len = std::strlen(tempStr);
    char* newStr = new char[len + 1]; // Allocate new memory
    std::strcpy(newStr, tempStr); // Copy the string
    env->ReleaseStringUTFChars(jstr, tempStr); // Release JNI string
    return newStr;
}

// Copies a string into a buffer the state owns itself, for what the render thread reads without a lock.
// A line longer than the buffer is cut behind its last whole utf-8 sequence, so no half glyph is left.
void copyStringInto(JNIEnv *env, jstring jstr, char* target, size_t capacity) {
    if (target == nullptr || capacity == 0) return;

    target[0] = '\0';
    if (jstr == nullptr) return;

    const char* source = env->GetStringUTFChars(jstr, nullptr);
    if (source == nullptr) return;

    size_t length = std::strlen(source);
    if (length > capacity - 1) {
        length = capacity - 1;
        while (length > 0 && ((unsigned char) source[length] & 0xC0) == 0x80) length--;
    }
    std::memcpy(target, source, length);
    target[length] = '\0';
    env->ReleaseStringUTFChars(jstr, source);
}

/*** define JNI methods ***/

extern "C"
JNIEXPORT jstring JNICALL Java_com_grill_placebo_PlaceboManager_getWindowingSystem(JNIEnv* env, jobject obj) {
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

extern "C"
JNIEXPORT jlong JNICALL Java_com_grill_placebo_PlaceboManager_plLogCreate
  (JNIEnv *env, jobject obj, jint logLevel, jobject logCallback) {
    env->GetJavaVM(&g_vm);

    // Repeated calls would otherwise leak the previous global reference.
    if (g_callback) {
        env->DeleteGlobalRef(g_callback);
        g_callback = nullptr;
    }
    g_callback = env->NewGlobalRef(logCallback);

    jclass cls = env->GetObjectClass(logCallback);
    g_onLog = env->GetMethodID(cls, "onLog", "(ILjava/lang/String;)V");

  enum pl_log_level plLevel = static_cast<enum pl_log_level>(logLevel);

  struct pl_log_params log_params = {
      .log_cb = LogCallbackFunction,
      .log_level = plLevel,
  };

  pl_log placebo_log = pl_log_create(PL_API_VER, &log_params);

  return reinterpret_cast<jlong>(placebo_log);
}

extern "C"
JNIEXPORT jlong JNICALL Java_com_grill_placebo_PlaceboManager_plLogCreate2
  (JNIEnv *env, jobject obj, jint logLevel) {
  enum pl_log_level plLevel = static_cast<enum pl_log_level>(logLevel);

  struct pl_log_params log_params = {
      .log_cb = LogCallbackFunction,
      .log_level = plLevel,
  };

  pl_log placebo_log = pl_log_create(PL_API_VER, &log_params);

  return reinterpret_cast<jlong>(placebo_log);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_grill_placebo_PlaceboManager_plLogDestroy(JNIEnv *env, jobject /*obj*/, jlong placebo_log)
{
    pl_log log = reinterpret_cast<pl_log>(placebo_log);
    if (log) {
        pl_log_destroy(&log);
    }

    if (g_callback) {
        env->DeleteGlobalRef(g_callback);
        g_callback = nullptr;
    }

    g_onLog = nullptr;
    g_vm = nullptr;
}

JNIEXPORT jlong JNICALL Java_com_grill_placebo_PlaceboManager_plVkInstCreate(JNIEnv *env, jobject obj, jlong placebo_log, jint windowingSystemType) {
  const char *vk_exts[2];  // Array to hold Vulkan extensions
  vk_exts[0] = VK_KHR_SURFACE_EXTENSION_NAME;

  #ifdef _WIN32
      vk_exts[1] = VK_KHR_WIN32_SURFACE_EXTENSION_NAME;
  #elif defined(__APPLE__)
      vk_exts[1] = VK_EXT_METAL_SURFACE_EXTENSION_NAME;
  #else
      if (windowingSystemType == 2) {
          // Wayland surface extension
          vk_exts[1] = VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME;
      } else {
          // XCB surface extension (default for other values)
          vk_exts[1] = VK_KHR_XCB_SURFACE_EXTENSION_NAME;
      }
  #endif

  const char *opt_extensions[] = {
      VK_EXT_HDR_METADATA_EXTENSION_NAME,
  };

  // Handing libplacebo the loader entry point ourselves keeps this independent of
  // PL_HAVE_VK_PROC_ADDR. On macOS the Vulkan implementation is the MoltenVK archive
  // this library links, so libplacebo is built without a loader to link against and
  // would otherwise refuse to create the instance. It is the same function pointer
  // libplacebo picks up on its own everywhere else.
  struct pl_vk_inst_params vk_inst_params = {
      .get_proc_addr = vkGetInstanceProcAddr,
      .extensions = vk_exts,
      .num_extensions = 2,
      .opt_extensions = opt_extensions,
      .num_opt_extensions = 1,
  };

  pl_log log = reinterpret_cast<pl_log>(placebo_log);
  pl_vk_inst instance = pl_vk_inst_create(log, &vk_inst_params);

  return reinterpret_cast<jlong>(instance);
}

extern "C"
JNIEXPORT void JNICALL Java_com_grill_placebo_PlaceboManager_plVkInstDestroy
  (JNIEnv *env, jobject obj, jlong placebo_vk_inst) {
  pl_vk_inst instance = reinterpret_cast<pl_vk_inst>(placebo_vk_inst);
  if (instance != nullptr) {
      pl_vk_inst_destroy(&instance);
  }

  // reset stored color space
  m_LastColorspace = {};
  // reset indication if wait for rendering was used
  m_using_wait_for_rendering = false;
}

extern "C"
JNIEXPORT jlong JNICALL Java_com_grill_placebo_PlaceboManager_plVulkanCreate
  (JNIEnv *env, jobject obj, jlong placebo_log, jlong placebo_vk_inst, jlong surface, jboolean hwAccelBackend) {
  pl_log log = reinterpret_cast<pl_log>(placebo_log);
  pl_vk_inst instance = reinterpret_cast<pl_vk_inst>(placebo_vk_inst);
  VkSurfaceKHR vkSurfaceKHR = reinterpret_cast<VkSurfaceKHR>(static_cast<uint64_t>(surface));

  struct pl_vulkan_params vulkan_params = {
      .instance = instance->instance,
      .get_proc_addr = instance->get_proc_addr,
      .surface = vkSurfaceKHR,
      .allow_software = true,
      PL_VULKAN_DEFAULTS
      .extra_queues = hwAccelBackend ? static_cast<VkQueueFlags>(VK_QUEUE_VIDEO_DECODE_BIT_KHR) : static_cast<VkQueueFlags>(0),
      .opt_extensions = opt_dev_extensions_min,
      .num_opt_extensions = std::size(opt_dev_extensions_min),
  };

  pl_vulkan placebo_vulkan = pl_vulkan_create(log, &vulkan_params);
  return reinterpret_cast<jlong>(placebo_vulkan);
}

extern "C"
JNIEXPORT jlong JNICALL Java_com_grill_placebo_PlaceboManager_plVulkanCreateForBestDevice
  (JNIEnv *env, jobject obj, jlong placebo_log, jlong placebo_vk_inst, jlong surface, jint decoderType, jboolean hdr, jboolean hwAccelBackend) {
  pl_log log = reinterpret_cast<pl_log>(placebo_log);
  pl_vk_inst instance = reinterpret_cast<pl_vk_inst>(placebo_vk_inst);
  VkSurfaceKHR vkSurfaceKHR = reinterpret_cast<VkSurfaceKHR>(static_cast<uint64_t>(surface));

  uint32_t physicalDeviceCount = 0;
  if (vk_funcs.vkEnumeratePhysicalDevices(instance->instance, &physicalDeviceCount, nullptr) != VK_SUCCESS ||
      physicalDeviceCount == 0) {
      LogCallbackFunction(nullptr, PL_LOG_ERR, "No Vulkan physical devices available!");
      return 0;
  }

  std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
  if (vk_funcs.vkEnumeratePhysicalDevices(instance->instance, &physicalDeviceCount, physicalDevices.data()) != VK_SUCCESS ||
      physicalDeviceCount == 0) {
      LogCallbackFunction(nullptr, PL_LOG_ERR, "Failed to enumerate Vulkan physical devices!");
      return 0;
  }
  physicalDevices.resize(physicalDeviceCount);

  std::set<uint32_t> devicesTried;
  VkPhysicalDeviceProperties deviceProps;

  vk_funcs.vkGetPhysicalDeviceProperties(physicalDevices[0], &deviceProps);

  pl_vulkan placebo_vulkan = nullptr;
  if (tryInitializeDevice(log, instance, physicalDevices[0], &deviceProps, vkSurfaceKHR, decoderType, hdr, hwAccelBackend, placebo_vulkan)) {
      return reinterpret_cast<jlong>(placebo_vulkan);
  }
  devicesTried.emplace(0);

  // Next, we'll try to match an integrated GPU, since we want to minimize
  // power consumption and inter-GPU copies.
  for (uint32_t i = 0; i < physicalDeviceCount; i++) {
    // Skip devices we've already tried
    if (devicesTried.find(i) != devicesTried.end()) {
        continue;
    }

    VkPhysicalDeviceProperties deviceProps;
    vk_funcs.vkGetPhysicalDeviceProperties(physicalDevices[i], &deviceProps);
    if (deviceProps.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
        if (tryInitializeDevice(log, instance, physicalDevices[i], &deviceProps, vkSurfaceKHR, decoderType, hdr, hwAccelBackend, placebo_vulkan)) {
            return reinterpret_cast<jlong>(placebo_vulkan);
        }
        devicesTried.emplace(i);
    }
  }

  // Next, we'll try to match a discrete GPU.
  for (uint32_t i = 0; i < physicalDeviceCount; i++) {
    // Skip devices we've already tried
    if (devicesTried.find(i) != devicesTried.end()) {
        continue;
    }

    VkPhysicalDeviceProperties deviceProps;
    vk_funcs.vkGetPhysicalDeviceProperties(physicalDevices[i], &deviceProps);
    if (deviceProps.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        if (tryInitializeDevice(log, instance, physicalDevices[i], &deviceProps, vkSurfaceKHR, decoderType, hdr, hwAccelBackend, placebo_vulkan)) {
            return reinterpret_cast<jlong>(placebo_vulkan);
        }
        devicesTried.emplace(i);
    }
  }

  // Finally, we'll try matching any non-software device.
  for (uint32_t i = 0; i < physicalDeviceCount; i++) {
    // Skip devices we've already tried
    if (devicesTried.find(i) != devicesTried.end()) {
        continue;
    }

    VkPhysicalDeviceProperties deviceProps;
    vk_funcs.vkGetPhysicalDeviceProperties(physicalDevices[i], &deviceProps);
    if (tryInitializeDevice(log, instance, physicalDevices[i], &deviceProps, vkSurfaceKHR, decoderType, hdr, hwAccelBackend, placebo_vulkan)) {
        return reinterpret_cast<jlong>(placebo_vulkan);
    }
    devicesTried.emplace(i);
  }

  return 0;
}

extern "C"
JNIEXPORT jboolean JNICALL Java_com_grill_placebo_PlaceboManager_plInitQueue
  (JNIEnv *env, jobject obj, jlong placebo_vulkan) {
  pl_vulkan vulkan = reinterpret_cast<pl_vulkan>(placebo_vulkan);

  uint32_t queueFamilyCount = 0;
  vk_funcs.vkGetPhysicalDeviceQueueFamilyProperties(vulkan->phys_device, &queueFamilyCount, nullptr);
  std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyCount);
  vk_funcs.vkGetPhysicalDeviceQueueFamilyProperties(vulkan->phys_device, &queueFamilyCount, queueFamilyProperties.data());
  auto queue_it = std::find_if(queueFamilyProperties.begin(), queueFamilyProperties.end(), [](VkQueueFamilyProperties prop) {
      return prop.queueFlags & VK_QUEUE_VIDEO_DECODE_BIT_KHR;
  });

  vk_decode_queue_index = -1;
  if (queue_it != queueFamilyProperties.end()) {
      vk_decode_queue_index = static_cast<int>(std::distance(queueFamilyProperties.begin(), queue_it));
  }

  // Family 0 is a perfectly valid decode queue, so anything non-negative is a
  // success. Only the absence of a decode-capable family is a failure.
  return static_cast<jboolean>(vk_decode_queue_index >= 0);
}

// What fills the space around the video, see AMBIENT_MODE_* in ambient_consts.h.
// Latched from whichever thread changed the setting, picked up by the next frame.
// It lives up here because plVulkanDestroy below ends the session that owns it.
std::atomic<int> ambientMode{AMBIENT_MODE_OFF};

// Defined with the rest of the ambient background further down; declared here
// because the resources belong to the gpu that plVulkanDestroy tears down.
namespace { void ambient_destroy(); }

extern "C"
JNIEXPORT void JNICALL Java_com_grill_placebo_PlaceboManager_plVulkanDestroy
  (JNIEnv *env, jobject obj, jlong placebo_vulkan) {
  // These are all owned by the gpu below, so they have to go before it does.
  fsr1_destroy_hooks();
  custom_shader_release_hook();
  ambient_destroy();

  pl_vulkan vulkan = reinterpret_cast<pl_vulkan>(placebo_vulkan);
  if (vulkan != nullptr) {
      // The plane textures are owned by this gpu, so they have to go first.
      // plTexDestroy may already have done it; destroying a null texture is a
      // no-op, which is what makes the call order irrelevant.
      for (int i = 0; i < 4; i++) {
          if (placebo_tex_global[i])
              pl_tex_destroy(vulkan->gpu, &placebo_tex_global[i]);
      }

      pl_vulkan_destroy(&vulkan);
  }

  // Nothing below outlives the device, so do not let it leak into a later session.
  vk_decode_queue_index = -1;
  // The ambient setting belongs to the session that pushed it. A next session
  // that never asks for a background must not inherit this one's.
  ambientMode.store(AMBIENT_MODE_OFF, std::memory_order_relaxed);
  g_active_gpu.store(nullptr, std::memory_order_release);
  m_HasPendingSwapchainFrame = false;
  m_SwapchainFrame = {};
  m_LastColorspace = {};
}

extern "C"
JNIEXPORT jlong JNICALL Java_com_grill_placebo_PlaceboManager_plGetVkDevice
  (JNIEnv *env, jobject obj, jlong placebo_vulkan) {
  pl_vulkan vulkan = reinterpret_cast<pl_vulkan>(placebo_vulkan);
  VkDevice device = vulkan->device;
  return reinterpret_cast<jlong>(device);
}

extern "C"
JNIEXPORT jlong JNICALL Java_com_grill_placebo_PlaceboManager_plGetVkPhysicalDevice
  (JNIEnv *env, jobject obj, jlong placebo_vulkan) {
  pl_vulkan vulkan = reinterpret_cast<pl_vulkan>(placebo_vulkan);
  VkPhysicalDevice phys_device = vulkan->phys_device;
  return reinterpret_cast<jlong>(phys_device);
}

extern "C"
JNIEXPORT jboolean JNICALL Java_com_grill_placebo_PlaceboManager_plInitFunctionPointers
  (JNIEnv *env, jobject obj, jlong placebo_vk_inst) {
  pl_vk_inst instance = reinterpret_cast<pl_vk_inst>(placebo_vk_inst);
  #ifdef _WIN32
       vk_funcs.vkCreateWin32SurfaceKHR = reinterpret_cast<PFN_vkCreateWin32SurfaceKHR>(
               instance->get_proc_addr(instance->instance, "vkCreateWin32SurfaceKHR"));
       if(!vk_funcs.vkCreateWin32SurfaceKHR) {
           LogCallbackFunction(nullptr, PL_LOG_ERR, "Failed to resolve vkCreateWin32SurfaceKHR!");
           return static_cast<jboolean>(false);
       }
  #elif defined(__APPLE__)
      vk_funcs.vkCreateMetalSurfaceEXT = reinterpret_cast<PFN_vkCreateMetalSurfaceEXT>(
               instance->get_proc_addr(instance->instance, "vkCreateMetalSurfaceEXT"));
      if(!vk_funcs.vkCreateMetalSurfaceEXT) {
          LogCallbackFunction(nullptr, PL_LOG_ERR, "Failed to resolve vkCreateMetalSurfaceEXT!");
          return static_cast<jboolean>(false);
      }
  #else
       bool xcbSurface = true;
       vk_funcs.vkCreateXcbSurfaceKHR = reinterpret_cast<PFN_vkCreateXcbSurfaceKHR>(
               instance->get_proc_addr(instance->instance, "vkCreateXcbSurfaceKHR"));
       if(!vk_funcs.vkCreateXcbSurfaceKHR) {
           LogCallbackFunction(nullptr, PL_LOG_ERR, "Failed to resolve vkCreateXcbSurfaceKHR!");
           xcbSurface = false;
       }
       bool waylandSurface = true;
       vk_funcs.vkCreateWaylandSurfaceKHR = reinterpret_cast<PFN_vkCreateWaylandSurfaceKHR>(
               instance->get_proc_addr(instance->instance, "vkCreateWaylandSurfaceKHR"));
       if(!vk_funcs.vkCreateWaylandSurfaceKHR) {
           LogCallbackFunction(nullptr, PL_LOG_ERR, "Failed to resolve vkCreateWaylandSurfaceKHR!");
           waylandSurface = false;
       }

       if(!xcbSurface && !waylandSurface){
           return static_cast<jboolean>(false);
       }
  #endif
  vk_funcs.vkDestroySurfaceKHR = reinterpret_cast<PFN_vkDestroySurfaceKHR>(
             instance->get_proc_addr(instance->instance, "vkDestroySurfaceKHR"));
  if(!vk_funcs.vkDestroySurfaceKHR) {
      LogCallbackFunction(nullptr, PL_LOG_ERR, "Failed to resolve vkDestroySurfaceKHR!");
      return static_cast<jboolean>(false);
  }

  vk_funcs.vkGetPhysicalDeviceQueueFamilyProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(
             instance->get_proc_addr(instance->instance, "vkGetPhysicalDeviceQueueFamilyProperties"));
  if(!vk_funcs.vkGetPhysicalDeviceQueueFamilyProperties) {
      LogCallbackFunction(nullptr, PL_LOG_ERR, "Failed to resolve vkGetPhysicalDeviceQueueFamilyProperties!");
      return static_cast<jboolean>(false);
  }

  vk_funcs.vkGetPhysicalDeviceSurfacePresentModesKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfacePresentModesKHR>(
             instance->get_proc_addr(instance->instance, "vkGetPhysicalDeviceSurfacePresentModesKHR"));
  if(!vk_funcs.vkGetPhysicalDeviceSurfacePresentModesKHR) {
      LogCallbackFunction(nullptr, PL_LOG_ERR, "Failed to resolve vkGetPhysicalDeviceSurfacePresentModesKHR!");
      return static_cast<jboolean>(false);
  }

  vk_funcs.vkGetPhysicalDeviceSurfaceFormatsKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceFormatsKHR>(
             instance->get_proc_addr(instance->instance, "vkGetPhysicalDeviceSurfaceFormatsKHR"));
  if(!vk_funcs.vkGetPhysicalDeviceSurfaceFormatsKHR) {
      LogCallbackFunction(nullptr, PL_LOG_ERR, "Failed to resolve vkGetPhysicalDeviceSurfaceFormatsKHR!");
      return static_cast<jboolean>(false);
  }

  vk_funcs.vkEnumeratePhysicalDevices = reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(
             instance->get_proc_addr(instance->instance, "vkEnumeratePhysicalDevices"));
  if(!vk_funcs.vkEnumeratePhysicalDevices) {
      LogCallbackFunction(nullptr, PL_LOG_ERR, "Failed to resolve vkEnumeratePhysicalDevices!");
      return static_cast<jboolean>(false);
  }

  vk_funcs.vkGetPhysicalDeviceProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(
             instance->get_proc_addr(instance->instance, "vkGetPhysicalDeviceProperties"));
  if(!vk_funcs.vkGetPhysicalDeviceProperties) {
      LogCallbackFunction(nullptr, PL_LOG_ERR, "Failed to resolve vkGetPhysicalDeviceProperties!");
      return static_cast<jboolean>(false);
  }

  vk_funcs.vkGetPhysicalDeviceSurfaceSupportKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceSupportKHR>(
             instance->get_proc_addr(instance->instance, "vkGetPhysicalDeviceSurfaceSupportKHR"));
  if(!vk_funcs.vkGetPhysicalDeviceSurfaceSupportKHR) {
      LogCallbackFunction(nullptr, PL_LOG_ERR, "Failed to resolve vkGetPhysicalDeviceSurfaceSupportKHR!");
      return static_cast<jboolean>(false);
  }

  vk_funcs.vkEnumerateDeviceExtensionProperties = reinterpret_cast<PFN_vkEnumerateDeviceExtensionProperties>(
             instance->get_proc_addr(instance->instance, "vkEnumerateDeviceExtensionProperties"));
  if(!vk_funcs.vkEnumerateDeviceExtensionProperties) {
      LogCallbackFunction(nullptr, PL_LOG_ERR, "Failed to resolve vkEnumerateDeviceExtensionProperties!");
      return static_cast<jboolean>(false);
  }

  return static_cast<jboolean>(true);
}

extern "C"
JNIEXPORT jlong JNICALL Java_com_grill_placebo_PlaceboManager_plCacheCreate
  (JNIEnv *env, jobject obj, jlong placebo_log, jint max_size) {
  pl_log log = reinterpret_cast<pl_log>(placebo_log);
  size_t max_cache_size;
  if (max_size >= 0) {
      max_cache_size = static_cast<size_t>(max_size);
  } else {
      max_cache_size = 10 << 20; // default use 10 MB
  }
  struct pl_cache_params cache_params = {
      .log = log,
      .max_total_size = max_cache_size,
  };

  pl_cache placebo_cache = pl_cache_create(&cache_params);
  return reinterpret_cast<jlong>(placebo_cache);
}

extern "C"
JNIEXPORT void JNICALL Java_com_grill_placebo_PlaceboManager_plCacheDestroy
  (JNIEnv *env, jobject obj, jlong placebo_cache) {
  pl_cache cache = reinterpret_cast<pl_cache>(placebo_cache);
  if (cache != nullptr) {
      pl_cache_destroy(&cache);
  }
}

extern "C"
JNIEXPORT void JNICALL Java_com_grill_placebo_PlaceboManager_plGpuSetCache
  (JNIEnv *env, jobject obj, jlong placebo_vulkan, jlong placebo_cache) {
  pl_vulkan vulkan = reinterpret_cast<pl_vulkan>(placebo_vulkan);
  pl_cache cache = reinterpret_cast<pl_cache>(placebo_cache);
  pl_gpu_set_cache(vulkan->gpu, cache);
}

extern "C"
JNIEXPORT void JNICALL Java_com_grill_placebo_PlaceboManager_plCacheLoadFile
  (JNIEnv *env, jobject obj, jlong placebo_cache, jstring cache_filePath) {
  const char *path = env->GetStringUTFChars(cache_filePath, nullptr);
  if (path == nullptr) {
      return;
  }
  pl_cache cache = reinterpret_cast<pl_cache>(placebo_cache);
  FILE *file = fopen(path, "rb");
  if (file) {
      pl_cache_load_file(cache, file);
      fclose(file);
  }

  env->ReleaseStringUTFChars(cache_filePath, path);
}

extern "C"
JNIEXPORT void JNICALL Java_com_grill_placebo_PlaceboManager_plCacheSaveFile
  (JNIEnv *env, jobject obj, jlong placebo_cache, jstring cache_filePath) {
  const char *path = env->GetStringUTFChars(cache_filePath, nullptr);
  if (path == nullptr) {
      return;
  }
  pl_cache cache = reinterpret_cast<pl_cache>(placebo_cache);
  FILE *file = fopen(path, "wb");
  if (file) {
      pl_cache_save_file(cache, file);
      fclose(file);
  }

  env->ReleaseStringUTFChars(cache_filePath, path);
}

extern "C"
JNIEXPORT jlong JNICALL Java_com_grill_placebo_PlaceboManager_plGetVkInstance
  (JNIEnv *env, jobject obj, jlong placebo_vk_inst) {
  pl_vk_inst instance = reinterpret_cast<pl_vk_inst>(placebo_vk_inst);
  VkInstance vk_instance = instance->instance;
  return reinterpret_cast<jlong>(vk_instance);
}

extern "C"
JNIEXPORT jlong JNICALL Java_com_grill_placebo_PlaceboManager_plGetWin32SurfaceFunctionPointer
  (JNIEnv *env, jobject obj) {
  #ifdef _WIN32
       return reinterpret_cast<jlong>(vk_funcs.vkCreateWin32SurfaceKHR);
  #endif

  return 0;
}

extern "C"
JNIEXPORT jlong JNICALL Java_com_grill_placebo_PlaceboManager_plGetMetalSurfaceEXT
  (JNIEnv *env, jobject obj) {
  #ifdef __APPLE__
       return reinterpret_cast<jlong>(vk_funcs.vkCreateMetalSurfaceEXT);
  #endif

  return 0;
}

extern "C"
JNIEXPORT jlong JNICALL Java_com_grill_placebo_PlaceboManager_plGetXcbSurfaceFunctionPointer
  (JNIEnv *env, jobject obj) {
#if !defined(_WIN32) && !defined(__APPLE__)
  if (vk_funcs.vkCreateXcbSurfaceKHR != nullptr) {
       return reinterpret_cast<jlong>(vk_funcs.vkCreateXcbSurfaceKHR);
  }
#endif

  return 0;
}

extern "C"
JNIEXPORT jlong JNICALL Java_com_grill_placebo_PlaceboManager_plGetWaylandSurfaceFunctionPointer
  (JNIEnv *env, jobject obj) {
#if !defined(_WIN32) && !defined(__APPLE__)
  if (vk_funcs.vkCreateWaylandSurfaceKHR != nullptr) {
       return reinterpret_cast<jlong>(vk_funcs.vkCreateWaylandSurfaceKHR);
  }
#endif

  return 0;
}

extern "C"
JNIEXPORT void JNICALL Java_com_grill_placebo_PlaceboManager_plDestroySurface
  (JNIEnv *env, jobject obj, jlong placebo_vk_inst, jlong surface) {
  pl_vk_inst instance = reinterpret_cast<pl_vk_inst>(placebo_vk_inst);
  VkSurfaceKHR vkSurfaceKHR = reinterpret_cast<VkSurfaceKHR>(static_cast<uint64_t>(surface));
  vk_funcs.vkDestroySurfaceKHR(instance->instance, vkSurfaceKHR, nullptr);
}

extern "C"
JNIEXPORT jlong JNICALL Java_com_grill_placebo_PlaceboManager_plCreateSwapchain
  (JNIEnv *env, jobject obj, jlong placebo_vulkan, jlong surface, jint vkPresentModeKHR ) {
  pl_vulkan vulkan = reinterpret_cast<pl_vulkan>(placebo_vulkan);
  VkSurfaceKHR vkSurfaceKHR = reinterpret_cast<VkSurfaceKHR>(static_cast<uint64_t>(surface));
  VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR; // Default mode

  switch (vkPresentModeKHR) {
      case 0: // VK_PRESENT_MODE_IMMEDIATE_KHR
          present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
          break;
      case 1: // VK_PRESENT_MODE_MAILBOX_KHR
          present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
          break;
      case 2: // VK_PRESENT_MODE_FIFO_KHR
          present_mode = VK_PRESENT_MODE_FIFO_KHR;
          break;
      case 3: // VK_PRESENT_MODE_FIFO_RELAXED_KHR
          present_mode = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
          break;
  }

  struct pl_vulkan_swapchain_params swapchain_params = {
      .surface = vkSurfaceKHR,
      .present_mode = present_mode,
      .swapchain_depth = 1,
  };

  pl_swapchain placebo_swapchain = pl_vulkan_create_swapchain(vulkan, &swapchain_params);
  return reinterpret_cast<jlong>(placebo_swapchain);
}

extern "C"
JNIEXPORT jlong JNICALL Java_com_grill_placebo_PlaceboManager_plCreateSwapchainWithBestPresentMode
  (JNIEnv *env, jobject obj, jlong placebo_vulkan, jlong surface, jboolean vsync) {
  pl_vulkan vulkan = reinterpret_cast<pl_vulkan>(placebo_vulkan);
  VkSurfaceKHR vkSurfaceKHR = reinterpret_cast<VkSurfaceKHR>(static_cast<uint64_t>(surface));
  VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR; // Default mode

  if(vsync) {
      LogCallbackFunction(nullptr, PL_LOG_INFO, "Using VK_PRESENT_MODE_MAILBOX_KHR present mode with V-Sync enabled");
      present_mode = VK_PRESENT_MODE_FIFO_KHR;
  } else {
      if (isPresentModeSupportedByPhysicalDevice(vulkan->phys_device, VK_PRESENT_MODE_IMMEDIATE_KHR, vkSurfaceKHR)) {
          LogCallbackFunction(nullptr, PL_LOG_INFO, "Using Immediate present mode with V-Sync disabled");
          present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
      } else {
          LogCallbackFunction(nullptr, PL_LOG_INFO, "Immediate present mode is not supported.");

          // FIFO Relaxed can tear if the frame is running late
          if (isPresentModeSupportedByPhysicalDevice(vulkan->phys_device, VK_PRESENT_MODE_FIFO_RELAXED_KHR, vkSurfaceKHR)) {
              LogCallbackFunction(nullptr, PL_LOG_INFO, "Using VK_PRESENT_MODE_FIFO_RELAXED_KHR present mode with V-Sync disabled");
              present_mode = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
          }
          // Mailbox at least provides non-blocking behavior
          else if (isPresentModeSupportedByPhysicalDevice(vulkan->phys_device, VK_PRESENT_MODE_MAILBOX_KHR, vkSurfaceKHR)) {
              LogCallbackFunction(nullptr, PL_LOG_INFO, "Using VK_PRESENT_MODE_MAILBOX_KHR present mode with V-Sync disabled");
              present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
          }
          // FIFO is always supported
          else {
              LogCallbackFunction(nullptr, PL_LOG_INFO, "Using VK_PRESENT_MODE_FIFO_KHR present mode with V-Sync disabled");
              present_mode = VK_PRESENT_MODE_FIFO_KHR;
          }
      }
  }

  struct pl_vulkan_swapchain_params swapchain_params = {
      .surface = vkSurfaceKHR,
      .present_mode = present_mode,
      .swapchain_depth = 1, // do not queue frames
  };

  pl_swapchain placebo_swapchain = pl_vulkan_create_swapchain(vulkan, &swapchain_params);
  return reinterpret_cast<jlong>(placebo_swapchain);
}

extern "C"
JNIEXPORT void JNICALL Java_com_grill_placebo_PlaceboManager_plDestroySwapchain
  (JNIEnv *env, jobject obj, jlong swapchain) {
  pl_swapchain placebo_swapchain = reinterpret_cast<pl_swapchain>(swapchain);
  pl_swapchain_destroy(&placebo_swapchain);
}

extern "C"
JNIEXPORT jlong JNICALL Java_com_grill_placebo_PlaceboManager_plCreateRenderer
  (JNIEnv *env, jobject obj, jlong placebo_vulkan, jlong placebo_log) {
  pl_vulkan vulkan = reinterpret_cast<pl_vulkan>(placebo_vulkan);
  pl_log log = reinterpret_cast<pl_log>(placebo_log);
  pl_renderer placebo_renderer = pl_renderer_create(log, vulkan->gpu);
  if (placebo_renderer != nullptr)
      g_active_gpu.store(vulkan->gpu, std::memory_order_release);
  return reinterpret_cast<jlong>(placebo_renderer);
}

extern "C"
JNIEXPORT void JNICALL Java_com_grill_placebo_PlaceboManager_plDestroyRenderer
  (JNIEnv *env, jobject obj, jlong renderer) {
  pl_renderer placebo_renderer = reinterpret_cast<pl_renderer>(renderer);
  pl_renderer_destroy(&placebo_renderer);
  g_active_gpu.store(nullptr, std::memory_order_release);
}

extern "C"
JNIEXPORT jboolean JNICALL Java_com_grill_placebo_PlaceboManager_plSwapchainResize
  (JNIEnv *env, jobject obj, jlong swapchain, jint width, jint height) {
  pl_swapchain placebo_swapchain = reinterpret_cast<pl_swapchain>(swapchain);
  int int_width = static_cast<int>(width);
  int int_height = static_cast<int>(height);
  bool result = pl_swapchain_resize(placebo_swapchain, &int_width, &int_height);
  return static_cast<jboolean>(result);
}

extern "C"
JNIEXPORT jboolean JNICALL Java_com_grill_placebo_PlaceboManager_plSwapchainResizeWithBuffer
  (JNIEnv *env, jobject obj, jlong swapchain, jlong widthBuffer, jlong heightBuffer) {
  pl_swapchain placebo_swapchain = reinterpret_cast<pl_swapchain>(swapchain);
  int* intWidth = reinterpret_cast<int*>(widthBuffer);
  int* intHeight = reinterpret_cast<int*>(heightBuffer);
  bool result = pl_swapchain_resize(placebo_swapchain, intWidth, intHeight);
  return static_cast<jboolean>(result);
}

extern "C"
JNIEXPORT jboolean JNICALL Java_com_grill_placebo_PlaceboManager_plWaitToRender(
    JNIEnv *env, jobject obj, jlong placebo_vulkan, jlong swapchain, jint width, jint height) {

    pl_vulkan vulkan = reinterpret_cast<pl_vulkan>(placebo_vulkan);
    pl_swapchain placebo_swapchain = reinterpret_cast<pl_swapchain>(swapchain);
    m_using_wait_for_rendering = true; // store the information that we are using wait to render

    if (pl_gpu_is_failed(vulkan->gpu)) {
        LogCallbackFunction(nullptr, PL_LOG_ERR, "GPU is in failed state. Recreating renderer.");
        return JNI_FALSE;
    }

    if (m_HasPendingSwapchainFrame) {
        // The previous frame was started but never submitted, e.g. because
        // mapping its AVFrame failed. Acquiring a second image on top of it
        // would fail, so retire it first.
        m_HasPendingSwapchainFrame = false;
        pl_swapchain_submit_frame(placebo_swapchain);
    }

#ifndef _WIN32
    // On non-Windows platforms, wait for previously queued presents to finish
    pl_swapchain_swap_buffers(placebo_swapchain);
#endif

    int int_width = static_cast<int>(width);
    int int_height = static_cast<int>(height);

    if (!pl_swapchain_resize(placebo_swapchain, &int_width, &int_height)) {
        // Swapchain resize can fail, e.g. if window is occluded
        return JNI_FALSE;
    }

    if (!pl_swapchain_start_frame(placebo_swapchain, &m_SwapchainFrame)) {
        // Reporting success here would let the next render call reuse a stale
        // swapchain frame that was never acquired.
        m_HasPendingSwapchainFrame = false;
        return JNI_FALSE;
    }

    m_HasPendingSwapchainFrame = true;
    return JNI_TRUE;
}

extern "C"
JNIEXPORT jboolean JNICALL Java_com_grill_placebo_PlaceboManager_plSetHwDeviceCtx
  (JNIEnv *env, jobject obj, jlong vulkan_hw_dev_ctx_handle, jlong placebo_vulkan, jlong placebo_vk_inst) {
  if (vk_decode_queue_index < 0) {
    LogCallbackFunction(nullptr, PL_LOG_ERR, "Can not configure vulkan hardware device context!");
    return static_cast<jboolean>(false); // not possible
  }

  AVBufferRef *vulkan_hw_dev_ctx = reinterpret_cast<AVBufferRef *>(vulkan_hw_dev_ctx_handle);
  pl_vulkan vulkan = reinterpret_cast<pl_vulkan>(placebo_vulkan);
  pl_vk_inst instance = reinterpret_cast<pl_vk_inst>(placebo_vk_inst);

  AVHWDeviceContext *hwctx = reinterpret_cast<AVHWDeviceContext*>(vulkan_hw_dev_ctx->data);
  hwctx->user_opaque = const_cast<void*>(reinterpret_cast<const void*>(vulkan));
  AVVulkanDeviceContext *vkctx = reinterpret_cast<AVVulkanDeviceContext*>(hwctx->hwctx);
  vkctx->get_proc_addr = vulkan->get_proc_addr;
  vkctx->inst = vulkan->instance;
  vkctx->phys_dev = vulkan->phys_device;
  vkctx->act_dev = vulkan->device;
  vkctx->device_features = *vulkan->features;

  vkctx->enabled_inst_extensions = instance->extensions;
  vkctx->nb_enabled_inst_extensions = instance->num_extensions;

  vkctx->enabled_dev_extensions = vulkan->extensions;
  vkctx->nb_enabled_dev_extensions = vulkan->num_extensions;
  // libavutil 59.32 replaced the per-purpose queue family fields with a single
  // ordered array. The old fields still work but are deprecated, and they are
  // removed at libavutil 61. Filling qf[] here produces the same contents
  // libavutil would otherwise derive from the legacy fields, video_caps
  // included, so the two branches behave identically.
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(59, 32, 100)
  auto add_queue_family = [vkctx](int idx, int num, VkQueueFlagBits flags) {
      if (idx < 0 || num <= 0)
          return;
      if (vkctx->nb_qf >= static_cast<int>(sizeof(vkctx->qf) / sizeof(vkctx->qf[0])))
          return;
      AVVulkanDeviceQueueFamily *qf = &vkctx->qf[vkctx->nb_qf++];
      qf->idx = idx;
      qf->num = num;
      qf->flags = flags;
      qf->video_caps = static_cast<VkVideoCodecOperationFlagBitsKHR>(0);
  };

  // Ordered by preference, as the field documentation requires. Duplicate
  // indices are permitted, which matters because libplacebo often maps several
  // purposes onto the same family.
  vkctx->nb_qf = 0;
  add_queue_family(vulkan->queue_graphics.index, vulkan->queue_graphics.count, VK_QUEUE_GRAPHICS_BIT);
  add_queue_family(vulkan->queue_compute.index, vulkan->queue_compute.count, VK_QUEUE_COMPUTE_BIT);
  add_queue_family(vulkan->queue_transfer.index, vulkan->queue_transfer.count, VK_QUEUE_TRANSFER_BIT);
  add_queue_family(vk_decode_queue_index, 1, VK_QUEUE_VIDEO_DECODE_BIT_KHR);
#else
  vkctx->queue_family_index = vulkan->queue_graphics.index;
  vkctx->nb_graphics_queues = vulkan->queue_graphics.count;
  vkctx->queue_family_tx_index = vulkan->queue_transfer.index;
  vkctx->nb_tx_queues = vulkan->queue_transfer.count;
  vkctx->queue_family_comp_index = vulkan->queue_compute.index;
  vkctx->nb_comp_queues = vulkan->queue_compute.count;

  vkctx->queue_family_decode_index = vk_decode_queue_index;
  vkctx->nb_decode_queues = 1;
#endif

  vkctx->lock_queue = [](struct AVHWDeviceContext *dev_ctx, uint32_t queue_family, uint32_t index) {
      auto vk = reinterpret_cast<pl_vulkan>(dev_ctx->user_opaque);
      vk->lock_queue(vk, queue_family, index);
  };
  vkctx->unlock_queue = [](struct AVHWDeviceContext *dev_ctx, uint32_t queue_family, uint32_t index) {
      auto vk = reinterpret_cast<pl_vulkan>(dev_ctx->user_opaque);
      vk->unlock_queue(vk, queue_family, index);
  };

  return static_cast<jboolean>(true);
}

pl_render_params render_params = pl_render_fast_params; // default params, others -> pl_render_high_quality_params, pl_render_default_params

extern "C"
JNIEXPORT void JNICALL Java_com_grill_placebo_PlaceboManager_plActivateFastRendering
  (JNIEnv *env, jobject obj) {
  render_params = pl_render_fast_params;
}

extern "C"
JNIEXPORT void JNICALL Java_com_grill_placebo_PlaceboManager_plActivateHighQualityRendering
  (JNIEnv *env, jobject obj) {
  render_params = pl_render_high_quality_params;
}

extern "C"
JNIEXPORT void JNICALL Java_com_grill_placebo_PlaceboManager_plActivateDefaultRendering
  (JNIEnv *env, jobject obj) {
  render_params = pl_render_default_params;
}

// Both are read from the render thread and written from whichever thread changes the setting.
std::atomic<int> renderingFormat{0};
std::atomic<float> targetAspect{0.0f};

extern "C"
JNIEXPORT void JNICALL Java_com_grill_placebo_PlaceboManager_plSetRenderingFormat
  (JNIEnv *env, jobject obj, jint format) {
  if(format >= 0 && format < 3){
    renderingFormat.store(format, std::memory_order_relaxed);
  }
}

extern "C"
JNIEXPORT void JNICALL Java_com_grill_placebo_PlaceboManager_plSetTargetAspect
  (JNIEnv *env, jobject obj, jfloat aspect) {
  targetAspect.store(aspect > 0.0f ? aspect : 0.0f, std::memory_order_relaxed);
}

extern "C"
JNIEXPORT void JNICALL Java_com_grill_placebo_PlaceboManager_plSetAmbientBackground
  (JNIEnv *env, jobject obj, jint mode) {
  int value = static_cast<int>(mode);
  if (value < AMBIENT_MODE_OFF || value > AMBIENT_MODE_EDGE)
      value = AMBIENT_MODE_OFF;

  // Nothing is allocated here. The render thread picks the mode up and creates
  // the ambient resources on the frame it first needs them.
  ambientMode.store(value, std::memory_order_relaxed);
}

// AVFrame ownership contract for plRenderAvFrame and plRenderAvFrameWithUi:
//
//   The caller keeps ownership and frees the frame itself once the call has
//   returned. These functions borrow it for the duration of the call only and
//   must never free it.
//
// Freeing it here would be wrong even though the frame is unused afterwards,
// because this library links its own static FFmpeg while the caller allocates
// the frame through a separate FFmpeg build. On Windows the two use different
// C runtimes -- the javacpp DLLs allocate on the msvcrt heap, this library
// frees on the UCRT heap -- so av_frame_free() here hands a foreign pointer to
// RtlFreeHeap and the process dies within a few frames. Linux and macOS have a
// single process allocator and only survive it by luck.
//
// Freeing after the call returns is safe: pl_map_avframe_ex() takes its own
// av_frame_clone() and pl_unmap_avframe() releases it before we return, so
// nothing of ours outlives the call. It also keeps the caller's reference alive
// across the whole call, which guarantees the clone can never drop the last
// reference to a buffer and free an AVBufferRef that the caller's FFmpeg
// allocated.
namespace {

// Unmaps the mapped pl_frame on scope exit.
struct PlFrameUnmapper {
    pl_gpu gpu;
    struct pl_frame *frame;

    PlFrameUnmapper(pl_gpu g, struct pl_frame *f) : gpu(g), frame(f) {}
    ~PlFrameUnmapper() { pl_unmap_avframe(gpu, frame); }

    PlFrameUnmapper(const PlFrameUnmapper &) = delete;
    PlFrameUnmapper &operator=(const PlFrameUnmapper &) = delete;
};

void apply_target_crop(struct pl_frame *target_frame, const struct pl_frame *source_frame)
{
    pl_rect2df crop = source_frame->crop;
    switch (renderingFormat.load(std::memory_order_relaxed)) {
        case 0: { // normal
            // A target aspect shrinks the crop to that ratio instead of the source ratio, so the
            // video is stretched into a fixed frame (16:10, 21:9, ...) rather than letterboxed.
            const float aspect = targetAspect.load(std::memory_order_relaxed);
            if (aspect > 0.0f)
                pl_rect2df_aspect_set(&target_frame->crop, aspect, 0.0);
            else
                pl_rect2df_aspect_copy(&target_frame->crop, &crop, 0.0);
            break;
        }
        case 1: // stretched
            // Nothing to do, target.crop already covers the full image
            break;
        case 2: // zoomed
            pl_rect2df_aspect_copy(&target_frame->crop, &crop, 1.0);
            break;
    }
}

// Builds the hook chain for a single render call. `chain` is only borrowed by
// `params`, so it has to belong to the caller and stay alive until the matching
// pl_render_image call has returned.
void apply_render_hooks(pl_vulkan vulkan, pl_render_params *params,
                        const struct pl_hook *chain[2])
{
    int count = 0;

    // A side-loaded shader is the user's own upscaler, so it runs first and
    // FSR1 sharpens whatever came out of it.
    const struct pl_hook *custom = custom_shader_get_or_build_hook(vulkan->gpu);
    if (custom)
        chain[count++] = custom;

    if (g_fsr1.enabled) {
        if (fsr1_ensure_hooks(vulkan) && g_fsr1.hook) {
            chain[count++] = g_fsr1.hook;
        } else {
            LogCallbackFunction(nullptr, PL_LOG_WARN,
                "FSR1 enabled but hook unavailable; rendering without FSR1");
        }
    }

    params->hooks = count > 0 ? chain : nullptr;
    params->num_hooks = count;
}

void present_swapchain_frame(pl_swapchain placebo_swapchain)
{
#ifdef _WIN32
    pl_swapchain_swap_buffers(placebo_swapchain);
#else
    if (!m_using_wait_for_rendering)
        pl_swapchain_swap_buffers(placebo_swapchain);
#endif
}

// ---------- Ambient background ----------
//
// All three modes build a tiny copy of the frame through a second renderer, in
// the colour space of the swapchain, and then draw the bars from it before the
// video is rendered on top with `border = PL_CLEAR_SKIP`. Blurred Video and
// Ambient Colors also reduce that copy 4x4; Edge Extend samples the 128x72
// source directly. Cost stays independent of the window, the stream and FSR.
//
// Everything here belongs to the pl_gpu of the session and is created on the
// first frame that actually draws a background, so an unused ambient setting
// costs one atomic load per frame and nothing else.

struct AmbientVertex {
    float pos[2];   // absolute pixels of the target, y down
    float rel[2];   // 0..1 across the video rectangle, outside it in the bars
};

struct AmbientState {
    pl_gpu gpu = nullptr;
    pl_renderer renderer = nullptr;   // second, deliberately tiny renderer
    pl_dispatch dp = nullptr;
    pl_tex src = nullptr;             // AMBIENT_SRC_W x AMBIENT_SRC_H
    pl_tex reduced[2] = {nullptr, nullptr};
    int  reducedCurrent = 0;
    bool reducedPrimed = false;
    bool unavailable = false;         // creation failed once, do not try again
    int  lastMode = AMBIENT_MODE_OFF;
    struct pl_vertex_attrib attribs[2] = {};
};

AmbientState g_ambient;

// The video rectangle, and the bars around it. The bars reach one pixel *into*
// the video: they are drawn first and the video covers them, which is cheaper
// and safer than trying to predict the exact pixel libplacebo's own quad starts
// at, and it can never leave a seam.
struct AmbientBars {
    pl_rect2d rects[2] = {};
    int count = 0;
    float origin[2] = {0.f, 0.f};
    float size[2] = {0.f, 0.f};
    float distScale[2] = {0.f, 0.f}; // video size over bar size, per axis
    float fbSize[2] = {0.f, 0.f};
};

bool ambient_compute_bars(const struct pl_frame *target, int fbWidth, int fbHeight, AmbientBars &out)
{
    out = {};
    if (fbWidth <= 0 || fbHeight <= 0)
        return false;

    out.fbSize[0] = (float) fbWidth;
    out.fbSize[1] = (float) fbHeight;

    const float x0 = fminf(target->crop.x0, target->crop.x1);
    const float x1 = fmaxf(target->crop.x0, target->crop.x1);
    const float y0 = fminf(target->crop.y0, target->crop.y1);
    const float y1 = fmaxf(target->crop.y0, target->crop.y1);

    out.origin[0] = x0;
    out.origin[1] = y0;
    out.size[0] = fmaxf(x1 - x0, 1.f);
    out.size[1] = fmaxf(y1 - y0, 1.f);
    out.distScale[0] = out.size[0] / fmaxf((fbWidth  - out.size[0]) * 0.5f, 1.f);
    out.distScale[1] = out.size[1] / fmaxf((fbHeight - out.size[1]) * 0.5f, 1.f);

    auto clampInt = [](float v, int lo, int hi) {
        int i = (int) lroundf(v);
        return i < lo ? lo : (i > hi ? hi : i);
    };

    auto push = [&out](int px0, int py0, int px1, int py1) {
        if (px1 <= px0 || py1 <= py0 || out.count >= 2)
            return;
        pl_rect2d &r = out.rects[out.count++];
        r.x0 = px0;
        r.y0 = py0;
        r.x1 = px1;
        r.y1 = py1;
    };

    if (y0 > 0.5f || y1 < fbHeight - 0.5f) {
        push(0, 0, fbWidth, clampInt(y0 + 1.f, 0, fbHeight));
        push(0, clampInt(y1 - 1.f, 0, fbHeight), fbWidth, fbHeight);
    } else if (x0 > 0.5f || x1 < fbWidth - 0.5f) {
        push(0, 0, clampInt(x0 + 1.f, 0, fbWidth), fbHeight);
        push(clampInt(x1 - 1.f, 0, fbWidth), 0, fbWidth, fbHeight);
    }

    return out.count > 0;
}

void ambient_destroy()
{
    pl_gpu gpu = g_ambient.gpu;

    if (gpu) {
        pl_tex_destroy(gpu, &g_ambient.src);
        pl_tex_destroy(gpu, &g_ambient.reduced[0]);
        pl_tex_destroy(gpu, &g_ambient.reduced[1]);
        pl_dispatch_destroy(&g_ambient.dp);
        pl_renderer_destroy(&g_ambient.renderer);
    } else {
        // Everything below was created from a gpu that is already gone, so there
        // is nothing left to hand it back to and destroying it would follow dead
        // handles. Dropping ours is all that can be done.
        g_ambient.src = nullptr;
        g_ambient.reduced[0] = nullptr;
        g_ambient.reduced[1] = nullptr;
        g_ambient.dp = nullptr;
        g_ambient.renderer = nullptr;
    }

    // The vertex formats belong to that gpu as well, so the next session has to
    // look them up again rather than reuse these.
    g_ambient.attribs[0] = {};
    g_ambient.attribs[1] = {};

    g_ambient.gpu = nullptr;
    g_ambient.reducedCurrent = 0;
    g_ambient.reducedPrimed = false;
    g_ambient.lastMode = AMBIENT_MODE_OFF;
    // A failure belongs to the session it happened in; the next one starts over.
    g_ambient.unavailable = false;
}

pl_tex ambient_create_tex(pl_gpu gpu, int w, int h)
{
    // 16 bit float holds the PQ and the sRGB range alike, so one format covers
    // every colour space the swapchain can be in.
    pl_fmt fmt = pl_find_fmt(gpu, PL_FMT_FLOAT, 4, 16, 0,
                             (enum pl_fmt_caps) (PL_FMT_CAP_RENDERABLE | PL_FMT_CAP_LINEAR));
    if (!fmt)
        return nullptr;

    pl_tex_params tparams = {};
    tparams.w = w;
    tparams.h = h;
    tparams.format = fmt;
    tparams.sampleable = true;
    tparams.renderable = true;

    return pl_tex_create(gpu, &tparams);
}

bool ambient_ensure(pl_gpu gpu, bool needReduced)
{
    if (!gpu)
        return false;

    if (g_ambient.gpu && g_ambient.gpu != gpu) {
        // A new session on a new device, without the old one having been torn
        // down. Its resources went away with it, so drop them instead of handing
        // stale handles to this gpu.
        g_ambient.gpu = nullptr;
        ambient_destroy();
    }

    if (g_ambient.unavailable)
        return false;

    g_ambient.gpu = gpu;

    if (!g_ambient.renderer)
        g_ambient.renderer = pl_renderer_create(gpu->log, gpu);
    if (!g_ambient.dp)
        g_ambient.dp = pl_dispatch_create(gpu->log, gpu);
    if (!g_ambient.src)
        g_ambient.src = ambient_create_tex(gpu, AMBIENT_SRC_W, AMBIENT_SRC_H);

    if (needReduced && !g_ambient.reduced[0]) {
        for (int i = 0; i < 2; i++)
            g_ambient.reduced[i] = ambient_create_tex(gpu, AMBIENT_REDUCED_W, AMBIENT_REDUCED_H);
        // Nothing has been rendered into them yet, so the first blend must not read them.
        g_ambient.reducedPrimed = false;
    }

    if (!g_ambient.attribs[0].fmt) {
        g_ambient.attribs[0].name = "amb_pos";
        g_ambient.attribs[0].fmt = pl_find_vertex_fmt(gpu, PL_FMT_FLOAT, 2);
        g_ambient.attribs[0].offset = offsetof(struct AmbientVertex, pos);
        g_ambient.attribs[1].name = "amb_rel";
        g_ambient.attribs[1].fmt = pl_find_vertex_fmt(gpu, PL_FMT_FLOAT, 2);
        g_ambient.attribs[1].offset = offsetof(struct AmbientVertex, rel);
    }

    const bool ok = g_ambient.renderer && g_ambient.dp && g_ambient.src &&
                    g_ambient.attribs[0].fmt && g_ambient.attribs[1].fmt &&
                    (!needReduced || (g_ambient.reduced[0] && g_ambient.reduced[1]));

    if (!ok) {
        LogCallbackFunction(nullptr, PL_LOG_WARN,
            "Ambient background unavailable, keeping black bars");
        // Latched after the teardown, which clears the flag for the next session.
        ambient_destroy();
        g_ambient.unavailable = true;
        return false;
    }

    return true;
}

// Renders the whole frame into the tiny texture, in the colour space of the
// swapchain. libplacebo does the conversion, the tone mapping and the display
// encode with the same code the video goes through, so foreground and background
// match and the fills below never have to know about colour at all.
bool ambient_render_source(const struct pl_frame *image, const struct pl_frame *target)
{
    struct pl_frame tiny = {};
    tiny.num_planes = 1;
    tiny.planes[0].texture = g_ambient.src;
    tiny.planes[0].components = 4;
    tiny.planes[0].component_mapping[0] = PL_CHANNEL_R;
    tiny.planes[0].component_mapping[1] = PL_CHANNEL_G;
    tiny.planes[0].component_mapping[2] = PL_CHANNEL_B;
    tiny.planes[0].component_mapping[3] = PL_CHANNEL_A;
    tiny.planes[0].address_mode = PL_TEX_ADDRESS_CLAMP;

    tiny.repr = target->repr;
    // The swapchain's bit depth says nothing about a float texture, and leaving
    // it in would put a quantisation step in front of a target that has none.
    tiny.repr.bits = {};
    tiny.color = target->color;

    tiny.crop.x0 = 0.f;
    tiny.crop.y0 = 0.f;
    tiny.crop.x1 = (float) AMBIENT_SRC_W;
    tiny.crop.y1 = (float) AMBIENT_SRC_H;

    // Deliberately the cheapest configuration there is, and deliberately without
    // the hooks of the video path: FSR upscaling a frame on its way down to
    // 128x72 would cost real time and change nothing anyone can see.
    pl_render_params params = pl_render_fast_params;
    params.border = PL_CLEAR_SKIP;      // the image covers the whole tiny target

    return pl_render_image(g_ambient.renderer, image, &tiny, &params);
}

// Fills one rectangle of the target from `source`. `body` is the algorithm, the
// uniforms below are shared by all of them.
bool ambient_dispatch_fill(pl_tex target,
                           pl_tex source,
                           const char *description,
                           const char *body,
                           const AmbientBars &bars,
                           int barIndex,
                           bool flipped,
                           float dim,
                           float band,
                           float falloff,
                           float tapNear,
                           float tapFar)
{
    const pl_rect2d rect = bars.rects[barIndex];

    const float texel[2] = { 1.f / (float) source->params.w, 1.f / (float) source->params.h };

    struct pl_shader_desc desc = {};
    desc.desc.name = "amb_tex";
    desc.desc.type = PL_DESC_SAMPLED_TEX;
    desc.binding.object = source;
    desc.binding.sample_mode = PL_TEX_SAMPLE_LINEAR;
    desc.binding.address_mode = PL_TEX_ADDRESS_CLAMP;

    struct pl_shader_var vars[10] = {};
    vars[0].var = pl_var_vec2("amb_texel");
    vars[0].data = texel;
    vars[1].var = pl_var_vec2("amb_distScale");
    vars[1].data = bars.distScale;
    vars[1].dynamic = true;
    vars[2].var = pl_var_float("amb_dim");
    vars[2].data = &dim;
    vars[3].var = pl_var_float("amb_band");
    vars[3].data = &band;
    vars[4].var = pl_var_float("amb_falloff");
    vars[4].data = &falloff;
    vars[5].var = pl_var_float("amb_tapNear");
    vars[5].data = &tapNear;
    vars[6].var = pl_var_float("amb_tapFar");
    vars[6].data = &tapFar;
    vars[7].var = pl_var_vec2("amb_fbSize");
    vars[7].data = bars.fbSize;
    vars[7].dynamic = true;
    vars[8].var = pl_var_vec2("amb_origin");
    vars[8].data = bars.origin;
    vars[8].dynamic = true;
    vars[9].var = pl_var_vec2("amb_videoSize");
    vars[9].data = bars.size;
    vars[9].dynamic = true;

    struct pl_custom_shader custom = {};
    custom.description = description;
    custom.body = body;
    custom.output = PL_SHADER_SIG_COLOR;
    custom.descriptors = &desc;
    custom.num_descriptors = 1;
    custom.variables = vars;
    custom.num_variables = 10;

    pl_shader sh = pl_dispatch_begin(g_ambient.dp);
    if (!pl_shader_custom(sh, &custom)) {
        pl_dispatch_abort(g_ambient.dp, &sh);
        return false;
    }

    const AmbientVertex quad[6] = {
        { { (float) rect.x0, (float) rect.y0 }, {} },
        { { (float) rect.x1, (float) rect.y0 }, {} },
        { { (float) rect.x0, (float) rect.y1 }, {} },
        { { (float) rect.x1, (float) rect.y0 }, {} },
        { { (float) rect.x1, (float) rect.y1 }, {} },
        { { (float) rect.x0, (float) rect.y1 }, {} },
    };

    AmbientVertex vertices[6];
    for (int i = 0; i < 6; i++) {
        vertices[i] = quad[i];
        vertices[i].rel[0] = (vertices[i].pos[0] - bars.origin[0]) / bars.size[0];
        vertices[i].rel[1] = (vertices[i].pos[1] - bars.origin[1]) / bars.size[1];
    }

    struct pl_dispatch_vertex_params vparams = {};
    vparams.shader = &sh;
    vparams.target = target;
    // Both the vertices and the scissor are in the space of the visible image;
    // pl_dispatch_vertex turns them into framebuffer rows, exactly as the
    // Nuklear pass above hands it its clip rects.
    vparams.scissors = rect;
    vparams.vertex_attribs = g_ambient.attribs;
    vparams.num_vertex_attribs = 2;
    vparams.vertex_stride = sizeof(struct AmbientVertex);
    vparams.vertex_position_idx = 0;
    vparams.vertex_coords = PL_COORDS_ABSOLUTE;
    vparams.vertex_flipped = flipped;
    vparams.vertex_type = PL_PRIM_TRIANGLE_LIST;
    vparams.vertex_count = 6;
    vparams.vertex_data = vertices;

    return pl_dispatch_vertex(g_ambient.dp, &vparams);
}

// Ambient Colors takes its colour from a band just inside the nearest edge and
// fades it towards the window border, off a copy that is reduced further and
// blended over time. Edge Extend mirrors the outer band of the frame outwards,
// softening the taps the further out it goes.
const char *kAmbientColorsBody = R"(
vec2 before = max(-amb_rel, vec2(0.0));
vec2 after  = max(amb_rel - vec2(1.0), vec2(0.0));
vec2 dlo = clamp(before * amb_distScale, vec2(0.0), vec2(1.0));
vec2 dhi = clamp(after  * amb_distScale, vec2(0.0), vec2(1.0));
float dist = max(max(dlo.x, dlo.y), max(dhi.x, dhi.y));

vec2 uv = clamp(amb_rel, vec2(0.0), vec2(1.0));
uv += step(vec2(1e-6), before) * amb_band;
uv -= step(vec2(1e-6), after)  * amb_band;

vec3 c = textureLod(amb_tex, uv, 0.0).rgb;
color = vec4(c * mix(1.0, amb_falloff, dist) * amb_dim, 1.0);
)";

const char *kAmbientEdgeBody = R"(
vec2 before = max(-amb_rel, vec2(0.0));
vec2 after  = max(amb_rel - vec2(1.0), vec2(0.0));
vec2 dlo = clamp(before * amb_distScale, vec2(0.0), vec2(1.0));
vec2 dhi = clamp(after  * amb_distScale, vec2(0.0), vec2(1.0));
float dist = max(max(dlo.x, dlo.y), max(dhi.x, dhi.y));

vec2 uv = clamp(amb_rel, vec2(0.0), vec2(1.0));
uv += dlo * amb_band;
uv -= dhi * amb_band;

vec2 o = amb_texel * mix(amb_tapNear, amb_tapFar, dist);

vec3 c = textureLod(amb_tex, uv, 0.0).rgb * 2.0;
c += textureLod(amb_tex, uv + vec2(o.x, 0.0), 0.0).rgb;
c += textureLod(amb_tex, uv - vec2(o.x, 0.0), 0.0).rgb;
c += textureLod(amb_tex, uv + vec2(0.0, o.y), 0.0).rgb;
c += textureLod(amb_tex, uv - vec2(0.0, o.y), 0.0).rgb;
color = vec4(c * (1.0 / 6.0) * amb_dim, 1.0);
)";

// Blurred Video: the reduced frame scaled to cover the window, tent filtered.
// Pixel position is reconstructed from amb_rel so the cover UV lives in the
// same Y-down window space the bars were built in, matching D3D11 and Metal.
const char *kAmbientBlurBody = R"(
vec2 px = amb_origin + amb_rel * amb_videoSize;
vec2 cover = amb_fbSize / max(amb_videoSize, vec2(1.0));
vec2 uv = (px - amb_fbSize * 0.5) / (amb_videoSize * max(cover.x, cover.y)) + 0.5;
vec2 o = amb_texel * amb_tapNear;

vec3 c = textureLod(amb_tex, uv, 0.0).rgb * 4.0;
c += textureLod(amb_tex, uv + vec2(-o.x, -o.y), 0.0).rgb;
c += textureLod(amb_tex, uv + vec2( o.x, -o.y), 0.0).rgb;
c += textureLod(amb_tex, uv + vec2(-o.x,  o.y), 0.0).rgb;
c += textureLod(amb_tex, uv + vec2( o.x,  o.y), 0.0).rgb;
color = vec4(c * 0.125 * amb_dim, 1.0);
)";

// Reduces the tiny source by 4x4 and blends it into the previous result. Four
// bilinear taps are an exact box average, and the blend is what makes Ambient
// Colors settle instead of following every flash of the game.
const char *kAmbientReduceBody = R"(
vec3 c = textureLod(amb_tex, amb_rel + vec2(-amb_texel.x, -amb_texel.y), 0.0).rgb;
c += textureLod(amb_tex, amb_rel + vec2( amb_texel.x, -amb_texel.y), 0.0).rgb;
c += textureLod(amb_tex, amb_rel + vec2(-amb_texel.x,  amb_texel.y), 0.0).rgb;
c += textureLod(amb_tex, amb_rel + vec2( amb_texel.x,  amb_texel.y), 0.0).rgb;
c *= 0.25;

if (amb_blend < 0.999) {
    c = mix(textureLod(amb_prev, amb_rel, 0.0).rgb, c, amb_blend);
}

color = vec4(c, 1.0);
)";

// The reduce pass runs over the whole reduced texture, so its quad is the
// texture itself and `amb_rel` is a plain 0..1 texture coordinate.
bool ambient_dispatch_reduce(float blend)
{
    const int prev = g_ambient.reducedCurrent;
    const int next = 1 - prev;

    pl_tex target = g_ambient.reduced[next];
    if (!target)
        return false;

    const float texel[2] = { 1.f / (float) AMBIENT_SRC_W, 1.f / (float) AMBIENT_SRC_H };

    struct pl_shader_desc descs[2] = {};
    descs[0].desc.name = "amb_tex";
    descs[0].desc.type = PL_DESC_SAMPLED_TEX;
    descs[0].binding.object = g_ambient.src;
    descs[0].binding.sample_mode = PL_TEX_SAMPLE_LINEAR;
    descs[0].binding.address_mode = PL_TEX_ADDRESS_CLAMP;
    descs[1].desc.name = "amb_prev";
    descs[1].desc.type = PL_DESC_SAMPLED_TEX;
    descs[1].binding.object = g_ambient.reduced[prev];
    descs[1].binding.sample_mode = PL_TEX_SAMPLE_LINEAR;
    descs[1].binding.address_mode = PL_TEX_ADDRESS_CLAMP;

    struct pl_shader_var vars[2] = {};
    vars[0].var = pl_var_vec2("amb_texel");
    vars[0].data = texel;
    vars[1].var = pl_var_float("amb_blend");
    vars[1].data = &blend;
    vars[1].dynamic = true;

    struct pl_custom_shader custom = {};
    custom.description = "ambient reduce";
    custom.body = kAmbientReduceBody;
    custom.output = PL_SHADER_SIG_COLOR;
    custom.descriptors = descs;
    custom.num_descriptors = 2;
    custom.variables = vars;
    custom.num_variables = 2;

    pl_shader sh = pl_dispatch_begin(g_ambient.dp);
    if (!pl_shader_custom(sh, &custom)) {
        pl_dispatch_abort(g_ambient.dp, &sh);
        return false;
    }

    const float w = (float) AMBIENT_REDUCED_W;
    const float h = (float) AMBIENT_REDUCED_H;

    const AmbientVertex vertices[6] = {
        { { 0.f, 0.f }, { 0.f, 0.f } },
        { {   w, 0.f }, { 1.f, 0.f } },
        { { 0.f,   h }, { 0.f, 1.f } },
        { {   w, 0.f }, { 1.f, 0.f } },
        { {   w,   h }, { 1.f, 1.f } },
        { { 0.f,   h }, { 0.f, 1.f } },
    };

    struct pl_dispatch_vertex_params vparams = {};
    vparams.shader = &sh;
    vparams.target = target;
    vparams.scissors.x1 = AMBIENT_REDUCED_W;
    vparams.scissors.y1 = AMBIENT_REDUCED_H;
    vparams.vertex_attribs = g_ambient.attribs;
    vparams.num_vertex_attribs = 2;
    vparams.vertex_stride = sizeof(struct AmbientVertex);
    vparams.vertex_position_idx = 0;
    vparams.vertex_coords = PL_COORDS_ABSOLUTE;
    vparams.vertex_type = PL_PRIM_TRIANGLE_LIST;
    vparams.vertex_count = 6;
    vparams.vertex_data = vertices;

    if (!pl_dispatch_vertex(g_ambient.dp, &vparams))
        return false;

    g_ambient.reducedCurrent = next;
    g_ambient.reducedPrimed = true;
    return true;
}

// Draws the background into the bars of the swapchain image. Returns false when
// anything went wrong, in which case the caller lets libplacebo clear the border
// as usual and the frame simply shows the black bars of today.
bool ambient_fill_border(pl_gpu gpu,
                         int mode,
                         const struct pl_frame *image,
                         const struct pl_frame *target,
                         const struct pl_swapchain_frame *sc_frame)
{
    if (!sc_frame || !sc_frame->fbo)
        return false;

    const int fbWidth = sc_frame->fbo->params.w;
    const int fbHeight = sc_frame->fbo->params.h;

    AmbientBars bars;
    if (!ambient_compute_bars(target, fbWidth, fbHeight, bars))
        return false;   // the video covers everything, nothing to fill

    const bool needReduced = (mode != AMBIENT_MODE_EDGE);
    if (!ambient_ensure(gpu, needReduced))
        return false;

    if (g_ambient.lastMode != mode) {
        // Another algorithm, or ambient coming back: do not blend into colours
        // that were left over from before
        g_ambient.reducedPrimed = false;
        g_ambient.lastMode = mode;
    }

    if (!ambient_render_source(image, target))
        return false;

    pl_tex source = g_ambient.src;

    if (needReduced) {
        const float blend = (mode == AMBIENT_MODE_COLORS && g_ambient.reducedPrimed)
            ? AMBIENT_BLEND_SMOOTHED
            : AMBIENT_BLEND_INSTANT;
        if (!ambient_dispatch_reduce(blend))
            return false;
        source = g_ambient.reduced[g_ambient.reducedCurrent];
    }

    // libplacebo hands out display-encoded values, so the reduction has to be
    // encoded as well. See AMBIENT_DIM_HDR_ENCODED.
    const float dim = pl_color_transfer_is_hdr(target->color.transfer)
        ? AMBIENT_DIM_HDR_ENCODED
        : AMBIENT_DIM_SDR;

    const char *description;
    const char *body;
    float band;
    float falloff;
    float tapNear;
    float tapFar;
    switch (mode) {
        case AMBIENT_MODE_COLORS:
            description = "ambient colors";
            body = kAmbientColorsBody;
            band = AMBIENT_COLORS_BAND;
            falloff = AMBIENT_COLORS_FALLOFF;
            tapNear = 0.f;
            tapFar = 0.f;
            break;
        case AMBIENT_MODE_BLUR:
            description = "ambient blur";
            body = kAmbientBlurBody;
            band = 0.f;
            falloff = 1.f;
            tapNear = AMBIENT_BLUR_TAP;
            tapFar = 0.f;
            break;
        default:
            description = "ambient edge extend";
            body = kAmbientEdgeBody;
            band = AMBIENT_EDGE_BAND;
            falloff = 1.f;
            tapNear = AMBIENT_EDGE_TAP_NEAR;
            tapFar = AMBIENT_EDGE_TAP_FAR;
            break;
    }

    for (int i = 0; i < bars.count; i++) {
        if (!ambient_dispatch_fill(sc_frame->fbo, source, description, body, bars, i,
                                   sc_frame->flipped,
                                   dim, band, falloff, tapNear, tapFar))
            return false;
    }

    return true;
}

// Nothing of ours drew this frame because the space around the video is black
// again. The smoothed colours belong to whatever was on screen back then, so
// a later switch back starts over instead of fading in from a frame that is
// minutes old. This is what the other two renderers do with their own last mode.
void ambient_note_inactive()
{
    if (g_ambient.lastMode != AMBIENT_MODE_OFF)
        g_ambient.lastMode = AMBIENT_MODE_OFF;
}

// Shared body of both render entry points. `ui_instance` is null for the
// variant without an overlay. Keeping this in one place is deliberate: the two
// copies had already drifted apart, which is how the FSR1 hooks ended up being
// ignored in one of them.
bool render_avframe(AVFrame *raw_frame,
                    pl_vulkan vulkan,
                    pl_swapchain placebo_swapchain,
                    pl_renderer placebo_renderer,
                    struct ui *ui_instance,
                    int width,
                    int height)
{
    if (!raw_frame || !vulkan)
        return false;

    if (m_using_wait_for_rendering && !m_HasPendingSwapchainFrame)
        return false;

    struct pl_frame placebo_frame = {0};
    struct pl_avframe_params avparams = {
        .frame = raw_frame,
        .tex = placebo_tex_global,
    };

    if (!pl_map_avframe_ex(vulkan->gpu, &placebo_frame, &avparams)) {
        LogCallbackFunction(nullptr, PL_LOG_ERR, "Failed to map AVFrame to Placebo frame!");
        return false;
    }
    PlFrameUnmapper unmapper(vulkan->gpu, &placebo_frame);

    if (!pl_color_space_equal(&placebo_frame.color, &m_LastColorspace)) {
        m_LastColorspace = placebo_frame.color;
        pl_swapchain_colorspace_hint(placebo_swapchain, &placebo_frame.color);
    }

    if (!m_using_wait_for_rendering) { // otherwise plWaitToRender already started one
        if (!pl_swapchain_start_frame(placebo_swapchain, &m_SwapchainFrame)) {
            LogCallbackFunction(nullptr, PL_LOG_ERR, "Failed to start Placebo frame!");
            return false;
        }
        m_HasPendingSwapchainFrame = true;
    }

    // The swapchain image is acquired from here on, so it has to be submitted
    // before returning even when rendering fails. Leaving it unsubmitted makes
    // the next pl_swapchain_start_frame fail and leaks the image.
    struct pl_frame target_frame = {0};
    pl_frame_from_swapchain(&target_frame, &m_SwapchainFrame);

    if (ui_instance)
        render_ui(ui_instance, width, height);

    apply_target_crop(&target_frame, &placebo_frame);

    pl_render_params params = render_params;
    const struct pl_hook *hook_chain[2] = {nullptr, nullptr};
    apply_render_hooks(vulkan, &params, hook_chain);

    // Ambient background. Only the Normal format leaves anything empty. A
    // disabled ambient setting costs the two loads below and nothing else: no
    // resources are created until a frame actually draws a background.
    const int ambient_mode = ambientMode.load(std::memory_order_relaxed);
    if (ambient_mode != AMBIENT_MODE_OFF && renderingFormat.load(std::memory_order_relaxed) == 0) {
        if (ambient_fill_border(vulkan->gpu, ambient_mode, &placebo_frame,
                                &target_frame, &m_SwapchainFrame)) {
            // The bars already hold the background, so the video must not clear
            // over it. A failure above keeps the border libplacebo would draw.
            params.border = PL_CLEAR_SKIP;
        }
    } else {
        ambient_note_inactive();
    }

    bool rendered = pl_render_image(placebo_renderer, &placebo_frame, &target_frame, &params);
    if (!rendered)
        LogCallbackFunction(nullptr, PL_LOG_ERR, "Failed to render Placebo frame!");

    if (rendered && g_pending_screenshot) {
        g_pending_screenshot = false;
        std::string dir  = g_screenshot_dir;
        std::string name = g_screenshot_name;
        g_screenshot_dir.clear();
        g_screenshot_name.clear();

        if (!save_pl_frame_to_file(vulkan, placebo_renderer, &placebo_frame, dir, name))
            LogCallbackFunction(nullptr, PL_LOG_ERR, "Failed to save screenshot from plRenderAvFrame");
    }

    if (rendered && ui_instance) {
        if (!ui_draw(ui_instance, &m_SwapchainFrame))
            LogCallbackFunction(nullptr, PL_LOG_ERR, "Could not draw UI!");
    }

    m_HasPendingSwapchainFrame = false;
    if (!pl_swapchain_submit_frame(placebo_swapchain)) {
        LogCallbackFunction(nullptr, PL_LOG_ERR, "Failed to submit Placebo frame!");
        return false;
    }

    present_swapchain_frame(placebo_swapchain);
    return rendered;
}

} // namespace

extern "C"
JNIEXPORT jboolean JNICALL Java_com_grill_placebo_PlaceboManager_plRenderAvFrame
  (JNIEnv *env, jobject obj, jlong avframe, jlong placebo_vulkan, jlong swapchain, jlong renderer) {
  return static_cast<jboolean>(render_avframe(
      reinterpret_cast<AVFrame *>(avframe),
      reinterpret_cast<pl_vulkan>(placebo_vulkan),
      reinterpret_cast<pl_swapchain>(swapchain),
      reinterpret_cast<pl_renderer>(renderer),
      nullptr, 0, 0));
}

extern "C"
JNIEXPORT jboolean JNICALL Java_com_grill_placebo_PlaceboManager_plRenderAvFrameWithUi
  (JNIEnv *env, jobject obj, jlong avframe, jlong placebo_vulkan, jlong swapchain, jlong renderer, jlong ui, jint width, jint height) {
  return static_cast<jboolean>(render_avframe(
      reinterpret_cast<AVFrame *>(avframe),
      reinterpret_cast<pl_vulkan>(placebo_vulkan),
      reinterpret_cast<pl_swapchain>(swapchain),
      reinterpret_cast<pl_renderer>(renderer),
      reinterpret_cast<struct ui *>(ui),
      static_cast<int>(width),
      static_cast<int>(height)));
}

extern "C"
JNIEXPORT jboolean JNICALL Java_com_grill_placebo_PlaceboManager_plRenderUiOnly
  (JNIEnv *env, jobject obj, jlong placebo_vulkan, jlong swapchain, jlong renderer, jlong ui, jint width, jint height) {
    if(m_using_wait_for_rendering && !m_HasPendingSwapchainFrame) {
        return JNI_FALSE;
    }
    pl_swapchain placebo_swapchain = reinterpret_cast<pl_swapchain>(swapchain);
    pl_renderer placebo_renderer = reinterpret_cast<pl_renderer>(renderer);
    struct ui *ui_instance = reinterpret_cast<struct ui *>(ui);

    struct pl_color_space hint = {
        .primaries = PL_COLOR_PRIM_UNKNOWN,
        .transfer = PL_COLOR_TRC_UNKNOWN,
        .hdr = PL_HDR_METADATA_NONE
    };
    if (!pl_color_space_equal(&hint, &m_LastColorspace)) { // reset color space hint if needed
        m_LastColorspace = hint;
        pl_swapchain_colorspace_hint(placebo_swapchain, &hint);
    }

    if (!m_using_wait_for_rendering) { // otherwise plWaitToRender already started one
        if (!pl_swapchain_start_frame(placebo_swapchain, &m_SwapchainFrame)) {
            LogCallbackFunction(nullptr, PL_LOG_ERR, "Failed to start Placebo frame!");
            return JNI_FALSE;
        }
        m_HasPendingSwapchainFrame = true;
    }

    // Acquired from here on, so it has to be submitted before returning.
    struct pl_frame target_frame = {0};
    pl_frame_from_swapchain(&target_frame, &m_SwapchainFrame);

    if (ui_instance)
        render_ui(ui_instance, width, height);

    bool rendered = pl_render_image(placebo_renderer, NULL, &target_frame, &render_params);
    if (!rendered)
        LogCallbackFunction(nullptr, PL_LOG_ERR, "Failed to render Placebo frame!");

    if (rendered && ui_instance) {
        if (!ui_draw(ui_instance, &m_SwapchainFrame))
            LogCallbackFunction(nullptr, PL_LOG_ERR, "Could not draw UI!");
    }

    m_HasPendingSwapchainFrame = false;
    if (!pl_swapchain_submit_frame(placebo_swapchain)) {
        LogCallbackFunction(nullptr, PL_LOG_ERR, "Failed to submit Placebo frame!");
        return JNI_FALSE;
    }

    present_swapchain_frame(placebo_swapchain);
    return static_cast<jboolean>(rendered);
}

extern "C"
JNIEXPORT void JNICALL Java_com_grill_placebo_PlaceboManager_plCleanupRendererContext
  (JNIEnv *env, jobject obj, jlong swapchain) {
  pl_swapchain placebo_swapchain = reinterpret_cast<pl_swapchain>(swapchain);
  if (!m_HasPendingSwapchainFrame) {
      return;
  }

  // A frame was started but never submitted, most likely because the renderer
  // was torn down mid-frame. Submit it so the swapchain can be destroyed
  // without leaking the acquired image. Starting another frame here, as this
  // used to do, would acquire a second image and leak the first.
  m_HasPendingSwapchainFrame = false;
  if (!pl_swapchain_submit_frame(placebo_swapchain)) {
      LogCallbackFunction(nullptr, PL_LOG_ERR, "Failed to submit pending Placebo frame during cleanup!");
  }
}

extern "C"
JNIEXPORT void JNICALL Java_com_grill_placebo_PlaceboManager_plTextDestroy
  (JNIEnv *env, jobject obj, jlong placebo_vulkan) {
  pl_vulkan vulkan = reinterpret_cast<pl_vulkan>(placebo_vulkan);
  for (int i = 0; i < 4; i++) {
      if (placebo_tex_global[i])
          pl_tex_destroy(vulkan->gpu, &placebo_tex_global[i]);
  }
}

extern "C"
JNIEXPORT jboolean JNICALL Java_com_grill_placebo_PlaceboManager_plRequestSaveCurrentFrame
  (JNIEnv *env, jclass clazz, jstring jDirectory, jstring jFileName) {

    if (!jDirectory || !jFileName) {
        return JNI_FALSE;
    }

    const char *dirChars  = env->GetStringUTFChars(jDirectory, nullptr);
    const char *nameChars = env->GetStringUTFChars(jFileName, nullptr);

    std::string dir  = dirChars  ? dirChars  : "";
    std::string name = nameChars ? nameChars : "";

    if (dirChars)  env->ReleaseStringUTFChars(jDirectory, dirChars);
    if (nameChars) env->ReleaseStringUTFChars(jFileName, nameChars);

    if (dir.empty() || name.empty()) {
        g_pending_screenshot = false;
        g_screenshot_dir.clear();
        g_screenshot_name.clear();
        return JNI_FALSE;
    }

    // Normalise extension a bit like on Metal side (force .jpg if no ext)
    {
        auto pos = name.find_last_of('.');
        std::string ext;
        if (pos != std::string::npos) {
            ext = name.substr(pos + 1);
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        }
        if (ext != "jpg" && ext != "jpeg" && ext != "png") {
            name += ".jpg"; // default to jpg
        }
    }

    g_pending_screenshot = true;
    g_screenshot_dir  = std::move(dir);
    g_screenshot_name = std::move(name);

    return JNI_TRUE;
}

/**** create UI methods ****/

nk_rune ranges_icons[] = {
    0xE802, 0xE803,
    0xE804, 0xF11B,
    0xF130, 0xF131,
    0
};

struct ui_vertex {
  float pos[2];
  float coord[2];
  uint8_t color[4];
};

#define NUM_VERTEX_ATTRIBS 3

struct ui {
  pl_gpu gpu;
  pl_dispatch dp;
  struct nk_font *default_font;
  struct nk_font *default_bold_font;
  struct nk_font *default_small_font;
  struct nk_font *icon_font;
  struct nk_context nk;
  struct nk_font_atlas atlas;
  struct nk_buffer cmds, verts, idx;
  pl_tex font_tex;
  struct pl_vertex_attrib attribs_pl[NUM_VERTEX_ATTRIBS];
  struct nk_draw_vertex_layout_element attribs_nk[NUM_VERTEX_ATTRIBS+1];
  struct nk_convert_config convert_cfg;
};

void ui_destroy(struct ui *ui)
{
  if (!ui)
      return;

  nk_buffer_free(&ui->cmds);
  nk_buffer_free(&ui->verts);
  nk_buffer_free(&ui->idx);
  nk_free(&ui->nk);
  nk_font_atlas_clear(&ui->atlas);
  pl_tex_destroy(ui->gpu, &ui->font_tex);
  pl_dispatch_destroy(&ui->dp);

  delete[] globalUiState.notStreamableText;
  delete[] globalUiState.popupState.headerText;
  delete[] globalUiState.popupState.popupText;
  delete[] globalUiState.popupState.popupButtonLeft;
  delete[] globalUiState.popupState.popupButtonRight;
  delete[] globalUiState.popupState.checkboxText;

  // After deleting, set pointers to nullptr to avoid dangling pointers
  globalUiState.notStreamableText = nullptr;
  globalUiState.popupState.headerText = nullptr;
  globalUiState.popupState.popupText = nullptr;
  globalUiState.popupState.popupButtonLeft = nullptr;
  globalUiState.popupState.popupButtonRight = nullptr;
  globalUiState.popupState.checkboxText = nullptr;

  delete ui;
}

struct ui *ui_create(pl_gpu gpu, const char* locale)
{
  struct ui *ui = new struct ui;
  if (!ui)
      return NULL;

  *ui = (struct ui) {
      .gpu = gpu,
      .dp = pl_dispatch_create(gpu->log, gpu),
      .attribs_pl = {
          {
              .name = "pos",
              .fmt = pl_find_vertex_fmt(gpu, PL_FMT_FLOAT, 2),
              .offset = offsetof(struct ui_vertex, pos),
          }, {
              .name = "coord",
              .fmt = pl_find_vertex_fmt(gpu, PL_FMT_FLOAT, 2),
              .offset = offsetof(struct ui_vertex, coord),
          }, {
              .name = "vcolor",
              .fmt = pl_find_named_fmt(gpu, "rgba8"),
              .offset = offsetof(struct ui_vertex, color),
          }
      },
      .attribs_nk = {
          {NK_VERTEX_POSITION, NK_FORMAT_FLOAT, offsetof(struct ui_vertex, pos)},
          {NK_VERTEX_TEXCOORD, NK_FORMAT_FLOAT, offsetof(struct ui_vertex, coord)},
          {NK_VERTEX_COLOR, NK_FORMAT_R8G8B8A8, offsetof(struct ui_vertex, color)},
          {NK_VERTEX_LAYOUT_END}
      },
      .convert_cfg = {
          .global_alpha = 1.0f,
          .line_AA = NK_ANTI_ALIASING_ON,
          .shape_AA = NK_ANTI_ALIASING_ON,
          .circle_segment_count = 22,
          .arc_segment_count = 22,
          .curve_segment_count = 22,
          .vertex_layout = ui->attribs_nk,
          .vertex_size = sizeof(struct ui_vertex),
          .vertex_alignment = NK_ALIGNOF(struct ui_vertex),
      }
  };

  // Initialize font atlas using built-in font
  nk_font_atlas_init_default(&ui->atlas);
  nk_font_atlas_begin(&ui->atlas);
  struct nk_font_config fontConfig = nk_font_config(0);
  fontConfig.range = pick_glyph_range(locale);
  fontConfig.oversample_h = 1; fontConfig.oversample_v = 1;
  fontConfig.pixel_snap = true;
  unsigned int textFontSize = 0;
  unsigned char* textFont = pick_font(locale, &textFontSize);
  ui->default_font = nk_font_atlas_add_from_memory(&ui->atlas, textFont, textFontSize, 26, &fontConfig);
  ui->default_bold_font = nk_font_atlas_add_from_memory(&ui->atlas, textFont, textFontSize, 34, &fontConfig);
  ui->default_small_font = nk_font_atlas_add_from_memory(&ui->atlas, textFont, textFontSize, 16, &fontConfig);
  struct nk_font_config iconConfig = nk_font_config(0);
  iconConfig.range = ranges_icons;
  iconConfig.oversample_h = 1; iconConfig.oversample_v = 1;
  iconConfig.pixel_snap = true;
  ui->icon_font = nk_font_atlas_add_from_memory(&ui->atlas, gui_font, gui_font_size, 30, &iconConfig);
  struct pl_tex_params tparams = {
      .format = pl_find_named_fmt(gpu, "r8"),
      .sampleable = true,
      .initial_data = nk_font_atlas_bake(&ui->atlas, &tparams.w, &tparams.h,
                                         NK_FONT_ATLAS_ALPHA8),
  };
  ui->font_tex = pl_tex_create(gpu, &tparams);
  nk_font_atlas_end(&ui->atlas, nk_handle_ptr((void *) ui->font_tex),
                    &ui->convert_cfg.tex_null);
  nk_font_atlas_cleanup(&ui->atlas);

  if (!ui->font_tex) {
      LogCallbackFunction(nullptr, PL_LOG_ERR, "NK: failed to init font!");
      goto error;
  }

  // Initialize nuklear state
  if (!nk_init_default(&ui->nk, &ui->default_font->handle)) {
      LogCallbackFunction(nullptr, PL_LOG_ERR, "NK: failed initializing UI!");
      goto error;
  }

  nk_buffer_init_default(&ui->cmds);
  nk_buffer_init_default(&ui->verts);
  nk_buffer_init_default(&ui->idx);

  return ui;

error:
  ui_destroy(ui);
  return NULL;
}

bool ui_draw(struct ui *ui, const struct pl_swapchain_frame *frame)
{
  // The nuklear buffers have to be reset on every path. Bailing out with them
  // still populated makes each following frame append to the leftovers, so a
  // persistent failure grows them without bound.
  struct NuklearBuffersReset {
      struct ui *instance;
      ~NuklearBuffersReset() {
          nk_clear(&instance->nk);
          nk_buffer_clear(&instance->cmds);
          nk_buffer_clear(&instance->verts);
          nk_buffer_clear(&instance->idx);
      }
  } reset_buffers{ui};

  if (nk_convert(&ui->nk, &ui->cmds, &ui->verts, &ui->idx, &ui->convert_cfg) != NK_CONVERT_SUCCESS) {
      return false;
  }

  const struct nk_draw_command *cmd = NULL;
  const uint8_t* vertices = reinterpret_cast<const uint8_t*>(nk_buffer_memory(&ui->verts));
  const nk_draw_index* indices = reinterpret_cast<const nk_draw_index*>(nk_buffer_memory(&ui->idx));
  nk_draw_foreach(cmd, &ui->nk, &ui->cmds) {
      if (!cmd->elem_count)
          continue;

      pl_shader sh = pl_dispatch_begin(ui->dp);
      struct pl_shader_desc shader_desc = {
          .desc = {
              .name = "ui_tex",
              .type = PL_DESC_SAMPLED_TEX,
          },
          .binding = {
              .object = cmd->texture.ptr,
              .sample_mode = PL_TEX_SAMPLE_NEAREST,
          },
      };
      struct pl_custom_shader custom_shader = {
          .description = "nuklear UI",
          .body = "color = textureLod(ui_tex, coord, 0.0).r * vcolor;",
          .output = PL_SHADER_SIG_COLOR,
          .descriptors = &shader_desc,
          .num_descriptors = 1,
      };
      pl_shader_custom(sh, &custom_shader);

      struct pl_color_repr repr = frame->color_repr;
      struct pl_color_map_args cmap_args = {
          .src = pl_color_space_srgb,
          .dst = frame->color_space
      };
      pl_shader_color_map_ex(sh, NULL, &cmap_args);
      pl_shader_encode_color(sh, &repr);

      struct pl_dispatch_vertex_params vertex_params = {
          .shader = &sh,
          .target = frame->fbo,
          .scissors = {
              .x0 = static_cast<int>(cmd->clip_rect.x),
              .y0 = static_cast<int>(cmd->clip_rect.y),
              .x1 = static_cast<int>(cmd->clip_rect.x + cmd->clip_rect.w),
              .y1 = static_cast<int>(cmd->clip_rect.y + cmd->clip_rect.h),
          },
          .blend_params = &pl_alpha_overlay,
          .vertex_attribs = ui->attribs_pl,
          .num_vertex_attribs = NUM_VERTEX_ATTRIBS,
          .vertex_stride = sizeof(struct ui_vertex),
          .vertex_position_idx = 0,
          .vertex_coords = PL_COORDS_ABSOLUTE,
          .vertex_flipped = frame->flipped,
          .vertex_type = PL_PRIM_TRIANGLE_LIST,
          .vertex_count = static_cast<int>(cmd->elem_count),
          .vertex_data = vertices,
          .index_data = indices,
          .index_fmt = PL_INDEX_UINT32,
      };
      bool ok = pl_dispatch_vertex(ui->dp, &vertex_params);
      if (!ok) {
          return false;
      }

      indices += cmd->elem_count;
  }

  return true;
}

void render_ui(struct ui *ui, int width, int height) {
  if (!ui || (!globalUiState.showTouchpad && !globalUiState.showPanel && !globalUiState.showPopup
              && !globalUiState.showContentNotStreamable && !globalUiState.showPerfOverlay))
      return;

  struct nk_context *ctx = &ui->nk;
  const struct nk_rect bounds = nk_rect(0, 0, width, height);

  nk_style_push_style_item(ctx, &ctx->style.window.fixed_background, nk_style_item_hide());
  if (nk_begin(ctx, "FULLSCREEN", bounds, NK_WINDOW_NO_SCROLLBAR)) {
      nk_layout_space_begin(ctx, NK_STATIC, bounds.w, bounds.h); // use whole window space

      // dynamic sizes
      float centerPosition = (bounds.w / 2) - panelCenterOffset;
      // cache button style
      struct nk_style_button cachedButtonStyle = ctx->style.button;

      if(globalUiState.showPanel) {
          // **** PS button ****

          if(globalUiState.panelState.psButtonPressed){
              ctx->style.button.normal = nk_style_item_color(pressed_dark_grey_button_color);
              ctx->style.button.hover = nk_style_item_color(pressed_dark_grey_button_color);
              ctx->style.button.active = nk_style_item_color(pressed_dark_grey_button_color);
          } else {
              ctx->style.button.normal = nk_style_item_color(dark_grey_button_color);
              ctx->style.button.hover = nk_style_item_color(dark_grey_button_color);
              ctx->style.button.active = nk_style_item_color(dark_grey_button_color);
          }
          ctx->style.button.border_color = white_button_color;
          ctx->style.button.text_background = white_button_color;
          ctx->style.button.text_normal = white_button_color;
          ctx->style.button.text_hover = white_button_color;
          ctx->style.button.text_active = white_button_color;
          ctx->style.button.rounding = 8;

          nk_layout_space_push(ctx, nk_rect(centerPosition, (bounds.h - buttonSize) - bottomPadding, buttonSize, buttonSize));
          if (nk_button_label(ctx, "PS")) {
              // event handling (ignored here)
          }

          ctx->style.button = cachedButtonStyle;

          // **** Menu buttons ****

          /*** change font to default ***/
          nk_style_set_font(ctx, &ui->default_small_font->handle);
          /*** change font to default ***/

          if(globalUiState.panelState.shareButtonPressed){
              ctx->style.button.normal = nk_style_item_color(pressed_grey_button_color);
              ctx->style.button.hover = nk_style_item_color(pressed_grey_button_color);
              ctx->style.button.active = nk_style_item_color(pressed_grey_button_color);
          } else {
              ctx->style.button.normal = nk_style_item_color(grey_button_color);
              ctx->style.button.hover = nk_style_item_color(grey_button_color);
              ctx->style.button.active = nk_style_item_color(grey_button_color);
          }
          ctx->style.button.border_color = black_button_color;
          ctx->style.button.text_background = black_button_color;
          ctx->style.button.text_normal = black_button_color;
          ctx->style.button.text_hover = black_button_color;
          ctx->style.button.text_active = black_button_color;
          ctx->style.button.rounding = 10;
          // -> Share
          nk_layout_space_push(ctx, nk_rect(centerPosition - ((buttonSize * 1.5) + (buttonSize * 0.15)), ((bounds.h - menuButtonHeight) - bottomPadding) - menuButtonFontSize, buttonSize, menuButtonFontSize));
          nk_label(ctx, "SHARE", NK_TEXT_ALIGN_LEFT);
          nk_layout_space_push(ctx, nk_rect(centerPosition - (buttonSize * 1.5), (bounds.h - menuButtonHeight) - bottomPadding, buttonSize * 0.5, menuButtonHeight));
          if(globalUiState.panelState.shareButtonPressed){
              nk_button_color(ctx, pressed_grey_button_color);
          } else {
              nk_button_color(ctx, grey_button_color);
          }
          // -> Options
          if(globalUiState.panelState.optionsButtonPressed){
              ctx->style.button.normal = nk_style_item_color(pressed_grey_button_color);
              ctx->style.button.hover = nk_style_item_color(pressed_grey_button_color);
              ctx->style.button.active = nk_style_item_color(pressed_grey_button_color);
          } else {
              ctx->style.button.normal = nk_style_item_color(grey_button_color);
              ctx->style.button.hover = nk_style_item_color(grey_button_color);
              ctx->style.button.active = nk_style_item_color(grey_button_color);
          }
          nk_layout_space_push(ctx, nk_rect(centerPosition + ((buttonSize * 2) - (buttonSize * 0.25)), ((bounds.h - menuButtonHeight) - bottomPadding) - menuButtonFontSize, buttonSize, menuButtonFontSize));
          nk_label(ctx, "OPTIONS", NK_TEXT_ALIGN_LEFT);
          nk_layout_space_push(ctx, nk_rect(centerPosition + (buttonSize * 2), (bounds.h - menuButtonHeight) - bottomPadding, buttonSize * 0.5, menuButtonHeight ));
          if(globalUiState.panelState.optionsButtonPressed){
              nk_button_color(ctx, pressed_grey_button_color);
          } else {
              nk_button_color(ctx, grey_button_color);
          }

          ctx->style.button = cachedButtonStyle;

          // **** Mic button

          /*** change font to icon ***/
          nk_style_set_font(ctx, &ui->icon_font->handle);
          /*** change font to icon ***/

          if(globalUiState.panelState.showMicButton) {

              if(globalUiState.panelState.micButtonPressed){
                  ctx->style.button.normal = nk_style_item_color(pressed_white_button_color_alpha);
                  ctx->style.button.hover = nk_style_item_color(pressed_white_button_color_alpha);
                  ctx->style.button.active = nk_style_item_color(pressed_white_button_color_alpha);
              } else {
                  ctx->style.button.normal = nk_style_item_color(white_button_color_alpha);
                  ctx->style.button.hover = nk_style_item_color(white_button_color_alpha);
                  ctx->style.button.active = nk_style_item_color(white_button_color_alpha);
              }
              ctx->style.button.border_color = black_button_color;
              ctx->style.button.text_background = black_button_color;
              ctx->style.button.text_normal = black_button_color;
              ctx->style.button.text_hover = black_button_color;
              ctx->style.button.text_active = black_button_color;
              ctx->style.button.rounding = 8;
              ctx->style.button.border = 0;

              nk_layout_space_push(ctx, nk_rect(panelLeftSlotX(0), (bounds.h - buttonSize) - bottomPadding, buttonSize, buttonSize));
              if(globalUiState.panelState.micButtonActive) {
                  if (nk_button_label(ctx, "\uf130")) {
                      // event handling (ignored here)
                  }
              } else {
                  if (nk_button_label(ctx, "\uf131")) {
                      // event handling (ignored here)
                  }
              }

              ctx->style.button = cachedButtonStyle;
          }

          // **** Volume, the pair right of the mic button, or in its slot when the session hides it

          if(globalUiState.panelState.showVolumeButtons) {
              nk_draw_volume_buttons(ctx, bounds, globalUiState.panelState.showMicButton,
                                     globalUiState.panelState.volumeDownPressed,
                                     globalUiState.panelState.volumeUpPressed);
          }

          // **** Aspect ratio, one slot left of the fullscreen button, while the strip has room for it

          if(globalUiState.panelState.showAspectButton && panelRightSlotFits(bounds.w, 2)) {
              const struct nk_rect aspectBounds = nk_rect(panelRightSlotX(bounds.w, 2), (bounds.h - buttonSize) - bottomPadding, buttonSize, buttonSize);

              if(globalUiState.panelState.aspectButtonPressed){
                  ctx->style.button.normal = nk_style_item_color(pressed_white_button_color_alpha);
                  ctx->style.button.hover = nk_style_item_color(pressed_white_button_color_alpha);
                  ctx->style.button.active = nk_style_item_color(pressed_white_button_color_alpha);
              } else {
                  ctx->style.button.normal = nk_style_item_color(white_button_color_alpha);
                  ctx->style.button.hover = nk_style_item_color(white_button_color_alpha);
                  ctx->style.button.active = nk_style_item_color(white_button_color_alpha);
              }
              ctx->style.button.border_color = black_button_color;
              ctx->style.button.rounding = 8;
              ctx->style.button.border = 0;
              // the chip carries no glyph, the icon of the mode is drawn onto the canvas over it. The
              // layout space takes its rect in local coordinates while the canvas draws in screen ones,
              // so the icon has to be placed where the chip ends up rather than where it was asked for.
              nk_layout_space_push(ctx, aspectBounds);
              if (nk_button_label(ctx, "")) {
                  // event handling (ignored here)
              }
              nk_draw_aspect_icon(nk_window_get_canvas(ctx), nk_layout_space_rect_to_screen(ctx, aspectBounds), globalUiState.panelState.aspectModeIndex, black_button_color);

              ctx->style.button = cachedButtonStyle;
          }

          // **** Fullscreen

          if(globalUiState.panelState.showFullscreenButton) {
              if(globalUiState.panelState.fullscreenButtonPressed){
                  ctx->style.button.normal = nk_style_item_color(pressed_white_button_color_alpha);
                  ctx->style.button.hover = nk_style_item_color(pressed_white_button_color_alpha);
                  ctx->style.button.active = nk_style_item_color(pressed_white_button_color_alpha);
              } else {
                  ctx->style.button.normal = nk_style_item_color(white_button_color_alpha);
                  ctx->style.button.hover = nk_style_item_color(white_button_color_alpha);
                  ctx->style.button.active = nk_style_item_color(white_button_color_alpha);
              }
              ctx->style.button.border_color = black_button_color;
              ctx->style.button.text_background = black_button_color;
              ctx->style.button.text_normal = black_button_color;
              ctx->style.button.text_hover = black_button_color;
              ctx->style.button.text_active = black_button_color;
              ctx->style.button.rounding = 8;
              ctx->style.button.border = 0;
              nk_layout_space_push(ctx, nk_rect((bounds.w - (buttonSize * 2)) - (edgePadding + panelButtonGap), (bounds.h - buttonSize) - bottomPadding, buttonSize, buttonSize));
              if(globalUiState.panelState.fullscreenButtonActive) {
                  if (nk_button_label(ctx, "\ue804")) {
                      // event handling (ignored here)
                  }
              } else {
                  if (nk_button_label(ctx, "\ue802")) {
                      // event handling (ignored here)
                  }
              }

              ctx->style.button = cachedButtonStyle;
          }

          // **** Close

          if(globalUiState.panelState.closeButtonPressed){
              ctx->style.button.normal = nk_style_item_color(pressed_white_button_color_alpha);
              ctx->style.button.hover = nk_style_item_color(pressed_white_button_color_alpha);
              ctx->style.button.active = nk_style_item_color(pressed_white_button_color_alpha);
          } else {
              ctx->style.button.normal = nk_style_item_color(white_button_color_alpha);
              ctx->style.button.hover = nk_style_item_color(white_button_color_alpha);
              ctx->style.button.active = nk_style_item_color(white_button_color_alpha);
          }
          ctx->style.button.border_color = black_button_color;
          ctx->style.button.text_background = black_button_color;
          ctx->style.button.text_normal = black_button_color;
          ctx->style.button.text_hover = black_button_color;
          ctx->style.button.text_active = black_button_color;
          ctx->style.button.rounding = 8;
          ctx->style.button.border = 0;
          nk_layout_space_push(ctx, nk_rect((bounds.w - buttonSize) - edgePadding, (bounds.h - buttonSize) - bottomPadding, buttonSize, buttonSize));
          if (nk_button_label(ctx, "\ue803")) {
              // event handling (ignored here)
          }

          ctx->style.button = cachedButtonStyle;

          /*** change font to default ***/
          nk_style_set_font(ctx, &ui->default_font->handle);
          /*** change font to default ***/
      }

      // **** Touchpad

      if(globalUiState.touchpadPressed) {
          ctx->style.button.normal = nk_style_item_color(touchpad_white_background_color_alpha);
          ctx->style.button.hover = nk_style_item_color(touchpad_white_background_color_alpha);
          ctx->style.button.active = nk_style_item_color(touchpad_white_background_color_alpha);
          ctx->style.button.border_color = touchpad_white_border_color_alpha;
          ctx->style.button.text_background = touchpad_white_border_color_alpha;
          ctx->style.button.text_normal = touchpad_white_border_color_alpha;
          ctx->style.button.text_hover = touchpad_white_border_color_alpha;
          ctx->style.button.text_active = touchpad_white_border_color_alpha;
          ctx->style.button.rounding = 8;
          ctx->style.button.border = 1;
          ctx->style.button.padding = nk_vec2(touchpadPadding, touchpadPadding);
          nk_layout_space_push(ctx, nk_rect(0, 0, bounds.w - ((touchpadPadding * 0.6)), bounds.h - (panelStripHeight + touchpadPadding)));
          if (nk_button_label(ctx, "")) {
              // event handling (ignored here)
          }

          ctx->style.button = cachedButtonStyle;
      }

      // **** Content not streamable
      if(globalUiState.showContentNotStreamable && globalUiState.notStreamableText && globalUiState.notStreamableText[0] != '\0') {
          float maxWidth = bounds.w * 0.8f;
          float lineHeight = 36.0f;
          float labelX = (bounds.w - maxWidth) / 2.0f;

          nk_style_set_font(ctx, &ui->default_bold_font->handle);

          const char *text = globalUiState.notStreamableText;
          const char *lineStart = text;
          int lineCount = 0;

          // Count lines to vertically center
          for (const char *c = text; *c; c++)
              if (*c == '\n') lineCount++;
          lineCount += 1; // final line

          float totalHeight = lineHeight * lineCount;
          float startY = (bounds.h - totalHeight) / 2.0f;

          while (*lineStart) {
              const char *lineEnd = strchr(lineStart, '\n');
              if (!lineEnd)
                  lineEnd = lineStart + strlen(lineStart); // last line

              // a hebrew line is drawn in the order it reads, see bidi_text.h
              char lineBuffer[NK_BIDI_MAX_BYTES];
              int lineLen = (int)(lineEnd - lineStart);
              const char *line = nk_bidi_visual(lineStart, lineLen, lineBuffer, (int) sizeof(lineBuffer),
                                                &lineLen);

              nk_layout_space_push(ctx, nk_rect(labelX, startY, maxWidth, lineHeight));
              nk_text(ctx, line, lineLen, NK_TEXT_CENTERED);

              startY += lineHeight;
              lineStart = *lineEnd ? lineEnd + 1 : lineEnd;
          }

          nk_style_set_font(ctx, &ui->default_font->handle);
      }

      // **** Performance overlay, before the popup so its scrim dims the overlay as well
      // the regular font, not the small one: see the note on the size in perf_overlay.h
      if(globalUiState.showPerfOverlay && ui->default_font != NULL) {
          nk_draw_perf_overlay(nk_window_get_canvas(ctx), &ui->default_font->handle,
                               bounds.w, bounds.h, globalUiState.perfOverlayText,
                               globalUiState.perfOverlayCollapsed ? nk_true : nk_false,
                               globalUiState.perfOverlayClosePressed ? nk_true : nk_false,
                               globalUiState.perfOverlayArrowPressed ? nk_true : nk_false);
      }

      // **** Fullscreen popup
      if(globalUiState.showPopup && ui->default_bold_font != NULL && ui->default_font != NULL) {
          struct nk_dialog_content content = {};
          content.title = globalUiState.popupState.headerText;
          content.text = globalUiState.popupState.popupText;
          content.showSwitch = globalUiState.popupState.showCheckbox ? nk_true : nk_false;
          content.switchText = globalUiState.popupState.checkboxText;
          content.switchChecked = globalUiState.popupState.checkboxChecked ? nk_true : nk_false;
          content.switchFocused = globalUiState.popupState.checkboxFocused ? nk_true : nk_false;
          content.leftButtonText = globalUiState.popupState.popupButtonLeft;
          content.leftButtonFocused = globalUiState.popupState.leftButtonFocused ? nk_true : nk_false;
          content.leftButtonPressed = globalUiState.popupState.leftButtonPressed ? nk_true : nk_false;
          content.rightButtonText = globalUiState.popupState.popupButtonRight;
          content.rightButtonFocused = globalUiState.popupState.rightButtonFocused ? nk_true : nk_false;
          content.rightButtonPressed = globalUiState.popupState.rightButtonPressed ? nk_true : nk_false;

          nk_dialog_draw(ctx, bounds.w, bounds.h, &ui->default_bold_font->handle, &ui->default_font->handle, &content);
      }

      // **** END

      nk_layout_space_end(ctx);
  }
  nk_end(ctx);
  nk_style_pop_style_item(ctx);
}

extern "C"
JNIEXPORT jlong JNICALL Java_com_grill_placebo_PlaceboManager_nkCreateUI
  (JNIEnv *env, jobject obj, jlong placebo_vulkan, jstring jlocale) {
  pl_vulkan vulkan = reinterpret_cast<pl_vulkan>(placebo_vulkan);
  const char* locale = env->GetStringUTFChars(jlocale, nullptr);
  struct ui *ui_instance = ui_create(vulkan->gpu, locale);
  env->ReleaseStringUTFChars(jlocale, locale);
  if (ui_instance == NULL) {
      return 0L;
  }
  return reinterpret_cast<jlong>(ui_instance);
}

extern "C" JNIEXPORT void JNICALL
Java_com_grill_placebo_PlaceboManager_nkUpdateUIState(JNIEnv *env, jobject obj,
  jboolean showTouchpad, jboolean showPanel, jboolean showPopup,
  jboolean touchpadPressed, jboolean panelPressed, jboolean panelShowMicButton,
  jboolean panelShowFullscreenButton, jboolean panelMicButtonPressed, jboolean panelMicButtonActive,
  jboolean panelShareButtonPressed, jboolean panelPsButtonPressed, jboolean panelOptionsButtonPressed,
  jboolean panelFullscreenButtonPressed, jboolean panelFullscreenButtonActive, jboolean panelCloseButtonPressed,
  jboolean panelShowAspectButton, jboolean panelAspectButtonPressed, jint panelAspectModeIndex,
  jstring popupHeaderText, jstring popupPopupText, jboolean popupShowCheckbox,
  jstring popupButtonLeft, jstring popupButtonRight, jstring popupCheckboxText,
  jboolean popupCheckboxChecked, jboolean popupCheckboxFocused, jboolean popupLeftButtonPressed,
  jboolean popupLeftButtonFocused, jboolean popupRightButtonPressed, jboolean popupRightButtonFocused,
  jstring contentNotStreamableText, jboolean showContentNotStreamable,
  jboolean showPerfOverlay, jboolean perfOverlayCollapsed, jboolean perfOverlayClosePressed,
  jboolean perfOverlayArrowPressed, jstring perfOverlayText,
  // grown at the end, so a java side without them keeps working against this native as well
  jboolean panelShowVolumeButtons, jboolean panelVolumeDownPressed, jboolean panelVolumeUpPressed ) {

  globalUiState.showTouchpad = showTouchpad;
  globalUiState.showPanel = showPanel;
  globalUiState.showPopup = showPopup;
  globalUiState.touchpadPressed = touchpadPressed;
  globalUiState.panelPressed = panelPressed;
  globalUiState.showContentNotStreamable = showContentNotStreamable;

  delete[] globalUiState.notStreamableText;
  globalUiState.notStreamableText = copyString(env, contentNotStreamableText);

  globalUiState.panelState.showMicButton = panelShowMicButton;
  globalUiState.panelState.showFullscreenButton = panelShowFullscreenButton;
  globalUiState.panelState.micButtonPressed = panelMicButtonPressed;
  globalUiState.panelState.micButtonActive = panelMicButtonActive;
  globalUiState.panelState.shareButtonPressed = panelShareButtonPressed;
  globalUiState.panelState.psButtonPressed = panelPsButtonPressed;
  globalUiState.panelState.optionsButtonPressed = panelOptionsButtonPressed;
  globalUiState.panelState.fullscreenButtonPressed = panelFullscreenButtonPressed;
  globalUiState.panelState.fullscreenButtonActive = panelFullscreenButtonActive;
  globalUiState.panelState.closeButtonPressed = panelCloseButtonPressed;
  globalUiState.panelState.showAspectButton = panelShowAspectButton;
  globalUiState.panelState.aspectButtonPressed = panelAspectButtonPressed;
  globalUiState.panelState.aspectModeIndex = (panelAspectModeIndex >= 0 && panelAspectModeIndex < NK_ASPECT_MODE_COUNT)
                                             ? (int) panelAspectModeIndex : 0;
  globalUiState.panelState.showVolumeButtons = panelShowVolumeButtons;
  globalUiState.panelState.volumeDownPressed = panelVolumeDownPressed;
  globalUiState.panelState.volumeUpPressed = panelVolumeUpPressed;

  delete[] globalUiState.popupState.headerText;
  globalUiState.popupState.headerText = copyString(env, popupHeaderText);
  delete[] globalUiState.popupState.popupText;
  globalUiState.popupState.popupText = copyString(env, popupPopupText);
  delete[] globalUiState.popupState.popupButtonLeft;
  globalUiState.popupState.popupButtonLeft = copyString(env, popupButtonLeft);
  delete[] globalUiState.popupState.popupButtonRight;
  globalUiState.popupState.popupButtonRight = copyString(env, popupButtonRight);
  delete[] globalUiState.popupState.checkboxText;
  globalUiState.popupState.checkboxText = copyString(env, popupCheckboxText);

  globalUiState.popupState.showCheckbox = popupShowCheckbox;
  globalUiState.popupState.checkboxChecked = popupCheckboxChecked;
  globalUiState.popupState.checkboxFocused = popupCheckboxFocused;
  globalUiState.popupState.leftButtonPressed = popupLeftButtonPressed;
  globalUiState.popupState.leftButtonFocused = popupLeftButtonFocused;
  globalUiState.popupState.rightButtonPressed = popupRightButtonPressed;
  globalUiState.popupState.rightButtonFocused = popupRightButtonFocused;

  // the line first, so the render thread never sees the overlay turned on with the text of the last one
  copyStringInto(env, perfOverlayText, globalUiState.perfOverlayText, sizeof(globalUiState.perfOverlayText));
  globalUiState.perfOverlayCollapsed = perfOverlayCollapsed;
  globalUiState.perfOverlayClosePressed = perfOverlayClosePressed;
  globalUiState.perfOverlayArrowPressed = perfOverlayArrowPressed;
  globalUiState.showPerfOverlay = showPerfOverlay;
}

extern "C"
JNIEXPORT void JNICALL Java_com_grill_placebo_PlaceboManager_nkDestroyUI
  (JNIEnv *env, jobject obj, jlong ui) {
  struct ui *ui_instance = reinterpret_cast<struct ui *>(ui);
  ui_destroy(ui_instance);
}

extern "C"
JNIEXPORT jlong JNICALL Java_com_grill_placebo_PlaceboManager_getVkGetInstanceProcAddr(JNIEnv *env, jobject obj) {
    return reinterpret_cast<jlong>(&vkGetInstanceProcAddr);
}

extern "C" JNIEXPORT void JNICALL
Java_com_grill_placebo_PlaceboManager_plSetFsr1Enabled(JNIEnv*, jobject, jboolean enabled) {
    bool en = (bool) enabled;
    if (g_fsr1.enabled == en) return;
    g_fsr1.enabled = en;

    if (!en) {
        fsr1_destroy_hooks();
        g_fsr1.dirty = true;
    } else {
        g_fsr1.dirty = true;
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_grill_placebo_PlaceboManager_plSetFsr1RcasEnabled(JNIEnv*, jobject, jboolean enabled) {
    bool v = (bool) enabled;
    if (g_fsr1.enable_rcas == v) return;
    g_fsr1.enable_rcas = v;
    g_fsr1.dirty = true;
}

extern "C" JNIEXPORT void JNICALL
Java_com_grill_placebo_PlaceboManager_plSetFsr1Sharpness(JNIEnv*, jobject, jfloat sharpness) {
    float v = (float) sharpness;
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;

    if (fabsf(g_fsr1.rcas_sharpness - v) < 1e-6f) return;
    g_fsr1.rcas_sharpness = v;
    g_fsr1.dirty = true;
}

// Installs, or with null/empty clears, a side-loaded user shader. Only the text
// is stored here; the parse happens lazily on the render thread, because it can
// create gpu resources and must not run underneath a frame that is in flight.
extern "C" JNIEXPORT void JNICALL
Java_com_grill_placebo_PlaceboManager_plSetCustomShaderText(JNIEnv *env, jobject obj, jstring shaderText) {
    std::string text;
    if (shaderText != nullptr) {
        const char *utf8 = env->GetStringUTFChars(shaderText, nullptr);
        if (utf8 != nullptr) {
            const jsize len = env->GetStringUTFLength(shaderText);
            if (len > 0)
                text.assign(utf8, static_cast<size_t>(len));
            env->ReleaseStringUTFChars(shaderText, utf8);
        }
    }

    bool has_text;
    {
        std::lock_guard<std::mutex> lock(g_custom.mtx);
        custom_shader_destroy_hook_locked();
        g_custom.text = std::move(text);
        has_text = !g_custom.text.empty();
        // Bumped under the lock so the builder can never record this version
        // against the text it replaced, which would cost a redundant reparse.
        g_custom_text_version.fetch_add(1, std::memory_order_acq_rel);
    }

    g_custom_text_set.store(has_text, std::memory_order_release);
}

// Pre-flight parse against the live gpu, throwing the result away. Usable only
// while a session is up, since the parser needs that gpu.
extern "C" JNIEXPORT jboolean JNICALL
Java_com_grill_placebo_PlaceboManager_plValidateUserShaderText(JNIEnv *env, jobject obj, jstring shaderText) {
    if (shaderText == nullptr)
        return JNI_FALSE;

    pl_gpu gpu = g_active_gpu.load(std::memory_order_acquire);
    if (gpu == nullptr)
        return JNI_FALSE;

    // This parses on the caller's thread while the render thread is driving the
    // same gpu, which only backends that advertise it can tolerate. Vulkan does.
    if (!gpu->limits.thread_safe) {
        LogCallbackFunction(nullptr, PL_LOG_WARN,
            "Skipping user shader validation: this gpu cannot be used from two threads");
        return JNI_FALSE;
    }

    const char *utf8 = env->GetStringUTFChars(shaderText, nullptr);
    if (utf8 == nullptr)
        return JNI_FALSE;

    const jsize len = env->GetStringUTFLength(shaderText);

    bool ok = false;
    if (len > 0) {
        const struct pl_hook *hook = pl_mpv_user_shader_parse(gpu, utf8, static_cast<size_t>(len));
        ok = (hook != nullptr);
        if (hook)
            pl_mpv_user_shader_destroy(&hook);
    }

    env->ReleaseStringUTFChars(shaderText, utf8);
    return ok ? JNI_TRUE : JNI_FALSE;
}