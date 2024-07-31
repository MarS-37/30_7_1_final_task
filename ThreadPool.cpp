#include "ThreadPool.h"


// constructor initializing the thread pool
ThreadPool::ThreadPool(size_t m_threads) : stop(false), workers(m_threads)
{
    // creating threads
    for (size_t i = 0; i < m_threads; ++i) {
        // binding the thread to the worker_thread function
        workers[i] = std::thread(&ThreadPool::worker_thread, this, i);
    }
}

// destructor stopping the thread pool
ThreadPool::~ThreadPool()
{
    // setting the stop flag
    stop = true;
    // notifying all threads
    condition.notify_all();
    //  waiting for threads to complete
    for (std::thread& worker : workers) {
        // checking if the thread is joinable
        if (worker.joinable()) {
            // joining the thread
            worker.join();
        }
    }
}

// function executed by each thread
void ThreadPool::worker_thread(size_t index)
{
    // infinite loop
    while (true) {
        // object to store the task
        std::function<void()> task;

        {
            // locking the mutex to access the queue
            std::unique_lock<std::mutex> lock(queue_mutex);
            // waiting for signal or tasks
            condition.wait(lock, [this] { return stop || !tasks.empty(); });
            // if stopped and tasks are done
            if (stop && tasks.empty()) {
                // exiting the thread
                return;
            }

            // getting the task from the queue
            task = std::move(tasks.front());
            // removing the task from the queue
            tasks.pop();
        }

        // if the task exists
        if (task) {
            // executing the task
            task();
        }
    }
}