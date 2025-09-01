#pragma once

#include "../Shared.hpp"
#include "Shared.hpp"
#include "Core.hpp"

#include "Presenter.hpp"

#include "Camera.hpp"

#include "IO/Gltf.hpp"
#include "IO/Urdf.hpp"

namespace Jangine::Logging
{
    class Logger;
}

namespace Jangine::Gfx
{
    struct MeshVertex
    {
        Eigen::Vector3f position;
        Eigen::Vector3f normal;
        Eigen::Vector2f uv;
    };

    struct MeshMaterial
    {
        using Id = uint32_t;

        Eigen::Vector4f base;
        float_t metalness;
        float_t roughness;
    };

    struct MeshPrimitive
    {
        uint32_t indexOffset;
        uint32_t indexCount;
        uint32_t vertexOffset;
        uint32_t vertexCount;
        MeshMaterial::Id materialIndex;
        bool_t materialHasTransparency;
    };

    /// @brief Vertex, index, and material buffer for one or more meshes
    struct MeshBuffer
    {
        MeshBuffer(Handle<MemoryBuffer> &&vertexBuffer,
                   Handle<MemoryBuffer> &&indexBuffer,
                   Handle<MemoryBuffer> &&materialBuffer,
                   std::vector<MeshVertex> &&vertices,
                   std::vector<uint32_t> &&indices,
                   std::vector<MeshMaterial> &&materials)
            : vertexBuffer(std::move(vertexBuffer)),
              indexBuffer(std::move(indexBuffer)),
              materialBuffer(std::move(materialBuffer)),
              vertices(std::move(vertices)),
              indices(std::move(indices)),
              materials(std::move(materials)) {}

        MeshBuffer &operator=(const MeshBuffer &) = delete;
        MeshBuffer(const MeshBuffer &) = delete;
        MeshBuffer &operator=(MeshBuffer &&) = delete;
        MeshBuffer(MeshBuffer &&from) = delete;

        const MemoryBuffer *GetVertexBuffer() const { return vertexBuffer.get(); }
        const MemoryBuffer *GetIndexBuffer() const { return indexBuffer.get(); }
        const MemoryBuffer *GetMaterialBuffer() const { return materialBuffer.get(); }

        const std::vector<MeshVertex> &GetVertices() const { return vertices; }
        const std::vector<uint32_t> &GetIndices() const { return indices; }
        const std::vector<MeshMaterial> &GetMaterials() const { return materials; }

    private:
        Handle<MemoryBuffer> vertexBuffer;
        Handle<MemoryBuffer> indexBuffer;
        Handle<MemoryBuffer> materialBuffer;

        const std::vector<MeshVertex> vertices;
        const std::vector<uint32_t> indices;
        const std::vector<MeshMaterial> materials;
    };

    template <typename TNode>
    struct Id
    {
        Id() : value(-1) {}
        template <typename T>
        Id(T t) : value(static_cast<int32_t>(t)) {}
        int32_t value;
        operator size_t() const { return static_cast<size_t>(value); }
        operator bool_t() const { return value >= 0; }

        bool operator==(const Id<TNode> &other) const { return value == other.value; }
        bool operator!=(const Id<TNode> &other) const { return value != other.value; }
    };

    /// @brief Mesh consisting of multiple primitives and a shared vertex/index buffer
    struct Mesh
    {
        using Id = Jangine::Gfx::Id<Mesh>;

        explicit Mesh(Id id, SharedHandle<MeshBuffer> buffer, std::vector<MeshPrimitive> &&primitives)
            : id(id),
              buffer(buffer),
              primitives(std::move(primitives))
        {
        }

        Mesh &operator=(const Mesh &) = delete;
        Mesh(const Mesh &) = delete;
        Mesh &operator=(Mesh &&) = default;
        Mesh(Mesh &&from) = default;

        const MeshBuffer *GetBuffer() const { return buffer.get(); }
        const std::vector<MeshPrimitive> &GetPrimitives() const { return primitives; }
        Id GetId() const { return id; }

    private:
        Id id;
        SharedHandle<MeshBuffer> buffer;
        std::vector<MeshPrimitive> primitives;
    };

    /// @brief Node in the scene graph
    struct Node
    {
        using Id = Jangine::Gfx::Id<Node>;

        friend class Scene;

