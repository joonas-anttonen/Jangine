#pragma once

#include "Shared.hpp"

namespace Jangine
{
    class UserInput
    {
    public:
        struct Event
        {
            enum class Type
            {
                MOUSE_MOVE,
                MOUSE_SCROLL,
                DIGITAL,
            };

            float_t field0;
            float_t field1;
            float_t field2;
            float_t field3;
            Type type;
        };

        enum class Mods
        {
            NONE = 0,
            SHIFT = 1,
            CONTROL = 2,
            ALT = 4,
            SUPER = 8,
        };

        enum class Action
        {
            UNKNOWN,
            RELEASE,
            PRESS,
            REPEAT,
        };

        enum class Analog
        {
            LEFT_X,
            LEFT_Y,
            RIGHT_X,
            RIGHT_Y,
            LT,
            RT,

            MOUSE_X,
            MOUSE_Y,
            MOUSE_WHEEL_X,
            MOUSE_WHEEL_Y,

            COUNT
        };

        enum class Digital
        {
            UNKNOWN,

            MOUSE_L,
            MOUSE_R,
            MOUSE_M,
            MOUSE_X4,
            MOUSE_X5,

            PAD_A,
            PAD_B,
            PAD_X,
            PAD_Y,
            PAD_BUMPER_L,
            PAD_BUMPER_R,
            PAD_BACK,
            PAD_START,
            PAD_GUIDE,
            PAD_THUMB_L,
            PAD_THUMB_R,
            PAD_UP,
            PAD_RIGHT,
            PAD_DOWN,
            PAD_LEFT,

            KEYBOARD_SPACE,
            KEYBOARD_APOSTROPHE,
            KEYBOARD_COMMA,
            KEYBOARD_MINUS,
            KEYBOARD_PERIOD,
            KEYBOARD_SLASH,
            KEYBOARD_N_0,
            KEYBOARD_N_1,
            KEYBOARD_N_2,
            KEYBOARD_N_3,
            KEYBOARD_N_4,
            KEYBOARD_N_5,
            KEYBOARD_N_6,
            KEYBOARD_N_7,
            KEYBOARD_N_8,
            KEYBOARD_N_9,
            KEYBOARD_SEMICOLON,
            KEYBOARD_EQUAL,
            KEYBOARD_A,
            KEYBOARD_B,
            KEYBOARD_C,
            KEYBOARD_D,
            KEYBOARD_E,
            KEYBOARD_F,
            KEYBOARD_G,
            KEYBOARD_H,
            KEYBOARD_I,
            KEYBOARD_J,
            KEYBOARD_K,
            KEYBOARD_L,
            KEYBOARD_M,
            KEYBOARD_N,
            KEYBOARD_O,
            KEYBOARD_P,
            KEYBOARD_Q,
            KEYBOARD_R,
            KEYBOARD_S,
            KEYBOARD_T,
            KEYBOARD_U,
            KEYBOARD_V,
            KEYBOARD_W,
            KEYBOARD_X,
            KEYBOARD_Y,
            KEYBOARD_Z,
            KEYBOARD_LEFT_BRACKET,
            KEYBOARD_BACKSLASH,
            KEYBOARD_RIGHT_BRACKET,
            KEYBOARD_GRAVE_ACCENT,
            KEYBOARD_WORLD_1,
            KEYBOARD_WORLD_2,
            KEYBOARD_ESCAPE,
            KEYBOARD_ENTER,
            KEYBOARD_TAB,
            KEYBOARD_BACKSPACE,
            KEYBOARD_INSERT,
            KEYBOARD_DELETE,
            KEYBOARD_RIGHT,
            KEYBOARD_LEFT,
            KEYBOARD_DOWN,
            KEYBOARD_UP,
            KEYBOARD_PAGE_UP,
            KEYBOARD_PAGE_DOWN,
            KEYBOARD_HOME,
            KEYBOARD_END,
            KEYBOARD_CAPS_LOCK,
            KEYBOARD_SCROLL_LOCK,
            KEYBOARD_NUM_LOCK,
            KEYBOARD_PRINT_SCREEN,
            KEYBOARD_PAUSE,
            KEYBOARD_SHIFT,
            KEYBOARD_CONTROL,
            KEYBOARD_ALT,
            KEYBOARD_SUPER,
            KEYBOARD_MENU,
            KEYBOARD_F1,
            KEYBOARD_F2,
            KEYBOARD_F3,
            KEYBOARD_F4,
            KEYBOARD_F5,
            KEYBOARD_F6,
            KEYBOARD_F7,
            KEYBOARD_F8,
            KEYBOARD_F9,
            KEYBOARD_F10,
            KEYBOARD_F11,
            KEYBOARD_F12,
            KEYBOARD_F13,
            KEYBOARD_F14,
            KEYBOARD_F15,
            KEYBOARD_F16,
            KEYBOARD_F17,
            KEYBOARD_F18,
            KEYBOARD_F19,
            KEYBOARD_F20,
            KEYBOARD_F21,
            KEYBOARD_F22,
            KEYBOARD_F23,
            KEYBOARD_F24,
            KEYBOARD_F25,
            KEYBOARD_KP_0,
            KEYBOARD_KP_1,
            KEYBOARD_KP_2,
            KEYBOARD_KP_3,
            KEYBOARD_KP_4,
            KEYBOARD_KP_5,
            KEYBOARD_KP_6,
            KEYBOARD_KP_7,
            KEYBOARD_KP_8,
            KEYBOARD_KP_9,
            KEYBOARD_KP_DECIMAL,
            KEYBOARD_KP_DIVIDE,
            KEYBOARD_KP_MULTIPLY,
            KEYBOARD_KP_SUBTRACT,
            KEYBOARD_KP_ADD,
            KEYBOARD_KP_ENTER,
            KEYBOARD_KP_EQUAL,

