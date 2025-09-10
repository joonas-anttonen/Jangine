#pragma once

#include "../Shared.hpp"
#include "../Core.hpp"
#include "../Gfx/Core.hpp"
#include "../Gfx/Overlay.hpp"
#include "../Gfx/Core3D.hpp"

namespace Jangine::Gui
{
    struct Scene;

    enum class Unit
    {
        AUTO,
        PIXELS,
        FRACTION,
        // EM,
    };

    enum class Orientation
    {
        HORIZONTAL,
        VERTICAL,
    };

    struct Measure
    {
        Unit unit = Unit::FRACTION;
        float_t value = 1.0f;
    };

    struct Spacing
    {
        Measure left{Unit::PIXELS, 0.0f};
        Measure top{Unit::PIXELS, 0.0f};
        Measure right{Unit::PIXELS, 0.0f};
        Measure bottom{Unit::PIXELS, 0.0f};
    };

    /// @brief Style settings for GUI elements. Defined either fully or partially. Applied to elements as needed.
    struct Style
    {
        std::optional<Color> backgroundColor;
        std::optional<Color> foregroundColor;
        std::optional<Spacing> margin;
        std::optional<Spacing> padding;

        void Apply(const Style &other)
        {
            if (other.backgroundColor)
                backgroundColor = other.backgroundColor;
            if (other.foregroundColor)
                foregroundColor = other.foregroundColor;
            if (other.margin)
                margin = other.margin;
            if (other.padding)
                padding = other.padding;
        }
    };

    struct Node
    {
        struct Id
        {
            Id() : value(-1) {}
            template <typename T>
            Id(T t) : value(static_cast<int32_t>(t)) {}
            operator size_t() const { return static_cast<size_t>(value); }

            bool operator==(const Id &other) const { return value == other.value; }
            bool operator!=(const Id &other) const { return value != other.value; }

            int32_t value;
        };

        struct GridCoordinates
        {
            size_t column = 0;
            size_t row = 0;
            size_t columnSpan = 1;
            size_t rowSpan = 1;
        };

        GridCoordinates gridCoordinates;

        std::type_index type;
        Scene &scene;

        std::string name;
        Node *ancestor = nullptr;
        std::vector<Node *> descendants;

        Gfx::Rectangle bounds;
        Color backgroundColor{Color::Black};
        Color foregroundColor{Color::White};

        bool_t isVisible = true;
        bool_t isEnabled = true;
        bool_t isActive = false;
        bool_t isHovered = false;
        bool_t isFocused = false;

        bool_t isFocusable = true;

        // --------------------- STYLE

        Style style;
        std::optional<Style> enabledStyle;
        std::optional<Style> disabledStyle;
        std::optional<Style> activeStyle;
        std::optional<Style> hoveredStyle;
        std::optional<Style> focusedStyle;
        Style computedStyle;

        // --------------------- STYLE

        explicit Node(std::type_index type, Scene &scene)
            : type(type), scene(scene)
        {
        }

        std::string GetName() const
        {
            return name;
        }
        void SetName(const std::string &in_name)
        {
            this->name = in_name;
        }

        const std::vector<Node *> &GetDescendants() const
        {
            return descendants;
        }

        virtual void UpdateLayout(Gfx::Overlay &overlay, Gfx::Rectangle area)
        {
            bounds = area;
            bounds = bounds.Crop(
                computedStyle.margin ? computedStyle.margin->left.value : 0.0f,
                computedStyle.margin ? computedStyle.margin->top.value : 0.0f,
                computedStyle.margin ? computedStyle.margin->right.value : 0.0f,
                computedStyle.margin ? computedStyle.margin->bottom.value : 0.0f);

            for (auto &child : descendants)
            {
                child->UpdateLayout(overlay, bounds);
            }
        }

        virtual void Render(Gfx::Overlay &overlay, Gfx::Overlay::CommandBuffer *commandBuffer)
        {
            if (!isVisible)
                return;

            for (auto &child : descendants)
            {
                child->Render(overlay, commandBuffer);
            }
        }

        // --------------------- STYLE

