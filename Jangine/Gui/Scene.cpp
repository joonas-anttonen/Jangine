#include "Scene.hpp"

namespace Jangine::Gui
{
    void GuiButton::HandleMouseButton(UserInput::Digital button, UserInput::Action action, UserInput::Mods, Eigen::Vector2f)
    {
        if (button == UserInput::Digital::MOUSE_L)
        {
            if (action == UserInput::Action::PRESS)
            {
                isActive = true;
                scene.InvalidateLayout();

                if (onClick)
                    onClick(*this);
            }
            else if (action == UserInput::Action::RELEASE)
            {
                isActive = false;
                scene.InvalidateLayout();
            }
        }
    }

    void GuiSlider::HandleMouseButton(UserInput::Digital button, UserInput::Action action, UserInput::Mods, Eigen::Vector2f position)
    {
        if (button == UserInput::Digital::MOUSE_L)
        {
            if (action == UserInput::Action::PRESS)
            {
                dragStartPosition = GetLocalPosition(thumbArea.position() + thumbArea.extent() / 2.0f);

                if (!thumbArea.Contains(GetGlobalPosition(position)))
                {
                    UpdateValueFromPosition(position);
                }

                dragStartPosition = position;

                isActive = true;
                scene.InvalidateLayout();
            }
            else if (action == UserInput::Action::RELEASE)
            {
                isActive = false;
                scene.InvalidateLayout();
            }
        }
    }
}