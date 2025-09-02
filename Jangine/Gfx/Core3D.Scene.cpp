#include "Core3D.hpp"

namespace Jangine::Gfx
{
    struct PrimordialMesh
    {
        PrimordialMesh() = default;

        std::string name;
        std::vector<Mesh::Primitive> primitives;

        PrimordialMesh &operator=(const PrimordialMesh &) = delete;
        PrimordialMesh(const PrimordialMesh &) = delete;
        PrimordialMesh &operator=(PrimordialMesh &&) = default;
        PrimordialMesh(PrimordialMesh &&) = default;
    };

    void Core3D::Clear()
    {
        scene.Clear();
    }

    void Core3D::Import(const Jangine::IO::Gltf::Model &gltf)
    {
        std::vector<MeshBuffer::Vertex> vertices;
        std::vector<uint32_t> indices;
        std::vector<MeshBuffer::Material> materials;
        materials.reserve(gltf.materials.size());

        for (const auto &gltfMaterial : gltf.materials)
        {
            MeshBuffer::Material material;
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
            materials.push_back(MeshBuffer::Material{Eigen::Vector4f(1, 1, 1, 1), 0.0f, 0.5f});
        }

        std::vector<PrimordialMesh> primordialMeshes;
        primordialMeshes.reserve(gltf.meshes.size());

        for (const auto &gltfMesh : gltf.meshes)
        {
            PrimordialMesh primordialMesh;
            primordialMesh.name = gltfMesh.name;
            primordialMesh.primitives.reserve(gltfMesh.primitives.size());

            for (const auto &gltfPrimitive : gltfMesh.primitives)
            {
                bool_t hasNormals = gltfPrimitive.attributes.normal != -1;
                bool_t hasUVs = gltfPrimitive.attributes.uv != -1;
                bool_t hasIndices = gltfPrimitive.indices != -1;

                Jangine::IO::Gltf::Model::Accessor positionAccessor = gltf.accessors[gltfPrimitive.attributes.position];
                Jangine::IO::Gltf::Model::BufferView positionBufferView = gltf.bufferViews[positionAccessor.bufferView];
                Jangine::IO::Gltf::Model::Accessor normalAccessor = hasNormals ? gltf.accessors[gltfPrimitive.attributes.normal] : Jangine::IO::Gltf::Model::Accessor{};
                Jangine::IO::Gltf::Model::BufferView normalBufferView = hasNormals ? gltf.bufferViews[normalAccessor.bufferView] : Jangine::IO::Gltf::Model::BufferView{};
                Jangine::IO::Gltf::Model::Accessor uvAccessor = hasUVs ? gltf.accessors[gltfPrimitive.attributes.uv] : Jangine::IO::Gltf::Model::Accessor{};
                Jangine::IO::Gltf::Model::BufferView uvBufferView = hasUVs ? gltf.bufferViews[uvAccessor.bufferView] : Jangine::IO::Gltf::Model::BufferView{};
                Jangine::IO::Gltf::Model::Accessor indexAccessor = hasIndices ? gltf.accessors[gltfPrimitive.indices] : Jangine::IO::Gltf::Model::Accessor{};
                Jangine::IO::Gltf::Model::BufferView indexBufferView = hasIndices ? gltf.bufferViews[indexAccessor.bufferView] : Jangine::IO::Gltf::Model::BufferView{};

                ThrowInvalidDataIf(positionAccessor.type != Jangine::IO::Gltf::Model::Accessor::Type::VEC3, "Position accessor must be of type VEC3");
                ThrowInvalidDataIf(hasNormals && normalAccessor.type != Jangine::IO::Gltf::Model::Accessor::Type::VEC3, "Normal accessor must be of type VEC3");
                ThrowInvalidDataIf(hasUVs && uvAccessor.type != Jangine::IO::Gltf::Model::Accessor::Type::VEC2, "UV accessor must be of type VEC2");
                ThrowInvalidDataIf((hasNormals && positionAccessor.count != normalAccessor.count) || (hasUVs && positionAccessor.count != uvAccessor.count), "Inconsistent accessor counts");

                auto calculateOffset = [](const Jangine::IO::Gltf::Model::BufferView &bufferView, const Jangine::IO::Gltf::Model::Accessor &accessor, size_t i) -> size_t
                {
                    return bufferView.byteOffset + accessor.offset + i * accessor.stride;
                };

                auto readPosition = [&](size_t i) -> Eigen::Vector3f
                {
                    return Eigen::Vector3f{reinterpret_cast<const float_t *>(gltf.data.data() + calculateOffset(positionBufferView, positionAccessor, i))};
                };

                auto readNormal = [&](size_t i) -> Eigen::Vector3f
                {
                    return Eigen::Vector3f{reinterpret_cast<const float_t *>(gltf.data.data() + calculateOffset(normalBufferView, normalAccessor, i))};
                };

                auto readUV = [&](size_t i) -> Eigen::Vector2f
                {
                    return Eigen::Vector2f{reinterpret_cast<const float_t *>(gltf.data.data() + calculateOffset(uvBufferView, uvAccessor, i))};
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

                size_t primitiveVertexOffset = vertices.size();
                size_t vertexCount = positionAccessor.count;

                for (size_t i = 0; i < positionAccessor.count; i++)
                {
                    MeshBuffer::Vertex vertex;
                    vertex.position = readPosition(i);
                    vertex.normal = hasNormals ? readNormal(i) : Eigen::Vector3f(0, 0, 1);
                    vertex.uv = hasUVs ? readUV(i) : Eigen::Vector2f(0, 0);

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

                Mesh::Primitive primitive;
                primitive.indexOffset = static_cast<uint32_t>(primitiveIndexOffset);
                primitive.indexCount = static_cast<uint32_t>(indexCount);
                primitive.vertexOffset = static_cast<uint32_t>(primitiveVertexOffset);
                primitive.vertexCount = static_cast<uint32_t>(vertexCount);
                primitive.materialIndex = static_cast<MeshBuffer::Material::Id>(std::clamp(gltfPrimitive.material, 0u, static_cast<uint32_t>(materials.size() - 1)));
                primitive.materialHasTransparency = materials[primitive.materialIndex].base[3] < 1.0f;

                primordialMesh.primitives.push_back(primitive);
            }

            primordialMeshes.push_back(std::move(primordialMesh));
        }

        auto _vertexBuffer = gfx->CreateMemoryBuffer(
            vertices.size() * sizeof(MeshBuffer::Vertex),
            MemoryBufferUsage::Vertex | MemoryBufferUsage::TransferDst,
            MemoryAccess::None);
        gfx->StageToMemoryBuffer(_vertexBuffer.get(), Span(vertices));

        auto _indexBuffer = gfx->CreateMemoryBuffer(
            indices.size() * sizeof(MeshBuffer::Index),
            MemoryBufferUsage::Index | MemoryBufferUsage::TransferDst,
            MemoryAccess::None);
        gfx->StageToMemoryBuffer(_indexBuffer.get(), Span(indices));

        auto _materialBuffer = gfx->CreateMemoryBuffer(
            materials.size() * sizeof(MeshBuffer::Material),
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
            Mesh::Id meshId = scene.CreateMesh(meshBuffer, std::move(primordialMesh.primitives));
            meshIds.push_back(meshId);
        }

        std::function<void(size_t, Node::Id)> importNode = [&](size_t gltfNodeIndex, Node::Id parentId)
        {
            const auto &gltfNode = gltf.nodes[gltfNodeIndex];
            Node& node = scene.CreateNode();

            if (gltfNode.mesh >= 0)
            {
                Mesh::Id meshId = meshIds[gltfNode.mesh];
                node.SetMeshId(meshId);
            }

            scene.SetRelativeTransform(node, gltfNode.transform);
            scene.SetName(node.GetId(), gltfNode.name);
            scene.SetAncestor(node, parentId);

            for (auto childGltfNodeId : gltfNode.children)
            {
                importNode(childGltfNodeId, node.GetId());
            }
        };

        for (size_t i = 0; i < gltf.nodes.size(); ++i)
        {
            if (gltf.nodes[i].parent == -1)
            {
                importNode(i, scene.GetWorld().GetId());
            }
        }

        scene.Print([&](std::string output)
                    { logger.Warning(output); });

        ReadOnlyScene copy;
        scene.FillReadOnlyCopy(copy);

        copy.Print([&](std::string output)
                   { logger.Error(output); });
    }

    void Core3D::Export(Jangine::IO::Gltf::Model &gltf)
    {
        (void)gltf;

        std::unordered_map<Mesh::Id::value_type, Jangine::IO::Gltf::Model::MeshIndex> meshToGltfMesh;
        std::unordered_map<Node::Id::value_type, Jangine::IO::Gltf::Model::NodeIndex> nodeToGltfNode;
        std::vector<Jangine::IO::Gltf::Model::Mesh::Primitive> gltfPrimitives;

        for (const auto &mesh : scene.GetMeshes())
        {
            const MeshBuffer *meshBuffer = mesh.GetBuffer();
            const auto &vertices = meshBuffer->GetVertices();
            const auto &indices = meshBuffer->GetIndices();
            const auto &materials = meshBuffer->GetMaterials();

            gltfPrimitives.clear();
            gltfPrimitives.reserve(mesh.GetPrimitives().size());
            for (const auto &primitive : mesh.GetPrimitives())
            {
                Jangine::IO::Gltf::Model::Material material{
                    .name = std::format("Material_{:03d}", primitive.materialIndex),
                    .pbrMetallicRoughness = {
                        .baseColorFactor = {
                            materials[primitive.materialIndex].base[0],
                            materials[primitive.materialIndex].base[1],
                            materials[primitive.materialIndex].base[2],
                            materials[primitive.materialIndex].base[3]},
                        .metallicFactor = materials[primitive.materialIndex].metalness,
                        .roughnessFactor = materials[primitive.materialIndex].roughness}};

                std::vector<Eigen::Vector3f> positions(primitive.vertexCount);
                std::vector<Eigen::Vector3f> normals(primitive.vertexCount);
                std::vector<Eigen::Vector2f> uvs(primitive.vertexCount);
                for (size_t i = 0; i < primitive.vertexCount; ++i)
                {
                    const MeshBuffer::Vertex &v = vertices[primitive.vertexOffset + i];
                    positions[i] = v.position;
                    normals[i] = v.normal;
                    uvs[i] = v.uv;
                }

                std::vector<uint32_t> primitiveIndices(primitive.indexCount);
                std::transform(
                    indices.begin() + primitive.indexOffset,
                    indices.begin() + primitive.indexOffset + primitive.indexCount,
                    primitiveIndices.begin(),
                    [&](uint32_t index)
                    { return index - primitive.vertexOffset; });

                Jangine::IO::Gltf::Model::Mesh::Primitive gltfPrimitive{
                    .indices = gltf.CreateAccessorFromData(std::span<const uint32_t>(primitiveIndices)),
                    .material = gltf.CreateMaterial(material),
                    .attributes = {
                        .position = gltf.CreateAccessorFromData(std::span<const Eigen::Vector3f>(positions)),
                        .normal = gltf.CreateAccessorFromData(std::span<const Eigen::Vector3f>(normals)),
                        .uv = gltf.CreateAccessorFromData(std::span<const Eigen::Vector2f>(uvs))}};
                gltfPrimitives.push_back(gltfPrimitive);
            }

            Jangine::IO::Gltf::Model::Mesh gltfMesh{
                .name = std::format("Mesh_{:03d}", mesh.GetId().value),
                .primitives = std::move(gltfPrimitives)};

            Jangine::IO::Gltf::Model::MeshIndex gltfMeshIndex = gltf.CreateMesh(gltfMesh);
            meshToGltfMesh[mesh.GetId()] = gltfMeshIndex;
        }

        // First pass: create all nodes
        for (const auto &node : scene.GetNodes())
        {
            // Skip world node
            if (node.GetId() == Node::Id{0})
                continue;

            Jangine::IO::Gltf::Model::Node gltfNode;
            gltfNode.name = scene.GetName(node.GetId()).value_or(std::format("Node_{:03d}", node.GetId().value));
            gltfNode.transform = node.GetRelativeTransform();

            Mesh::Id associatedMeshId = node.GetMeshId();
            if (associatedMeshId)
            {
                gltfNode.mesh = meshToGltfMesh.at(associatedMeshId);
            }

            Jangine::IO::Gltf::Model::NodeIndex gltfNodeIndex = gltf.CreateNode(gltfNode);
            nodeToGltfNode[node.GetId()] = gltfNodeIndex;
        }

        // Second pass: set up parent-child relationships
        for (const auto &node : scene.GetNodes())
        {
            // Skip world node
            if (node.GetId() == Node::Id{0})
                continue;

            Node::Id descendant = node.GetDescendant();
            while (descendant)
            {
                Jangine::IO::Gltf::Model::Node &gltfNode = gltf.nodes[nodeToGltfNode.at(node.GetId())];
                gltfNode.children.push_back(nodeToGltfNode.at(descendant));
                descendant = scene.GetNode(descendant).GetSibling();
            }
        }

        gltf.ConnectHierarchy();
        gltf.CompactMaterials();
    }
}