#include "vepch.h"
#include "JobSystem.h"

namespace ve {

JobSystem& JobSystem::get() {
    static JobSystem instance;
    static bool started = [] {
        unsigned hw = std::thread::hardware_concurrency();
        instance.start(hw > 1 ? hw - 1 : 1);
        return true;
    }();
    return instance;
}

JobSystem::~JobSystem() {
    stop();
}

void JobSystem::start(size_t threadsCount) {
    m_running.store(true);
    threadsCount = std::max<size_t>(1, threadsCount);
    m_workers = std::make_unique<Worker[]>(threadsCount);
    m_threads.reserve(threadsCount);
    for (size_t i = 0; i < threadsCount; i++) {
        m_threads.emplace_back([this, i] { workerLoop((int)i); });
    }
}

void JobSystem::stop() {
    {
        std::lock_guard lock(m_sleepMutex);
        m_running = false;
    }
    m_sleepCv.notify_all();
    for (auto& t : m_threads) t.join();
    m_threads.clear();
}

TaskHandle<void> JobSystem::combine(std::vector<TaskHandle<void>> deps) {
    return schedule([] {}, std::move(deps));
}

void JobSystem::wait(TaskHandle<void> h) {
    std::unique_lock lock(m_sleepMutex);
    m_sleepCv.wait(lock, [&] { return h.task->done.load(); });
}

void JobSystem::enqueue(std::shared_ptr<Task> task) {
    int id;
    {
        std::lock_guard lock(m_sleepMutex);
        id = m_nextWorkerId++ % (int)m_threads.size();
    }
    pushReady(id, std::move(task));
}

void JobSystem::workerLoop(int i) {
    while (true) {
        std::shared_ptr<Task> task;
        {
            std::lock_guard lock(m_workers[i].mutex);
            if (!m_running && m_workers[i].queue.empty()) break;
            if (!m_workers[i].queue.empty()) {
                task = m_workers[i].queue.back();
                m_workers[i].queue.pop_back();
            }
        }
        if (!task) trySteal(i, task);
        if (task) {
            task->function();
            onTaskDone(task, i);
            continue;
        }
        std::unique_lock lock(m_sleepMutex);
        m_sleepCv.wait(lock, [this] { return !m_running || anyQueueHasJob(); });
    }
}

void JobSystem::onTaskDone(std::shared_ptr<Task> task, int workerId) {
    {
        std::lock_guard lock(task->mtx);
        task->done.store(true);

        for (auto& dep : task->dependents) {
            if (dep->unfinished.fetch_sub(1) == 1)
                pushReady(workerId, dep);
        }
    }
    m_sleepCv.notify_all();
}

void JobSystem::pushReady(int id, std::shared_ptr<Task> task) {
    {
        std::lock_guard lock(m_workers[id].mutex);
        m_workers[id].queue.push_back(std::move(task));
    }
    std::lock_guard lock(m_sleepMutex);
    m_sleepCv.notify_all();
}

bool JobSystem::trySteal(int id, std::shared_ptr<Task>& out) {
    for (int i = 1; i < (int)m_threads.size(); i++) {
        int victimId = (i + id) % (int)m_threads.size();
        {
            std::lock_guard lock(m_workers[victimId].mutex);
            if (!m_workers[victimId].queue.empty()) {
                out = m_workers[victimId].queue.front();
                m_workers[victimId].queue.pop_front();
                return true;
            }
        }
    }
    return false;
}

bool JobSystem::anyQueueHasJob() {
    for (int i = 0; i < (int)m_threads.size(); i++) {
        std::lock_guard g(m_workers[i].mutex);
        if (!m_workers[i].queue.empty()) return true;
    }
    return false;
}

}
