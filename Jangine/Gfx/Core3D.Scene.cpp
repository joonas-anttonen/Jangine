#include "Core3D.hpp"

namespace Jangine::Gfx
{
    struct PrimordialMesh
    {
        PrimordialMesh() = default;

        std::string name;
        std::vector<MeshPrimitive> primitives;

        PrimordialMesh &operator=(const PrimordialMesh &) = delete;
        PrimordialMesh(const PrimordialMesh &) = delete;
        PrimordialMesh &operator=(PrimordialMesh &&) = default;
        PrimordialMesh(PrimordialMesh &&) = default;
    };

    void Core3D::Import(const Jangine::IO::Gltf::Model &gltf)
    {
        std::vector<MeshVertex> vertices;
        std::vector<uint32_t> indices;
        std::vector<MeshMaterial> materials;

        for (const auto &gltfMaterial : gltf.materials)
        {
            MeshMaterial material;
            material.base = Eigen::Vector4f(
                gltfMaterial.pbrMetallicRoughness.baseColorFactor[0],
                gltfMaterial.pbrMetallicRoughness.baseColorFactor[1],
                gltfMaterial.pbrMetallicRoughness.baseColorFactor[2],
                gltfMaterial.pbrMetallicRoughness.baseColorFactor[3]);
            material.metalness = gltfMaterial.pbrMetallicRoughness.metallicFactor;
            material.roughness = gltfMaterial.pbrMetallicRoughness.roughnessFactor;
            materials.push_back(material);
        }

        if (materials.empty())
        {
            materials.push_back(MeshMaterial{Eigen::Vector4f(1, 1, 1, 1), 0.0f, 0.5f});
        }

        std::vector<PrimordialMesh> primordialMeshes;
        primordialMeshes.reserve(gltf.meshes.size());

        for (const auto &gltfMesh : gltf.meshes)
        {
            PrimordialMesh primordialMesh;
            primordialMesh.name = gltfMesh.name;

            for (const auto &gltfPrimitive : gltfMesh.primitives)
            {
                bool_t hasNormals = gltfPrimitive.attributes.normal != -1;
                bool_t hasUVs = gltfPrimitive.attributes.uv != -1;
                bool_t hasIndices = gltfPrimitive.indices != -1;

                // TODO: Support no positions?
                Jangine::IO::Gltf::Model::Accessor positionAccessor = gltf.accessors[gltfPrimitive.attributes.position];
                Jangine::IO::Gltf::Model::BufferView positionBufferView = gltf.bufferViews[positionAccessor.bufferView];
                Jangine::IO::Gltf::Model::Accessor normalAccessor = hasNormals ? gltf.accessors[gltfPrimitive.attributes.normal] : Jangine::IO::Gltf::Model::Accessor{};
                Jangine::IO::Gltf::Model::BufferView normalBufferView = hasNormals ? gltf.bufferViews[normalAccessor.bufferView] : Jangine::IO::Gltf::Model::BufferView{};
                Jangine::IO::Gltf::Model::Accessor uvAccessor = hasUVs ? gltf.accessors[gltfPrimitive.attributes.uv] : Jangine::IO::Gltf::Model::Accessor{};
                Jangine::IO::Gltf::Model::BufferView uvBufferView = hasUVs ? gltf.bufferViews[uvAccessor.bufferView] : Jangine::IO::Gltf::Model::BufferView{};
                Jangine::IO::Gltf::Model::Accessor indexAccessor = hasIndices ? gltf.accessors[gltfPrimitive.indices] : Jangine::IO::Gltf::Model::Accessor{};
                Jangine::IO::Gltf::Model::BufferView indexBufferView = hasIndices ? gltf.bufferViews[indexAccessor.bufferView] : Jangine::IO::Gltf::Model::BufferView{};

                auto calculateOffset = [](const Jangine::IO::Gltf::Model::BufferView &bufferView, const Jangine::IO::Gltf::Model::Accessor &accessor, size_t i) -> size_t
                {
                    return bufferView.byteOffset + accessor.offset + i * accessor.stride;
                };

                auto readPosition = [&](size_t i) -> Eigen::Vector3f
                {
                    ThrowInvalidDataIf(positionAccessor.type != Jangine::IO::Gltf::Model::Accessor::Type::VEC3, "Position accessor must be of type VEC3");
                    return Eigen::Vector3f{reinterpret_cast<const float *>(gltf.data.data() + calculateOffset(positionBufferView, positionAccessor, i))};
                };

                auto readNormal = [&](size_t i) -> Eigen::Vector3f
                {
                    if (!hasNormals)
                    {
                        return Eigen::Vector3f(0, 0, 1);
                    }

                    ThrowInvalidDataIf(normalAccessor.type != Jangine::IO::Gltf::Model::Accessor::Type::VEC3, "Normal accessor must be of type VEC3");
                    return Eigen::Vector3f{reinterpret_cast<const float *>(gltf.data.data() + calculateOffset(normalBufferView, normalAccessor, i))};
                };

                auto readUV = [&](size_t i) -> Eigen::Vector2f
                {
                    if (!hasUVs)
                    {
                        return Eigen::Vector2f(0, 0);
                    }

                    ThrowInvalidDataIf(uvAccessor.type != Jangine::IO::Gltf::Model::Accessor::Type::VEC2, "UV accessor must be of type VEC2");
                    return Eigen::Vector2f{reinterpret_cast<const float *>(gltf.data.data() + calculateOffset(uvBufferView, uvAccessor, i))};
                };

                auto readIndex = [&](size_t i) -> size_t
                {
                    auto offset = calculateOffset(indexBufferView, indexAccessor, i);
                    auto data = gltf.data.data() + offset;
                    if (indexAccessor.componentType == Jangine::IO::Gltf::Model::Accessor::ComponentType::UNSIGNED_BYTE)
                        return *reinterpret_cast<const uint8_t *>(data);
                    if (indexAccessor.componentType == Jangine::IO::Gltf::Model::Accessor::ComponentType::UNSIGNED_SHORT)
                        return *reinterpret_cast<const uint16_t *>(data);
                    if (indexAccessor.componentType == Jangine::IO::Gltf::Model::Accessor::ComponentType::UNSIGNED_INT)
                        return *reinterpret_cast<const uint32_t *>(data);

                    throw std::runtime_error("Unsupported index component type");
                };

                if ((hasNormals && positionAccessor.count != normalAccessor.count) || (hasUVs && positionAccessor.count != uvAccessor.count))
                {
                    throw std::runtime_error("Inconsistent accessor counts");
                }

                size_t primitiveVertexOffset = vertices.size();

                for (size_t i = 0; i < positionAccessor.count; i++)
                {
                    MeshVertex vertex;
                    vertex.position = readPosition(i);
                    vertex.normal = readNormal(i);
                    vertex.uv = readUV(i);

                    vertices.push_back(vertex);
                }

                size_t primitiveIndexOffset = indices.size();
                size_t indexCount = hasIndices ? indexAccessor.count : positionAccessor.count;

                if (!hasIndices)
                {
                    for (size_t i = 0; i < indexCount; i++)
                    {
                        indices.push_back(static_cast<uint32_t>(i + primitiveVertexOffset));
                    }
                }
                else
                {
                    for (size_t i = 0; i < indexCount; i++)
                    {
                        size_t index = readIndex(i);

                        indices.push_back(static_cast<uint32_t>(index + primitiveVertexOffset));
                    }
                }

                if (!hasNormals)
                {
                    // Recalculate normals
                    for (size_t i = 0; i < indexCount; i += 3)
                    {
                        uint32_t index0 = indices[primitiveIndexOffset + i + 0];
                        uint32_t index1 = indices[primitiveIndexOffset + i + 1];
                        uint32_t index2 = indices[primitiveIndexOffset + i + 2];

                        Eigen::Vector3f edge1 = vertices[index1].position - vertices[index0].position;
                        Eigen::Vector3f edge2 = vertices[index2].position - vertices[index0].position;
                        Eigen::Vector3f faceNormal = edge1.cross(edge2).normalized();

                        vertices[index0].normal = -1 * faceNormal;
                        vertices[index1].normal = -1 * faceNormal;
                        vertices[index2].normal = -1 * faceNormal;
                    }
                }

                MeshPrimitive primitive;
                primitive.indexOffset = static_cast<uint32_t>(primitiveIndexOffset);
                primitive.indexCount = static_cast<uint32_t>(indexCount);
                primitive.materialIndex = static_cast<int8_t>(std::clamp(gltfPrimitive.material, 0u, static_cast<uint32_t>(materials.size() - 1)));
                primitive.materialHasTransparency = materials[primitive.materialIndex].base[3] < 1.0f;

                primordialMesh.primitives.push_back(primitive);
            }

            primordialMeshes.push_back(std::move(primordialMesh));
        }

        auto _vertexBuffer = gfx->CreateMemoryBuffer(
            vertices.size() * sizeof(MeshVertex),
            MemoryBufferUsage::Vertex | MemoryBufferUsage::TransferDst,
            MemoryAccess::None);
        gfx->StageToMemoryBuffer(_vertexBuffer.get(), Span(vertices));

        auto _indexBuffer = gfx->CreateMemoryBuffer(
            indices.size() * sizeof(uint32_t),
            MemoryBufferUsage::Index | MemoryBufferUsage::TransferDst,
            MemoryAccess::None);
        gfx->StageToMemoryBuffer(_indexBuffer.get(), Span(indices));

        auto _materialBuffer = gfx->CreateMemoryBuffer(
            materials.size() * sizeof(MeshMaterial),
            MemoryBufferUsage::Storage | MemoryBufferUsage::TransferDst,
            MemoryAccess::None);
        gfx->StageToMemoryBuffer(_materialBuffer.get(), Span(materials));

        SharedHandle<MeshBuffer> meshBuffer = std::make_shared<MeshBuffer>(
            std::move(_vertexBuffer),
            std::move(_indexBuffer),
            std::move(_materialBuffer),
            std::move(vertices),
            std::move(indices),
            std::move(materials));

        std::vector<Mesh::Id> meshIds;

        for (auto &primordialMesh : primordialMeshes)
        {
            Mesh::Id meshId = scene.AddMesh(Mesh(meshBuffer, std::move(primordialMesh.primitives)));
            meshIds.push_back(meshId);
        }

        std::unordered_map<size_t, Node::Id> gltfNodeToSceneNode;

        // First pass: create all nodes
        for (size_t i = 0; i < gltf.nodes.size(); ++i)
        {
            Node *node;

            const auto &gltfNode = gltf.nodes[i];
            if (gltfNode.mesh >= 0)
            {
                Mesh::Id meshId = meshIds[gltfNode.mesh];
                MeshNode *meshNode = scene.CreateNode<MeshNode>();
                meshNode->SetMeshId(meshId);
                node = meshNode;
            }
            else
            {
                node = scene.CreateNode<Node>();
            }

            scene.SetRelativeTransform(node, gltfNode.transform);
            scene.SetName(node, gltfNode.name);
            gltfNodeToSceneNode[i] = node->GetId();
        }

        // Second pass: set up parent-child relationships
        for (size_t i = 0; i < gltf.nodes.size(); ++i)
        {
            const auto &gltfNode = gltf.nodes[i];

            for (auto childGltfNodeId : gltfNode.children)
            {
                Node *childSceneNode = scene.GetNode(gltfNodeToSceneNode[childGltfNodeId]);
                scene.SetAncestor(childSceneNode, gltfNodeToSceneNode[i]);
            }
        }
    }

    void Core3D::Export(Jangine::IO::Gltf::Model &gltf) const
    {
        (void)gltf;

        // std::vector<Jangine::IO::Gltf::Model::Mesh> gltfMeshes;
        // std::vector<Jangine::IO::Gltf::Model::Node> gltfNodes;
        // std::vector<Jangine::IO::Gltf::Model::Accessor> gltfAccessors;
        // std::vector<Jangine::IO::Gltf::Model::BufferView> gltfBufferViews;
        // std::vector<Jangine::IO::Gltf::Model::Material> gltfMaterials;
    }
}