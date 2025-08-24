#pragma once

#include "../Shared.hpp"
#include "../Core.hpp"
#include "../Gfx/Core.hpp"

typedef struct GLFWwindow GLFWwindow;

namespace Jangine::Gui
{
    struct JANGINE_API ApiParameters
    {
        bool_t enableDebugging = false;
        bool_t preferX11 = false;
    };

    struct JANGINE_API Parameters
    {
        std::string windowTitle = "Jangine Window";

        /// @brief Maybe the X position of the window
        std::optional<int32_t> X;
        /// @brief Maybe the Y position of the window
        std::optional<int32_t> Y;
        /// @brief Maybe the width of the window
        std::optional<int32_t> Width;
        /// @brief Maybe the height of the window
        std::optional<int32_t> Height;

        int32_t windowMonitor = 0;
    };

    class JANGINE_API Core
    {
        struct WindowFrameState
        {
        };

    public:
        Core(const ApiParameters &parameters, Gfx::Core *gfx);
        ~Core();

        Core &operator=(const Core &) = delete;
        Core(const Core &) = delete;
        Core &operator=(Core &&) = delete;
        Core(Core &&) = delete;

        void Create(const Parameters &parameters);
        Gfx::Surface GetSurface(void_t *surfaceCreationHandle) const;

        void Render(double_t absoluteTime, float_t deltaTime);

        bool_t ProcessEvents() const;
        bool_t WaitForEvents(double_t timeout_s) const;
        bool_t ShouldExit() const;
        static void WakeUp();

    private:
        static void HandleWindowResize(GLFWwindow *window, int width, int height);
        static void HandleMouseButton(GLFWwindow *window, int button, int action, int mods);
        static void HandleMouseMotion(GLFWwindow *window, double xpos, double ypos);
        static void HandleMouseEnter(GLFWwindow *window, int entered);
        static void HandleScroll(GLFWwindow *window, double xoffset, double yoffset);
        static void HandleKey(GLFWwindow *window, int key, int scancode, int action, int mods);

        bool_t mouseInsideWindow{false};
        Eigen::Vector2f mousePosition;

        GLFWwindow *glfwWindow = nullptr;

        Gfx::Core *gfx = nullptr;

        const Logging::Logger &logger;
    };
}