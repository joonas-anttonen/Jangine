#include "Core.hpp"

#include "Gfx/Core.hpp"
#include "Gui/Core.hpp"

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

            Gui::ApiParameters guiApiParameters = {
                .enableDebugging = parameters.enableDebugging,
                .preferX11 = false};
            Gui::Core gui(guiApiParameters, &gfx);

            Gui::Parameters guiParameters{
                .windowTitle = parameters.appName,
                .windowX = {},
                .windowY = {},
                .windowWidth = {},
                .windowHeight = {}};
            gui.Create(guiParameters);

            gfx.CreatePresenter(gui.GetSurface(gfx.GetSurfaceCreationHandle()));

            gfxThread = std::thread([this, &gfx]()
                                    {
                while (!gfxShutdownRequested.load(std::memory_order_relaxed))
                {
                    /*GetLogger("Core").Debug("On the graphics thread");

                    auto future = this->PostToMainThread([] {
                        GetLogger("Core").Debug("On the main thread");
                    });

                    future.wait();*/

                    gfx.Render(0.0, 0.0);
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                } });

            while (gui.ProcessEvents())
            {
                ProcessMainThreadQueue(gui);

                // Sleep for a short duration to avoid busy-waiting
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }

            gfxShutdownRequested.store(true, std::memory_order_relaxed);
            if (gfxThread.joinable())
            {
                gfxThread.join();
            }
        }
        catch (const Jangine::JangineException &e)
        {
            std::cerr << e.what() << std::endl;
        }
    }

    void Core::ProcessMainThreadQueue(const Gui::Core &gui)
    {
        {
            std::lock_guard<SpinLock> lock(mainThreadQueueLock);
            std::swap(writeMainThreadQueue, readMainThreadQueue);
        }

        while (!readMainThreadQueue->empty())
        {
            auto func = std::move(readMainThreadQueue->front());
            readMainThreadQueue->pop();
            func();

            if (!gui.ProcessEvents())
            {
                break;
            }
        }
    }
}