        explicit Node(std::type_index type, Id id)
            : type(type),
              self(id),
              ancestor(-1),
              relativeTransform(Eigen::Isometry3f::Identity()),
              worldTransform(Eigen::Isometry3f::Identity()) {}

        Node &operator=(const Node &) = delete;
        Node(const Node &) = delete;
        Node &operator=(Node &&) = default;
        Node(Node &&from) = default;

        /// @brief Apply the parent transform
        virtual void ApplyTransform(const Eigen::Isometry3f &parentTransform)
        {
            worldTransform = parentTransform * relativeTransform;
        }

        /// @brief Get the world transform of the node
        const Eigen::Isometry3f &GetWorldTransform() const { return worldTransform; }
        /// @brief Get the relative transform of the node
        const Eigen::Isometry3f &GetRelativeTransform() const { return relativeTransform; }

        Node::Id GetDescendant() const { return descendant; }
        Node::Id GetSibling() const { return sibling; }

        Id GetId() const { return self; }
        std::type_index GetType() const { return type; }
        std::string_view GetName() const { return name; }
        void SetName(std::string_view in_name) { name = in_name; }

    private:
        std::type_index type;

        Id self;
        Id ancestor;
        Id sibling;
        Id descendant;

        std::string name;

        Eigen::Isometry3f relativeTransform;
        Eigen::Isometry3f worldTransform;
    };

    struct MeshNode : Node
    {
        explicit MeshNode(std::type_index type, Node::Id id)
            : Node(type, id),
              mesh(Mesh::Id{-1}) {}
        MeshNode &operator=(const MeshNode &) = delete;
        MeshNode(const MeshNode &) = delete;
        MeshNode &operator=(MeshNode &&) = default;
        MeshNode(MeshNode &&) = default;

        Mesh::Id GetMeshId() const { return mesh; }
        void SetMeshId(Mesh::Id in_id) { mesh = in_id; }

        Mesh::Id mesh;
    };

    /// @brief Read-only copy of the scene graph
    struct ReadOnlyScene
    {
        struct ReadOnlyNode
        {
            std::type_index type;

            Node::Id self;
            Node::Id descendant;
            Node::Id sibling;

            Eigen::Isometry3f relativeTransform;
            Eigen::Isometry3f worldTransform;
        };

        std::vector<ReadOnlyNode> nodes;

        void Print(std::function<void(std::string)> printFunc, const ReadOnlyNode *node = nullptr, int depth = 0) const
        {
            if (node == nullptr)
                node = &nodes[0];
            printFunc(std::format("{} {:03d} {}", std::string(depth * 2, ' '), node->self.value, ""));

            if (node->descendant)
            {
                Print(printFunc, &nodes[node->descendant], depth + 1);
            }
            if (node->sibling)
            {
                Print(printFunc, &nodes[node->sibling], depth);
            }
        }
    };

    /// @brief Scene graph structure
    class Scene
    {
    public:
        Scene()
        {
            Node *world = new Node(typeid(Node), Node::Id{0});
            world->SetName("World");
            nodes.push_back(world);
        }

        ~Scene() = default;
        Scene &operator=(const Scene &) = delete;
        Scene(const Scene &) = delete;
        Scene &operator=(Scene &&) = default;
        Scene(Scene &&from) = default;

        void Clear()
        {
            for (size_t i = 1; i < nodes.size(); i++)
            {
                delete nodes[i];
            }
            nodes.resize(1);
            meshes.clear();
            nodes[0]->descendant = Node::Id{-1};
            nodes[0]->sibling = Node::Id{-1};
            nodes[0]->relativeTransform = Eigen::Isometry3f::Identity();
            nodes[0]->worldTransform = Eigen::Isometry3f::Identity();
        }

        template <DerivedFrom<Node> T>
        T *CreateNode()
        {
            Node::Id id = static_cast<Node::Id>(nodes.size());
            T *node = new T(typeid(T), id);
            nodes.push_back(node);
            SetAncestor(node, Node::Id{0});
            return node;
        }

        Mesh::Id CreateMesh(SharedHandle<MeshBuffer> buffer, std::vector<MeshPrimitive> &&primitives)
        {
            Mesh::Id id{static_cast<int32_t>(meshes.size())};
            meshes.emplace_back(id, std::move(buffer), std::move(primitives));
            return id;
        }

