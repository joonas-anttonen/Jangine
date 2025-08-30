#pragma once

#include "IO.hpp"
#include <nlohmann/json.hpp>

namespace Jangine::IO::Gltf
{
    struct Model
    {
        using AccessorIndex = int32_t;
        using BufferViewIndex = int32_t;
        using BufferIndex = int32_t;
        using MeshIndex = int32_t;
        using NodeIndex = int32_t;
        using SceneIndex = int32_t;
        using MaterialIndex = uint32_t;

        /// @brief Metadata about the glTF asset.
        struct Asset
        {
            std::string generator;
            std::string version;

            friend void from_json(const nlohmann::json &j, Asset &asset)
            {
                asset.version = j.at("version").get<std::string>();
                asset.generator = j.value<std::string>("generator", "");
            }

            friend void to_json(nlohmann::json &j, const Asset &asset)
            {
                j = nlohmann::json{
                    {"version", asset.version},
                };
                if (!asset.generator.empty())
                {
                    j["generator"] = asset.generator;
                }
            }
        };

        /// @brief The root nodes of a scene.
        struct Scene
        {
            std::string name;
            std::vector<NodeIndex> nodes;

            friend void from_json(const nlohmann::json &j, Scene &scene)
            {
                scene.name = j.at("name").get<std::string>();
                scene.nodes = j.at("nodes").get<std::vector<NodeIndex>>();
            }

            friend void to_json(nlohmann::json &j, const Scene &scene)
            {
                j = nlohmann::json{
                    {"name", scene.name},
                    {"nodes", scene.nodes},
                };
            }
        };

        /// @brief A set of primitives to be rendered. Its global transform is defined by a node that references it.
        struct Mesh
        {
            /// @brief Geometry to be rendered with the given material.
            struct Primitive
            {
                struct Attributes
                {
                    AccessorIndex position = -1;
                    AccessorIndex normal = -1;
                    AccessorIndex uv = -1;

                    friend void from_json(const nlohmann::json &j, Attributes &attributes)
                    {
                        attributes.position = j.value<AccessorIndex>("POSITION", -1);
                        attributes.normal = j.value<AccessorIndex>("NORMAL", -1);
                        attributes.uv = j.value<AccessorIndex>("TEXCOORD_0", -1);
                    }

                    friend void to_json(nlohmann::json &j, const Attributes &attributes)
                    {
                        j = nlohmann::json{
                            {"POSITION", attributes.position},
                        };
                        if (attributes.normal != -1)
                        {
                            j["NORMAL"] = attributes.normal;
                        }
                        if (attributes.uv != -1)
                        {
                            j["TEXCOORD_0"] = attributes.uv;
                        }
                    }
                };

                AccessorIndex indices = -1;
                MaterialIndex material = 0;
                int32_t mode = 4;
                Attributes attributes;

                friend void from_json(const nlohmann::json &j, Primitive &primitive)
                {
                    primitive.attributes = j.at("attributes").get<Attributes>();
                    primitive.indices = j.value<AccessorIndex>("indices", -1);
                    primitive.material = j.value<MaterialIndex>("material", 0);
                    primitive.mode = j.value<int32_t>("mode", 4);
                }

                friend void to_json(nlohmann::json &j, const Primitive &primitive)
                {
                    j = nlohmann::json{
                        {"attributes", primitive.attributes},
                        {"indices", primitive.indices},
                        {"material", primitive.material},
                    };
                    if (primitive.mode != 4)
                    {
                        j["mode"] = primitive.mode;
                    }
                }
            };

            std::string name;
            std::vector<Primitive> primitives;

            friend void from_json(const nlohmann::json &j, Mesh &mesh)
            {
                mesh.name = j.at("name").get<std::string>();
                mesh.primitives = j.at("primitives").get<std::vector<Primitive>>();
            }

            friend void to_json(nlohmann::json &j, const Mesh &mesh)
            {
                j = nlohmann::json{
                    {"name", mesh.name},
                    {"primitives", mesh.primitives},
                };
            }
        };

        /// @brief A node in the node hierarchy.
        struct Node
        {
            std::string name;
            NodeIndex parent;
            std::vector<NodeIndex> children;
            MeshIndex mesh;

            Eigen::Vector3f scale;
            Eigen::Isometry3f transform;

