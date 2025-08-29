#pragma once

#include "Shared.hpp"
#include "IO.hpp"

namespace Jangine::IO::Urdf
{
    struct Model
    {
    };

    Jangine::IO::Status LoadFromFile(const std::string &filename, Model &model);
}