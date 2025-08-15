#include "Core.hpp"
#include "../Gfx/CommandBuffer2D.hpp"
#include "../Gfx/Core2D.hpp"

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
                Jangine::Core::GetInstance().PostToGfxThread([core, width, height]()
                                                             {
                    Gfx::DisplayParameters newDisplayParameters = core->gfx->GetDisplayParameters();
                    newDisplayParameters.surfaceWidth = static_cast<uint32_t>(width);
                    newDisplayParameters.surfaceHeight = static_cast<uint32_t>(height);
                    core->gfx->InitializeRendering(newDisplayParameters);
                });
            } });

        glfwSetWindowSizeLimits(glfwWindow, (int)(256 * 0.5f), (int)(144 * 0.5f), -1, -1);

        int32_t platform = glfwGetPlatform();
        if (platform != GLFW_PLATFORM_WAYLAND)
        {
            glfwSetWindowPos(window, windowX, windowY);
        }
    }

    void Core::Render(double_t absoluteTime, float_t deltaTime)
    {
        (void)absoluteTime; // Avoid unused parameter warning
        (void)deltaTime;    // Avoid unused parameter warning

        int32_t ww, wh;
        glfwGetWindowSize(glfwWindow, &ww, &wh);
        float_t wwf = static_cast<float_t>(ww);
        float_t whf = static_cast<float_t>(wh);

        Gfx::Core2D *gfx2D = gfx->GetCore2D();
        Gfx::CommandBuffer2D *commandBuffer = nullptr;

        bool_t acquiredCommandBuffer = gfx2D->TryAcquireCommandBuffer(&commandBuffer);
        if (!acquiredCommandBuffer)
        {
            logger.Warning("Failed to acquire command buffer", __func__);
            return;
        }

        {
            static constexpr float_t ControlsOffFromFrameSide = 7.f;
            static constexpr float_t ControlButtonWidth = 40.f;
            static constexpr float_t ControlsButtonSeparation = 2.f;
            static const Eigen::Vector4f sizeOfFrame = Eigen::Vector4f(2.0f, 32.0f, 2.0f, 2.0f);
            static const Color colorOfFrame = Color::FromUInt(0x282c34);

            static constexpr bool_t mouseOnClose = false;
            static constexpr bool_t mouseOnMaximize = false;
            static constexpr bool_t mouseOnMinimize = false;
            static constexpr bool_t mouseOnFrameControls = false;

            auto controlsCloseRect = Gfx::Rectangle(wwf - ControlButtonWidth - ControlsOffFromFrameSide, 0.0f, wwf - ControlsOffFromFrameSide, sizeOfFrame.y());
            auto controlsCloseIconRect = Gfx::Rectangle(0, 0, 8, 8).CenterOn(controlsCloseRect.center());
            auto controlsMaximizeRect = Gfx::Rectangle(controlsCloseRect.left - ControlsButtonSeparation - ControlButtonWidth, 0.0f, controlsCloseRect.left - ControlsButtonSeparation, sizeOfFrame.y());
            auto controlsMaximizeIconRect = Gfx::Rectangle(0, 0, 8, 8).CenterOn(controlsMaximizeRect.center());
            auto controlsMinimizeRect = Gfx::Rectangle(controlsMaximizeRect.left - ControlsButtonSeparation - ControlButtonWidth, 0.0f, controlsMaximizeRect.left - ControlsButtonSeparation, sizeOfFrame.y());
            auto controlsMinimizeIconRect = Gfx::Rectangle(0, 0, 8, 8).CenterOn(controlsMinimizeRect.center());

            commandBuffer->BeginBatch();

            // Draw window frame
            commandBuffer->FillRectangle(
                Gfx::Rectangle(0, 0, wwf, sizeOfFrame.y()),
                colorOfFrame);
            commandBuffer->FillRectangle(
                Gfx::Rectangle(wwf - sizeOfFrame.z(), sizeOfFrame.y(), wwf, whf - sizeOfFrame.w()),
                colorOfFrame);
            commandBuffer->FillRectangle(
                Gfx::Rectangle(0, whf - sizeOfFrame.w(), wwf, whf),
                colorOfFrame);
            commandBuffer->FillRectangle(
                Gfx::Rectangle(0, sizeOfFrame.y(), sizeOfFrame.x(), whf),
                colorOfFrame);

            commandBuffer->FillRectangle(
                controlsCloseRect,
                mouseOnClose ? Color::NordAuroraRed : colorOfFrame);
            commandBuffer->FillRectangle(
                controlsMaximizeRect,
                mouseOnMaximize ? Color::White.WithAlpha(0.5f) : colorOfFrame);
            commandBuffer->FillRectangle(
                controlsMinimizeRect,
                mouseOnMinimize ? Color::White.WithAlpha(0.5f) : colorOfFrame);

            commandBuffer->FillRectangle(controlsCloseIconRect, Color::White);
            commandBuffer->FillRectangle(controlsMaximizeIconRect, Color::White);
            commandBuffer->FillRectangle(controlsMinimizeIconRect, Color::White);

            static std::string statusText;
            static Gfx::Text::Layout statusLayout;

            // Update with absolute time and delta time
            statusText = std::format("Absolute Time: {:.2f}s, Delta Time: {:.2f}s", absoluteTime, deltaTime);
            gfx2D->GetDefaultShaper()->CalculateTextLayout(statusText, 1.f, Eigen::Vector2f(wwf, whf), true, statusLayout);
            commandBuffer->DrawText(statusLayout, Eigen::Vector2f(ControlsOffFromFrameSide + 2.f, sizeOfFrame.y() + 2.f), Color::White);

            commandBuffer->EndBatch();
        }
        gfx2D->SubmitCommandBuffer(commandBuffer);
    }

    Gfx::Surface Core::GetSurface(void_t *surfaceCreationHandle) const
    {
        Gfx::Surface surface;
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

    bool_t Core::WaitForEvents(double_t timeout_s) const
    {
        glfwWaitEventsTimeout(timeout_s);

        return ShouldExit() == false;
    }

    bool_t Core::ShouldExit() const
    {
        return glfwWindowShouldClose(glfwWindow) == GLFW_TRUE;
    }

    void Core::WakeUp()
    {
        glfwPostEmptyEvent();
    }
}