        /// @brief Sets the base style for the GUI node.
        void SetStyle(const Style &in_style)
        {
            style = in_style;
        }

        void SetEnabledStyle(const Style &in_style)
        {
            enabledStyle = in_style;
        }

        void SetDisabledStyle(const Style &in_style)
        {
            disabledStyle = in_style;
        }

        void SetActiveStyle(const Style &in_style)
        {
            activeStyle = in_style;
        }

        void SetHoveredStyle(const Style &in_style)
        {
            hoveredStyle = in_style;
        }

        void SetFocusedStyle(const Style &in_style)
        {
            focusedStyle = in_style;
        }

        /// @brief Computes the effective style for the GUI node, applying conditional styles as needed.
        void ComputeStyle()
        {
            computedStyle = style;

            // Apply conditional styles
            if (isEnabled && enabledStyle)
                computedStyle.Apply(*enabledStyle);
            else if (!isEnabled && disabledStyle)
                computedStyle.Apply(*disabledStyle);
            if (isHovered && hoveredStyle)
                computedStyle.Apply(*hoveredStyle);
            if (isFocused && focusedStyle)
                computedStyle.Apply(*focusedStyle);
            if (isActive && activeStyle)
                computedStyle.Apply(*activeStyle);

            // Commit computed style to actual properties
            if (computedStyle.backgroundColor)
                backgroundColor = *computedStyle.backgroundColor;
            if (computedStyle.foregroundColor)
                foregroundColor = *computedStyle.foregroundColor;

            for (auto &child : descendants)
            {
                child->ComputeStyle();
            }
        }

        // --------------------- STYLE

        std::optional<Node *> ContainsPoint(const Eigen::Vector2f &point)
        {
            bool_t isTestable = isVisible && isEnabled;
            if (!isTestable)
                return std::nullopt;

            auto contains = bounds.Contains(point);
            if (contains)
            {
                for (int32_t i = static_cast<int32_t>(descendants.size()) - 1; i >= 0; --i)
                {
                    auto result = descendants[i]->ContainsPoint(point);
                    if (result)
                        return result;
                }

                if (isFocusable)
                    return this;
            }

            return std::nullopt;
        }

        virtual void HandleMouseFocus(bool_t)
        {
        }

        virtual void HandleKeyboardFocus(bool_t)
        {
        }

        virtual void HandleMouseMotion(Eigen::Vector2f, Eigen::Vector2f)
        {
        }

        virtual void HandleMouseButton(UserInput::Digital, UserInput::Action, UserInput::Mods, Eigen::Vector2f)
        {
        }

        virtual void HandleMouseScroll(Eigen::Vector2f, Eigen::Vector2f)
        {
        }

        virtual void HandleKey(UserInput::Digital, UserInput::Action, UserInput::Mods)
        {
        }

        Eigen::Vector2f GetLocalPosition(const Eigen::Vector2f &position) const
        {
            return position - bounds.position();
        }

        Eigen::Vector2f GetGlobalPosition(const Eigen::Vector2f &localPosition) const
        {
            return localPosition + bounds.position();
        }

        virtual Eigen::Vector2f Measure(Gfx::Overlay &)
        {
            return Eigen::Vector2f{0.0f, 0.0f};
        }
    };

    struct GuiButton : public Node
    {
        std::function<void(GuiButton &)> onClick;

        explicit GuiButton(std::type_index type, Scene &scene)
            : Node(type, scene)
        {
        }

        void Render(Gfx::Overlay &overlay, Gfx::Overlay::CommandBuffer *commandBuffer) override
        {
            if (!isVisible)
                return;

            commandBuffer->FillRectangle(bounds, backgroundColor);

            Node::Render(overlay, commandBuffer);
        }

        void HandleMouseButton(UserInput::Digital button, UserInput::Action action, UserInput::Mods, Eigen::Vector2f) override;
    };

    /// @brief A viewport for 3D content.
    struct Viewport : public Node
    {
        explicit Viewport(std::type_index type, Scene &scene)
            : Node(type, scene)
        {
        }