            Node()
                : parent(-1),
                  mesh(-1),
                  scale(Eigen::Vector3f::Ones()),
                  transform(Eigen::Isometry3f::Identity())
            {
            }

            friend void from_json(const nlohmann::json &j, Node &node)
            {
                node.name = j.value<std::string>("name", "");
                node.children = j.value<std::vector<NodeIndex>>("children", {});
                node.mesh = j.value<MeshIndex>("mesh", -1);

                Eigen::Quaternionf q{Eigen::Quaternionf::Identity()};
                Eigen::Vector3f s{1, 1, 1};
                Eigen::Vector3f t{0, 0, 0};

                auto maybe_matrix = j.value<std::optional<std::array<float, 16>>>("matrix", std::nullopt);
                if (maybe_matrix.has_value())
                {
                    Eigen::Affine3f m{Eigen::Map<const Eigen::Matrix<float, 4, 4, Eigen::ColMajor>>(maybe_matrix->data())};

                    q = Eigen::Quaternionf(m.rotation());
                    s = m.linear().colwise().norm();
                    t = m.translation();
                }

                auto maybe_rotation = j.value<std::optional<std::array<float, 4>>>("rotation", std::nullopt);
                if (maybe_rotation.has_value())
                {
                    q = Eigen::Quaternionf(maybe_rotation->data());
                }

                auto maybe_scale = j.value<std::optional<std::array<float, 3>>>("scale", std::nullopt);
                if (maybe_scale.has_value())
                {
                    s = Eigen::Vector3f(maybe_scale->data());
                }

                auto maybe_translation = j.value<std::optional<std::array<float, 3>>>("translation", std::nullopt);
                if (maybe_translation.has_value())
                {
                    t = Eigen::Vector3f(maybe_translation->data());
                }

                Eigen::Isometry3f tf{Eigen::Isometry3f::Identity()};
                tf.linear() = q.toRotationMatrix();
                tf.translation() = t;

                node.transform = tf;
                node.scale = s;
            }

            friend void to_json(nlohmann::json &j, const Node &node)
            {
                j = nlohmann::json{
                    {"name", node.name},
                };

                if (!node.children.empty())
                {
                    j["children"] = node.children;
                }

                if (node.mesh != -1)
                {
                    j["mesh"] = node.mesh;
                }

                // Only write transform
                if (!node.transform.isApprox(Eigen::Isometry3f::Identity()))
                {
                    std::array<float, 16> mat;
                    Eigen::Map<Eigen::Matrix<float, 4, 4, Eigen::ColMajor>>(mat.data()) = node.transform.matrix();
                    j["matrix"] = mat;
                }
            }
        };

        /// @brief A typed view into a buffer view that contains raw binary data.
        struct Accessor
        {
            enum class ComponentType
            {
                BYTE = 5120,
                UNSIGNED_BYTE = 5121,
                SHORT = 5122,
                UNSIGNED_SHORT = 5123,
                UNSIGNED_INT = 5125,
                FLOAT = 5126
            };

            enum class Type
            {
                SCALAR,
                VEC2,
                VEC3,
                VEC4,
                MAT2,
                MAT3,
                MAT4
            };

            Type type = Type::SCALAR;
            ComponentType componentType = ComponentType::FLOAT;
            BufferViewIndex bufferView = -1;
            size_t offset = 0;
            size_t count = 0;
            size_t stride = 0;
            bool_t normalized = false;

