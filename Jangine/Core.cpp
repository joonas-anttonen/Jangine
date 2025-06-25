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
            std::filesystem::path testShaderPath = "J:/projects/Gossamer/Gossamer/Gfx/Shaders/built-in-2d-composition.hlsl";
            if (!std::filesystem::exists(testShaderPath))
            {
                GetLogger("Core").Error("Test shader file does not exist: " + testShaderPath.string(), __func__);
                std::abort();
            }

            std::string testShaderContents;
            {
                std::ifstream shaderFile(testShaderPath);
                if (!shaderFile.is_open())
                {
                    GetLogger("Core").Error("Failed to open test shader file: " + testShaderPath.string(), __func__);
                    std::abort();
                }
                testShaderContents.assign((std::istreambuf_iterator<char>(shaderFile)), std::istreambuf_iterator<char>());
            }

            Gfx::ShaderCompiler shaderCompiler;
            auto program = shaderCompiler.Compile(testShaderContents, "built-in-2d-composition");
            if (program.stages.empty())
            {
                GetLogger("Core").Error("Shader compilation failed: No stages found in the shader program.", __func__);
                std::abort();
            }
            else
            {
                GetLogger("Core").Information("Shader compiled successfully with " + std::to_string(program.stages.size()) + " stages.", __func__);

                for (const auto &stage : program.stages)
                {
                    GetLogger("Core").Information("Shader stage: " + stage.entryPoint + " (" + std::to_string(stage.bytecode.size()) + " bytes)", __func__);
                }
            }

            Gfx::IO::ShaderPackage::SerializeToHeader(
                {{"built-in-2d-composition", program}},
                "c:/users/jant/downloads/built-in-shaders.hpp",
                "built_in_shaders",
                "Jangine::Gfx");

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

    void Core::GuiThreadWakeUp()
    {
        Gui::Core::WakeUp();
    }

    void Core::GuiThread(Gui::Core &gui)
    {
        while (gui.ProcessEvents())
        {
            ProcessMainThreadQueue(gui);

            // Sleep for a short duration to avoid busy-waiting
            gui.WaitForEvents(10);
        }
    }

    void Core::GfxThread(Gfx::Core &gfx)
    {
        try
        {
            while (!gfxThreadExitRequested.load(std::memory_order_relaxed))
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                ProcessGfxThreadQueue(gfx);

                /*GetLogger("Core").Debug("On the graphics thread");

                auto future = this->PostToMainThread([]
                                                     { GetLogger("Core").Debug("On the main thread"); });

                future.wait_for(std::chrono::milliseconds(100));*/

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
