#include "Urdf.hpp"

#include <tinyxml2.h>

namespace Jangine::IO::Urdf
{
    static Gltf::Model::AccessorIndex AddVector3s(Gltf::Model &gltf, const std::vector<Eigen::Vector3f> &data)
    {
        size_t originalSize = gltf.data.size();
        gltf.data.resize(originalSize + data.size() * sizeof(Eigen::Vector3f));
        std::memcpy(gltf.data.data() + originalSize, data.data(), data.size() * sizeof(Eigen::Vector3f));

        Gltf::Model::BufferView bufferView{
            .buffer = Gltf::Model::BufferIndex{0},
            .byteOffset = originalSize,
            .byteLength = data.size() * sizeof(Eigen::Vector3f)};

        gltf.bufferViews.push_back(std::move(bufferView));

        Gltf::Model::BufferViewIndex bufferViewIndex = static_cast<Gltf::Model::BufferViewIndex>(gltf.bufferViews.size() - 1);
        Gltf::Model::Accessor accessor{
            .type = Gltf::Model::Accessor::Type::VEC3,
            .componentType = Gltf::Model::Accessor::ComponentType::FLOAT,
            .bufferView = bufferViewIndex,
            .count = data.size(),
            .stride = sizeof(Eigen::Vector3f),
        };

        gltf.accessors.push_back(std::move(accessor));
        return static_cast<Gltf::Model::AccessorIndex>(gltf.accessors.size() - 1);
    }

    static Gltf::Model::AccessorIndex AddUInt32s(Gltf::Model &gltf, const std::vector<uint32_t> &data)
    {
        size_t originalSize = gltf.data.size();
        gltf.data.resize(originalSize + data.size() * sizeof(uint32_t));
        std::memcpy(gltf.data.data() + originalSize, data.data(), data.size() * sizeof(uint32_t));

        Gltf::Model::BufferView bufferView{
            .buffer = Gltf::Model::BufferIndex{0},
            .byteOffset = originalSize,
            .byteLength = data.size() * sizeof(uint32_t)};

        gltf.bufferViews.push_back(std::move(bufferView));

        Gltf::Model::BufferViewIndex bufferViewIndex = static_cast<Gltf::Model::BufferViewIndex>(gltf.bufferViews.size() - 1);
        Gltf::Model::Accessor accessor{
            .type = Gltf::Model::Accessor::Type::SCALAR,
            .componentType = Gltf::Model::Accessor::ComponentType::UNSIGNED_INT,
            .bufferView = bufferViewIndex,
            .count = data.size(),
            .stride = sizeof(uint32_t),
        };

        gltf.accessors.push_back(std::move(accessor));
        return static_cast<Gltf::Model::AccessorIndex>(gltf.accessors.size() - 1);
    }

    static Gltf::Model::MeshIndex AddCylinderMesh(Gltf::Model &gltf, float_t radius, float_t length, const Gltf::Model::Material &material)
    {
        const int segments = 32;

        std::vector<Eigen::Vector3f> vertices;
        std::vector<Eigen::Vector3f> normals;
        std::vector<uint32_t> indices;

        // Generate vertices and normals for the side
        for (int i = 0; i <= segments; ++i)
        {
            float theta = (static_cast<float>(i) / segments) * 2.0f * Math::PI;
            float x = radius * std::cos(theta);
            float y = radius * std::sin(theta);
            vertices.emplace_back(x, y, -length / 2);
            vertices.emplace_back(x, y, length / 2);
            normals.emplace_back(std::cos(theta), std::sin(theta), 0.0f);
            normals.emplace_back(std::cos(theta), std::sin(theta), 0.0f);
        }

        // Generate indices for the side
        for (int i = 0; i < segments; ++i)
        {
            int base0 = i * 2;
            int base1 = ((i + 1) % segments) * 2;
            int top0 = base0 + 1;
            int top1 = base1 + 1;

            // First triangle
            indices.push_back(base0);
            indices.push_back(top0);
            indices.push_back(base1);

            // Second triangle
            indices.push_back(base1);
            indices.push_back(top0);
            indices.push_back(top1);
        }

        // Add center vertices for caps
        int bottomCenterIndex = static_cast<int>(vertices.size());
        vertices.emplace_back(0.0f, 0.0f, -length / 2);
        normals.emplace_back(0.0f, 0.0f, -1.0f);

        int topCenterIndex = static_cast<int>(vertices.size());
        vertices.emplace_back(0.0f, 0.0f, length / 2);
        normals.emplace_back(0.0f, 0.0f, 1.0f);

        // Indices for bottom cap
        for (int i = 0; i < segments; ++i)
        {
            int curr = i * 2;
            int next = ((i + 1) % segments) * 2;
            indices.push_back(bottomCenterIndex);
            indices.push_back(next);
            indices.push_back(curr);
        }

        // Indices for top cap
        for (int i = 0; i < segments; ++i)
        {
            int curr = i * 2 + 1;
            int next = ((i + 1) % segments) * 2 + 1;
            indices.push_back(topCenterIndex);
            indices.push_back(curr);
            indices.push_back(next);
        }

        gltf.materials.push_back(material);

        Gltf::Model::Mesh cylinderMesh{
            .name = "cylinder",
            .primitives = {{.indices = AddUInt32s(gltf, indices),
                            .material = static_cast<Gltf::Model::MaterialIndex>(gltf.materials.size() - 1),
                            .attributes = {
                                .position = AddVector3s(gltf, vertices),
                                .normal = AddVector3s(gltf, normals)}}}};
        gltf.meshes.push_back(std::move(cylinderMesh));
        return static_cast<Gltf::Model::MeshIndex>(gltf.meshes.size() - 1);
    }

