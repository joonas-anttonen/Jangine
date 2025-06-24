#include "Core.hpp"

#define VK_VERSION_1_0
typedef void *PFN_vkGetInstanceProcAddr;
typedef void *VkAllocationCallbacks;
typedef int32_t VkResult;
#include <glfw3.h>
#undef VK_VERSION_1_0

namespace Jangine::Gui
{
    static void ThrowGlfwIfFailed(const std::string &message)
    {
        const char *description;
        int result = glfwGetError(&description);
        if (result != GLFW_NO_ERROR)
        {
            throw Jangine::JangineException(std::format("{} -> {}({})", message, result, description ? description : "NO_DESCRIPTION"));
        }
    }

    Core::Core(const ApiParameters &parameters, Gfx::Core *gfx)
        : logger(Jangine::Core::GetLogger("Gui::Core")), gfx(gfx)
    {
        logger.Func(__func__);

#if defined(JANGINE_PLATFORM_LINUX)
        glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_WAYLAND);
        if (parameters.preferX11)
        {
            glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
        }
#else
        (void)parameters;
#endif

        glfwInit();
        ThrowGlfwIfFailed("glfwInit");
    }

    Core::~Core()
    {
        logger.Func(__func__);

        if (glfwWindow)
        {
            glfwDestroyWindow(glfwWindow);
            glfwWindow = nullptr;
        }

        glfwTerminate();
    }

    void Core::Create(const Parameters &parameters)
    {
        (void)parameters; // Avoid unused parameter warning

        logger.Func(__func__);

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
        glfwWindowHint(GLFW_AUTO_ICONIFY, GLFW_FALSE);

        int32_t monitorCount = 0;
        GLFWmonitor **monitors = glfwGetMonitors(&monitorCount);
        GLFWmonitor *selectedMonitor = monitors[parameters.windowMonitor % monitorCount];

        int32_t monitorX, monitorY, monitorWidth, monitorHeight;
        glfwGetMonitorWorkarea(selectedMonitor, &monitorX, &monitorY, &monitorWidth, &monitorHeight);

        int32_t windowWidth = parameters.windowWidth.value_or(1280);
        int32_t windowHeight = parameters.windowHeight.value_or(720);
        int32_t windowX = parameters.windowX.value_or(monitorX + (monitorWidth - windowWidth) / 2);
        int32_t windowY = parameters.windowY.value_or(monitorY + (monitorHeight - windowHeight) / 2);

        GLFWwindow *window = glfwCreateWindow(windowWidth, windowHeight, parameters.windowTitle.c_str(), nullptr, nullptr);
        ThrowGlfwIfFailed("glfwCreateWindow");
        glfwSetWindowUserPointer(window, this);
        this->glfwWindow = window;

        glfwSetWindowSizeCallback(glfwWindow, [](GLFWwindow *window, int width, int height)
                                  {
            if (width == 0 || height == 0)
            {
                return;
            }

            Core *core = reinterpret_cast<Core *>(glfwGetWindowUserPointer(window));
            if (core)
            {
                Gfx::DisplayParameters newDisplayParameters = core->gfx->GetCurrentDisplayParameters();
                newDisplayParameters.displayWidth = static_cast<uint32_t>(width);
                newDisplayParameters.displayHeight = static_cast<uint32_t>(height);
                core->gfx->SetDisplayParameters(newDisplayParameters);
            } });

        glfwSetWindowSizeLimits(glfwWindow, (int)(256 * 0.5f), (int)(144 * 0.5f), -1, -1);

        int32_t platform = glfwGetPlatform();
        if (platform != GLFW_PLATFORM_WAYLAND)
        {
            glfwSetWindowPos(window, windowX, windowY);
        }
    }

    Surface Core::GetSurface(void_t *surfaceCreationHandle) const
    {
        Surface surface;
        glfwCreateWindowSurface(static_cast<VkInstance>(surfaceCreationHandle), glfwWindow, nullptr, reinterpret_cast<VkSurfaceKHR *>(&surface.vulkanHandle));
        ThrowGlfwIfFailed("glfwCreateWindowSurface");
        glfwGetWindowSize(glfwWindow, reinterpret_cast<int32_t *>(&surface.width), reinterpret_cast<int32_t *>(&surface.height));
        ThrowGlfwIfFailed("glfwGetWindowSize");
        return surface;
    }

    bool_t Core::ProcessEvents() const
    {
        if (!ShouldExit())
        {
            glfwPollEvents();
        }

        return ShouldExit() == false;
    }

    bool_t Core::ShouldExit() const
    {
        return glfwWindowShouldClose(glfwWindow) == GLFW_TRUE;
    }
}