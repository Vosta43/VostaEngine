#pragma once

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <exception>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <thread>
#include <type_traits>
#include <vector>

namespace ve {

struct Task;

template<class T>
struct TaskHandle {
    std::shared_ptr<Task> task;
    std::shared_future<T> future;

    T get() { return future.get(); }

    operator TaskHandle<void>() const { return TaskHandle<void>{ task, {} }; }
};

struct Task {
    std::function<void()> function;
    std::atomic<int> unfinished{0};
    std::vector<std::shared_ptr<Task>> dependents;
    std::atomic<bool> done{false};
    std::mutex mtx;
};

class JobSystem {
public:
    static JobSystem& get();

    JobSystem() = default;
    ~JobSystem();

    void start(size_t threadsCount);
    void stop();

    template<class F, class R = std::invoke_result_t<F&>>
    TaskHandle<R> schedule(F&& function, std::vector<TaskHandle<void>> deps = {});

    TaskHandle<void> combine(std::vector<TaskHandle<void>> deps);

    template<class F>
    TaskHandle<void> parallelFor(int begin, int end, F&& function);

    void wait(TaskHandle<void> h);

private:
    struct Worker {
        std::deque<std::shared_ptr<Task>> queue;
        std::mutex mutex;
    };

    void enqueue(std::shared_ptr<Task> task);
    void workerLoop(int i);
    void onTaskDone(std::shared_ptr<Task> task, int workerId);
    void pushReady(int id, std::shared_ptr<Task> task);
    bool trySteal(int id, std::shared_ptr<Task>& out);
    bool anyQueueHasJob();

    std::vector<std::thread> m_threads;
    std::unique_ptr<Worker[]> m_workers;
    std::condition_variable m_sleepCv;
    std::mutex m_sleepMutex;
    std::atomic<int> m_nextWorkerId{0};
    std::atomic<bool> m_running{true};
};

template<class F, class R>
TaskHandle<R> JobSystem::schedule(F&& function, std::vector<TaskHandle<void>> deps) {
    auto task    = std::make_shared<Task>();
    auto promise = std::make_shared<std::promise<R>>();
    auto future  = promise->get_future().share();

    task->function = [promise, function = std::forward<F>(function)]() mutable {
        try {
            if constexpr (std::is_void_v<R>) {
                function();
                promise->set_value();
            } else {
                promise->set_value(function());
            }
        } catch (...) {
            promise->set_exception(std::current_exception());
        }
    };

    task->unfinished.store((int)deps.size());

    for (auto& d : deps) {
        std::lock_guard lock(d.task->mtx);
        if (d.task->done.load()) {
            task->unfinished.fetch_sub(1);
        } else {
            d.task->dependents.push_back(task);
        }
    }

    if (task->unfinished.load() == 0)
        enqueue(task);
    return TaskHandle<R>{ std::move(task), std::move(future) };
}

template<class F>
TaskHandle<void> JobSystem::parallelFor(int begin, int end, F&& function) {
    const int n = end - begin;
    if (n <= 0)
        return combine({});

    const int threads   = (int)m_threads.size();
    const int chunkSize = (std::max)(1, (n + threads - 1) / threads);

    auto g      = std::make_shared<Task>();
    auto promise = std::make_shared<std::promise<void>>();
    auto future  = promise->get_future().share();
    g->function = [promise] { promise->set_value(); };

    for (int i = begin; i < end; i += chunkSize) {
        int low  = i;
        int high = (std::min)(i + chunkSize, end);
        auto task = std::make_shared<Task>();
        task->function = [low, high, function] {
            for (int k = low; k < high; ++k)
                function(k);
        };
        task->dependents.push_back(g);
        g->unfinished.fetch_add(1);
        enqueue(std::move(task));
    }
    return TaskHandle<void>{ g, std::move(future) };
}

}
