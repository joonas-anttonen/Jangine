#include "Core.hpp"

#include "Gfx/Core.hpp"

namespace Jangine
{
    Core *Core::instance = nullptr;

    const Logging::Logger &Core::GetLogger(const std::string &name)
    {
        ThrowInvalidOperationIfNull(instance);
        return instance->log.GetLogger(name);
    }

    Core::Core()
    {
        instance = this;

        log.AddConsoleListener();
    }

    void Core::Run(const Parameters &parameters)
    {
        GetLogger("Core").Func(__func__);

        try
        {
            Gfx::ApiParameters gfxApiParameters = {
                .enableDebugging = parameters.enableDebugging,
                .appVersion = parameters.appVersion,
                .appEngineVersion = {1, 0, 0},
                .requiredApiVersion = {1, 4, 0},
                .appName = parameters.appName,
                .appEngineName = "Jangine"};

            Gfx::Core gfx(gfxApiParameters);

            Gfx::Parameters gfxParameters = {
                .physicalDevice = gfx.SelectOptimalPhysicalDevice(gfx.GetPhysicalDevices())};

            gfx.Create(gfxParameters);
        }
        catch (const Jangine::JangineException &e)
        {
            std::cerr << e.what() << std::endl;
        }
    }
}
