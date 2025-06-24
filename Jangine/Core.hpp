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
            }
            return fut;
        }

        static const Logging::Logger &GetLogger(const std::string &name);

    private:
        void ProcessMainThreadQueue(const Gui::Core &gui);

        Logging::Log log;

        std::atomic<bool> gfxShutdownRequested{false};
        std::thread gfxThread;

        SpinLock mainThreadQueueLock;
        std::queue<std::function<void()>> mainThreadQueueA, mainThreadQueueB;
        std::queue<std::function<void()>> *writeMainThreadQueue = &mainThreadQueueA;
        std::queue<std::function<void()>> *readMainThreadQueue = &mainThreadQueueB;

        static Core *instance;
    };
}