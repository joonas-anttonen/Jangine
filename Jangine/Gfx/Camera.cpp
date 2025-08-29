#include "Camera.hpp"

namespace Jangine::Gfx
{
    BlenderCamera::BlenderCamera()
    {
    }

    void BlenderCamera::SetPerspective(float_t in_aspect, float_t in_fovY, float_t in_near, float_t in_far)
    {
        fovY = in_fovY;
        aspect = in_aspect;
        near = in_near;
        far = in_far;
        projectionType = ProjectionType::Perspective;
        dirtyProj = true;
    }

    void BlenderCamera::SetOrthographic(float_t in_aspect, float_t in_width, float_t in_near, float_t in_far)
    {
        aspect = in_aspect;
        orthoWidth = in_width;
        near = in_near;
        far = in_far;
        projectionType = ProjectionType::Orthographic;
        dirtyProj = true;
    }

    void BlenderCamera::Update(const UserInput &input, float_t deltaTime)
    {
        Eigen::Vector2f turn = input.GetTurnDelta();
        Eigen::Vector2f move = input.GetMoveDelta();
        float_t zoom = input.GetZoomDelta();

        float_t distanceScaling = 1.0f;
        float_t distanceScalingVariable = projectionType == ProjectionType::Perspective ? distance : orthoWidth;

        // Smoothly slow down zooming when close to the target
        if (distanceScalingVariable <= 5.0f)
        {
            distanceScaling = (distanceScalingVariable - 1.0f) / 9.0f;
            distanceScaling = std::clamp(distanceScaling, 0.1f, 1.0f);
        }
        else if (distanceScalingVariable > 5.0f)
        {
            distanceScaling = (distanceScalingVariable - 10.0f) / 90.0f + 1.0f;
            distanceScaling = std::clamp(distanceScaling, 1.0f, 2.0f);
        }

        // 1. Adjust camera distance ("scale")
        if (zoom != 0.0f)
        {
            const float_t zoomMetersPerSecond = 1.0f * deltaTime;

            if (projectionType == ProjectionType::Perspective)
            {
                distance -= zoom * zoomMetersPerSecond * distanceScaling;
                distance = std::max(distance, 0.1f); // Prevent negative/zero distance
                dirtyView = true;
            }
            else
            {
                orthoWidth -= zoom * zoomMetersPerSecond * distanceScaling;
                orthoWidth = std::max(orthoWidth, 0.1f);
                dirtyProj = true;
            }
        }

        // 2. Rotate camera ("rotate")
        if (!turn.isZero())
        {
            const float_t orbitRadiansPerSecond = Math::PI * 0.5f * deltaTime * std::clamp(distanceScaling, 0.5f, 1.0f);

            yaw -= -turn.x() * orbitRadiansPerSecond;
            pitch -= turn.y() * orbitRadiansPerSecond;
            pitch = std::clamp(pitch, -Math::PI * 0.5f + 0.01f, Math::PI * 0.5f - 0.01f);
            dirtyView = true;
        }

        Eigen::Isometry3f rotation = Eigen::Isometry3f::Identity();
        rotation.rotate(Eigen::AngleAxisf(yaw, Eigen::Vector3f::UnitY()));
        rotation.rotate(Eigen::AngleAxisf(pitch, Eigen::Vector3f::UnitX()));

        // 3. Move camera ("translate")
        if (!move.isZero())
        {
            const float_t moveMetersPerSecond = 1.0f * deltaTime;

            target += rotation * Eigen::Vector3f(-move.x(), -move.y(), 0) * moveMetersPerSecond;
        }

        position = target + rotation * Eigen::Vector3f(0, 0, -distance);
        dirtyView = true;

        bool dirtyViewProj = dirtyProj || dirtyView;

        if (dirtyView)
        {
            UpdateViewMatrix();
            dirtyView = false;
        }

        if (dirtyProj)
        {
            UpdateProjectionMatrix();
            dirtyProj = false;
        }

        if (dirtyViewProj)
        {
            viewProjectionMatrix = projectionMatrix * viewMatrix;
        }
    }

    void BlenderCamera::UpdateViewMatrix()
    {
        Eigen::Vector3f f = (target - position).normalized();
        Eigen::Vector3f r = f.cross(Eigen::Vector3f::UnitY()).normalized(); // -X is right
        Eigen::Vector3f u = r.cross(f);

        Eigen::Matrix4f view;
        view.row(0) << r.x(), r.y(), r.z(), -r.dot(position);
        view.row(1) << u.x(), u.y(), u.z(), -u.dot(position);
        view.row(2) << -f.x(), -f.y(), -f.z(), f.dot(position);
        view.row(3) << 0, 0, 0, 1;

        viewMatrix = view.matrix();
        inverseViewMatrix = view.inverse().matrix();
    }

    void BlenderCamera::UpdateProjectionMatrix()
    {
        if (projectionType == ProjectionType::Perspective)
        {
            float_t f = 1.0f / std::tan(fovY * 0.5f);
            projectionMatrix.setZero();
            projectionMatrix(0, 0) = f / aspect;
            projectionMatrix(1, 1) = f;
            // Vulkan: Z in [0, 1]
            projectionMatrix(2, 2) = far / (near - far);
            projectionMatrix(2, 3) = -(far * near) / (far - near);
            projectionMatrix(3, 2) = -1.0f;
        }
        else
        {
            float_t orthoHeight = orthoWidth / aspect;
            float_t left = -orthoWidth * 0.5f;
            float_t right = orthoWidth * 0.5f;
            float_t bottom = -orthoHeight * 0.5f;
            float_t top = orthoHeight * 0.5f;

            projectionMatrix.setZero();
            projectionMatrix(0, 0) = 2.0f / (right - left);
            projectionMatrix(1, 1) = 2.0f / (top - bottom);
            // Vulkan: Z in [0, 1]
            projectionMatrix(2, 2) = -1.0f / (far - near);
            projectionMatrix(0, 3) = -(right + left) / (right - left);
            projectionMatrix(1, 3) = -(top + bottom) / (top - bottom);
            projectionMatrix(2, 3) = -near / (far - near);
            projectionMatrix(3, 3) = 1.0f;
        }
    }
}