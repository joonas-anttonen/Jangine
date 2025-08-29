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
        using MaterialIndex = int32_t;

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
        };

        /// @brief A set of primitives to be rendered. Its global transform is defined by a node that references it.
        struct Mesh
        {
            /// @brief Geometry to be rendered with the given material.
            struct Primitive
            {
                struct Attributes
                {
                    AccessorIndex position;
                    AccessorIndex normal;
                    AccessorIndex uv;

                    friend void from_json(const nlohmann::json &j, Attributes &attributes)
                    {
                        attributes.position = j.value<AccessorIndex>("POSITION", -1);
                        attributes.normal = j.value<AccessorIndex>("NORMAL", -1);
                        attributes.uv = j.value<AccessorIndex>("TEXCOORD_0", -1);
                    }
                };

                Attributes attributes;
                AccessorIndex indices;
                MaterialIndex material;
                int32_t mode;

                friend void from_json(const nlohmann::json &j, Primitive &primitive)
                {
                    primitive.attributes = j.at("attributes").get<Attributes>();
                    primitive.indices = j.value<AccessorIndex>("indices", -1);
                    primitive.material = j.value<MaterialIndex>("material", 0); // NOTE: Even with a glTF with no materials defined, 0 index material is always present
                    primitive.mode = j.value<int32_t>("mode", -1);
                }
            };

            std::string name;
            std::vector<Primitive> primitives;

            friend void from_json(const nlohmann::json &j, Mesh &mesh)
            {
                mesh.name = j.at("name").get<std::string>();
                mesh.primitives = j.at("primitives").get<std::vector<Primitive>>();
            }
        };

        /// @brief A node in the node hierarchy.
        struct Node
        {
            std::string name;
            std::vector<NodeIndex> children;
            MeshIndex mesh;

            Eigen::Vector3f scale;
            Eigen::Isometry3f transform;

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

            BufferViewIndex bufferView;
            uint32_t byteOffset;
            ComponentType componentType;
            bool_t normalized;
            uint32_t count;
            Type type;
            std::string name;

            size_t stride;

            friend void from_json(const nlohmann::json &j, Accessor &accessor)
            {
                // Required fields
                accessor.componentType = j.at("componentType").get<ComponentType>();
                accessor.count = j.at("count").get<uint32_t>();
                std::string typeString = j.at("type").get<std::string>();

                // Optional fields
                accessor.bufferView = j.value<BufferViewIndex>("bufferView", -1);
                accessor.byteOffset = j.value<uint32_t>("byteOffset", 0);
                accessor.normalized = j.value<bool_t>("normalized", false);
                accessor.name = j.value<std::string>("name", "");

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
                if (accessor.componentType == ComponentType::BYTE)
                    componentSize = 1;
                else if (accessor.componentType == ComponentType::UNSIGNED_BYTE)
                    componentSize = 1;
                else if (accessor.componentType == ComponentType::SHORT)
                    componentSize = 2;
                else if (accessor.componentType == ComponentType::UNSIGNED_SHORT)
                    componentSize = 2;
                else if (accessor.componentType == ComponentType::UNSIGNED_INT)
                    componentSize = 4;
                else if (accessor.componentType == ComponentType::FLOAT)
                    componentSize = 4;

                accessor.stride = numComponents * componentSize;
            }
        };

        /// @brief A buffer points to binary geometry, animation, or skins.
        struct Buffer
        {
            uint32_t byteLength;
            std::string uri;
            std::string name;

            friend void from_json(const nlohmann::json &j, Buffer &buffer)
            {
                // Required fields
                buffer.byteLength = j.at("byteLength").get<uint32_t>();

                // Optional fields
                buffer.uri = j.value<std::string>("uri", "");
                buffer.name = j.value<std::string>("name", "");
            }
        };

        /// @brief A view into a buffer generally representing a subset of the buffer.
        struct BufferView
        {
            BufferIndex buffer;
            size_t byteOffset;
            size_t byteLength;
            size_t byteStride;
            int32_t target;
            std::string name;

            friend void from_json(const nlohmann::json &j, BufferView &bufferView)
            {
                // Required fields
                bufferView.buffer = j.at("buffer").get<BufferIndex>();
                bufferView.byteLength = j.at("byteLength").get<size_t>();

                // Optional fields
                bufferView.byteOffset = j.value<size_t>("byteOffset", 0);
                bufferView.byteStride = j.value<size_t>("byteStride", 0);
                bufferView.target = j.value<uint32_t>("target", 0);
                bufferView.name = j.value<std::string>("name", "");
            }
        };

        /// @brief The material appearance of a primitive.
        struct Material
        {
            struct PbrMetallicRoughness
            {
                std::array<float_t, 4> baseColorFactor;
                float_t metallicFactor;
                float_t roughnessFactor;

                friend void from_json(const nlohmann::json &j, PbrMetallicRoughness &pbr)
                {
                    pbr.baseColorFactor = j.value<std::array<float_t, 4>>("baseColorFactor", std::array<float_t, 4>{1, 1, 1, 1});
                    pbr.metallicFactor = j.value<float_t>("metallicFactor", 0.0f);
                    pbr.roughnessFactor = j.value<float_t>("roughnessFactor", 0.5f);
                }
            };

            std::string name;
            PbrMetallicRoughness pbrMetallicRoughness;

            friend void from_json(const nlohmann::json &j, Material &material)
            {
                material.name = j.value<std::string>("name", "");
                material.pbrMetallicRoughness = j.value<PbrMetallicRoughness>("pbrMetallicRoughness", PbrMetallicRoughness{});
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

        /// @brief Appends all supported aspects of another model into this one.
        void Add(const Model &other)
        {
            // Data
            size_t dataOffset = data.size();
            data.insert(data.end(), other.data.begin(), other.data.end());

            NodeIndex nodeOffset = static_cast<NodeIndex>(nodes.size());
            MeshIndex meshOffset = static_cast<MeshIndex>(meshes.size());
            MaterialIndex materialOffset = static_cast<MaterialIndex>(materials.size());
            AccessorIndex accessorOffset = static_cast<AccessorIndex>(accessors.size());
            BufferIndex bufferOffset = static_cast<BufferIndex>(buffers.size());
            BufferViewIndex bufferViewOffset = static_cast<BufferViewIndex>(bufferViews.size());

            // Buffers
            buffers.insert(buffers.end(), other.buffers.begin(), other.buffers.end());

            // BufferViews
            for (const auto &bv : other.bufferViews)
            {
                BufferView newBv = bv;
                newBv.buffer += bufferOffset;
                newBv.byteOffset += static_cast<uint32_t>(dataOffset);
                bufferViews.push_back(newBv);
            }

            // Nodes
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
            materials.insert(materials.end(), other.materials.begin(), other.materials.end());

            // Meshes
            for (const auto &mesh : other.meshes)
            {
                Mesh newMesh = mesh;
                for (auto &primitive : newMesh.primitives)
                {
                    primitive.indices += accessorOffset;
                    primitive.material += materialOffset;
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
    };

    Jangine::IO::Status LoadFromFile(const std::string &filename, Model &model);
}