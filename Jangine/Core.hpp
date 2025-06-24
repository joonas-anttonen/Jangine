#pragma once

#include "Shared.hpp"
#include "Logging/Log.hpp"
#include "Gfx/Core.hpp"

#include <string>

namespace Jangine
{
    namespace Logging
    {
        class Logger;
    }

    struct JANGINE_API Parameters
    {
        bool_t enableDebugging = false;

        Version appVersion;
        std::string appName;
    };

    class JANGINE_API Core
    {
    public:
        Core();
        ~Core() = default;

        void Run(const Parameters &parameters);

        static const Logging::Logger &GetLogger(const std::string &name);

    private:
        Logging::Log log;

        static Core *instance;
    };
}