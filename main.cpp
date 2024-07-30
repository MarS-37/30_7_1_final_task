#include <condition_variable>
#include <vector>
#include <random>
#include <thread>
#include <future>
#include <queue>


// generation of a random number array
class PayloadGenerator
{
public:
	// function to generate an array of random numbers
	static std::vector<int> generate(size_t size, int min, int max)
	{
		// creating a vector of the given size
		std::vector<int> vec_number(size);
		// random number device
		std::random_device rd;
		// random number generator
		std::mt19937 gen(rd());
		// uniform distribution of random numbers
		std::uniform_int_distribution<int> dis(min, max);

		// initial solution
		/*for (auto& d : data) {
			d = dis(gen);
		}*/
		// replacement suggested by static analyzer cppcheck
		std::generate(vec_number.begin(), vec_number.end(), [&]() { return dis(gen); });


		// returning the generated vector
		return vec_number;
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
			// binding the thread to the workerThread function
			workers[i] = std::thread(&ThreadPool::workerThread, this, i);
		}
	}
	// destructor stopping the thread pool
	~ThreadPool()
	{
		// setting the stop flag
		stop = true;
		// notifying all threads
		condition.notify_all();

		// waiting for threads to complete
		for (std::thread& worker : workers) {
			// checking if the thread is joinable
			if (worker.joinable()) {
				// joining the thread
				worker.join();
			}
		}
	}

	template <class F, class... Args>
	// adding a task to the queue
	auto inQueue(F&& f, Args&&... args) -> std::future<typename std::invoke_result<F, Args...>::type>
	{
		// defining the return type of the task
		using return_type = typename std::invoke_result<F, Args...>::type;

		// creating the task
		auto task = std::make_shared < std::packaged_task<return_type()>(
			std::bind(std::forward<F>(f), std::forward<Args>(args)...);
		);


		// obtaining a future object for the task
		std::future<return_type> res = task->get_future();
		{
			// checking the stop flag
			if (stop) {
				// exception if the thread pool is stopped
				throw std::runtime_error()
			}

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
	void workerThread(size_t index)
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
				if (stop && tasks.front()) {
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
		pool.inQueue(quicksort, std::ref(data), left, part_index - 1, remaining_tasks, std::ref(pool), left_promise);
	}
	else {
		// sorting in the current thread
		quicksort(data, left, part_index - 1, remaining_tasks, pool, left_promise);
	}

	// if the right part is larger than a threshold
	if (right - part_index > 100000) {
		// start sorting in a new thread
		pool.inQueue(quicksort, std::ref(data), part_index + 1, right, remaining_tasks, std::ref(pool), right_promise);
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