        void SetAncestor(Node *node, Node::Id newAncestor)
        {
            // Remove node from its current ancestor's child list
            if (node->ancestor)
            {
                auto *oldAncestor = nodes[node->ancestor];
                Node::Id prev = -1;
                Node::Id curr = oldAncestor->descendant;
                while (curr)
                {
                    if (curr == node->self)
                    {
                        if (!prev)
                        {
                            // Node is the first child
                            oldAncestor->descendant = node->sibling;
                        }
                        else
                        {
                            nodes[prev]->sibling = node->sibling;
                        }
                        break;
                    }
                    prev = curr;
                    curr = nodes[curr]->sibling;
                }
            }

            // Set new ancestor
            node->ancestor = newAncestor;
            node->sibling = -1;

            // Append to new ancestor
            if (node->ancestor)
            {
                auto *pnewAncestor = nodes[node->ancestor];
                // Insert node at the end of the new ancestor's child list
                if (!pnewAncestor->descendant)
                {
                    pnewAncestor->descendant = node->self;
                }
                else
                {
                    Node::Id curr = pnewAncestor->descendant;
                    while (nodes[curr]->sibling)
                    {
                        curr = nodes[curr]->sibling;
                    }
                    nodes[curr]->sibling = node->self;
                }
                node->sibling = -1;
            }
        }

        /// @brief Set the relative transform of the node
        void SetRelativeTransform(Node *node, const Eigen::Isometry3f &transform) { node->relativeTransform = transform; }

        /// @brief Set the name of the node
        void SetName(Node *node, std::string_view in_name) { node->SetName(in_name); }

        /// @brief Get the root node of the scene
        Node *GetWorld() { return nodes[0]; }
        /// @brief Get the root node of the scene
        const Node *GetWorld() const { return nodes[0]; }

        /// @brief Get a node by its Node::Id
        /// @throws InvalidOperationException if the Node::Id is out of range
        Node *GetNode(Node::Id id)
        {
            ThrowInvalidOperationIf(id.value < 0 || id.value >= static_cast<int32_t>(nodes.size()),
                                    std::format("Node::Id({}) is out of range.", id.value));
            return nodes[id];
        }
        /// @brief Get a node by its Node::Id
        /// @throws InvalidOperationException if the Node::Id is out of range
        const Node *GetNode(Node::Id id) const
        {
            ThrowInvalidOperationIf(id.value < 0 || id.value >= static_cast<int32_t>(nodes.size()),
                                    std::format("Node::Id({}) is out of range.", id.value));
            return nodes[id];
        }

        /// @brief Get a mesh by its Mesh::Id
        /// @throws InvalidOperationException if the Mesh::Id is out of range
        const Mesh *GetMesh(Mesh::Id id) const
        {
            ThrowInvalidOperationIf(id.value < 0 || id.value >= static_cast<int32_t>(meshes.size()),
                                    std::format("Mesh::Id({}) is out of range.", id.value));
            return &meshes[id.value];
        }

        const std::vector<Node *> &GetNodes() const { return nodes; }
        const std::vector<Mesh> &GetMeshes() const { return meshes; }

        void Update()
        {
            ApplyTransform(GetWorld(), Eigen::Isometry3f::Identity());
        }

        /// @brief Recursively apply transforms to node and its descendants
        void ApplyTransform(Node *node, const Eigen::Isometry3f &parentTransform)
        {
            node->ApplyTransform(parentTransform);

            if (node->descendant)
            {
                ApplyTransform(nodes[node->descendant.value], node->GetWorldTransform());
            }
            if (node->sibling)
            {
                ApplyTransform(nodes[node->sibling.value], parentTransform);
            }
        }

        void Print(std::function<void(std::string)> printFunc, const Node *node = nullptr, int depth = 0) const
        {
            if (node == nullptr)
                node = nodes[0];
            printFunc(std::format("{} {:03d} {}", std::string(depth * 2, ' '), node->GetId().value, node->GetName()));

            if (node->descendant)
            {
                Print(printFunc, nodes[node->descendant.value], depth + 1);
            }
            if (node->sibling)
            {
                Print(printFunc, nodes[node->sibling.value], depth);
            }
        }

