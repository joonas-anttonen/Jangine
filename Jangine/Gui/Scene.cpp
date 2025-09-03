#include "Scene.hpp"

namespace Jangine::Gui
{
    void GuiButton::HandleMouseButton(UserInput::Digital button, UserInput::Action action, UserInput::Mods)
    {
        if (button == UserInput::Digital::MOUSE_L)
        {
            if (action == UserInput::Action::PRESS)
            {
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