            COUNT
        };

        Eigen::Vector2f GetMoveDelta() const
        {
            Eigen::Vector2f mouseMoveDelta{0, 0};

            bool_t mouseMiddlePressed = digitalStates[static_cast<size_t>(Digital::MOUSE_M)];
            bool_t shiftPressed = digitalStates[static_cast<size_t>(Digital::KEYBOARD_SHIFT)];
            if (mouseMiddlePressed && shiftPressed)
            {
                // mouse movement
                float_t mouseX = analogStates[static_cast<size_t>(Analog::MOUSE_X)];
                float_t mouseY = analogStates[static_cast<size_t>(Analog::MOUSE_Y)];
                mouseMoveDelta = Eigen::Vector2f(-mouseX, -mouseY);
            }

            // keyboard wasd
            bool_t keyboardW = digitalStates[static_cast<size_t>(Digital::KEYBOARD_W)];
            bool_t keyboardA = digitalStates[static_cast<size_t>(Digital::KEYBOARD_A)];
            bool_t keyboardS = digitalStates[static_cast<size_t>(Digital::KEYBOARD_S)];
            bool_t keyboardD = digitalStates[static_cast<size_t>(Digital::KEYBOARD_D)];

            Eigen::Vector2f keyboardMoveDelta = Eigen::Vector2f(
                static_cast<float_t>(keyboardD - keyboardA),
                static_cast<float_t>(keyboardS - keyboardW));

            // Take the sum of mouse and keyboard movement
            return mouseMoveDelta + keyboardMoveDelta;
        }

        Eigen::Vector2f GetTurnDelta() const
        {
            Eigen::Vector2f mouseTurnDelta{0, 0};

            bool_t mouseMiddlePressed = digitalStates[static_cast<size_t>(Digital::MOUSE_M)];
            bool_t shiftPressed = digitalStates[static_cast<size_t>(Digital::KEYBOARD_SHIFT)];
            if (mouseMiddlePressed && !shiftPressed)
            {
                // mouse movement
                float_t mouseX = analogStates[static_cast<size_t>(Analog::MOUSE_X)];
                float_t mouseY = analogStates[static_cast<size_t>(Analog::MOUSE_Y)];
                mouseTurnDelta = Eigen::Vector2f(-mouseX, -mouseY);
            }

            bool_t keyboardUp = digitalStates[static_cast<size_t>(Digital::KEYBOARD_UP)];
            bool_t keyboardDown = digitalStates[static_cast<size_t>(Digital::KEYBOARD_DOWN)];
            bool_t keyboardLeft = digitalStates[static_cast<size_t>(Digital::KEYBOARD_LEFT)];
            bool_t keyboardRight = digitalStates[static_cast<size_t>(Digital::KEYBOARD_RIGHT)];

            Eigen::Vector2f keyboardPanDelta = Eigen::Vector2f(
                static_cast<float_t>(keyboardRight - keyboardLeft),
                static_cast<float_t>(keyboardDown - keyboardUp));

            // Take the sum of mouse and keyboard movement
            return mouseTurnDelta + keyboardPanDelta;
        }