        void FillReadOnlyCopy(ReadOnlyScene &readOnlyScene) const
        {
            readOnlyScene.nodes.clear();
            readOnlyScene.nodes.reserve(nodes.size());

            for (const auto &node : nodes)
            {
                ReadOnlyScene::ReadOnlyNode readOnlyNode{
                    .type = node->GetType(),
                    .self = node->GetId(),
                    .descendant = node->descendant,
                    .sibling = node->sibling,
                    .relativeTransform = node->GetRelativeTransform(),
                    .worldTransform = node->GetWorldTransform()};

                readOnlyScene.nodes.push_back(std::move(readOnlyNode));
            }
        }

    private:
        std::vector<Node *> nodes;
        std::vector<Mesh> meshes;
    };

    class Core3D
    {
        struct PerSceneData
        {
            Eigen::Matrix4f ViewProjection;

            Eigen::Matrix4f View;
            Eigen::Matrix4f ViewInverse;

            Eigen::Vector3f ViewPosition;
            float_t _Padding0;

            Eigen::Vector2f Screen;
        };

        struct PerMeshData
        {
            Eigen::Matrix4f Transform;
            Eigen::Vector4f Color;
            int32_t MaterialIndex;
        };

        struct ShapeVertex
        {
            Eigen::Vector3f Position;
            Eigen::Vector2f UV;
        };

        struct PerDiscMeshData
        {
            Eigen::Matrix4f Transform;

            Eigen::Vector4f Color;
            Eigen::Vector4f ColorOuterStart;
            Eigen::Vector4f ColorInnerEnd;
            Eigen::Vector4f ColorOuterEnd;
            float_t Radius;
            float_t Thickness;
            float_t AngleStart;
            float_t AngleEnd;
            int32_t ScaleSpace;
            int32_t Alignment;
        };

        struct PerLineMeshData
        {
            Eigen::Matrix4f Transform;

            Eigen::Vector3f Start;
            float_t _Padding0;
            Eigen::Vector3f End;
            float_t _Padding1;
            Eigen::Vector4f Color;
            Eigen::Vector4f ColorEnd;
            float_t Thickness;
            int32_t ScaleSpace;
            int32_t Alignment;
        };

        struct OITData
        {
            uint32_t count;
            uint32_t maxNodeCount;
        };

        struct OITNode
        {
            uint32_t color;
            float_t depth;
            uint32_t next;
        };

    public:
        Core3D(Gfx::Core *gfx);
        ~Core3D();

        void Create();
        void InitializeRendering(const DisplayParameters &wantedDisplayParameters);
        void Render(const Presenter &presenter, double_t absoluteTime, float_t deltaTime);

        void Clear();
        void Import(const Jangine::IO::Gltf::Model &gltf);
        void Export(Jangine::IO::Gltf::Model &gltf) const;

    private:
        Core *gfx;

        DisplayParameters displayParameters;

        static constexpr uint32_t MAX_VERTICES = 65536;
        static constexpr uint32_t MAX_INDICES = 65536;

        static constexpr uint32_t MAX_OIT_NODES_PER_PIXEL = 10;

        Handle<MemoryBuffer> perSceneBuffer;
        Handle<MemoryBuffer> perMeshBuffer;
        Handle<Pipeline> meshPipeline;
        Handle<Pipeline> oitCompositionPipeline;

        Handle<MemoryBuffer> oitDataBuffer;
        Handle<MemoryBuffer> oitNodeBuffer;
        Handle<PixelBuffer> oitNodeHeadBuffer;

        Handle<PixelBuffer> renderBuffer;
        Handle<PixelBuffer> depthBuffer;
        Handle<PixelBuffer> motionBuffer;
        Handle<PixelBuffer> displayBuffer;

        Handle<MemoryBuffer> perShapeMeshBuffer;
        Handle<MemoryBuffer> shapeVertexBuffer;
        Handle<MemoryBuffer> shapeIndexBuffer;
        Handle<Pipeline> shapePipeline;

        BlenderCamera camera;

        Scene scene;

        const Logging::Logger &logger;
    };
}

namespace std
{
    template <typename T>
    struct hash<Jangine::Gfx::Id<T>>
    {
        std::size_t operator()(const Jangine::Gfx::Id<T> &id) const noexcept
        {
            return std::hash<int32_t>{}(id.value);
        }
    };
}