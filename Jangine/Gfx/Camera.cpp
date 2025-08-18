#include "Camera.hpp"

namespace Jangine::Gfx
{
    BlenderCamera::BlenderCamera()
    {
        // position = Eigen::Vector3f(0.0f, 2.5f, 5.0f);
    }

    void BlenderCamera::SetPerspective(float_t in_fovY, float_t in_aspect, float_t in_near, float_t in_far)
    {
        fovY = in_fovY;
        aspect = in_aspect;
        near = in_near;
        far = in_far;
        projectionType = ProjectionType::Perspective;
        dirtyProj = true;
    }

    void BlenderCamera::SetOrthographic(float_t in_width, float_t in_height, float_t in_near, float_t in_far)
    {
        orthoWidth = in_width;
        orthoHeight = in_height;
        near = in_near;
        far = in_far;
        projectionType = ProjectionType::Orthographic;
        dirtyProj = true;
    }

    void BlenderCamera::Update(const UserInput &input, float_t deltaTime)
    {
        float_t dragX = input.GetDragDeltaX();
        float_t dragY = input.GetDragDeltaY();
        float_t zoomDelta = input.GetZoomDelta();

        // Orbit: update yaw and pitch angles
        static float_t yaw = 0.0f;
        static float_t pitch = 0.0f; // Start looking down at 45 degrees
        float_t sensitivity = 0.01f;

        if (dragX != 0.0f || dragY != 0.0f)
        {
            yaw -= dragX * sensitivity;
            pitch -= dragY * sensitivity;
            pitch = std::clamp(pitch, -1.5f, 1.5f); // Prevent flipping
            dirtyView = true;
        }

        // Zoom
        if (zoomDelta != 0.0f)
        {
            float_t zoomSpeed = 2.0f;
            if (projectionType == ProjectionType::Perspective)
            {
                distance -= zoomDelta * zoomSpeed * deltaTime;
                distance = std::max(distance, 0.1f); // Prevent negative/zero distance
                dirtyView = true;
            }
            else // Orthographic
            {
                float_t scale = zoomDelta * zoomSpeed * deltaTime;
                orthoWidth *= scale;
                orthoHeight *= scale;
                orthoWidth = std::min(std::max(orthoWidth, 0.1f), 100.0f);
                orthoHeight = std::min(std::max(orthoHeight, 0.1f), 100.0f);
                dirtyProj = true;
            }
        }

        // Calculate new position
        Eigen::AngleAxisf yawRot(yaw, Eigen::Vector3f::UnitY());
        Eigen::AngleAxisf pitchRot(pitch, Eigen::Vector3f::UnitX());
        Eigen::Vector3f offset = yawRot * pitchRot * Eigen::Vector3f(0, 0, distance);
        position = target + -1 * offset;

        // Look at target
        Eigen::Vector3f forward = (target - position).normalized();
        Eigen::Vector3f right = Eigen::Vector3f::UnitY().cross(forward).normalized();
        Eigen::Vector3f up = forward.cross(right);

        Eigen::Matrix3f rot;
        rot.col(0) = right;
        rot.col(1) = up;
        rot.col(2) = forward;
        orientation = Eigen::Quaternionf(rot);

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
        // Look-at matrix
        Eigen::Vector3f forward = (target - position).normalized();
        Eigen::Vector3f right = Eigen::Vector3f::UnitY().cross(forward).normalized();
        Eigen::Vector3f up = forward.cross(right);

        // Construct the isometry (rotation + translation)
        Eigen::Isometry3f iso = Eigen::Isometry3f::Identity();
        iso.linear().col(0) = right;
        iso.linear().col(1) = up;
        iso.linear().col(2) = forward;
        iso.translation() = position;

        viewMatrix = iso.matrix();
        inverseViewMatrix = iso.inverse().matrix();
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