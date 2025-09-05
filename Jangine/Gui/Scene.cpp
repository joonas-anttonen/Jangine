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

    void Slider::HandleMouseButton(UserInput::Digital button, UserInput::Action action, UserInput::Mods, Eigen::Vector2f position)
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

    ScrollView::ScrollView(std::type_index type, GuiScene &scene)
        : GuiNode(type, scene), horizontalSlider(*scene.CreateNode<Slider>()), verticalSlider(*scene.CreateNode<Slider>())
    {
        // Register scroll handler
        scene.mouseScrollHandlers.push_back(
            [this](Eigen::Vector2f offset, Eigen::Vector2f position)
            {
                std::cout << "ScrollView::HandleMouseScroll offset=" << offset.transpose() << " pos=" << position.transpose() << std::endl;

                bool_t shouldHandleHorizontal = horizontalSlider.isVisible && offset.x() != 0.0f;
                bool_t shouldHandleVertical = verticalSlider.isVisible && offset.y() != 0.0f;
                bool_t shouldHandleScroll = shouldHandleHorizontal || shouldHandleVertical;

                if (shouldHandleScroll && bounds.Contains(position))
                {
                    HandleMouseScroll(offset, position);
                    return true;
                }
                return false;
            });

        Style scrollViewBasicStyle{
            .backgroundColor = Color::Transparent,
            .foregroundColor = Color::White,
        };

        scene.SetAncestor(&horizontalSlider, this);
        scene.SetName(&horizontalSlider, "ScrollView::HSlider");
        horizontalSlider.orientation = Orientation::HORIZONTAL;
        horizontalSlider.isVisible = true;
        horizontalSlider.thumbSize = horizontalSlider.trackSize = 8.0f;
        horizontalSlider.SetStyle(scrollViewBasicStyle);
        horizontalSlider.onValueChanged = [this](Slider &, float_t value)
        {
            horizontalScroll = value;
        };
        scene.SetAncestor(&verticalSlider, this);
        scene.SetName(&verticalSlider, "ScrollView::VSlider");
        verticalSlider.orientation = Orientation::VERTICAL;
        verticalSlider.isVisible = true;
        verticalSlider.thumbSize = verticalSlider.trackSize = 8.0f;
        verticalSlider.SetStyle(scrollViewBasicStyle);
        verticalSlider.onValueChanged = [this](Slider &, float_t value)
        {
            verticalScroll = value;
        };
    }

    void ScrollView::UpdateLayout(Gfx::Core2D &gfx2D, Gfx::Rectangle area)
    {
        bounds = area;
        bounds = bounds.Crop(
            computedStyle.margin ? computedStyle.margin->left.value : 0.0f,
            computedStyle.margin ? computedStyle.margin->top.value : 0.0f,
            computedStyle.margin ? computedStyle.margin->right.value : 0.0f,
            computedStyle.margin ? computedStyle.margin->bottom.value : 0.0f);

        // Measure content size
        Eigen::Vector2f contentSize = {0.0f, 0.0f};
        for (auto &child : descendants)
        {
            if (child == &horizontalSlider || child == &verticalSlider)
                continue;

            auto childSize = child->Measure(gfx2D);
            contentSize = contentSize.cwiseMax(childSize);
        }

        // Determine if scrollbars are needed
        bool_t prevHVisible = horizontalSlider.isVisible;
        bool_t prevVVisible = verticalSlider.isVisible;
        horizontalSlider.isVisible = contentSize.x() > bounds.width();
        verticalSlider.isVisible = contentSize.y() > bounds.height();

        // Reset scroll position if scrollbar visibility changed
        if (prevHVisible != horizontalSlider.isVisible)
        {
            horizontalScroll = 0.0f;
            horizontalSlider.value = 0.0f;
        }
        if (prevVVisible != verticalSlider.isVisible)
        {
            verticalScroll = 0.0f;
            verticalSlider.value = 0.0f;
        }

        // Subtract scrollbar area from content area
        contentBounds = bounds.Crop(
            0,
            0,
            verticalSlider.isVisible ? verticalSlider.trackSize : 0,
            horizontalSlider.isVisible ? horizontalSlider.trackSize : 0);

        // Compute content offset based on scroll values
        Eigen::Vector2f sizeDifference = contentSize - contentBounds.extent();
        Eigen::Vector2f offset = {
            horizontalScroll * std::max(0.0f, sizeDifference.x()),
            verticalScroll * std::max(0.0f, sizeDifference.y())};
        offset = offset.array().round(); // Round scroll offset to avoid sub-pixel rendering issues
        auto scrolledContentBounds = contentBounds.Move(-offset);

        // Apply final layout
        Gfx::Rectangle horizontalSliderArea = Gfx::Rectangle(
            bounds.left,
            bounds.bottom - horizontalSlider.trackSize,
            bounds.right - verticalSlider.trackSize,
            bounds.bottom);
        Gfx::Rectangle verticalSliderArea = Gfx::Rectangle(
            bounds.right - verticalSlider.trackSize,
            bounds.top,
            bounds.right,
            bounds.bottom - horizontalSlider.trackSize);

        for (auto &child : descendants)
        {
            if (child == &horizontalSlider)
            {
                horizontalSlider.UpdateLayout(gfx2D, horizontalSliderArea);
            }
            else if (child == &verticalSlider)
            {
                verticalSlider.UpdateLayout(gfx2D, verticalSliderArea);
            }
            else
            {
                child->UpdateLayout(gfx2D, scrolledContentBounds);
            }
        }
    }

    void ScrollView::Render(Gfx::Core2D &gfx2D, Gfx::CommandBuffer2D *commandBuffer)
    {
        if (!isVisible)
            return;

        (void)gfx2D;
        (void)commandBuffer;

        commandBuffer->PushScissor(contentBounds);

        for (auto &child : descendants)
        {
            if (child == &horizontalSlider || child == &verticalSlider)
                continue;

            child->Render(gfx2D, commandBuffer);
        }

        commandBuffer->PopScissor();

        horizontalSlider.Render(gfx2D, commandBuffer);
        verticalSlider.Render(gfx2D, commandBuffer);
    }

    void ScrollView::HandleMouseScroll(Eigen::Vector2f offset, Eigen::Vector2f)
    {
        horizontalScroll = std::clamp(horizontalScroll - offset.x(), 0.0f, 1.0f);
        horizontalSlider.value = horizontalScroll;
        verticalScroll = std::clamp(verticalScroll - offset.y(), 0.0f, 1.0f);
        verticalSlider.value = verticalScroll;
        scene.InvalidateLayout();
    }
}