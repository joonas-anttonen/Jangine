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
            return 0.0f;
        }

        float_t GetDragDeltaY() const
        {
            return 0.0f;
        }

        float_t GetZoomDelta() const
        {
            return 0.0f;
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

        void SetPerspective(float_t in_fovY, float_t in_aspect, float_t in_near, float_t in_far);
        void SetOrthographic(float_t in_width, float_t in_height, float_t in_near, float_t in_far);

        void Update(const UserInput &input, float_t deltaTime);

        // Getters
        [[nodiscard]] const Eigen::Vector3f &GetPosition() const { return position; }
        [[nodiscard]] const Eigen::Vector3f &GetTarget() const { return target; }
        [[nodiscard]] const Eigen::Quaternionf &GetOrientation() const { return orientation; }

        // Matrix access
        [[nodiscard]] const Eigen::Matrix4f &GetViewMatrix() const { return viewMatrix; }
        [[nodiscard]] const Eigen::Matrix4f &GetInverseViewMatrix() const { return inverseViewMatrix; }
        [[nodiscard]] const Eigen::Matrix4f &GetProjectionMatrix() const { return projectionMatrix; }
        [[nodiscard]] const Eigen::Matrix4f &GetViewProjectionMatrix() const { return viewProjectionMatrix; }

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
        mutable Eigen::Matrix4f viewProjectionMatrix{Eigen::Matrix4f::Identity()};
        mutable bool_t dirtyView{true};
        mutable bool_t dirtyProj{true};

        Eigen::Vector3f target{0.0f, 0.0f, 0.0f};
        float_t distance{5.0f};
    };
}