#include "Urdf.hpp"

#include <tinyxml2.h>

namespace Jangine::IO::Urdf
{
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

        Gltf::Model::Mesh cylinderMesh{
            .name = "cylinder",
            .primitives = {{.indices = gltf.CreateAccessorFromData(indices),
                            .material = gltf.CreateMaterial(material),
                            .attributes = {
                                .position = gltf.CreateAccessorFromData(vertices),
                                .normal = gltf.CreateAccessorFromData(normals)}}}};
        return gltf.CreateMesh(cylinderMesh);
    }

    static Gltf::Model::MeshIndex AddSphereMesh(Gltf::Model &gltf, float_t radius, const Gltf::Model::Material &material)
    {
        const int latitudeBands = 16;
        const int longitudeBands = 32;

        std::vector<Eigen::Vector3f> vertices;
        std::vector<Eigen::Vector3f> normals;
        std::vector<uint32_t> indices;

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

        Gltf::Model::Mesh sphereMesh{
            .name = "sphere",
            .primitives = {{.indices = gltf.CreateAccessorFromData(indices),
                            .material = gltf.CreateMaterial(material),
                            .attributes = {
                                .position = gltf.CreateAccessorFromData(vertices),
                                .normal = gltf.CreateAccessorFromData(normals)}}}};
        return gltf.CreateMesh(sphereMesh);
    }

    static Gltf::Model::MeshIndex AddBoxMesh(Gltf::Model &gltf, const Eigen::Vector3f &size, const Gltf::Model::Material &material)
    {
        std::vector<Eigen::Vector3f> vertices;
        std::vector<Eigen::Vector3f> normals;
        std::vector<uint32_t> indices;

        float hx = size.x() / 2.0f;
        float hy = size.y() / 2.0f;
        float hz = size.z() / 2.0f;

        vertices = {
            {-hx, -hy, -hz},
            {hx, -hy, -hz},
            {hx, hy, -hz},
            {-hx, hy, -hz}, // Back face
            {-hx, -hy, hz},
            {hx, -hy, hz},
            {hx, hy, hz},
            {-hx, hy, hz}, // Front face
            {-hx, -hy, -hz},
            {-hx, hy, -hz},
            {-hx, hy, hz},
            {-hx, -hy, hz}, // Left face
            {hx, -hy, -hz},
            {hx, hy, -hz},
            {hx, hy, hz},
            {hx, -hy, hz}, // Right face
            {-hx, -hy, -hz},
            {hx, -hy, -hz},
            {hx, -hy, hz},
            {-hx, -hy, hz}, // Bottom face
            {-hx, hy, -hz},
            {hx, hy, -hz},
            {hx, hy, hz},
            {-hx, hy, hz} // Top face
        };
        normals = {
            {0, 0, -1},
            {0, 0, -1},
            {0, 0, -1},
            {0, 0, -1}, // Back face
            {0, 0, 1},
            {0, 0, 1},
            {0, 0, 1},
            {0, 0, 1}, // Front face
            {-1, 0, 0},
            {-1, 0, 0},
            {-1, 0, 0},
            {-1, 0, 0}, // Left face
            {1, 0, 0},
            {1, 0, 0},
            {1, 0, 0},
            {1, 0, 0}, // Right face
            {0, -1, 0},
            {0, -1, 0},
            {0, -1, 0},
            {0, -1, 0}, // Bottom face
            {0, 1, 0},
            {0, 1, 0},
            {0, 1, 0},
            {0, 1, 0} // Top face
        };
        indices = {
            0, 1, 2, 0, 2, 3,       // Back face
            4, 5, 6, 4, 6, 7,       // Front face
            8, 9, 10, 8, 10, 11,    // Left face
            12, 13, 14, 12, 14, 15, // Right face
            16, 17, 18, 16, 18, 19, // Bottom face
            20, 21, 22, 20, 22, 23  // Top face
        };

        Gltf::Model::Mesh boxMesh{
            .name = "box",
            .primitives = {{.indices = gltf.CreateAccessorFromData(indices),
                            .material = gltf.CreateMaterial(material),
                            .attributes = {
                                .position = gltf.CreateAccessorFromData(vertices),
                                .normal = gltf.CreateAccessorFromData(normals)}}}};
        return gltf.CreateMesh(boxMesh);
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
        const Gltf::Model::Material &fallbackMaterial,
        bool_t forceFallbackMaterial)
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

                        Gltf::Model meshModel;
                        Status meshStatus = Gltf::LoadFromFile(meshFilename, meshModel);
                        if (meshStatus == Status::SUCCESS)
                        {
                            for (size_t i = 0; i < meshModel.nodes.size(); i++)
                            {
                                meshModel.nodes[i].name = std::format("{}::{}", name, meshModel.nodes[i].name);
                            }

                            Gltf::Model::Node geometryNode;
                            geometryNode.parent = -1;
                            geometryNode.mesh = -1;
                            geometryNode.name = name;
                            geometryNode.scale = Eigen::Vector3f{1, 1, 1};
                            geometryNode.transform = origin;

                            for (size_t i = 0; i < meshModel.nodes.size(); i++)
                            {
                                if (forceFallbackMaterial)
                                {
                                    for (size_t m = 0; m < meshModel.materials.size(); m++)
                                    {
                                        meshModel.materials[m] = fallbackMaterial;
                                    }
                                }

                                // Only add root nodes as children of the geometry node
                                if (meshModel.nodes[i].parent == -1)
                                {
                                    geometryNode.children.push_back(static_cast<Gltf::Model::NodeIndex>(i));
                                }
                            }

                            meshModel.nodes.push_back(geometryNode);
                            gltf.Append(meshModel);
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
            LoadLinkGeometryNodes(
                linkElement,
                "visual",
                parentFolder,
                visual,
                visualFallbackMaterial,
                false);

            Gltf::Model::Material collisionFallbackMaterial{
                .name = "fallback::collision",
                .pbrMetallicRoughness = {
                    .baseColorFactor = {0.8f, 0.8f, 0.8f, 0.5f}}};
            Gltf::Model collision;
            LoadLinkGeometryNodes(
                linkElement,
                "collision",
                parentFolder,
                collision,
                collisionFallbackMaterial,
                true);

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

                gltf.Append(visual);
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

                gltf.Append(collision);
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

        gltf.CompactMaterials();
        return Jangine::IO::Status::SUCCESS;
    }
}