            friend void from_json(const nlohmann::json &j, Accessor &accessor)
            {
                // Required fields
                accessor.componentType = j.at("componentType").get<ComponentType>();
                accessor.count = j.at("count").get<size_t>();
                std::string typeString = j.at("type").get<std::string>();

                // Optional fields
                accessor.bufferView = j.value<BufferViewIndex>("bufferView", -1);
                accessor.offset = j.value<size_t>("byteOffset", 0);
                accessor.normalized = j.value<bool_t>("normalized", false);

                // Calculate stride
                int32_t numComponents = 0;
                if (typeString == "SCALAR")
                {
                    numComponents = 1;
                    accessor.type = Type::SCALAR;
                }
                else if (typeString == "VEC2")
                {
                    numComponents = 2;
                    accessor.type = Type::VEC2;
                }
                else if (typeString == "VEC3")
                {
                    numComponents = 3;
                    accessor.type = Type::VEC3;
                }
                else if (typeString == "VEC4")
                {
                    numComponents = 4;
                    accessor.type = Type::VEC4;
                }
                else if (typeString == "MAT2")
                {
                    numComponents = 4;
                    accessor.type = Type::MAT2;
                }
                else if (typeString == "MAT3")
                {
                    numComponents = 9;
                    accessor.type = Type::MAT3;
                }
                else if (typeString == "MAT4")
                {
                    numComponents = 16;
                    accessor.type = Type::MAT4;
                }

                int32_t componentSize = 0;
                if (accessor.componentType == ComponentType::BYTE || accessor.componentType == ComponentType::UNSIGNED_BYTE)
                    componentSize = 1;
                else if (accessor.componentType == ComponentType::SHORT || accessor.componentType == ComponentType::UNSIGNED_SHORT)
                    componentSize = 2;
                else if (accessor.componentType == ComponentType::UNSIGNED_INT || accessor.componentType == ComponentType::FLOAT)
                    componentSize = 4;

                accessor.stride = numComponents * componentSize;
            }

            friend void to_json(nlohmann::json &j, const Accessor &accessor)
            {
                std::string typeString;
                switch (accessor.type)
                {
                case Type::SCALAR:
                    typeString = "SCALAR";
                    break;
                case Type::VEC2:
                    typeString = "VEC2";
                    break;
                case Type::VEC3:
                    typeString = "VEC3";
                    break;
                case Type::VEC4:
                    typeString = "VEC4";
                    break;
                case Type::MAT2:
                    typeString = "MAT2";
                    break;
                case Type::MAT3:
                    typeString = "MAT3";
                    break;
                case Type::MAT4:
                    typeString = "MAT4";
                    break;
                }
                j["type"] = typeString;
                j["componentType"] = accessor.componentType;
                j["count"] = accessor.count;
                if (accessor.bufferView != -1)
                    j["bufferView"] = accessor.bufferView;
                if (accessor.offset != 0)
                    j["byteOffset"] = accessor.offset;
                if (accessor.normalized)
                    j["normalized"] = accessor.normalized;
            }
        };

        /// @brief A buffer points to binary geometry, animation, or skins.
        struct Buffer
        {
            size_t byteLength;
            std::string uri;
            std::string name;

            friend void from_json(const nlohmann::json &j, Buffer &buffer)
            {
                // Required fields
                buffer.byteLength = j.at("byteLength").get<size_t>();

                // Optional fields
                buffer.uri = j.value<std::string>("uri", "");
                buffer.name = j.value<std::string>("name", "");
            }

            friend void to_json(nlohmann::json &j, const Buffer &buffer)
            {
                j = nlohmann::json{
                    {"byteLength", buffer.byteLength},
                };
                if (!buffer.uri.empty())
                {
                    j["uri"] = buffer.uri;
                }
                if (!buffer.name.empty())
                {
                    j["name"] = buffer.name;
                }
            }
        };

        /// @brief A view into a buffer generally representing a subset of the buffer.
        struct BufferView
        {
            BufferIndex buffer = 0;
            size_t byteOffset = 0;
            size_t byteLength = 0;
            size_t byteStride = 0;
            size_t target = 34962;

            friend void from_json(const nlohmann::json &j, BufferView &bufferView)
            {
                // Required fields
                bufferView.buffer = j.at("buffer").get<BufferIndex>();
                bufferView.byteLength = j.at("byteLength").get<size_t>();

                // Optional fields
                bufferView.byteOffset = j.value<size_t>("byteOffset", 0);
                bufferView.byteStride = j.value<size_t>("byteStride", 0);
                bufferView.target = j.value<size_t>("target", 34962);
            }

            friend void to_json(nlohmann::json &j, const BufferView &bufferView)
            {
                j = nlohmann::json{
                    {"buffer", bufferView.buffer},
                    {"byteLength", bufferView.byteLength}};
                if (bufferView.byteOffset != 0)
                    j["byteOffset"] = bufferView.byteOffset;
                if (bufferView.byteStride != 0)
                    j["byteStride"] = bufferView.byteStride;
                if (bufferView.target != 34962)
                    j["target"] = bufferView.target;
            }
        };