        void HandleMouseFocus(bool_t focused) override;

        void HandleMouseMotion(Eigen::Vector2f, Eigen::Vector2f delta) override
        {
            UserInput::Event event;
            event.type = UserInput::Event::Type::MOUSE_MOVE;
            event.field0 = delta.x();
            event.field1 = delta.y();
            Jangine::Core::GetInstance().PostUserInput(event);
        }

        void HandleMouseButton(UserInput::Digital button, UserInput::Action action, UserInput::Mods mods, Eigen::Vector2f) override
        {
            UserInput::Event event;
            event.type = UserInput::Event::Type::DIGITAL;
            event.field0 = static_cast<float_t>(button);
            event.field1 = static_cast<float_t>(action);
            event.field2 = static_cast<float_t>(mods);
            Jangine::Core::GetInstance().PostUserInput(event);
        }

        void HandleMouseScroll(Eigen::Vector2f offset, Eigen::Vector2f) override
        {
            UserInput::Event event;
            event.type = UserInput::Event::Type::MOUSE_SCROLL;
            event.field0 = offset.x();
            event.field1 = offset.y();
            Jangine::Core::GetInstance().PostUserInput(event);
        }

        void HandleKey(UserInput::Digital key, UserInput::Action action, UserInput::Mods mods) override
        {
            UserInput::Event event;
            event.type = UserInput::Event::Type::DIGITAL;
            event.field0 = static_cast<float_t>(key);
            event.field1 = static_cast<float_t>(action);
            event.field2 = static_cast<float_t>(mods);
            Jangine::Core::GetInstance().PostUserInput(event);
        }
    };

    struct Slider;

    struct ScrollView : public Node
    {
        explicit ScrollView(std::type_index type, Scene &scene);

        void UpdateLayout(Gfx::Overlay &overlay, Gfx::Rectangle area) override;

        void Render(Gfx::Overlay &overlay, Gfx::Overlay::CommandBuffer *commandBuffer) override;

        Slider &horizontalSlider;
        Slider &verticalSlider;

        float_t horizontalScroll = 0.0f; // 0.0 - 1.0
        float_t verticalScroll = 0.0f;   // 0.0 - 1.0

        Gfx::Rectangle contentBounds;

        void HandleMouseScroll(Eigen::Vector2f, Eigen::Vector2f) override;
    };

    struct TreeView : public Node
    {
        struct Item
        {
            std::string text;
            std::vector<Item> children;
            bool_t isExpanded = false;
            bool_t isSelected = false;
            void *userData = nullptr;

            Gfx::Rectangle bounds;
            Gfx::Text::Layout textLayout;
        };

        std::vector<Item> items;
        float_t itemHeight = 24.0f;
        float_t indentSize = 12.0f;

        explicit TreeView(std::type_index type, Scene &scene)
            : Node(type, scene)
        {
        }

        Eigen::Vector2f Measure(Gfx::Overlay &overlay) override
        {
            float_t width = 0.0f;
            float_t height = 0.0f;
            float_t currentY = 0.0f;

            // TODO: We CAN know the available area here, would reduce the amount of text layouting we do
            std::function<void(std::vector<Item> &, float_t)> layoutItems = [&](std::vector<Item> &items, float_t indent)
            {
                for (auto &item : items)
                {
                    overlay.GetDefaultFont()->CalculateTextLayout(
                        item.text,
                        Eigen::Vector2f(9999.0f, 9999.0f),
                        Gfx::Text::Wrap::None,
                        {.scale = 0.5f,
                         .width = 0.125f},
                        item.textLayout);
                    item.bounds = Gfx::Rectangle(indent, currentY, indent + item.textLayout.extent().x(), currentY + itemHeight);
                    width = std::max(width, item.bounds.right);
                    height = std::max(height, item.bounds.bottom);
                    item.bounds = item.bounds.Crop(1, 1, 1, 1);
                    currentY += itemHeight;
                    if (item.isExpanded && !item.children.empty())
                    {
                        layoutItems(item.children, indent + indentSize);
                    }
                }
            };
            layoutItems(items, 0.0f);

            return {width, height};
        }

