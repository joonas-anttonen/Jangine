// ThreadPool.hpp
#pragma once
#include <coroutine>
#include <future>
#include <queue>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>
#include <memory>
#include <exception>

namespace Jangine
{
    class CancellationToken
    {
    public:
        void cancel() noexcept { cancelled.store(true, std::memory_order_relaxed); }
        bool is_cancelled() const noexcept { return cancelled.load(std::memory_order_relaxed); }

    private:
        std::atomic<bool> cancelled{false};
    };

    class ThreadPool
    {
    public:
        ThreadPool(size_t threadCount = std::thread::hardware_concurrency());
        ~ThreadPool();

        template <typename Func>
        auto enqueue(Func &&func, std::shared_ptr<CancellationToken> token = nullptr)
            -> std::future<std::invoke_result_t<Func>>;

    private:
        std::vector<std::thread> workers;
        std::queue<std::function<void()>> tasks;
        std::mutex queueMutex;
        std::condition_variable condition;
        bool stop = false;

        void worker();
    };

    ThreadPool::ThreadPool(size_t threadCount)
    {
        for (size_t i = 0; i < threadCount; ++i)
            workers.emplace_back([this]
                                 { worker(); });
    }

    ThreadPool::~ThreadPool()
    {
        {
            std::lock_guard lock(queueMutex);
            stop = true;
        }
        condition.notify_all();
        for (auto &t : workers)
            t.join();
    }

    void ThreadPool::worker()
    {
        while (true)
        {
            std::function<void()> task;
            {
                std::unique_lock lock(queueMutex);
                condition.wait(lock, [this]
                               { return stop || !tasks.empty(); });
                if (stop && tasks.empty())
                    return;
                task = std::move(tasks.front());
                tasks.pop();
            }
            task();
        }
    }

    template <typename Func>
    auto ThreadPool::enqueue(Func &&func, std::shared_ptr<CancellationToken> token)
        -> std::future<std::invoke_result_t<Func>>
    {
        using RetType = std::invoke_result_t<Func>;
        auto task = std::make_shared<std::packaged_task<RetType()>>([func = std::forward<Func>(func), token]()
                                                                    {
        if (token && token->is_cancelled())
            throw std::runtime_error("Task cancelled");
        return func(); });
        std::future<RetType> res = task->get_future();
        {
            std::lock_guard lock(queueMutex);
            tasks.emplace([task]()
                          { (*task)(); });
        }
        condition.notify_one();
        return res;
    }

    // Coroutine-friendly Task type (supports void and cancellation)
    template <typename T = void>
    struct ThreadPoolTask;

    template <typename T>
    struct ThreadPoolTask
    {
        struct promise_type
        {
            std::promise<T> prom;
            ThreadPoolTask get_return_object() { return {prom.get_future()}; }
            std::suspend_never initial_suspend() noexcept { return {}; }
            std::suspend_always final_suspend() noexcept { return {}; }
            void return_value(T value) { prom.set_value(value); }
            void unhandled_exception() { prom.set_exception(std::current_exception()); }
        };

        std::future<T> fut;
        bool await_ready() const noexcept { return fut.wait_for(std::chrono::seconds(0)) == std::future_status::ready; }
        void await_suspend(std::coroutine_handle<>) { /* nothing */ }
        T await_resume() { return fut.get(); }
    };

    // Specialization for void
    template <>
    struct ThreadPoolTask<void>
    {
        struct promise_type
        {
            std::promise<void> prom;
            ThreadPoolTask get_return_object() { return {prom.get_future()}; }
            std::suspend_never initial_suspend() noexcept { return {}; }
            std::suspend_always final_suspend() noexcept { return {}; }
            void return_void() { prom.set_value(); }
            void unhandled_exception() { prom.set_exception(std::current_exception()); }
        };

        std::future<void> fut;
        bool await_ready() const noexcept { return fut.wait_for(std::chrono::seconds(0)) == std::future_status::ready; }
        void await_suspend(std::coroutine_handle<>) { /* nothing */ }
        void await_resume() { fut.get(); }
    };

    // Usage example
    /*inline ThreadPool pool;

    inline ThreadPoolTask<int> async_add(int a, int b, std::shared_ptr<CancellationToken> token = nullptr)
    {
        co_return co_await pool.enqueue([=]
                                        { return a + b; }, token);
    }

    inline ThreadPoolTask<void> async_print(const char *msg, std::shared_ptr<CancellationToken> token = nullptr)
    {
        co_await pool.enqueue([=]
                              { printf("%s\n", msg); }, token);
        co_return;
    }*/

    // Example cancellation usage:
    // auto token = std::make_shared<CancellationToken>();
    // auto task = async_add(1, 2, token);
    // token->cancel();
    // try { int result = co_await task; } catch (const std::exception& e) { /* handle cancel */ }
}