        /// @brief The material appearance of a primitive.
        struct Material
        {
            struct PbrMetallicRoughness
            {
                std::array<float_t, 4> baseColorFactor;
                float_t metallicFactor = 0.0f;
                float_t roughnessFactor = 0.5f;

                friend void from_json(const nlohmann::json &j, PbrMetallicRoughness &pbr)
                {
                    pbr.baseColorFactor = j.value<std::array<float_t, 4>>("baseColorFactor", std::array<float_t, 4>{1, 1, 1, 1});
                    pbr.metallicFactor = j.value<float_t>("metallicFactor", 0.0f);
                    pbr.roughnessFactor = j.value<float_t>("roughnessFactor", 0.5f);
                }

                friend void to_json(nlohmann::json &j, const PbrMetallicRoughness &pbr)
                {
                    j = nlohmann::json{
                        {"baseColorFactor", pbr.baseColorFactor},
                        {"metallicFactor", pbr.metallicFactor},
                        {"roughnessFactor", pbr.roughnessFactor},
                    };
                }
            };

            std::string name;
            PbrMetallicRoughness pbrMetallicRoughness;

            friend void from_json(const nlohmann::json &j, Material &material)
            {
                material.name = j.value<std::string>("name", "");
                material.pbrMetallicRoughness = j.value<PbrMetallicRoughness>("pbrMetallicRoughness", PbrMetallicRoughness{});
            }

            friend void to_json(nlohmann::json &j, const Material &material)
            {
                j = nlohmann::json{
                    {"pbrMetallicRoughness", material.pbrMetallicRoughness},
                };
                if (!material.name.empty())
                {
                    j["name"] = material.name;
                }
            }
        };

        std::vector<uint8_t> data;

        Asset asset;
        SceneIndex defaultScene;
        std::vector<Scene> scenes;
        std::vector<Node> nodes;
        std::vector<Mesh> meshes;
        std::vector<Material> materials;
        std::vector<Accessor> accessors;
        std::vector<Buffer> buffers;
        std::vector<BufferView> bufferViews;

        friend void from_json(const nlohmann::json &j, Model &model)
        {
            model.asset = j.at("asset").get<Asset>();
            model.defaultScene = j.at("scene").get<int32_t>();
            model.scenes = j.at("scenes").get<std::vector<Scene>>();
            model.nodes = j.at("nodes").get<std::vector<Node>>();
            model.meshes = j.at("meshes").get<std::vector<Mesh>>();
            model.materials = j.value<std::vector<Material>>("materials", {Material{
                                                                              .name = "FALLBACK",
                                                                              .pbrMetallicRoughness = Material::PbrMetallicRoughness{
                                                                                  .baseColorFactor = {1.0f, 1.0f, 1.0f, 1.0f},
                                                                                  .metallicFactor = 0.0f,
                                                                                  .roughnessFactor = 0.5f}}});
            model.accessors = j.at("accessors").get<std::vector<Accessor>>();
            model.buffers = j.at("buffers").get<std::vector<Buffer>>();
            model.bufferViews = j.at("bufferViews").get<std::vector<BufferView>>();
        }

        friend void to_json(nlohmann::json &j, const Model &model)
        {
            Asset asset{
                .generator = "Jangine",
                .version = "2.0"};

            j = nlohmann::json{
                {"asset", asset},
                {"nodes", model.nodes},
                {"meshes", model.meshes},
                {"materials", model.materials},
                {"accessors", model.accessors},
                {"bufferViews", model.bufferViews}};

            Scene scene{
                .name = "Scene"};

            for (NodeIndex i = 0; i < static_cast<NodeIndex>(model.nodes.size()); i++)
            {
                if (model.nodes[i].parent == -1)
                {
                    scene.nodes.push_back(i);
                }
            }

            j["scenes"] = {scene};
            j["scene"] = 0;

            Buffer buffer{
                .byteLength = model.data.size()};
            j["buffers"] = {buffer};
        }

