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

    void Core3D::Add(const IO::Gltf::Model &gltf)
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

        std::vector<PrimordialMesh> primordialMeshes;
        primordialMeshes.reserve(gltf.meshes.size());

        for (const auto &gltfMesh : gltf.meshes)
        {
            PrimordialMesh primordialMesh;
            primordialMesh.name = gltfMesh.name;

            for (const auto &gltfPrimitive : gltfMesh.primitives)
            {
                bool_t hasNormals = gltfPrimitive.attributes.normal != -1;
                bool_t hasUVs = gltfPrimitive.attributes.texcoord != -1;
                bool_t hasIndices = gltfPrimitive.indices != -1;

                // TODO: Support no positions?
                IO::Gltf::Model::Accessor positionAccessor = gltf.accessors[gltfPrimitive.attributes.position];
                IO::Gltf::Model::BufferView positionBufferView = gltf.bufferViews[positionAccessor.bufferView];
                IO::Gltf::Model::Accessor normalAccessor = hasNormals ? gltf.accessors[gltfPrimitive.attributes.normal] : IO::Gltf::Model::Accessor{};
                IO::Gltf::Model::BufferView normalBufferView = hasNormals ? gltf.bufferViews[normalAccessor.bufferView] : IO::Gltf::Model::BufferView{};
                IO::Gltf::Model::Accessor uvAccessor = hasUVs ? gltf.accessors[gltfPrimitive.attributes.texcoord] : IO::Gltf::Model::Accessor{};
                IO::Gltf::Model::BufferView uvBufferView = hasUVs ? gltf.bufferViews[uvAccessor.bufferView] : IO::Gltf::Model::BufferView{};
                IO::Gltf::Model::Accessor indexAccessor = hasIndices ? gltf.accessors[gltfPrimitive.indices] : IO::Gltf::Model::Accessor{};
                IO::Gltf::Model::BufferView indexBufferView = hasIndices ? gltf.bufferViews[indexAccessor.bufferView] : IO::Gltf::Model::BufferView{};

                auto calculateOffset = [](const IO::Gltf::Model::BufferView &bufferView, const IO::Gltf::Model::Accessor &accessor, size_t i) -> size_t
                {
                    return bufferView.byteOffset + accessor.byteOffset + i * accessor.stride;
                };

                auto readPosition = [&](size_t i) -> Eigen::Vector3f
                {
                    ThrowInvalidDataIf(positionAccessor.type != IO::Gltf::Model::Accessor::Type::VEC3, "Position accessor must be of type VEC3");
                    return Eigen::Vector3f{reinterpret_cast<const float *>(gltf.data.data() + calculateOffset(positionBufferView, positionAccessor, i))};
                };

                auto readNormal = [&](size_t i) -> Eigen::Vector3f
                {
                    if (!hasNormals)
                    {
                        return Eigen::Vector3f(0, 0, 1);
                    }

                    ThrowInvalidDataIf(normalAccessor.type != IO::Gltf::Model::Accessor::Type::VEC3, "Normal accessor must be of type VEC3");
                    return Eigen::Vector3f{reinterpret_cast<const float *>(gltf.data.data() + calculateOffset(normalBufferView, normalAccessor, i))};
                };

                auto readUV = [&](size_t i) -> Eigen::Vector2f
                {
                    if (!hasUVs)
                    {
                        return Eigen::Vector2f(0, 0);
                    }

                    ThrowInvalidDataIf(uvAccessor.type != IO::Gltf::Model::Accessor::Type::VEC2, "Texcoord accessor must be of type VEC2");
                    return Eigen::Vector2f{reinterpret_cast<const float *>(gltf.data.data() + calculateOffset(uvBufferView, uvAccessor, i))};
                };

                auto readIndex = [&](size_t i) -> size_t
                {
                    auto offset = calculateOffset(indexBufferView, indexAccessor, i);
                    auto data = gltf.data.data() + offset;
                    if (indexAccessor.componentType == IO::Gltf::Model::Accessor::ComponentType::UNSIGNED_BYTE)
                        return *reinterpret_cast<const uint8_t *>(data);
                    if (indexAccessor.componentType == IO::Gltf::Model::Accessor::ComponentType::UNSIGNED_SHORT)
                        return *reinterpret_cast<const uint16_t *>(data);
                    if (indexAccessor.componentType == IO::Gltf::Model::Accessor::ComponentType::UNSIGNED_INT)
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

                for (size_t i = 0; i < indexAccessor.count; i++)
                {
                    size_t index = readIndex(i);

                    indices.push_back(static_cast<uint32_t>(index + primitiveVertexOffset));
                }

                MeshPrimitive primitive;
                primitive.indexOffset = static_cast<uint32_t>(primitiveIndexOffset);
                primitive.indexCount = static_cast<uint32_t>(indexAccessor.count);
                primitive.materialIndex = static_cast<uint8_t>(gltfPrimitive.material);
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

        std::unordered_map<uint32_t, Node::Id> gltfNodeToSceneNode;

        // First pass: create all nodes
        for (size_t i = 0; i < gltf.nodes.size(); ++i)
        {
            const auto &gltfNode = gltf.nodes[i];
            if (gltfNode.mesh >= 0)
            {
                Mesh::Id meshId = meshIds[gltfNode.mesh];
                MeshNode *node = scene.CreateMeshNode(meshId);
                scene.SetRelativeTransform(node, gltfNode.transform);
                gltfNodeToSceneNode[static_cast<uint32_t>(i)] = node->GetId();
            }
            else
            {
                Node *node = scene.CreateNode();
                scene.SetRelativeTransform(node, gltfNode.transform);
                gltfNodeToSceneNode[static_cast<uint32_t>(i)] = node->GetId();
            }
        }

        // Second pass: set up parent-child relationships
        for (size_t i = 0; i < gltf.nodes.size(); ++i)
        {
            const auto &gltfNode = gltf.nodes[i];

            for (auto childGltfNodeId : gltfNode.children)
            {
                Node *childSceneNode = scene.GetNode(gltfNodeToSceneNode[childGltfNodeId]);
                scene.SetAncestor(childSceneNode, gltfNodeToSceneNode[static_cast<uint32_t>(i)]);
            }
        }
    }
}