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

        std::optional<int32_t> windowX;
        std::optional<int32_t> windowY;
        std::optional<int32_t> windowWidth;
        std::optional<int32_t> windowHeight;

        int32_t windowMonitor = 0;
    };

    class JANGINE_API Core
    {

    public:
        Core(const ApiParameters &parameters, Gfx::Core *gfx);
        ~Core();

        void Create(const Parameters &parameters);
        Surface GetSurface(void_t *surfaceCreationHandle) const;

        bool_t ProcessEvents() const;
        bool_t ShouldExit() const;

    private:
        GLFWwindow *glfwWindow = nullptr;

        Gfx::Core *gfx = nullptr;

        const Logging::Logger &logger;
    };
}