        /// @brief Appends all supported aspects of another model into this one.
        void Append(const Model &other)
        {
            // Data
            size_t dataOffset = data.size();
            data.reserve(dataOffset + other.data.size());
            data.insert(data.end(), other.data.begin(), other.data.end());

            NodeIndex nodeOffset = static_cast<NodeIndex>(nodes.size());
            MeshIndex meshOffset = static_cast<MeshIndex>(meshes.size());
            MaterialIndex materialOffset = static_cast<MaterialIndex>(materials.size());
            AccessorIndex accessorOffset = static_cast<AccessorIndex>(accessors.size());
            BufferIndex bufferOffset = static_cast<BufferIndex>(buffers.size());
            BufferViewIndex bufferViewOffset = static_cast<BufferViewIndex>(bufferViews.size());

            // Buffers
            buffers.reserve(bufferOffset + other.buffers.size());
            buffers.insert(buffers.end(), other.buffers.begin(), other.buffers.end());

            // BufferViews
            bufferViews.reserve(bufferViewOffset + other.bufferViews.size());
            for (const auto &bv : other.bufferViews)
            {
                BufferView newBv = bv;
                newBv.buffer += bufferOffset;
                newBv.byteOffset += static_cast<uint32_t>(dataOffset);
                bufferViews.push_back(newBv);
            }

            // Nodes
            nodes.reserve(nodeOffset + other.nodes.size());
            for (const auto &node : other.nodes)
            {
                Node newNode = node;
                if (newNode.mesh >= 0)
                {
                    newNode.mesh += meshOffset;
                }
                for (auto &child : newNode.children)
                {
                    child += nodeOffset;
                }
                nodes.push_back(newNode);
            }

            // Accessors
            accessors.reserve(accessorOffset + other.accessors.size());
            for (const auto &accessor : other.accessors)
            {
                Accessor newAccessor = accessor;
                if (newAccessor.bufferView >= 0)
                {
                    newAccessor.bufferView += bufferViewOffset;
                }
                accessors.push_back(newAccessor);
            }

            // Materials
            materials.reserve(materialOffset + other.materials.size());
            materials.insert(materials.end(), other.materials.begin(), other.materials.end());

            // Meshes
            meshes.reserve(meshOffset + other.meshes.size());
            for (const auto &mesh : other.meshes)
            {
                Mesh newMesh = mesh;
                for (auto &primitive : newMesh.primitives)
                {
                    primitive.material += materialOffset;

                    if (primitive.indices >= 0)
                    {
                        primitive.indices += accessorOffset;
                    }
                    if (primitive.attributes.position >= 0)
                    {
                        primitive.attributes.position += accessorOffset;
                    }
                    if (primitive.attributes.normal >= 0)
                    {
                        primitive.attributes.normal += accessorOffset;
                    }
                    if (primitive.attributes.uv >= 0)
                    {
                        primitive.attributes.uv += accessorOffset;
                    }
                }
                meshes.push_back(newMesh);
            }

            // Scenes
            scenes.reserve(scenes.size() + other.scenes.size());
            for (const auto &scene : other.scenes)
            {
                Scene newScene = scene;
                for (auto &node : newScene.nodes)
                {
                    node += nodeOffset;
                }
                scenes.push_back(newScene);
            }
        }

        /// @brief Connects all nodes to their parents based on the children lists.
        void ConnectHierarchy()
        {
            for (Model::NodeIndex i = 0; i < static_cast<Model::NodeIndex>(nodes.size()); i++)
            {
                for (Model::NodeIndex childIndex : nodes[i].children)
                {
                    nodes[childIndex].parent = i;
                }
            }
        }

        /// @brief Removes duplicate materials and updates all references to them.
        void CompactMaterials()
        {
            std::unordered_map<std::string, MaterialIndex> materialMap;
            materialMap.reserve(materials.size());
            std::vector<Material> uniqueMaterials;
            uniqueMaterials.reserve(materials.size());
            
            for (auto &material : materials)
            {
                if (materialMap.find(material.name) != materialMap.end())
                    continue;

                materialMap[material.name] = static_cast<MaterialIndex>(materialMap.size());
                uniqueMaterials.push_back(material);
            }

            for (auto &mesh : meshes)
            {
                for (auto &primitive : mesh.primitives)
                {
                    if (primitive.material >= 0)
                    {
                        primitive.material = materialMap[materials[primitive.material].name];
                    }
                }
            }

            materials = std::move(uniqueMaterials);
        }