        void UpdateLayout(Gfx::Overlay &overlay, Gfx::Rectangle area) override
        {
            auto measuredSize = Measure(overlay);
            (void)measuredSize;

            Node::UpdateLayout(overlay, area);
        }

        void Render(Gfx::Overlay &overlay, Gfx::Overlay::CommandBuffer *commandBuffer) override
        {
            if (!isVisible)
                return;

            commandBuffer->FillRectangle(bounds, backgroundColor);

            // TODO: Don't render items outside of current scissor
            std::function<void(const std::vector<Item> &)> renderItems = [&](const std::vector<Item> &items)
            {
                for (const auto &item : items)
                {
                    Color itemColor = item.isSelected ? foregroundColor.WithAlpha(0.5f) : backgroundColor;
                    commandBuffer->FillRectangle(item.bounds.Move(bounds.position()), itemColor);
                    commandBuffer->DrawText(
                        item.textLayout,
                        item.bounds.position() + bounds.position(),
                        Color::White);

                    if (item.isExpanded && !item.children.empty())
                    {
                        renderItems(item.children);
                    }
                }
            };
            renderItems(items);

            Node::Render(overlay, commandBuffer);
        }
    };

    struct Slider : public Node
    {
        Orientation orientation = Orientation::HORIZONTAL;

        float_t thumbSize = 16.0f;
        float_t trackSize = 8.0f;
        float_t value = 0.5f; // 0.0 - 1.0

        Gfx::Rectangle thumbArea;
        Gfx::Rectangle trackArea;

        Eigen::Vector2f dragStartPosition;

        std::function<void(Slider &, float_t)> onValueChanged;

        explicit Slider(std::type_index type, Scene &scene)
            : Node(type, scene)
        {
        }

        void HandleMouseButton(UserInput::Digital button, UserInput::Action action, UserInput::Mods, Eigen::Vector2f position) override;
        void HandleMouseMotion(Eigen::Vector2f position, Eigen::Vector2f) override
        {
            if (isActive)
            {
                UpdateValueFromPosition(position);
            }
        }

        void UpdateValueFromPosition(Eigen::Vector2f position)
        {
            float_t previousValue = value;

            if (orientation == Orientation::HORIZONTAL)
            {
                float_t delta = position.x() - dragStartPosition.x();
                float_t sliderRange = bounds.width() - thumbSize;
                float_t newValue = (thumbArea.left - bounds.left + delta) / sliderRange;
                value = std::clamp(newValue, 0.0f, 1.0f);
            }
            else
            {
                float_t delta = position.y() - dragStartPosition.y();
                float_t sliderRange = bounds.height() - thumbSize;
                float_t newValue = (thumbArea.top - bounds.top + delta) / sliderRange;
                value = std::clamp(newValue, 0.0f, 1.0f);
            }

            if (value != previousValue)
            {
                dragStartPosition = position;

                if (onValueChanged)
                    onValueChanged(*this, value);
            }
        }

        void UpdateLayout(Gfx::Overlay &overlay, Gfx::Rectangle area) override
        {
            bounds = area;
            bounds = bounds.Crop(
                computedStyle.margin ? computedStyle.margin->left.value : 0.0f,
                computedStyle.margin ? computedStyle.margin->top.value : 0.0f,
                computedStyle.margin ? computedStyle.margin->right.value : 0.0f,
                computedStyle.margin ? computedStyle.margin->bottom.value : 0.0f);

            if (orientation == Orientation::HORIZONTAL)
            {
                thumbArea = Gfx::Rectangle(
                    bounds.left + (bounds.width() - thumbSize) * value,
                    bounds.center().y() - thumbSize / 2.0f,
                    bounds.left + (bounds.width() - thumbSize) * value + thumbSize,
                    bounds.center().y() + thumbSize / 2.0f);

                trackArea = Gfx::Rectangle(
                    bounds.left + thumbSize / 2.0f,
                    bounds.center().y() - trackSize / 2.0f,
                    bounds.right - thumbSize / 2.0f,
                    bounds.center().y() + trackSize / 2.0f);
            }
            else
            {
                thumbArea = Gfx::Rectangle(
                    bounds.center().x() - thumbSize / 2.0f,
                    bounds.top + (bounds.height() - thumbSize) * value,
                    bounds.center().x() + thumbSize / 2.0f,
                    bounds.top + (bounds.height() - thumbSize) * value + thumbSize);

                trackArea = Gfx::Rectangle(
                    bounds.center().x() - trackSize / 2.0f,
                    bounds.top + thumbSize / 2.0f,
                    bounds.center().x() + trackSize / 2.0f,
                    bounds.bottom - thumbSize / 2.0f);
            }

            Node::UpdateLayout(overlay, area);
        }

