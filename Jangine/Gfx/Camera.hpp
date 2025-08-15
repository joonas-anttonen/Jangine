#pragma once

#include "../Shared.hpp"
#include "Shared.hpp"
#include "Core.hpp"

#include <Eigen/Core>
#include <Eigen/Geometry>

namespace Jangine::Gfx
{
    class UserInput
    {
    public:
        float_t GetDragDeltaX() const
        {
            return 0;
        }

        float_t GetDragDeltaY() const
        {
            return 0;
        }

        float_t GetZoomDelta() const
        {
            return 0;
        }
    };

    /// @brief Camera class for 3D rendering with Blender-like functionality.
    class BlenderCamera
    {
    public:
        enum class ProjectionType
        {
            Perspective,
            Orthographic
        };

        BlenderCamera();

        void Update(const UserInput &input, float_t deltaTime);

        // Setters
        //void SetPosition(const Eigen::Vector3f &pos);
        //void SetOrientation(const Eigen::Quaternionf &orientation);
        //void SetPerspective(float_t fovY, float_t aspect, float_t near, float_t far);

        // Getters
        [[nodiscard]] const Eigen::Vector3f &GetPosition() const { return position; }
        [[nodiscard]] const Eigen::Quaternionf &GetOrientation() const { return orientation; }

        // Matrix access
        [[nodiscard]] const Eigen::Matrix4f &GetViewMatrix() const { return viewMatrix; }
        [[nodiscard]] const Eigen::Matrix4f &GetInverseViewMatrix() const { return inverseViewMatrix; }
        [[nodiscard]] const Eigen::Matrix4f &GetProjectionMatrix() const { return projectionMatrix; }

    private:
        void UpdateViewMatrix();
        void UpdateProjectionMatrix();

        ProjectionType projectionType{ProjectionType::Perspective};
        float orthoWidth{10.0f};
        float orthoHeight{10.0f};

        Eigen::Vector3f position{0.0f, 0.0f, 0.0f};
        Eigen::Quaternionf orientation{Eigen::Quaternionf::Identity()};
        float_t fovY{45.0f};
        float_t aspect{1.0f};
        float_t near{0.1f};
        float_t far{100.0f};

        mutable Eigen::Matrix4f viewMatrix{Eigen::Matrix4f::Identity()};
        mutable Eigen::Matrix4f inverseViewMatrix{Eigen::Matrix4f::Identity()};
        mutable Eigen::Matrix4f projectionMatrix{Eigen::Matrix4f::Identity()};
        mutable bool_t dirtyView{true};
        mutable bool_t dirtyProj{true};

        Eigen::Vector3f target{0.0f, 0.0f, 0.0f};
        float_t distance{5.0f};
    };
}