    static Gltf::Model::MeshIndex AddSphereMesh(Gltf::Model &gltf, float_t radius, const Gltf::Model::Material &material)
    {
        const int latitudeBands = 16;
        const int longitudeBands = 32;

        std::vector<Eigen::Vector3f> vertices;
        std::vector<Eigen::Vector3f> normals;
        std::vector<uint32_t> indices;

        // Generate vertices and normals
        for (int latNumber = 0; latNumber <= latitudeBands; ++latNumber)
        {
            float theta = latNumber * Math::PI / latitudeBands;
            float sinTheta = std::sin(theta);
            float cosTheta = std::cos(theta);

            for (int longNumber = 0; longNumber <= longitudeBands; ++longNumber)
            {
                float phi = longNumber * 2.0f * Math::PI / longitudeBands;
                float sinPhi = std::sin(phi);
                float cosPhi = std::cos(phi);

                Eigen::Vector3f position = {
                    radius * cosPhi * sinTheta,
                    radius * sinPhi * sinTheta,
                    radius * cosTheta};
                vertices.push_back(position);
                normals.push_back(position.normalized());
            }
        }

        for (int latNumber = 0; latNumber < latitudeBands; ++latNumber)
        {
            for (int longNumber = 0; longNumber < longitudeBands; ++longNumber)
            {
                int first = (latNumber * (longitudeBands + 1)) + longNumber;
                int second = first + longitudeBands + 1;
                indices.push_back(first);
                indices.push_back(second);
                indices.push_back(first + 1);
                indices.push_back(second);
                indices.push_back(second + 1);
                indices.push_back(first + 1);
            }
        }

        gltf.materials.push_back(material);

        Gltf::Model::Mesh sphereMesh{
            .name = "sphere",
            .primitives = {{.indices = AddUInt32s(gltf, indices),
                            .material = static_cast<Gltf::Model::MaterialIndex>(gltf.materials.size() - 1),
                            .attributes = {
                                .position = AddVector3s(gltf, vertices),
                                .normal = AddVector3s(gltf, normals)}}}};
        gltf.meshes.push_back(std::move(sphereMesh));
        return static_cast<Gltf::Model::MeshIndex>(gltf.meshes.size() - 1);
    }