        void Render(Gfx::Overlay &overlay, Gfx::Overlay::CommandBuffer *commandBuffer) override
        {
            if (!isVisible)
                return;

            commandBuffer->FillRectangle(trackArea, backgroundColor);
            commandBuffer->FillRectangle(thumbArea, foregroundColor);

            Node::Render(overlay, commandBuffer);
        }
    };

    struct AcrylicPanel : public Node
    {
        explicit AcrylicPanel(std::type_index type, Scene &scene)
            : Node(type, scene)
        {
            isFocusable = false;
        }

        void Render(Gfx::Overlay &overlay, Gfx::Overlay::CommandBuffer *commandBuffer) override
        {
            if (!isVisible)
                return;

            if (overlay.IsReady())
            {
                commandBuffer->DrawImage(
                    overlay.GetAcrylicBuffer(),
                    bounds.position(),
                    bounds.extent(),
                    Gfx::ImageFit::None,
                    Color::White);
            }

            for (auto &child : descendants)
            {
                child->Render(overlay, commandBuffer);
            }
        }
    };

    struct GuiRoot : public Node
    {
        enum class FrameMode
        {
            NONE,
            TITLE,
            FULL,
        };

        explicit GuiRoot(std::type_index type, Scene &scene)
            : Node(type, scene)
        {
        }

        Eigen::Vector4f frameSize{2.0f, 32.0f, 2.0f, 2.0f};
        FrameMode frameMode = FrameMode::FULL;
        Color frameColor{Color::FromUInt(0x282c34)};

        FrameMode GetFrameMode() const
        {
            return frameMode;
        }
        void SetFrameMode(FrameMode mode)
        {
            frameMode = mode;
        }
        Eigen::Vector4f GetFrameSize() const
        {
            return frameSize;
        }
        void SetFrameSize(const Eigen::Vector4f &size)
        {
            frameSize = size;
        }
        Color GetFrameColor() const
        {
            return frameColor;
        }
        void SetFrameColor(const Color &color)
        {
            frameColor = color;
        }

        void UpdateLayout(Gfx::Overlay &overlay, Gfx::Rectangle area) override
        {
            bounds = area;

            Gfx::Rectangle contentArea = area;

            if (frameMode == FrameMode::TITLE)
            {
                contentArea.top += frameSize.y();
            }
            else if (frameMode == FrameMode::FULL)
            {
                contentArea.left += frameSize.x();
                contentArea.top += frameSize.y();
                contentArea.right -= frameSize.z();
                contentArea.bottom -= frameSize.w();
            }

            for (auto &child : descendants)
            {
                child->UpdateLayout(overlay, contentArea);
            }
        }

        void Render(Gfx::Overlay &overlay, Gfx::Overlay::CommandBuffer *commandBuffer) override
        {
            if (frameMode != FrameMode::NONE)
            {
                commandBuffer->FillRectangle(
                    Gfx::Rectangle(0, 0, bounds.width(), frameSize.y()),
                    frameColor);

                if (frameMode == FrameMode::FULL)
                {
                    commandBuffer->FillRectangle(
                        Gfx::Rectangle(bounds.width() - frameSize.z(), frameSize.y(), bounds.width(), bounds.height() - frameSize.w()),
                        frameColor);
                    commandBuffer->FillRectangle(
                        Gfx::Rectangle(0, bounds.height() - frameSize.w(), bounds.width(), bounds.height()),
                        frameColor);
                    commandBuffer->FillRectangle(
                        Gfx::Rectangle(0, frameSize.y(), frameSize.x(), bounds.height()),
                        frameColor);
                }
            }

            for (auto &child : descendants)
            {
                child->Render(overlay, commandBuffer);
            }
        }
    };

