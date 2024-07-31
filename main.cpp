//+------------------------------------------------------------------+
//|                      30.7.1 Final assignment                     |
//|                                                                  |
//|              TESTED: MSVC v.143 - VS 2022 C++ 86/64              |
//|                                                                  |
//|           https:https://github.com/MarS-37/30_7_1_final_task.git |
//|                                       markin.sergey.37@yandex.ru |
//+------------------------------------------------------------------+

#include "PayloadGenerator.h"
#include "ThreadPool.h"
#include <iostream>
#include <vector>
#include <memory>
#include <future>
#include <atomic>
#include <chrono>


// multithreaded quicksort
void quicksort(std::vector<int>& data, int left, int right, std::shared_ptr<std::atomic<int>> remaining_tasks, ThreadPool& pool, std::shared_ptr<std::promise<void>> promise);


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


// multithreaded quicksort
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