        float_t GetZoomDelta() const
        {
            float_t mouseWheelDelta = analogStates[static_cast<size_t>(Analog::MOUSE_WHEEL_Y)];
            if (mouseWheelDelta != 0.0f)
            {
                // Zoom with mouse wheel
                return mouseWheelDelta * 100.0f;
            }

            // keyboard page up/down
            bool_t keyboardPageUp = digitalStates[static_cast<size_t>(Digital::KEYBOARD_PAGE_UP)];
            bool_t keyboardPageDown = digitalStates[static_cast<size_t>(Digital::KEYBOARD_PAGE_DOWN)];

            float_t keyboardZoomDelta = static_cast<float_t>(keyboardPageUp - keyboardPageDown);
            return keyboardZoomDelta;
        }

        auto ApplyEvent(UserInput::Event &event)
        {
            switch (event.type)
            {
            case UserInput::Event::Type::MOUSE_MOVE:
                mouseIsMoving = true;
                analogStates[static_cast<size_t>(Analog::MOUSE_X)] = event.field0;
                analogStates[static_cast<size_t>(Analog::MOUSE_Y)] = event.field1;
                break;
            case UserInput::Event::Type::MOUSE_SCROLL:
                mouseIsScrolling = true;
                analogStates[static_cast<size_t>(Analog::MOUSE_WHEEL_X)] = event.field0;
                analogStates[static_cast<size_t>(Analog::MOUSE_WHEEL_Y)] = event.field1;
                break;
            case UserInput::Event::Type::DIGITAL:
                digitalStates[static_cast<size_t>(static_cast<UserInput::Digital>(event.field0))] = static_cast<UserInput::Action>(event.field1) != UserInput::Action::RELEASE;
                break;
            default:
                break;
            }
        }

        auto Update(float_t deltaTime)
        {
            for (size_t i = 0; i < digitalStates.size(); ++i)
            {
                if (digitalStates[i])
                {
                    digitalDurations[i] = (digitalDurations[i] < 0.0f) ? 0.0f : digitalDurations[i] + deltaTime;
                }
                else
                {
                    digitalDurations[i] = (digitalDurations[i] > 0.0f) ? 0.0f : -1.0f;
                }
            }

            // Since scroll and move events never report a zero/stop state,
            // we reset their values if we have not seen a new event since last frame.

            if (!mouseIsScrolling)
            {
                analogStates[static_cast<size_t>(Analog::MOUSE_WHEEL_X)] = 0.0f;
                analogStates[static_cast<size_t>(Analog::MOUSE_WHEEL_Y)] = 0.0f;
            }

            if (!mouseIsMoving)
            {
                analogStates[static_cast<size_t>(Analog::MOUSE_X)] = 0.0f;
                analogStates[static_cast<size_t>(Analog::MOUSE_Y)] = 0.0f;
            }

            mouseIsScrolling = false;
            mouseIsMoving = false;
        }

    private:
        std::array<float_t, static_cast<size_t>(UserInput::Analog::COUNT)> analogStates = {};
        std::array<bool_t, static_cast<size_t>(UserInput::Digital::COUNT)> digitalStates = {};
        std::array<float_t, static_cast<size_t>(UserInput::Digital::COUNT)> digitalDurations = {};

        bool_t mouseIsMoving;
        bool_t mouseIsScrolling;
    };
}