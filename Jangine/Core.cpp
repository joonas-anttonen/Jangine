#include "Core.hpp"

#include "Gfx/Core.hpp"
#include "Gui/Core.hpp"

#include "ThreadPool.hpp"

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
        log.SetThreadIdMapping("Main");
        log.GetLogger("Core").Func(__func__);

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
                .physicalDevice = gfx.SelectOptimalDevice(gfx.GetPhysicalDevices())};
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
                                    { GfxThread(gfx); });

            GuiThread(gui);

            gfxThreadExitRequested.store(true, std::memory_order_relaxed);
            if (gfxThread.joinable())
            {
                gfxThread.join();
            }
        }
        catch (const Jangine::JangineException &e)
        {
            GetLogger("Core").Error(e.what(), __func__);
            std::abort();
        }
        catch (const std::exception &e)
        {
            GetLogger("Core").Error(e.what(), __func__);
            std::abort();
        }
        catch (...)
        {
            GetLogger("Core").Error("Unknown error occurred.", __func__);
            std::abort();
        }
    }

    void Core::ProcessMainThreadQueue(Gui::Core &gui)
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

    void Core::GuiThreadWakeUp()
    {
        Gui::Core::WakeUp();
    }

    void Core::GuiThread(Gui::Core &gui)
    {
        auto startTime = std::chrono::steady_clock::now();
        auto lastFrameTime = startTime;

        while (gui.ProcessEvents())
        {
            auto now = std::chrono::steady_clock::now();
            double_t absoluteTime = std::chrono::duration<double_t>(now - startTime).count();
            float_t deltaTime = std::chrono::duration<float_t>(now - lastFrameTime).count();
            lastFrameTime = now;

            ProcessMainThreadQueue(gui);

            gui.Render(absoluteTime, deltaTime);

            // Sleep for a short duration to avoid busy-waiting
            gui.WaitForEvents(0.010);
        }
    }

    void Core::ProcessGfxThreadQueue(Gfx::Core &gfx)
    {
        (void)gfx; // Avoid unused parameter warning

        {
            std::lock_guard<SpinLock> lock(gfxThreadQueueLock);
            std::swap(writeGfxThreadQueue, readGfxThreadQueue);
        }

        while (!readGfxThreadQueue->empty())
        {
            auto func = std::move(readGfxThreadQueue->front());
            readGfxThreadQueue->pop();
            func();
        }
    }

    void Core::GfxThread(Gfx::Core &gfx)
    {
        try
        {
            log.SetThreadIdMapping("Gfx");
            gfx.SetThreadId(std::this_thread::get_id());

            while (!gfxThreadExitRequested.load(std::memory_order_relaxed))
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                ProcessGfxThreadQueue(gfx);

                gfx.Render(0.0, 0.0);
            }
        }
        catch (const Jangine::JangineException &e)
        {
            GetLogger("Core").Error(e.what(), __func__);
            std::abort();
        }
        catch (const std::exception &e)
        {
            GetLogger("Core").Error(e.what(), __func__);
            std::abort();
        }
        catch (...)
        {
            GetLogger("Core").Error("Unknown error occurred.", __func__);
            std::abort();
        }
    }
}