    static Gltf::Model::MeshIndex AddBoxMesh(Gltf::Model &gltf, const Eigen::Vector3f &size, const Gltf::Model::Material &material)
    {
        // Each face has 6 vertices (2 triangles), total 36 vertices for a box
        std::vector<Eigen::Vector3f> vertices = {
            // Back face (-Z)
            Eigen::Vector3f{-size.x() / 2, -size.y() / 2, -size.z() / 2},
            Eigen::Vector3f{size.x() / 2, -size.y() / 2, -size.z() / 2},
            Eigen::Vector3f{size.x() / 2, size.y() / 2, -size.z() / 2},
            Eigen::Vector3f{-size.x() / 2, -size.y() / 2, -size.z() / 2},
            Eigen::Vector3f{size.x() / 2, size.y() / 2, -size.z() / 2},
            Eigen::Vector3f{-size.x() / 2, size.y() / 2, -size.z() / 2},

            // Front face (+Z)
            Eigen::Vector3f{-size.x() / 2, -size.y() / 2, size.z() / 2},
            Eigen::Vector3f{size.x() / 2, size.y() / 2, size.z() / 2},
            Eigen::Vector3f{size.x() / 2, -size.y() / 2, size.z() / 2},
            Eigen::Vector3f{-size.x() / 2, -size.y() / 2, size.z() / 2},
            Eigen::Vector3f{-size.x() / 2, size.y() / 2, size.z() / 2},
            Eigen::Vector3f{size.x() / 2, size.y() / 2, size.z() / 2},

            // Left face (-X)
            Eigen::Vector3f{-size.x() / 2, -size.y() / 2, -size.z() / 2},
            Eigen::Vector3f{-size.x() / 2, size.y() / 2, -size.z() / 2},
            Eigen::Vector3f{-size.x() / 2, size.y() / 2, size.z() / 2},
            Eigen::Vector3f{-size.x() / 2, -size.y() / 2, -size.z() / 2},
            Eigen::Vector3f{-size.x() / 2, size.y() / 2, size.z() / 2},
            Eigen::Vector3f{-size.x() / 2, -size.y() / 2, size.z() / 2},

            // Right face (+X)
            Eigen::Vector3f{size.x() / 2, -size.y() / 2, -size.z() / 2},
            Eigen::Vector3f{size.x() / 2, -size.y() / 2, size.z() / 2},
            Eigen::Vector3f{size.x() / 2, size.y() / 2, size.z() / 2},
            Eigen::Vector3f{size.x() / 2, -size.y() / 2, -size.z() / 2},
            Eigen::Vector3f{size.x() / 2, size.y() / 2, size.z() / 2},
            Eigen::Vector3f{size.x() / 2, size.y() / 2, -size.z() / 2},

            // Top face (+Y)
            Eigen::Vector3f{-size.x() / 2, size.y() / 2, -size.z() / 2},
            Eigen::Vector3f{size.x() / 2, size.y() / 2, -size.z() / 2},
            Eigen::Vector3f{size.x() / 2, size.y() / 2, size.z() / 2},
            Eigen::Vector3f{-size.x() / 2, size.y() / 2, -size.z() / 2},
            Eigen::Vector3f{size.x() / 2, size.y() / 2, size.z() / 2},
            Eigen::Vector3f{-size.x() / 2, size.y() / 2, size.z() / 2},

            // Bottom face (-Y)
            Eigen::Vector3f{-size.x() / 2, -size.y() / 2, -size.z() / 2},
            Eigen::Vector3f{size.x() / 2, -size.y() / 2, size.z() / 2},
            Eigen::Vector3f{size.x() / 2, -size.y() / 2, -size.z() / 2},
            Eigen::Vector3f{-size.x() / 2, -size.y() / 2, -size.z() / 2},
            Eigen::Vector3f{-size.x() / 2, -size.y() / 2, size.z() / 2},
            Eigen::Vector3f{size.x() / 2, -size.y() / 2, size.z() / 2}};

        gltf.materials.push_back(material);

        Gltf::Model::Mesh boxMesh{
            .name = "box",
            .primitives = {{.material = static_cast<Gltf::Model::MaterialIndex>(gltf.materials.size() - 1),
                            .attributes = {
                                .position = AddVector3s(gltf, vertices)}}}};
        gltf.meshes.push_back(std::move(boxMesh));
        return static_cast<Gltf::Model::MeshIndex>(gltf.meshes.size() - 1);
    }

    static Eigen::Vector3f ParseXYZ(const char *xyzAttr)
    {
        float x = 0.0f, y = 0.0f, z = 0.0f;
        sscanf_s(xyzAttr, "%f %f %f", &x, &y, &z);
        return Eigen::Vector3f{x, y, z};
    }

