#pragma once

#include "../Shared.hpp"
#include "Shared.hpp"
#include "Core.hpp"

#include "Presenter.hpp"

#include "Camera.hpp"

#include "IO/Gltf.hpp"

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
        Eigen::Vector4f base;
        float_t metalness;
        float_t roughness;
    };

    struct MeshPrimitive
    {
        uint32_t indexOffset;
        uint32_t indexCount;
        uint8_t materialIndex;
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

    private:
        Handle<MemoryBuffer> vertexBuffer;
        Handle<MemoryBuffer> indexBuffer;
        Handle<MemoryBuffer> materialBuffer;

        const std::vector<MeshVertex> vertices;
        const std::vector<uint32_t> indices;
        const std::vector<MeshMaterial> materials;
    };

    /// @brief Mesh consisting of multiple primitives and a shared vertex/index buffer
    struct Mesh
    {
        struct Id
        {
            Id() : value(-1) {}
            explicit Id(int32_t value) : value(value) {}
            int32_t value;
            operator size_t() const { return static_cast<size_t>(value); }
            bool_t has_value() const { return value != -1; }
        };

        explicit Mesh(SharedHandle<MeshBuffer> buffer, std::vector<MeshPrimitive> &&primitives)
            : buffer(buffer),
              primitives(std::move(primitives))
        {
        }

        Mesh &operator=(const Mesh &) = delete;
        Mesh(const Mesh &) = delete;
        Mesh &operator=(Mesh &&) = default;
        Mesh(Mesh &&from) = default;

        const MeshBuffer *GetBuffer() const { return buffer.get(); }
        const std::vector<MeshPrimitive> &GetPrimitives() const { return primitives; }

    private:
        SharedHandle<MeshBuffer> buffer;
        std::vector<MeshPrimitive> primitives;
    };

    /// @brief Node in the scene graph
    struct Node
    {
        friend class Scene;

        /// @brief Type of the node.
        enum class Type
        {
            Empty,
            Mesh,
        };

        struct Id
        {
            Id() : value(-1) {}
            template <typename T>
            Id(T t) : value(static_cast<int32_t>(t)) {}
            int32_t value;
            operator size_t() const { return static_cast<size_t>(value); }
        };

        explicit Node(Type type, Id id)
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

        const std::vector<Id> &GetDescendants() const { return descendants; }

        Id GetId() const { return self; }
        Type GetType() const { return type; }

    private:
        Type type;
        Id self;
        Id ancestor;
        std::vector<Id> descendants;

        Eigen::Isometry3f relativeTransform;
        Eigen::Isometry3f worldTransform;
    };

    struct MeshNode : Node
    {
        explicit MeshNode(Id id, Mesh::Id mesh)
            : Node(Type::Mesh, id), mesh(mesh)
        {
        }

        MeshNode &operator=(const MeshNode &) = delete;
        MeshNode(const MeshNode &) = delete;
        MeshNode &operator=(MeshNode &&from)
        {
            Node::operator=(std::move(from));
            mesh = from.mesh;
            return *this;
        }
        MeshNode(MeshNode &&from)
            : Node(std::move(from)), mesh(from.mesh)
        {
        }

        Mesh::Id GetMeshId() const { return mesh; }

        Mesh::Id mesh;
    };

    /// @brief Scene graph structure
    class Scene
    {
    public:
        Scene()
        {
            Node *world = new Node(Node::Type::Empty, Node::Id{0});
            nodes.push_back(world);
        }

        ~Scene() = default;
        Scene &operator=(const Scene &) = delete;
        Scene(const Scene &) = delete;
        Scene &operator=(Scene &&) = default;
        Scene(Scene &&from) = default;

        Node *CreateNode()
        {
            Node::Id id = static_cast<Node::Id>(nodes.size());
            Node* node = new Node(Node::Type::Empty, id);
            nodes.push_back(node);
            node->ancestor = Node::Id{0}; // world
            GetWorld()->descendants.push_back(node->self);
            return node;
        }

        MeshNode *CreateMeshNode(Mesh::Id meshId)
        {
            Node::Id id = static_cast<Node::Id>(nodes.size());
            MeshNode* node = new MeshNode(id, meshId);
            nodes.push_back(node);
            node->ancestor = Node::Id{0}; // world
            GetWorld()->descendants.push_back(node->self);
            return dynamic_cast<MeshNode *>(nodes.back());
        }

        Mesh::Id AddMesh(Mesh &&mesh)
        {
            Mesh::Id id{static_cast<int32_t>(meshes.size())};
            meshes.emplace_back(std::move(mesh));
            return id;
        }

        void SetAncestor(Node *node, Node::Id ancestorId)
        {
            if (node->ancestor.value >= 0)
            {
                auto &ancestor = nodes[node->ancestor.value];
                ancestor->descendants.erase(std::remove(ancestor->descendants.begin(),
                                                        ancestor->descendants.end(),
                                                        node->self),
                                            ancestor->descendants.end());
            }

            node->ancestor = ancestorId;
            if (node->ancestor.value >= 0)
            {
                auto &ancestor = nodes[node->ancestor.value];
                ancestor->descendants.push_back(node->self);
            }
        }

        /// @brief Set the relative transform of the node
        void SetRelativeTransform(Node *node, const Eigen::Isometry3f &transform) { node->relativeTransform = transform; }

        Node *GetWorld() { return nodes[0]; }
        Node *GetNode(Node::Id id) { return nodes[id]; }
        const Mesh *GetMesh(Mesh::Id id) const
        {
            ThrowInvalidOperationIf(!id.has_value());
            ThrowInvalidOperationIf(id.value < 0 || id.value >= static_cast<int32_t>(meshes.size()),
                                    std::format("Mesh ID {} is out of range.", id.value));
            return &meshes[id.value];
        }
        const std::vector<Node*> &GetNodes() const { return nodes; }

        void Update()
        {
            ApplyTransform(GetWorld(), Eigen::Isometry3f::Identity());
        }

        /// @brief Recursively apply transforms to node and its descendants
        void ApplyTransform(Node *node, const Eigen::Isometry3f &parentTransform)
        {
            node->ApplyTransform(parentTransform);

            for (auto id : node->GetDescendants())
            {
                ApplyTransform(nodes[id.value], node->GetWorldTransform());
            }
        }

        void Print(const Node *node, int depth = 0) const
        {
            std::cout << std::string(depth * 2, ' ') << "Node ID: " << node->GetId().value << "\n";
            for (auto childId : node->GetDescendants())
            {
                Print(nodes[childId.value], depth + 1);
            }
        }

    private:
        std::vector<Node*> nodes;
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
            uint32_t MaterialIndex;
            uint32_t _Padding0;
            uint32_t _Padding1;
            uint32_t _Padding2;
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

    public:
        Core3D(Gfx::Core *gfx);
        ~Core3D();

        void Create();
        void InitializeRendering(const DisplayParameters &wantedDisplayParameters);
        void Render(const Presenter &presenter, double_t absoluteTime, float_t deltaTime);

        void Add(const IO::Gltf::Model &gltf);

    private:
        Core *gfx;

        DisplayParameters displayParameters;

        static constexpr uint32_t MAX_VERTICES = 65536;
        static constexpr uint32_t MAX_INDICES = 65536;

        Handle<MemoryBuffer> perSceneBuffer;
        Handle<MemoryBuffer> perMeshBuffer;
        Handle<Pipeline> meshPipeline;

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