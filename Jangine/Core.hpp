#pragma once

#include "Shared.hpp"
#include "Logging/Log.hpp"

#include <string>
#include <queue>
#include <future>
#include <functional>

namespace Jangine
{
    namespace Gui
    {
        class Core;
    }

    namespace Gfx
    {
        class Core;
    }

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

        Core &operator=(const Core &) = delete;
        Core(const Core &) = delete;
        Core &operator=(Core &&) = delete;
        Core(Core &&) = delete;

        static Core &GetInstance()
        {
            ThrowInvalidOperationIfNull(instance);
            return *instance;
        }

        void Run(const Parameters &parameters);

        template <typename F>
        auto PostToMainThread(F &&func) -> std::future<std::invoke_result_t<F>>
        {
            using R = std::invoke_result_t<F>;
            auto task = std::make_shared<std::packaged_task<R()>>(std::forward<F>(func));
            auto fut = task->get_future();

            {
                std::lock_guard<SpinLock> lock(mainThreadQueueLock);
                writeMainThreadQueue->push([task]()
                                           { (*task)(); });
                GuiThreadWakeUp();
            }
            return fut;
        }

        template <typename F>
        auto PostToGfxThread(F &&func) -> std::future<std::invoke_result_t<F>>
        {
            using R = std::invoke_result_t<F>;
            auto task = std::make_shared<std::packaged_task<R()>>(std::forward<F>(func));
            auto fut = task->get_future();

            {
                std::lock_guard<SpinLock> lock(gfxThreadQueueLock);
                writeGfxThreadQueue->push([task]()
                                          { (*task)(); });
            }
            return fut;
        }

        static const Logging::Logger &GetLogger(const std::string &name);

    private:
        void ProcessMainThreadQueue(Gui::Core &gui);
        void ProcessGfxThreadQueue(Gfx::Core &gfx);

        void GuiThreadWakeUp();
        void GuiThread(Gui::Core &gui);
        void GfxThread(Gfx::Core &gfx);

        Logging::Log log;

        std::atomic<bool> gfxThreadExitRequested{false};
        std::thread gfxThread;

        SpinLock mainThreadQueueLock;
        std::queue<std::function<void()>> mainThreadQueueA, mainThreadQueueB;
        std::queue<std::function<void()>> *writeMainThreadQueue = &mainThreadQueueA;
        std::queue<std::function<void()>> *readMainThreadQueue = &mainThreadQueueB;

        SpinLock gfxThreadQueueLock;
        std::queue<std::function<void()>> gfxThreadQueue;
        std::queue<std::function<void()>> *writeGfxThreadQueue = &gfxThreadQueue;
        std::queue<std::function<void()>> *readGfxThreadQueue = &gfxThreadQueue;

        static Core *instance;
    };
}