    static Eigen::Quaternionf ParseRPY(const char *rpyAttr)
    {
        float r = 0.0f, p = 0.0f, y = 0.0f;
        sscanf_s(rpyAttr, "%f %f %f", &r, &p, &y);
        Eigen::AngleAxisf rollAngle(r, Eigen::Vector3f::UnitX());
        Eigen::AngleAxisf pitchAngle(p, Eigen::Vector3f::UnitY());
        Eigen::AngleAxisf yawAngle(y, Eigen::Vector3f::UnitZ());
        return yawAngle * pitchAngle * rollAngle;
    }

    static Eigen::Isometry3f ParseOrigin(tinyxml2::XMLElement *originElement)
    {
        Eigen::Isometry3f tf = Eigen::Isometry3f::Identity();
        if (originElement)
        {
            const char *xyzAttr = originElement->Attribute("xyz");
            if (xyzAttr)
            {
                tf.translation() = ParseXYZ(xyzAttr);
            }
            const char *rpyAttr = originElement->Attribute("rpy");
            if (rpyAttr)
            {
                tf.linear() = ParseRPY(rpyAttr).toRotationMatrix();
            }
        }
        return tf;
    }

    static void LoadLinkGeometryNodes(
        tinyxml2::XMLElement *linkElement,
        const char *name,
        const std::filesystem::path &parentFolder,
        Gltf::Model &gltf,
        const Gltf::Model::Material &fallbackMaterial)
    {
        for (auto *visualElement = linkElement->FirstChildElement(name); visualElement; visualElement = visualElement->NextSiblingElement(name))
        {
            Eigen::Isometry3f origin = ParseOrigin(visualElement->FirstChildElement("origin"));

            auto geometryElement = visualElement->FirstChildElement("geometry");
            if (geometryElement)
            {
                auto meshElement = geometryElement->FirstChildElement("mesh");
                if (meshElement)
                {
                    const char *meshFilename = meshElement->Attribute("filename");
                    if (meshFilename)
                    {
                        bool_t isAbsolute = std::filesystem::path(meshFilename).is_absolute();
                        if (!isAbsolute)
                        {
                            meshFilename = std::filesystem::absolute(parentFolder / meshFilename).string().c_str();
                        }

                        Gltf::Model meshNode;
                        Status meshStatus = Gltf::LoadFromFile(meshFilename, meshNode);
                        if (meshStatus == Status::SUCCESS)
                        {
                            for (size_t i = 0; i < meshNode.nodes.size(); i++)
                            {
                                meshNode.nodes[i].name = std::format("{}::{}", name, meshNode.nodes[i].name);
                            }

                            Gltf::Model::Node geometryNode;
                            geometryNode.parent = -1;
                            geometryNode.mesh = -1;
                            geometryNode.name = name;
                            geometryNode.scale = Eigen::Vector3f{1, 1, 1};
                            geometryNode.transform = origin;

                            for (size_t i = 0; i < meshNode.nodes.size(); i++)
                            {
                                // Only add root nodes as children of the geometry node
                                if (meshNode.nodes[i].parent == -1)
                                {
                                    geometryNode.children.push_back(static_cast<Gltf::Model::NodeIndex>(i));
                                }
                            }

                            meshNode.nodes.push_back(geometryNode);
                            gltf.Add(meshNode);
                        }
                        else
                        {
                            std::cout << "    Failed to load mesh: " << meshFilename << std::endl;
                        }
                    }

                    continue;
                }

                auto boxElement = geometryElement->FirstChildElement("box");
                if (boxElement)
                {
                    const char *sizeAttr = boxElement->Attribute("size");
                    if (sizeAttr)
                    {
                        Eigen::Vector3f size = ParseXYZ(sizeAttr);

                        Gltf::Model::MeshIndex boxMeshIndex = AddBoxMesh(gltf, size, fallbackMaterial);
                        Gltf::Model::Node geometryNode;
                        geometryNode.parent = -1;
                        geometryNode.mesh = boxMeshIndex;
                        geometryNode.name = std::format("{}::box", name);
                        geometryNode.scale = Eigen::Vector3f{1, 1, 1};
                        geometryNode.transform = origin;
                        gltf.nodes.push_back(std::move(geometryNode));
                    }

                    continue;
                }

                auto cylinderElement = geometryElement->FirstChildElement("cylinder");
                if (cylinderElement)
                {
                    const char *radiusAttr = cylinderElement->Attribute("radius");
                    const char *lengthAttr = cylinderElement->Attribute("length");
                    if (radiusAttr && lengthAttr)
                    {
                        float_t radius = static_cast<float_t>(std::atof(radiusAttr));
                        float_t length = static_cast<float_t>(std::atof(lengthAttr));

                        Gltf::Model::MeshIndex cylinderMeshIndex = AddCylinderMesh(gltf, radius, length, fallbackMaterial);
                        Gltf::Model::Node geometryNode;
                        geometryNode.parent = -1;
                        geometryNode.mesh = cylinderMeshIndex;
                        geometryNode.name = std::format("{}::cylinder", name);
                        geometryNode.scale = Eigen::Vector3f{1, 1, 1};
                        geometryNode.transform = origin;
                        gltf.nodes.push_back(std::move(geometryNode));
                    }

                    continue;
                }

                auto sphereElement = geometryElement->FirstChildElement("sphere");
                if (sphereElement)
                {
                    const char *radiusAttr = sphereElement->Attribute("radius");
                    if (radiusAttr)
                    {
                        float_t radius = static_cast<float_t>(std::atof(radiusAttr));
                        Gltf::Model::MeshIndex sphereMeshIndex = AddSphereMesh(gltf, radius, fallbackMaterial);
                        Gltf::Model::Node geometryNode;
                        geometryNode.parent = -1;
                        geometryNode.mesh = sphereMeshIndex;
                        geometryNode.name = std::format("{}::sphere", name);
                        geometryNode.scale = Eigen::Vector3f{1, 1, 1};
                        geometryNode.transform = origin;
                        gltf.nodes.push_back(std::move(geometryNode));
                    }

                    continue;
                }

                // Fallback to a default box
                {
                    std::cout << std::format("Unrecognized geometry type [{}]", geometryElement->FirstChildElement()->Name()) << std::endl;

                    Eigen::Vector3f size = Eigen::Vector3f{0.1f, 0.1f, 0.1f};
                    Gltf::Model::MeshIndex boxMeshIndex = AddBoxMesh(gltf, size, fallbackMaterial);
                    Gltf::Model::Node geometryNode;
                    geometryNode.parent = -1;
                    geometryNode.mesh = boxMeshIndex;
                    geometryNode.name = std::format("{}::fallback", name);
                    geometryNode.scale = Eigen::Vector3f{1, 1, 1};
                    geometryNode.transform = origin;
                    gltf.nodes.push_back(std::move(geometryNode));
                }
            }
        }
    }