        AccessorIndex CreateAccessorFromData(std::span<const Eigen::Vector3f> in_data)
        {
            size_t in_byteSize = in_data.size() * sizeof(Eigen::Vector3f);
            size_t originalSize = data.size();
            data.resize(originalSize + in_byteSize);
            std::memcpy(data.data() + originalSize, in_data.data(), in_byteSize);

            Gltf::Model::BufferView bufferView{
                .buffer = Gltf::Model::BufferIndex{0},
                .byteOffset = originalSize,
                .byteLength = in_byteSize};
            bufferViews.push_back(bufferView);
            Gltf::Model::BufferViewIndex bufferViewIndex = static_cast<Gltf::Model::BufferViewIndex>(bufferViews.size() - 1);

            Gltf::Model::Accessor accessor{
                .type = Gltf::Model::Accessor::Type::VEC3,
                .componentType = Gltf::Model::Accessor::ComponentType::FLOAT,
                .bufferView = bufferViewIndex,
                .count = in_data.size(),
                .stride = sizeof(Eigen::Vector3f),
            };
            accessors.push_back(accessor);
            return static_cast<Gltf::Model::AccessorIndex>(accessors.size() - 1);
        }

        AccessorIndex CreateAccessorFromData(std::span<const Eigen::Vector2f> in_data)
        {
            size_t in_byteSize = in_data.size() * sizeof(Eigen::Vector2f);
            size_t originalSize = data.size();
            data.resize(originalSize + in_byteSize);
            std::memcpy(data.data() + originalSize, in_data.data(), in_byteSize);

            Gltf::Model::BufferView bufferView{
                .buffer = Gltf::Model::BufferIndex{0},
                .byteOffset = originalSize,
                .byteLength = in_byteSize};
            bufferViews.push_back(bufferView);
            Gltf::Model::BufferViewIndex bufferViewIndex = static_cast<Gltf::Model::BufferViewIndex>(bufferViews.size() - 1);

            Gltf::Model::Accessor accessor{
                .type = Gltf::Model::Accessor::Type::VEC2,
                .componentType = Gltf::Model::Accessor::ComponentType::FLOAT,
                .bufferView = bufferViewIndex,
                .count = in_data.size(),
                .stride = sizeof(Eigen::Vector2f),
            };
            accessors.push_back(accessor);
            return static_cast<Gltf::Model::AccessorIndex>(accessors.size() - 1);
        }

        AccessorIndex CreateAccessorFromData(std::span<const uint32_t> in_data)
        {
            size_t in_byteSize = in_data.size() * sizeof(uint32_t);
            size_t originalSize = data.size();
            data.resize(originalSize + in_byteSize);
            std::memcpy(data.data() + originalSize, in_data.data(), in_byteSize);

            Gltf::Model::BufferView bufferView{
                .buffer = Gltf::Model::BufferIndex{0},
                .byteOffset = originalSize,
                .byteLength = in_byteSize};
            bufferViews.push_back(bufferView);
            Gltf::Model::BufferViewIndex bufferViewIndex = static_cast<Gltf::Model::BufferViewIndex>(bufferViews.size() - 1);

            Gltf::Model::Accessor accessor{
                .type = Gltf::Model::Accessor::Type::SCALAR,
                .componentType = Gltf::Model::Accessor::ComponentType::UNSIGNED_INT,
                .bufferView = bufferViewIndex,
                .count = in_data.size(),
                .stride = sizeof(uint32_t),
            };
            accessors.push_back(accessor);
            return static_cast<Gltf::Model::AccessorIndex>(accessors.size() - 1);
        }

        MaterialIndex CreateMaterial(const Material &in_data)
        {
            materials.push_back(in_data);
            return static_cast<Gltf::Model::MaterialIndex>(materials.size() - 1);
        }

        MeshIndex CreateMesh(const Mesh &in_data)
        {
            meshes.push_back(in_data);
            return static_cast<Gltf::Model::MeshIndex>(meshes.size() - 1);
        }

        NodeIndex CreateNode(const Node &in_data)
        {
            nodes.push_back(in_data);
            return static_cast<Gltf::Model::NodeIndex>(nodes.size() - 1);
        }
    };

    Jangine::IO::Status LoadFromFile(const std::string &filename, Model &model);
    Jangine::IO::Status SaveToFile(const std::string &filename, const Model &model);
}