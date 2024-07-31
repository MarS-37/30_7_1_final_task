//+------------------------------------------------------------------+
//|                      30.7.1 Final assignment                     |
//|                                                                  |
//|              TESTED: MSVC v.143 - VS 2022 C++ 86/64              |
//|                                                                  |
//|           https:https://github.com/MarS-37/30_7_1_final_task.git |
//|                                       markin.sergey.37@yandex.ru |
//+------------------------------------------------------------------+

#include <condition_variable>
#include <iostream>
#include <vector>
#include <random>
#include <memory>
#include <future>
#include <thread>
#include <atomic>
#include <chrono>
#include <mutex>
#include <queue>


// generation of a random number array
class PayloadGenerator
{
public:
    // function to generate an array of random numbers
    static std::vector<int> generate(size_t size, int min, int max) {
        // creating a vector of the given size
        std::vector<int> data(size);
        // random number device
        std::random_device rd;
        // random number generator
        std::mt19937 gen(rd());
        // uniform distribution of random numbers
        std::uniform_int_distribution<> dis(min, max);

        // replacement suggested by static analyzer cppcheck
        // filling the vector with random numbers
        std::generate(data.begin(), data.end(), [&]() { return dis(gen); });

        // returning the generated vector
        return data;
    }
};


// thread pool with work stealing support
class ThreadPool
{
public:
    // constructor initializing the thread pool
    explicit ThreadPool(size_t m_threads) : stop(false), workers(m_threads)
    {
        // creating threads
        for (size_t i = 0; i < m_threads; ++i) {
            // binding the thread to the worker_thread function
            workers[i] = std::thread(&ThreadPool::worker_thread, this, i);
        }
    }
    // destructor stopping the thread pool
    ~ThreadPool()
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

    // adding a task to the queue
    template <class F, class... Args>
    auto enqueue(F&& f, Args&&... args) -> std::future<typename std::invoke_result<F, Args...>::type>
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

private:
    // function executed by each thread
    void worker_thread(size_t index)
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


// multithreaded quicksort / функция многопоточной быстрой сортировки
void quicksort(std::vector<int>& data, int left, int right, std::shared_ptr<std::atomic<int>> remaining_tasks, ThreadPool& pool, std::shared_ptr<std::promise<void>> promise)
{
    // base case for recursion termination
    if (left >= right) {
        // if all tasks are done
        if (--(*remaining_tasks) == 0) {
            // setting the promise value
            promise->set_value();
        }

        // exiting the function
        return;
    }

    // selecting the pivot element
    int pivot = data[right];
    // index for partitioning elements
    int i = left - 1;

    // loop over array elements
    for (int j = left; j <= right - 1; ++j) {
        // if element is less than or equal to pivot
        if (data[j] <= pivot) {
            // increment index i
            ++i;
            // swap elements
            std::swap(data[i], data[j]);
        }
    }

    // placing the pivot element at the correct position
    std::swap(data[i + 1], data[right]);
    // index of the pivot element
    int part_index = i + 1;

    // counting remaining tasks
    (*remaining_tasks) += 2;

    // promise for the left part
    auto left_promise = std::make_shared<std::promise<void>>();
    // promise for the right part
    auto right_promise = std::make_shared<std::promise<void>>();
    // future to wait for the left part
    auto left_future = left_promise->get_future();
    // future to wait for the right part
    auto right_future = right_promise->get_future();

    // if the left part is larger than a threshold
    if (part_index - 1 - left > 100000) {
        // start sorting in a new thread
        pool.enqueue(quicksort, std::ref(data), left, part_index - 1, remaining_tasks, std::ref(pool), left_promise);
    }
    else {
        // sorting in the current thread
        quicksort(data, left, part_index - 1, remaining_tasks, pool, left_promise);
    }

    // if the right part is larger than a threshold
    if (right - part_index > 100000) {
        // start sorting in a new thread
        pool.enqueue(quicksort, std::ref(data), part_index + 1, right, remaining_tasks, std::ref(pool), right_promise);
    }
    else {
        // sorting in the current thread
        quicksort(data, part_index + 1, right, remaining_tasks, pool, right_promise);
    }

    // creating a thread to wait for completion
    std::thread([left_future = std::move(left_future), right_future = std::move(right_future), remaining_tasks, promise]() mutable {
        // waiting for the left part to complete
        left_future.wait();
        // waiting for the right part to complete
        right_future.wait();

        // if all tasks are done
        if (--(*remaining_tasks) == 0) {
            // setting the promise value
            promise->set_value();
        }
        }).detach(); // detaching the thread
}


int main()
{
    // parameters
    // size of the array
    int size = 1'000'000;
    // minimum random number value
    int min_value = -1'000;
    // maximum random number value
    int max_value = 4'000;
    // number of available hardware threads
    auto ccore = std::thread::hardware_concurrency();

    // outputting the array size
    std::cout << "Array size: " << size << std::endl;
    // outputting the minimum value
    std::cout << "Min value: " << min_value << std::endl;
    // outputting the maximum value
    std::cout << "Max value: " << max_value << std::endl;
    // outputting the number of threads
    std::cout << "Number of threads: " << ccore << std::endl;

    // generating the data array
    // creating and filling the array with random numbers
    std::vector<int> data = PayloadGenerator::generate(size, min_value, max_value);

    // creating the thread pool
    // initializing the thread pool with the number of threads equal to the number of CPU cores
    ThreadPool pool(ccore);

    // starting the time measurement
    // start the time counter
    auto start = std::chrono::high_resolution_clock::now();

    // main promise for waiting for sorting completion
    auto main_promise = std::make_shared<std::promise<void>>();
    // future for the main promise
    auto main_future = main_promise->get_future();

    // starting the multithreaded quicksort
    // atomic counter for remaining tasks
    auto remaining_tasks = std::make_shared<std::atomic<int>>(1);
    // starting the sorting
    quicksort(data, 0, static_cast<int>(data.size() - 1), remaining_tasks, pool, main_promise);
    // waiting for all tasks to complete
    main_future.wait();

    // end of time measurement
    // stopping the time counter
    auto end = std::chrono::high_resolution_clock::now();
    // calculating the elapsed time
    std::chrono::duration<double> elapsed = end - start;
    // outputting the elapsed time
    std::cout << "Elapsed time: " << elapsed.count() << " seconds" << std::endl;

    // checking if the array is sorted
    if (std::is_sorted(data.begin(), data.end())) {
        // outputting a message about successful sorting
        std::cout << "Array is sorted." << std::endl;
    }
    else {
        // outputting a message about unsuccessful sorting
        std::cout << "Array is NOT sorted." << std::endl;
    }


    // exiting the program
    return 0;
}