    Jangine::IO::Status LoadFromFile(const std::string &path, Model &model, Gltf::Model &gltf)
    {
        (void)model;

        std::filesystem::path absolutePath = std::filesystem::absolute(path);
        if (!std::filesystem::exists(absolutePath))
        {
            return Jangine::IO::Status::FAILURE;
        }

        std::filesystem::path parentFolder = absolutePath.parent_path();

        tinyxml2::XMLDocument doc;
        if (doc.LoadFile(absolutePath.string().c_str()) != tinyxml2::XML_SUCCESS)
        {
            return Jangine::IO::Status::FAILURE;
        }

        // Robot
        tinyxml2::XMLElement *robotElement = doc.FirstChildElement("robot");
        if (!robotElement)
        {
            return Jangine::IO::Status::FAILURE;
        }

        // Links
        std::unordered_map<std::string, Model::Link> links;

        for (tinyxml2::XMLElement *linkElement = robotElement->FirstChildElement("link"); linkElement; linkElement = linkElement->NextSiblingElement("link"))
        {
            const char *linkName = linkElement->Attribute("name");
            if (!linkName)
            {
                std::cerr << "Invalid link: missing name" << std::endl;
                continue;
            }

            Model::Link link{
                .nodeIndex = -1,
            };

            Gltf::Model::Material visualFallbackMaterial{
                .name = "fallback::visual",
                .pbrMetallicRoughness = {
                    .baseColorFactor = {0.8f, 0.8f, 0.8f, 1.0f}}};
            Gltf::Model visual;
            LoadLinkGeometryNodes(linkElement, "visual", parentFolder, visual, visualFallbackMaterial);

            Gltf::Model::Material collisionFallbackMaterial{
                .name = "fallback::collision",
                .pbrMetallicRoughness = {
                    .baseColorFactor = {0.8f, 0.8f, 0.8f, 0.3f}}};
            Gltf::Model collision;
            LoadLinkGeometryNodes(linkElement, "collision", parentFolder, collision, collisionFallbackMaterial);

            bool_t hasVisual = visual.nodes.size() > 0;
            bool_t hasCollision = collision.nodes.size() > 0;

            Gltf::Model::Node linkNode;
            linkNode.mesh = -1;
            linkNode.name = std::format("link::{}", linkName);
            linkNode.scale = Eigen::Vector3f{1, 1, 1};
            linkNode.transform = Eigen::Isometry3f::Identity();
            link.nodeIndex = static_cast<Gltf::Model::NodeIndex>(gltf.nodes.size());
            gltf.nodes.push_back(linkNode);

            if (hasVisual)
            {
                Gltf::Model::NodeIndex nodeBaseIndex = static_cast<Gltf::Model::NodeIndex>(gltf.nodes.size());
                for (size_t i = 0; i < visual.nodes.size(); i++)
                {
                    if (visual.nodes[i].parent == -1)
                    {
                        gltf.nodes[link.nodeIndex].children.push_back(static_cast<Gltf::Model::NodeIndex>(i + nodeBaseIndex));
                    }
                }

                gltf.Add(visual);
            }

            if (hasCollision)
            {
                Gltf::Model::NodeIndex nodeBaseIndex = static_cast<Gltf::Model::NodeIndex>(gltf.nodes.size());
                for (size_t i = 0; i < collision.nodes.size(); i++)
                {
                    if (collision.nodes[i].parent == -1)
                    {
                        gltf.nodes[link.nodeIndex].children.push_back(static_cast<Gltf::Model::NodeIndex>(i + nodeBaseIndex));
                    }
                }

                gltf.Add(collision);
            }

            links[linkName] = link;
        }

        // Joints
        for (tinyxml2::XMLElement *jointElement = robotElement->FirstChildElement("joint"); jointElement; jointElement = jointElement->NextSiblingElement("joint"))
        {
            const char *jointName = jointElement->Attribute("name");
            if (!jointName)
            {
                std::cerr << "Invalid joint: missing name" << std::endl;
                continue; // Invalid joint, skip
            }

            Model::Joint joint;

            auto parentElement = jointElement->FirstChildElement("parent");
            auto childElement = jointElement->FirstChildElement("child");
            const char *parentLinkName = parentElement ? parentElement->Attribute("link") : nullptr;
            const char *childLinkName = childElement ? childElement->Attribute("link") : nullptr;
            if (!parentLinkName || !childLinkName)
            {
                std::cerr << "Invalid joint: missing parent or child link" << std::endl;
                continue; // Invalid joint, skip
            }

            joint.parent = &links[parentLinkName];
            joint.child = &links[childLinkName];

            Gltf::Model::Node jointNode;
            jointNode.parent = joint.parent->nodeIndex;
            jointNode.mesh = -1;
            jointNode.name = std::format("joint::{}", jointName);
            jointNode.scale = Eigen::Vector3f{1, 1, 1};
            jointNode.transform = ParseOrigin(jointElement->FirstChildElement("origin"));
            jointNode.children.push_back(joint.child->nodeIndex);

            Gltf::Model::NodeIndex jointNodeIndex = static_cast<Gltf::Model::NodeIndex>(gltf.nodes.size());
            gltf.nodes[joint.child->nodeIndex].parent = jointNodeIndex;
            gltf.nodes[joint.parent->nodeIndex].children.push_back(static_cast<Gltf::Model::NodeIndex>(gltf.nodes.size()));
            gltf.nodes.push_back(jointNode);

            std::cout
                << std::format(
                       "Joint: {} (Parent: {} [{}], Child: {} [{}])",
                       jointName,
                       parentLinkName,
                       joint.parent ? joint.parent->nodeIndex : -1,
                       childLinkName,
                       joint.child ? joint.child->nodeIndex : -1)
                << std::endl;
        }

        // Placeholder implementation
        return Jangine::IO::Status::SUCCESS;
    }
}