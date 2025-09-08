#include "Core.hpp"
#include "../Gfx/Overlay.hpp"

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

    Core::Core(const ApiParameters &parameters, Gfx::Core &gfx)
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

    void Core::SynchronizeWithGfx(Eigen::Vector2i currentWindow)
    {
        Jangine::Core::GetInstance().PostToGfxThread(
            [this, currentWindow]()
            {
                Gfx::DisplayParameters newDisplayParameters = gfx.GetDisplayParameters();
                newDisplayParameters.surfaceWidth = currentWindow.x();
                newDisplayParameters.surfaceHeight = currentWindow.y();
                gfx.InitializeRendering(newDisplayParameters);
            });
    }

    void Core::HandleWindowResize(GLFWwindow *window, int width, int height)
    {
        if (width == 0 || height == 0)
        {
            return;
        }

        Core *core = reinterpret_cast<Core *>(glfwGetWindowUserPointer(window));
        if (core)
        {
            core->windowSize = Eigen::Vector2f(static_cast<float_t>(width), static_cast<float_t>(height));
            core->SynchronizeWithGfx(Eigen::Vector2i(width, height));
        }
    }

    static UserInput::Digital ConvertGlfwMouseButtonToJangine(int button)
    {
        switch (button)
        {
        case GLFW_MOUSE_BUTTON_LEFT:
            return UserInput::Digital::MOUSE_L;
        case GLFW_MOUSE_BUTTON_RIGHT:
            return UserInput::Digital::MOUSE_R;
        case GLFW_MOUSE_BUTTON_MIDDLE:
            return UserInput::Digital::MOUSE_M;
        case GLFW_MOUSE_BUTTON_4:
            return UserInput::Digital::MOUSE_X4;
        case GLFW_MOUSE_BUTTON_5:
            return UserInput::Digital::MOUSE_X5;
        default:
            return UserInput::Digital::UNKNOWN;
        }
    }

    static UserInput::Action ConvertGlfwActionToJangine(int action)
    {
        switch (action)
        {
        case GLFW_PRESS:
            return UserInput::Action::PRESS;
        case GLFW_RELEASE:
            return UserInput::Action::RELEASE;
        case GLFW_REPEAT:
            return UserInput::Action::REPEAT;
        default:
            return UserInput::Action::UNKNOWN;
        }
    }

    static UserInput::Digital ConvertGlfwKeyToJangine(int key)
    {
        switch (key)
        {
        case GLFW_KEY_SPACE:
            return UserInput::Digital::KEYBOARD_SPACE;
        case GLFW_KEY_APOSTROPHE:
            return UserInput::Digital::KEYBOARD_APOSTROPHE;
        case GLFW_KEY_COMMA:
            return UserInput::Digital::KEYBOARD_COMMA;
        case GLFW_KEY_MINUS:
            return UserInput::Digital::KEYBOARD_MINUS;
        case GLFW_KEY_PERIOD:
            return UserInput::Digital::KEYBOARD_PERIOD;
        case GLFW_KEY_SLASH:
            return UserInput::Digital::KEYBOARD_SLASH;
        case GLFW_KEY_0:
            return UserInput::Digital::KEYBOARD_N_0;
        case GLFW_KEY_1:
            return UserInput::Digital::KEYBOARD_N_1;
        case GLFW_KEY_2:
            return UserInput::Digital::KEYBOARD_N_2;
        case GLFW_KEY_3:
            return UserInput::Digital::KEYBOARD_N_3;
        case GLFW_KEY_4:
            return UserInput::Digital::KEYBOARD_N_4;
        case GLFW_KEY_5:
            return UserInput::Digital::KEYBOARD_N_5;
        case GLFW_KEY_6:
            return UserInput::Digital::KEYBOARD_N_6;
        case GLFW_KEY_7:
            return UserInput::Digital::KEYBOARD_N_7;
        case GLFW_KEY_8:
            return UserInput::Digital::KEYBOARD_N_8;
        case GLFW_KEY_9:
            return UserInput::Digital::KEYBOARD_N_9;
        case GLFW_KEY_SEMICOLON:
            return UserInput::Digital::KEYBOARD_SEMICOLON;
        case GLFW_KEY_EQUAL:
            return UserInput::Digital::KEYBOARD_EQUAL;
        case GLFW_KEY_A:
            return UserInput::Digital::KEYBOARD_A;
        case GLFW_KEY_B:
            return UserInput::Digital::KEYBOARD_B;
        case GLFW_KEY_C:
            return UserInput::Digital::KEYBOARD_C;
        case GLFW_KEY_D:
            return UserInput::Digital::KEYBOARD_D;
        case GLFW_KEY_E:
            return UserInput::Digital::KEYBOARD_E;
        case GLFW_KEY_F:
            return UserInput::Digital::KEYBOARD_F;
        case GLFW_KEY_G:
            return UserInput::Digital::KEYBOARD_G;
        case GLFW_KEY_H:
            return UserInput::Digital::KEYBOARD_H;
        case GLFW_KEY_I:
            return UserInput::Digital::KEYBOARD_I;
        case GLFW_KEY_J:
            return UserInput::Digital::KEYBOARD_J;
        case GLFW_KEY_K:
            return UserInput::Digital::KEYBOARD_K;
        case GLFW_KEY_L:
            return UserInput::Digital::KEYBOARD_L;
        case GLFW_KEY_M:
            return UserInput::Digital::KEYBOARD_M;
        case GLFW_KEY_N:
            return UserInput::Digital::KEYBOARD_N;
        case GLFW_KEY_O:
            return UserInput::Digital::KEYBOARD_O;
        case GLFW_KEY_P:
            return UserInput::Digital::KEYBOARD_P;
        case GLFW_KEY_Q:
            return UserInput::Digital::KEYBOARD_Q;
        case GLFW_KEY_R:
            return UserInput::Digital::KEYBOARD_R;
        case GLFW_KEY_S:
            return UserInput::Digital::KEYBOARD_S;
        case GLFW_KEY_T:
            return UserInput::Digital::KEYBOARD_T;
        case GLFW_KEY_U:
            return UserInput::Digital::KEYBOARD_U;
        case GLFW_KEY_V:
            return UserInput::Digital::KEYBOARD_V;
        case GLFW_KEY_W:
            return UserInput::Digital::KEYBOARD_W;
        case GLFW_KEY_X:
            return UserInput::Digital::KEYBOARD_X;
        case GLFW_KEY_Y:
            return UserInput::Digital::KEYBOARD_Y;
        case GLFW_KEY_Z:
            return UserInput::Digital::KEYBOARD_Z;
        case GLFW_KEY_LEFT_BRACKET:
            return UserInput::Digital::KEYBOARD_LEFT_BRACKET;
        case GLFW_KEY_BACKSLASH:
            return UserInput::Digital::KEYBOARD_BACKSLASH;
        case GLFW_KEY_RIGHT_BRACKET:
            return UserInput::Digital::KEYBOARD_RIGHT_BRACKET;
        case GLFW_KEY_GRAVE_ACCENT:
            return UserInput::Digital::KEYBOARD_GRAVE_ACCENT;
        case GLFW_KEY_WORLD_1:
            return UserInput::Digital::KEYBOARD_WORLD_1;
        case GLFW_KEY_WORLD_2:
            return UserInput::Digital::KEYBOARD_WORLD_2;
        case GLFW_KEY_ESCAPE:
            return UserInput::Digital::KEYBOARD_ESCAPE;
        case GLFW_KEY_ENTER:
            return UserInput::Digital::KEYBOARD_ENTER;
        case GLFW_KEY_TAB:
            return UserInput::Digital::KEYBOARD_TAB;
        case GLFW_KEY_BACKSPACE:
            return UserInput::Digital::KEYBOARD_BACKSPACE;
        case GLFW_KEY_INSERT:
            return UserInput::Digital::KEYBOARD_INSERT;
        case GLFW_KEY_DELETE:
            return UserInput::Digital::KEYBOARD_DELETE;
        case GLFW_KEY_RIGHT:
            return UserInput::Digital::KEYBOARD_RIGHT;
        case GLFW_KEY_LEFT:
            return UserInput::Digital::KEYBOARD_LEFT;
        case GLFW_KEY_DOWN:
            return UserInput::Digital::KEYBOARD_DOWN;
        case GLFW_KEY_UP:
            return UserInput::Digital::KEYBOARD_UP;
        case GLFW_KEY_PAGE_UP:
            return UserInput::Digital::KEYBOARD_PAGE_UP;
        case GLFW_KEY_PAGE_DOWN:
            return UserInput::Digital::KEYBOARD_PAGE_DOWN;
        case GLFW_KEY_HOME:
            return UserInput::Digital::KEYBOARD_HOME;
        case GLFW_KEY_END:
            return UserInput::Digital::KEYBOARD_END;
        case GLFW_KEY_CAPS_LOCK:
            return UserInput::Digital::KEYBOARD_CAPS_LOCK;
        case GLFW_KEY_SCROLL_LOCK:
            return UserInput::Digital::KEYBOARD_SCROLL_LOCK;
        case GLFW_KEY_NUM_LOCK:
            return UserInput::Digital::KEYBOARD_NUM_LOCK;
        case GLFW_KEY_PRINT_SCREEN:
            return UserInput::Digital::KEYBOARD_PRINT_SCREEN;
        case GLFW_KEY_PAUSE:
            return UserInput::Digital::KEYBOARD_PAUSE;
        case GLFW_KEY_F1:
            return UserInput::Digital::KEYBOARD_F1;
        case GLFW_KEY_F2:
            return UserInput::Digital::KEYBOARD_F2;
        case GLFW_KEY_F3:
            return UserInput::Digital::KEYBOARD_F3;
        case GLFW_KEY_F4:
            return UserInput::Digital::KEYBOARD_F4;
        case GLFW_KEY_F5:
            return UserInput::Digital::KEYBOARD_F5;
        case GLFW_KEY_F6:
            return UserInput::Digital::KEYBOARD_F6;
        case GLFW_KEY_F7:
            return UserInput::Digital::KEYBOARD_F7;
        case GLFW_KEY_F8:
            return UserInput::Digital::KEYBOARD_F8;
        case GLFW_KEY_F9:
            return UserInput::Digital::KEYBOARD_F9;
        case GLFW_KEY_F10:
            return UserInput::Digital::KEYBOARD_F10;
        case GLFW_KEY_F11:
            return UserInput::Digital::KEYBOARD_F11;
        case GLFW_KEY_F12:
            return UserInput::Digital::KEYBOARD_F12;
        case GLFW_KEY_F13:
            return UserInput::Digital::KEYBOARD_F13;
        case GLFW_KEY_F14:
            return UserInput::Digital::KEYBOARD_F14;
        case GLFW_KEY_F15:
            return UserInput::Digital::KEYBOARD_F15;
        case GLFW_KEY_F16:
            return UserInput::Digital::KEYBOARD_F16;
        case GLFW_KEY_F17:
            return UserInput::Digital::KEYBOARD_F17;
        case GLFW_KEY_F18:
            return UserInput::Digital::KEYBOARD_F18;
        case GLFW_KEY_F19:
            return UserInput::Digital::KEYBOARD_F19;
        case GLFW_KEY_F20:
            return UserInput::Digital::KEYBOARD_F20;
        case GLFW_KEY_F21:
            return UserInput::Digital::KEYBOARD_F21;
        case GLFW_KEY_F22:
            return UserInput::Digital::KEYBOARD_F22;
        case GLFW_KEY_F23:
            return UserInput::Digital::KEYBOARD_F23;
        case GLFW_KEY_F24:
            return UserInput::Digital::KEYBOARD_F24;
        case GLFW_KEY_F25:
            return UserInput::Digital::KEYBOARD_F25;
        case GLFW_KEY_KP_0:
            return UserInput::Digital::KEYBOARD_KP_0;
        case GLFW_KEY_KP_1:
            return UserInput::Digital::KEYBOARD_KP_1;
        case GLFW_KEY_KP_2:
            return UserInput::Digital::KEYBOARD_KP_2;
        case GLFW_KEY_KP_3:
            return UserInput::Digital::KEYBOARD_KP_3;
        case GLFW_KEY_KP_4:
            return UserInput::Digital::KEYBOARD_KP_4;
        case GLFW_KEY_KP_5:
            return UserInput::Digital::KEYBOARD_KP_5;
        case GLFW_KEY_KP_6:
            return UserInput::Digital::KEYBOARD_KP_6;
        case GLFW_KEY_KP_7:
            return UserInput::Digital::KEYBOARD_KP_7;
        case GLFW_KEY_KP_8:
            return UserInput::Digital::KEYBOARD_KP_8;
        case GLFW_KEY_KP_9:
            return UserInput::Digital::KEYBOARD_KP_9;
        case GLFW_KEY_KP_DECIMAL:
            return UserInput::Digital::KEYBOARD_KP_DECIMAL;
        case GLFW_KEY_KP_DIVIDE:
            return UserInput::Digital::KEYBOARD_KP_DIVIDE;
        case GLFW_KEY_KP_MULTIPLY:
            return UserInput::Digital::KEYBOARD_KP_MULTIPLY;
        case GLFW_KEY_KP_SUBTRACT:
            return UserInput::Digital::KEYBOARD_KP_SUBTRACT;
        case GLFW_KEY_KP_ADD:
            return UserInput::Digital::KEYBOARD_KP_ADD;
        case GLFW_KEY_KP_ENTER:
            return UserInput::Digital::KEYBOARD_KP_ENTER;
        case GLFW_KEY_KP_EQUAL:
            return UserInput::Digital::KEYBOARD_KP_EQUAL;
        case GLFW_KEY_LEFT_SHIFT:
        case GLFW_KEY_RIGHT_SHIFT:
            return UserInput::Digital::KEYBOARD_SHIFT;
        case GLFW_KEY_LEFT_CONTROL:
        case GLFW_KEY_RIGHT_CONTROL:
            return UserInput::Digital::KEYBOARD_CONTROL;
        case GLFW_KEY_LEFT_ALT:
        case GLFW_KEY_RIGHT_ALT:
            return UserInput::Digital::KEYBOARD_ALT;
        case GLFW_KEY_LEFT_SUPER:
        case GLFW_KEY_RIGHT_SUPER:
            return UserInput::Digital::KEYBOARD_SUPER;
        case GLFW_KEY_MENU:
            return UserInput::Digital::KEYBOARD_MENU;
        default:
            return UserInput::Digital::UNKNOWN;
        }
    }

    static UserInput::Mods ConvertGlfwModsToJangine(int mods)
    {
        UserInput::Mods jangineMods = UserInput::Mods::NONE;
        if (mods & GLFW_MOD_SHIFT)
        {
            jangineMods = static_cast<UserInput::Mods>(static_cast<uint32_t>(jangineMods) | static_cast<uint32_t>(UserInput::Mods::SHIFT));
        }
        if (mods & GLFW_MOD_CONTROL)
        {
            jangineMods = static_cast<UserInput::Mods>(static_cast<uint32_t>(jangineMods) | static_cast<uint32_t>(UserInput::Mods::CONTROL));
        }
        if (mods & GLFW_MOD_ALT)
        {
            jangineMods = static_cast<UserInput::Mods>(static_cast<uint32_t>(jangineMods) | static_cast<uint32_t>(UserInput::Mods::ALT));
        }
        if (mods & GLFW_MOD_SUPER)
        {
            jangineMods = static_cast<UserInput::Mods>(static_cast<uint32_t>(jangineMods) | static_cast<uint32_t>(UserInput::Mods::SUPER));
        }
        return jangineMods;
    }

    void Core::HandleMouseButton(GLFWwindow *window, int glfw_button, int glfw_action, int glfw_mods)
    {
        UserInput::Digital button = ConvertGlfwMouseButtonToJangine(glfw_button);
        UserInput::Action action = ConvertGlfwActionToJangine(glfw_action);
        UserInput::Mods mods = ConvertGlfwModsToJangine(glfw_mods);

        Core *core = reinterpret_cast<Core *>(glfwGetWindowUserPointer(window));
        if (core)
        {
            core->scene.HandleMouseButton(button, action, mods);
        }
    }

    void Core::HandleMouseMotion(GLFWwindow *window, double xpos, double ypos)
    {
        Eigen::Vector2f newMousePosition(static_cast<float_t>(xpos), static_cast<float_t>(ypos));

        Core *core = reinterpret_cast<Core *>(glfwGetWindowUserPointer(window));
        if (core)
        {
            core->scene.HandleMouseMotion(newMousePosition);
        }
    }

    void Core::HandleScroll(GLFWwindow *window, double xoffset, double yoffset)
    {
        Core *core = reinterpret_cast<Core *>(glfwGetWindowUserPointer(window));
        if (core)
        {
            core->scene.HandleMouseScroll(Eigen::Vector2f(static_cast<float_t>(xoffset), static_cast<float_t>(yoffset)) / 10.0f);
        }
    }

    void Core::HandleMouseEnter(GLFWwindow *window, int entered)
    {
        (void)entered; // Avoid unused parameter warning

        Core *core = reinterpret_cast<Core *>(glfwGetWindowUserPointer(window));
        if (core)
        {
            core->scene.HandleMouseEnter(entered != 0);
        }
    }

    void Core::HandleKey(GLFWwindow *window, int glfw_key, int, int glfw_action, int glfw_mods)
    {
        UserInput::Digital key = ConvertGlfwKeyToJangine(glfw_key);
        UserInput::Action action = ConvertGlfwActionToJangine(glfw_action);
        UserInput::Mods mods = ConvertGlfwModsToJangine(glfw_mods);

        Core *core = reinterpret_cast<Core *>(glfwGetWindowUserPointer(window));
        if (core)
        {
            core->scene.HandleKey(key, action, mods);
        }
    }

    void Core::SynchronizeWithGlfw()
    {
        if (glfwWindow)
        {
            int32_t x, y, width, height;
            glfwGetWindowPos(glfwWindow, &x, &y);
            glfwGetWindowSize(glfwWindow, &width, &height);
            windowSize = {static_cast<float_t>(width), static_cast<float_t>(height)};
        }
    }

    TreeView *treeView;

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

        int32_t windowWidth = parameters.Width.value_or(1280);
        int32_t windowHeight = parameters.Height.value_or(720);
        int32_t windowX = parameters.X.value_or(monitorX + (monitorWidth - windowWidth) / 2);
        int32_t windowY = parameters.Y.value_or(monitorY + (monitorHeight - windowHeight) / 2);

        GLFWwindow *window = glfwCreateWindow(windowWidth, windowHeight, parameters.windowTitle.c_str(), nullptr, nullptr);
        ThrowGlfwIfFailed("glfwCreateWindow");

        glfwSetWindowUserPointer(window, this);
        glfwSetWindowSizeCallback(window, HandleWindowResize);
        glfwSetMouseButtonCallback(window, HandleMouseButton);
        glfwSetCursorPosCallback(window, HandleMouseMotion);
        glfwSetCursorEnterCallback(window, HandleMouseEnter);
        glfwSetScrollCallback(window, HandleScroll);
        glfwSetKeyCallback(window, HandleKey);
        glfwSetWindowSizeLimits(window, 256, 144, -1, -1);

        int32_t platform = glfwGetPlatform();
        if (platform != GLFW_PLATFORM_WAYLAND)
        {
            glfwSetWindowPos(window, windowX, windowY);
        }

        this->glfwWindow = window;

        SynchronizeWithGlfw();

        // --------------------- TESTING
        viewport = scene.CreateNode<Viewport>();
        viewport->SetName("Viewport");
        viewport->gridCoordinates = {0, 0, 1, 1};

        auto grid = scene.CreateNode<Grid>();
        scene.SetAncestor(grid, viewport);
        grid->SetName("Grid");
        grid->SetColumnsAndRows(
            {
                {Unit::FRACTION, 1.0f},
                {Unit::PIXELS, 512.0f},
            },
            {
                {Unit::FRACTION, 1.0f},
            });

        auto acrylic = scene.CreateNode<AcrylicPanel>();
        acrylic->SetName("AcrylicPanel");
        acrylic->gridCoordinates = {1, 0, 1, 1};
        scene.SetAncestor(acrylic, grid);

        auto gridRightPanel = scene.CreateNode<Grid>();
        gridRightPanel->SetName("GridRightPanel");
        gridRightPanel->gridCoordinates = {1, 0, 1, 1};
        scene.SetAncestor(gridRightPanel, grid);
        gridRightPanel->SetColumnsAndRows(
            {
                {Unit::FRACTION, 1.0f},
            },
            {
                {Unit::PIXELS, 32.0f},
                {Unit::PIXELS, 32.0f},
                {Unit::PIXELS, 32.0f},
                {Unit::PIXELS, 32.0f},
                {Unit::PIXELS, 32.0f},
                {Unit::FRACTION, 1.0f},
            });

        Style buttonBasicStyle{
            .backgroundColor = Color::FromUInt(0x3a3f4b),
            .foregroundColor = Color::FromUInt(0xabb2bf),
            .margin = Spacing{
                .left = {Unit::PIXELS, 4.0f},
                .top = {Unit::PIXELS, 4.0f},
                .right = {Unit::PIXELS, 4.0f},
                .bottom = {Unit::PIXELS, 4.0f},
            }};
        Style buttonHoveredStyle{
            .backgroundColor = Color::FromUInt(0x4b5160),
        };
        Style buttonActiveStyle{
            .backgroundColor = Color::FromUInt(0x282c34),
        };

        for (uint32_t i = 0; i < 3; ++i)
        {
            auto button = scene.CreateNode<GuiButton>();
            button->SetName(std::format("Button_{:03d}", i + 1));
            button->SetStyle(buttonBasicStyle);
            button->SetHoveredStyle(buttonHoveredStyle);
            button->SetActiveStyle(buttonActiveStyle);
            button->gridCoordinates = {0, i, 1, 1};
            scene.SetAncestor(button, gridRightPanel);
            button->onClick = [this](GuiButton &b)
            {
                logger.Warning(std::format("{} clicked", b.GetName()), __func__);
            };
        }

        Style sliderBasicStyle{
            .backgroundColor = Color::FromUInt(0x3a3f4b),
            .foregroundColor = Color::FromUInt(0xabb2bf),
            .margin = Spacing{
                .left = {Unit::PIXELS, 4.0f},
                .top = {Unit::PIXELS, 4.0f},
                .right = {Unit::PIXELS, 4.0f},
                .bottom = {Unit::PIXELS, 4.0f},
            }};
        Style sliderHoveredStyle{
            .backgroundColor = Color::FromUInt(0x4b5160),
            .foregroundColor = Color::FromUInt(0x61afef),
        };
        Style sliderActiveStyle{
            .foregroundColor = Color::FromUInt(0xc678dd),
        };

        auto biasSlider = scene.CreateNode<Slider>();
        biasSlider->SetName("BiasSlider_001");
        biasSlider->SetStyle(sliderBasicStyle);
        biasSlider->SetHoveredStyle(sliderHoveredStyle);
        biasSlider->SetActiveStyle(sliderActiveStyle);
        biasSlider->gridCoordinates = {0, 3, 1, 1};
        scene.SetAncestor(biasSlider, gridRightPanel);
        biasSlider->value = 0.0f;
        biasSlider->onValueChanged = [this](Slider &s, float_t value)
        {
            logger.Warning(std::format("{} value changed: {}", s.GetName(), value), __func__);
        };

        auto slider = scene.CreateNode<Slider>();
        slider->SetName("Slider_001");
        slider->SetStyle(sliderBasicStyle);
        slider->SetHoveredStyle(sliderHoveredStyle);
        slider->SetActiveStyle(sliderActiveStyle);
        slider->gridCoordinates = {0, 4, 1, 1};
        scene.SetAncestor(slider, gridRightPanel);
        slider->value = 0.1f;
        slider->onValueChanged = [this](Slider &s, float_t value)
        {
            logger.Warning(std::format("{} value changed: {}", s.GetName(), value), __func__);
        };

        auto treeViewScrollView = scene.CreateNode<ScrollView>();
        treeViewScrollView->SetName("ScrollView");
        treeViewScrollView->gridCoordinates = {0, 5, 1, 1};
        scene.SetAncestor(treeViewScrollView, gridRightPanel);

        Style treeViewBasicStyle{
            .backgroundColor = Color::Transparent,
            .foregroundColor = Color::FromUInt(0xabb2bf),
        };

        treeView = scene.CreateNode<TreeView>();
        treeView->SetName("TreeView_001");
        treeView->SetStyle(treeViewBasicStyle);
        scene.SetAncestor(treeView, treeViewScrollView);

        scene.Print(scene.GetRoot());
        //  --------------------- TESTING
    }

    Gfx::Scene::View sceneView;

    void Core::Render(double_t absoluteTime, float_t deltaTime)
    {
        SynchronizeWithGlfw();

        Gfx::Core3D &core3D = gfx.GetCore3D();
        auto &overlay = gfx.GetCore2D();
        Gfx::Overlay::CommandBuffer *commandBuffer = nullptr;

        scene.UpdateLayout(overlay, Gfx::Rectangle(0, 0, windowSize.x(), windowSize.y()));
        gfx.UnsafeSetViewport(viewport->bounds);

        core3D.GetScene().GetView(sceneView);
        if (treeView->items.empty())
        {
            // Duplicate scene hierarchy to treeView

            std::function<void(const Gfx::Scene::View::Node &, TreeView::Item &)> duplicateNodeToTreeView;
            duplicateNodeToTreeView = [&](const Gfx::Scene::View::Node &node, TreeView::Item &item)
            {
                item.text = std::format("{:03d} {}", node.self.value, core3D.GetScene().GetName(node.self).value_or("NO_NAME"));
                item.isExpanded = true;

                if (node.descendant)
                {
                    auto childNode = sceneView.at(node.descendant);
                    while (childNode.self)
                    {
                        item.children.emplace_back();
                        duplicateNodeToTreeView(childNode, item.children.back());
                        if (!childNode.sibling)
                        {
                            break;
                        }
                        childNode = sceneView.at(childNode.sibling);
                    }
                }
            };

            auto &nodes = sceneView.nodes;
            for (const auto &node : nodes)
            {
                if (!node.ancestor)
                {
                    treeView->items.emplace_back();
                    duplicateNodeToTreeView(node, treeView->items.back());
                }
            }
        }

        bool_t acquiredCommandBuffer = overlay.TryAcquireCommandBuffer(&commandBuffer);
        if (!acquiredCommandBuffer)
        {
            // logger.Warning("Failed to acquire command buffer", __func__);
            return;
        }

        {
            static const Eigen::Vector4f sizeOfFrame = Eigen::Vector4f(2.0f, 32.0f, 2.0f, 2.0f);
            static const Color colorOfFrame = Color::FromUInt(0x282c34);

            commandBuffer->BeginBatch();

            static std::string statusText;
            static Gfx::Text::Layout statusTextLayout;

            // Update with absolute time and delta time
            statusText = std::format(
                "Absolute Time: {:.2f}s, Delta Time: {:.2f}s",
                absoluteTime,
                deltaTime);
            overlay.GetDefaultFont()->CalculateTextLayout(statusText, windowSize, false, {.scale = 0.5f, .width = 0.125f}, statusTextLayout);

            Eigen::Vector2f statusTextMargin = Eigen::Vector2f(2.f, 2.f);
            Eigen::Vector2f statusTextPosition = Eigen::Vector2f(sizeOfFrame.x() + statusTextMargin.x(), sizeOfFrame.y() + statusTextMargin.y());

            if (overlay.IsReady())
            {
                commandBuffer->DrawImage(
                    overlay.GetAcrylicBuffer(),
                    Eigen::Vector2f(sizeOfFrame.x(), sizeOfFrame.y()),
                    Eigen::Vector2f(statusTextLayout.extent().x() + statusTextMargin.x() * 2, statusTextLayout.extent().y() + statusTextMargin.y() * 2),
                    Gfx::ImageFit::None,
                    Color::White);
            }

            commandBuffer->DrawText(statusTextLayout, statusTextPosition, Color{1.f, 1.f, 1.f, 1.f});

            scene.Render(overlay, commandBuffer);

            commandBuffer->EndBatch();
        }
        overlay.SubmitCommandBuffer(commandBuffer);
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