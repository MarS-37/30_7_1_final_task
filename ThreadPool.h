#ifndef THREADPOOL_H
#define THREADPOOL_H


#include <condition_variable>
#include <functional>
#include <vector>
#include <memory>
#include <future>
#include <thread>
#include <mutex>
#include <queue>


// thread pool with work stealing support
class ThreadPool
{
public:
    // constructor initializing the thread pool
    explicit ThreadPool(size_t m_threads);
    // destructor stopping the thread pool
    ~ThreadPool();

    template <class F, class... Args>
    auto enqueue(F&& f, Args&&... args) -> std::future<typename std::invoke_result<F, Args...>::type>;

private:
    // function executed by each thread
    void worker_thread(size_t index);

    // vector of threads
    std::vector<std::thread> workers;
    // queue of tasks
    std::queue<std::function<void()>> tasks;
    // mutex to synchronize access to the task queue
    std::mutex queue_mutex;
    // condition variable for notifying threads
    std::condition_variable condition;
    // flag to stop the thread pool
    bool stop;
};

// template method
template <class F, class... Args>
// adding a task to the queue
auto ThreadPool::enqueue(F&& f, Args&&... args) -> std::future<typename std::invoke_result<F, Args...>::type>
{
    // defining the return type of the task
    using return_type = typename std::invoke_result<F, Args...>::type;
    // creating the task
    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );

    // obtaining a future object for the task
    std::future<return_type> res = task->get_future();
    {
        // locking the mutex to protect the task queue
        std::unique_lock<std::mutex> lock(queue_mutex);
        // checking the stop flag
        if (stop)
            // exception if the thread pool is stopped
            throw std::runtime_error("enqueue on stopped ThreadPool");
        // adding the task to the queue
        tasks.emplace([task]() { (*task)(); });
    }

    // notifying one thread about the new task
    condition.notify_one();
    // returning the future
    return res;
}

#endif // THREADPOOL_H