    struct Grid : public Node
    {
        struct Column
        {
            Jangine::Gui::Measure width;
            float_t computedWidth = 0.0f;
        };

        struct Row
        {
            Jangine::Gui::Measure height;
            float_t computedHeight = 0.0f;
        };

        explicit Grid(std::type_index type, Scene &scene)
            : Node(type, scene)
        {
            isFocusable = false;
        }

        std::vector<Column> columns;
        std::vector<Row> rows;

        void SetColumnsAndRows(const std::vector<Column> &cols, const std::vector<Row> &rws)
        {
            columns = cols;
            rows = rws;
        }

        void UpdateLayout(Gfx::Overlay &overlay, Gfx::Rectangle area) override
        {
            bounds = area;
            bounds = bounds.Crop(computedStyle.margin ? computedStyle.margin->left.value : 0.0f,
                                 computedStyle.margin ? computedStyle.margin->top.value : 0.0f,
                                 computedStyle.margin ? computedStyle.margin->right.value : 0.0f,
                                 computedStyle.margin ? computedStyle.margin->bottom.value : 0.0f);

            auto boundsForDescendants = bounds.Crop(computedStyle.padding ? computedStyle.padding->left.value : 0.0f,
                                                    computedStyle.padding ? computedStyle.padding->top.value : 0.0f,
                                                    computedStyle.padding ? computedStyle.padding->right.value : 0.0f,
                                                    computedStyle.padding ? computedStyle.padding->bottom.value : 0.0f);

            float_t availableWidth = boundsForDescendants.width();
            float_t availableHeight = boundsForDescendants.height();
            float_t totalFractionWidth = 0.0f;
            float_t totalFractionHeight = 0.0f;
            uint32_t totalAutoColumns = 0;
            uint32_t totalAutoRows = 0;

            // 1. Fixed size columns/rows
            for (auto &col : columns)
            {
                if (col.width.unit == Unit::PIXELS)
                {
                    availableWidth -= col.width.value;
                    col.computedWidth = col.width.value;
                }
                else if (col.width.unit == Unit::FRACTION)
                {
                    totalFractionWidth += col.width.value;
                }
                else if (col.width.unit == Unit::AUTO)
                {
                    totalAutoColumns++;
                }
            }
            for (auto &row : rows)
            {
                if (row.height.unit == Unit::PIXELS)
                {
                    availableHeight -= row.height.value;
                    row.computedHeight = row.height.value;
                }
                else if (row.height.unit == Unit::FRACTION)
                {
                    totalFractionHeight += row.height.value;
                }
                else if (row.height.unit == Unit::AUTO)
                {
                    totalAutoRows++;
                }
            }

            // 2. Fractional size columns/rows
            for (auto &col : columns)
            {
                if (col.width.unit == Unit::FRACTION)
                {
                    col.computedWidth = col.width.value * availableWidth / totalFractionWidth;
                }
            }
            for (auto &row : rows)
            {
                if (row.height.unit == Unit::FRACTION)
                {
                    row.computedHeight = row.height.value * availableHeight / totalFractionHeight;
                }
            }

            // 3. Auto size columns/rows
            float_t autoColumnWidth = (totalAutoColumns > 0) ? (availableWidth / totalAutoColumns) : 0.0f;
            autoColumnWidth = std::max(1.0f, autoColumnWidth); // Ensure a minimum width.
            float_t autoRowHeight = (totalAutoRows > 0) ? (availableHeight / totalAutoRows) : 0.0f;
            autoRowHeight = std::max(1.0f, autoRowHeight); // Ensure a minimum height

            for (auto &col : columns)
            {
                if (col.width.unit == Unit::AUTO)
                {
                    col.computedWidth = autoColumnWidth;
                }
            }
            for (auto &row : rows)
            {
                if (row.height.unit == Unit::AUTO)
                {
                    row.computedHeight = autoRowHeight;
                }
            }

            // Update the layout of the grid elements based on the available area
            for (auto &element : descendants)
            {
                float_t left = boundsForDescendants.left;
                float_t top = boundsForDescendants.top;

                for (size_t c = 0; c < element->gridCoordinates.column; c++)
                {
                    left += columns[c].computedWidth;
                }
                for (size_t r = 0; r < element->gridCoordinates.row; r++)
                {
                    top += rows[r].computedHeight;
                }

                float_t right = left;
                float_t bottom = top;

                for (size_t c = element->gridCoordinates.column; c < element->gridCoordinates.column + element->gridCoordinates.columnSpan; c++)
                {
                    right += columns[c].computedWidth;
                }
                for (size_t r = element->gridCoordinates.row; r < element->gridCoordinates.row + element->gridCoordinates.rowSpan; r++)
                {
                    bottom += rows[r].computedHeight;
                }

                element->UpdateLayout(overlay, Gfx::Rectangle(left, top, right, bottom));
            }
        }
    };

