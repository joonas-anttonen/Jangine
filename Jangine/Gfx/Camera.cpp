#include "Camera.hpp"

namespace Jangine::Gfx
{
    BlenderCamera::BlenderCamera()
    {
        position = Eigen::Vector3f(0.0f, 0.0f, 5.0f);
    }

    void BlenderCamera::Update(const UserInput &input, float_t deltaTime)
    {
        float_t dragX = input.GetDragDeltaX();
        float_t dragY = input.GetDragDeltaY();
        float_t zoomDelta = input.GetZoomDelta();

        // Orbit: update yaw and pitch angles
        static float_t yaw = 0.0f;
        static float_t pitch = 0.0f;
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
                float_t scale = 1.0f - zoomDelta * zoomSpeed * 0.1f * deltaTime;
                orthoWidth *= scale;
                orthoHeight *= scale;
                orthoWidth = std::max(orthoWidth, 0.1f);
                orthoHeight = std::max(orthoHeight, 0.1f);
                dirtyProj = true;
            }
        }

        // Calculate new position
        Eigen::AngleAxisf yawRot(yaw, Eigen::Vector3f::UnitY());
        Eigen::AngleAxisf pitchRot(pitch, Eigen::Vector3f::UnitX());
        Eigen::Vector3f offset = yawRot * pitchRot * Eigen::Vector3f(0, 0, distance);
        position = target + offset;

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
    }

    void BlenderCamera::UpdateViewMatrix()
    {
        // Look-at matrix
        Eigen::Vector3f forward = (target - position).normalized();
        Eigen::Vector3f right = Eigen::Vector3f::UnitY().cross(forward).normalized();
        Eigen::Vector3f up = forward.cross(right);

        Eigen::Matrix4f view = Eigen::Matrix4f::Identity();
        view.block<3, 1>(0, 0) = right;
        view.block<3, 1>(0, 1) = up;
        view.block<3, 1>(0, 2) = -forward;
        view.block<3, 1>(0, 3) = position;

        // Invert rotation and translation
        Eigen::Matrix3f rot = view.block<3, 3>(0, 0);
        Eigen::Vector3f trans = view.block<3, 1>(0, 3);
        viewMatrix.topLeftCorner<3, 3>() = rot.transpose();
        viewMatrix.topRightCorner<3, 1>() = -rot.transpose() * trans;
        viewMatrix.row(3) = Eigen::Vector4f(0, 0, 0, 1);

        inverseViewMatrix.topLeftCorner<3, 3>() = rot;
        inverseViewMatrix.topRightCorner<3, 1>() = trans;
        inverseViewMatrix.row(3) = Eigen::Vector4f(0, 0, 0, 1);
    }

    void BlenderCamera::UpdateProjectionMatrix()
    {
        if (projectionType == ProjectionType::Perspective)
        {
            float_t f = 1.0f / std::tan(fovY * 0.5f);
            projectionMatrix.setZero();
            projectionMatrix(0, 0) = f / aspect;
            projectionMatrix(1, 1) = f;
            projectionMatrix(2, 2) = (far + near) / (near - far);
            projectionMatrix(2, 3) = (2.0f * far * near) / (near - far);
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
            projectionMatrix(2, 2) = -2.0f / (far - near);
            projectionMatrix(0, 3) = -(right + left) / (right - left);
            projectionMatrix(1, 3) = -(top + bottom) / (top - bottom);
            projectionMatrix(2, 3) = -(far + near) / (far - near);
            projectionMatrix(3, 3) = 1.0f;
        }
    }
}