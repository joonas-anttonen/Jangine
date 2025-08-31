#pragma once

#include "../Shared.hpp"
#include "../Core.hpp"
#include "../Gfx/Core.hpp"
#include "../Gfx/Core2D.hpp"

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

    enum class Unit
    {
        AUTO,
        PIXELS,
        FRACTION,
        // EM,
    };

    struct Measure
    {
        Unit unit = Unit::FRACTION;
        float_t value = 1.0f;
    };

    struct GridCoordinates
    {
        size_t column = 0;
        size_t row = 0;
        size_t columnSpan = 1;
        size_t rowSpan = 1;
    };

    struct GuiNode
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

        GridCoordinates gridCoordinates;

        std::type_index type;

        std::string name;
        GuiNode *ancestor = nullptr;
        std::vector<GuiNode *> descendants;

        Gfx::Rectangle bounds;
        Color backgroundColor{0.1f, 0.1f, 0.1f, 1.0f};
        Color foregroundColor{1.0f, 1.0f, 1.0f, 1.0f};

        explicit GuiNode(std::type_index type)
            : type(type)
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

        const std::vector<GuiNode *> &GetDescendants() const
        {
            return descendants;
        }

        virtual void UpdateLayout(Gfx::Rectangle area)
        {
            bounds = area;

            for (auto &child : descendants)
            {
                child->UpdateLayout(area);
            }
        }

        virtual void Render(Gfx::Core2D *gfx2D, Gfx::CommandBuffer2D *commandBuffer)
        {
            for (auto &child : descendants)
            {
                child->Render(gfx2D, commandBuffer);
            }
        }
    };

    struct AcrylicPanel : public GuiNode
    {
        explicit AcrylicPanel(std::type_index type)
            : GuiNode(type)
        {
        }

        void Render(Gfx::Core2D *gfx2D, Gfx::CommandBuffer2D *commandBuffer) override
        {
            if (gfx2D->IsReady())
            {
                commandBuffer->DrawImage(
                    gfx2D->GetAcrylicBuffer(),
                    bounds.position(),
                    bounds.extent(),
                    Gfx::ImageFit::None,
                    Color::White);
            }

            for (auto &child : descendants)
            {
                child->Render(gfx2D, commandBuffer);
            }
        }
    };

    struct GuiRoot : public GuiNode
    {
        enum class FrameMode
        {
            NONE,
            TITLE,
            FULL,
        };

        explicit GuiRoot(std::type_index type)
            : GuiNode(type)
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

        void UpdateLayout(Gfx::Rectangle area) override
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
                child->UpdateLayout(contentArea);
            }
        }

        void Render(Gfx::Core2D *gfx2D, Gfx::CommandBuffer2D *commandBuffer) override
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
                child->Render(gfx2D, commandBuffer);
            }
        }
    };

    struct GridNode : public GuiNode
    {
        struct Column
        {
            Measure width;
            float_t computedWidth = 0.0f;

            Eigen::Vector2f bounds{0.0f, 0.0f};
        };

        struct Row
        {
            Measure height;
            float_t computedHeight = 0.0f;

            Eigen::Vector2f bounds{0.0f, 0.0f};
        };

        explicit GridNode(std::type_index type)
            : GuiNode(type)
        {
        }

        std::vector<Column> columns;
        std::vector<Row> rows;

        void SetColumnsAndRows(const std::vector<Column> &cols, const std::vector<Row> &rws)
        {
            columns = cols;
            rows = rws;
        }

        void UpdateLayout(Gfx::Rectangle area) override
        {
            bounds = area;

            float_t availableWidth = area.width();
            float_t availableHeight = area.height();
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
                float_t left = area.left;
                float_t top = area.top;

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

                element->UpdateLayout(Gfx::Rectangle(left, top, right, bottom));
            }

            // Update column and row bounds for debugging
            float_t x = area.left;
            for (auto &column : columns)
            {
                column.bounds = {x, x + column.computedWidth};
                x += column.computedWidth;
            }

            float_t y = area.top;
            for (auto &row : rows)
            {
                row.bounds = {y, y + row.computedHeight};
                y += row.computedHeight;
            }
        }
    };

    struct GuiScene
    {
        std::vector<GuiNode *> nodes;

        GuiScene()
        {
            auto world = new GuiRoot(typeid(GuiRoot));
            world->SetName("Root");
            nodes.push_back(world);
        }

        ~GuiScene()
        {
            for (auto node : nodes)
            {
                delete node;
            }
        }

        GuiRoot *GetRoot() { return static_cast<GuiRoot *>(nodes[0]); }

        template <DerivedFrom<GuiNode> T>
        T *CreateNode()
        {
            auto node = new T(typeid(T));
            nodes.push_back(node);
            node->ancestor = GetRoot();
            GetRoot()->descendants.push_back(node);
            return node;
        }

        void SetName(GuiNode *node, const std::string &name)
        {
            node->SetName(name);
        }

        void SetAncestor(GuiNode *node, GuiNode *ancestor)
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

        void UpdateLayout(Gfx::Rectangle area)
        {
            GetRoot()->UpdateLayout(area);
        }

        void Render(Gfx::Core2D *gfx2D, Gfx::CommandBuffer2D *commandBuffer)
        {
            GetRoot()->Render(gfx2D, commandBuffer);
        }

        void Print(const GuiNode *node, int depth = 0) const
        {
            std::cout << std::format("{} {:03d} {}", std::string(depth * 2, ' '), 0, node->GetName()) << std::endl;
            for (auto child : node->GetDescendants())
            {
                Print(child, depth + 1);
            }
        }
    };

    class JANGINE_API Core
    {
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

        void SynchronizeWithGfx(Eigen::Vector2i currentWindow);
        void SynchronizeWithGlfw();

        bool_t mouseInsideWindow{false};
        Eigen::Vector2f mousePosition;

        GLFWwindow *glfwWindow = nullptr;

        Gfx::Core *gfx = nullptr;

        const Logging::Logger &logger;

        // --------------------- TESTING
        GuiScene scene;
        GuiNode *viewport{nullptr};
        Eigen::Vector2f windowSize{0.0f, 0.0f};
        // --------------------- TESTING
    };
}