    struct Scene
    {
        const Logging::Logger &logger;

        std::vector<Node *> nodes;

        bool_t pendingLayoutUpdate{true};

        Node *nodeThatHasMouse{nullptr};
        Node *nodeThatCapturedMouse{nullptr};
        Node *nodeThatHasKeyboard{nullptr};

        Eigen::Vector2f mousePosition{-1.0f, -1.0f};

        std::vector<std::function<bool_t(Eigen::Vector2f, Eigen::Vector2f)>> mouseScrollHandlers;

        Scene &operator=(const Scene &) = delete;
        Scene(const Scene &) = delete;
        Scene &operator=(Scene &&) = default;
        Scene(Scene &&from) = default;

        Scene()
            : logger(Jangine::Core::GetLogger("Gui::Scene"))
        {
            auto world = new GuiRoot(typeid(GuiRoot), *this);
            world->SetName("Root");
            nodes.push_back(world);

            nodeThatHasMouse = world;
            nodeThatCapturedMouse = nullptr;
            nodeThatHasKeyboard = world;
        }

        ~Scene()
        {
            for (auto node : nodes)
            {
                delete node;
            }
        }

        GuiRoot *GetRoot() { return static_cast<GuiRoot *>(nodes[0]); }

        template <DerivedFrom<Node> T>
        T *CreateNode()
        {
            auto node = new T(typeid(T), *this);
            nodes.push_back(node);
            node->ancestor = GetRoot();
            GetRoot()->descendants.push_back(node);
            return node;
        }

        void SetName(Node *node, const std::string &name)
        {
            node->SetName(name);
        }

        void SetAncestor(Node *node, Node *ancestor)
        {
            if (node->ancestor)
            {
                auto oldAncestor = node->ancestor;
                oldAncestor->descendants.erase(std::remove(oldAncestor->descendants.begin(),
                                                           oldAncestor->descendants.end(),
                                                           node),
                                               oldAncestor->descendants.end());
            }

            node->ancestor = ancestor;

            if (ancestor)
            {
                ancestor->descendants.push_back(node);
            }
        }

        void InvalidateLayout()
        {
            pendingLayoutUpdate = true;
        }

        void UpdateLayout(Gfx::Overlay &overlay, Gfx::Rectangle area)
        {
            GetRoot()->ComputeStyle();
            GetRoot()->UpdateLayout(overlay, area);
        }

        void Render(Gfx::Overlay &overlay, Gfx::Overlay::CommandBuffer *commandBuffer)
        {
            GetRoot()->Render(overlay, commandBuffer);
        }

        void Print(const Node *node, int depth = 0) const
        {
            std::cout << std::format("{} {:03d} {}", std::string(depth * 2, ' '), 0, node->GetName()) << std::endl;
            for (auto child : node->GetDescendants())
            {
                Print(child, depth + 1);
            }
        }

