#pragma once

#include "Shared.hpp"
#include "IO.hpp"
#include "Gltf.hpp"

namespace Jangine::IO::Urdf
{
    struct Model
    {
        struct Link
        {
            Gltf::Model::NodeIndex nodeIndex;
        };

        struct Joint
        {
            Link *parent;
            Link *child;
        };
    };

    Jangine::IO::Status LoadFromFile(const std::string &filename, Model &model, Gltf::Model &gltf);
}