        void HandleMouseEnter(bool_t entered)
        {
            if (!entered)
            {
                mousePosition = {-1.0f, -1.0f};

                if (nodeThatHasMouse)
                {
                    nodeThatHasMouse->isHovered = false;
                    nodeThatHasMouse->HandleMouseFocus(false);
                    nodeThatHasMouse = nullptr;
                    InvalidateLayout();
                }
            }
        }

        void HandleMouseMotion(const Eigen::Vector2f &position)
        {
            Eigen::Vector2f delta = position - mousePosition;
            mousePosition = position;

            if (nodeThatCapturedMouse)
            {
                // If a node has captured the mouse, it gets all mouse move events
                nodeThatCapturedMouse->HandleMouseMotion(
                    nodeThatCapturedMouse->GetLocalPosition(position),
                    delta);
                return;
            }

            auto nodeAtMouse = GetRoot()->ContainsPoint(position);
            if (nodeAtMouse != nodeThatHasMouse)
            {
                if (nodeThatHasMouse)
                {
                    nodeThatHasMouse->isHovered = false;
                    nodeThatHasMouse->HandleMouseFocus(false);
                    InvalidateLayout();
                }

                nodeThatHasMouse = nodeAtMouse.value_or(GetRoot());
                if (nodeThatHasMouse)
                {
                    nodeThatHasMouse->isHovered = true;
                    nodeThatHasMouse->HandleMouseFocus(true);
                    InvalidateLayout();
                }
            }

            if (nodeThatHasMouse)
            {
                nodeThatHasMouse->HandleMouseMotion(
                    nodeThatHasMouse->GetLocalPosition(position),
                    delta);
            }
        }

        void HandleMouseButton(UserInput::Digital button, UserInput::Action action, UserInput::Mods mods)
        {
            if (nodeThatCapturedMouse)
            {
                // If a node has captured the mouse, it gets all mouse button events
                nodeThatCapturedMouse->HandleMouseButton(
                    button,
                    action,
                    mods,
                    nodeThatCapturedMouse->GetLocalPosition(mousePosition));

                // End capture on mouse button release
                if (action == UserInput::Action::RELEASE)
                {
                    nodeThatCapturedMouse = nullptr;
                }
                return;
            }
            if (nodeThatHasMouse)
            {
                nodeThatHasMouse->HandleMouseButton(
                    button,
                    action,
                    mods,
                    nodeThatHasMouse->GetLocalPosition(mousePosition));

                // Start capture on mouse button press - if the node is enabled and focusable
                if (action == UserInput::Action::PRESS &&
                    nodeThatHasMouse->isEnabled &&
                    nodeThatHasMouse->isFocusable)
                {
                    nodeThatCapturedMouse = nodeThatHasMouse;
                    MoveKeyboardFocus(nodeThatHasMouse);
                }
            }
        }

        void HandleMouseScroll(const Eigen::Vector2f &offset)
        {
            for (auto &handler : mouseScrollHandlers)
            {
                if (handler(offset, mousePosition))
                    return;
            }

            if (nodeThatHasMouse)
            {
                nodeThatHasMouse->HandleMouseScroll(offset, nodeThatHasMouse->GetLocalPosition(mousePosition));
            }
        }

        void HandleKey(UserInput::Digital key, UserInput::Action action, UserInput::Mods mods)
        {
            if (nodeThatHasKeyboard)
            {
                nodeThatHasKeyboard->HandleKey(key, action, mods);
            }
        }

        void MoveKeyboardFocus(Node *newNodeThatHasKeyboard)
        {
            if (newNodeThatHasKeyboard == nodeThatHasKeyboard)
                return;

            if (nodeThatHasKeyboard)
            {
                nodeThatHasKeyboard->isFocused = false;
                nodeThatHasKeyboard->HandleKeyboardFocus(false);
                InvalidateLayout();
            }

            nodeThatHasKeyboard = newNodeThatHasKeyboard;

            if (nodeThatHasKeyboard)
            {
                nodeThatHasKeyboard->isFocused = true;
                nodeThatHasKeyboard->HandleKeyboardFocus(true);
                InvalidateLayout();
            }
        }
    };
}