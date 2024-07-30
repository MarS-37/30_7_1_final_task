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


// ��� ������� � ���������� work stealing / Thread pool with work stealing support
class ThreadPool
{
public:
	// �����������, ���������������� ��� ������� / constructor initializing the thread pool
	explicit ThreadPool(size_t m_threads) : stop(false), workers(m_threads)
	{
		// �������� ������� / creating threads
		for (size_t i = 0; i < m_threads; ++i) {
			// �������� ������ � ������� worker_thread / binding the thread to the worker_thread function
			workers[i] = std::thread(&ThreadPool::WorkerThread, this, i);
		}
	}
	// ����������, ��������������� ��� ������� / destructor stopping the thread pool
	~ThreadPool()
	{
		// ��������� ����� ��������� / setting the stop flag
		stop = true;
		// ����������� ���� ������� / notifying all threads
		condition.notify_all();

		// �������� ���������� ������ ������� / waiting for threads to complete
		for (std::thread& worker : workers) {
			// ��������, ����� �� �������������� � ������ / checking if the thread is joinable
			if (worker.joinable()) {
				// ������������� � ������ / joining the thread
				worker.join();
			}
		}
	}

	template <class F, class... Args>
	// ���������� ������ � ������� / adding a task to the queue
	auto InQueue(F&& f, Args&&... args) -> std::future<typename std::invoke_result<F, Args...>::type>
	{
		// ����������� ���� ������������� �������� ������ / defining the return type of the task
		using return_type = typename std::invoke_result<F, Args...>::type;

		// �������� ������ / creating the task
		auto task = std::make_shared < std::packaged_task<return_type()>(
			std::bind(std::forward<F>(f), std::forward<Args>(args)...);
		);


		// ��������� ������� future ��� ������ / obtaining a future object for the task
		std::future<return_type> res = task->get_future();
		{
			// �������� ����� ��������� / checking the stop flag
			if (stop) {
				// ����������, ���� ��� ������� ���������� / exception if the thread pool is stopped
				throw std::runtime_error()
			}

			// ���������� ������ � ������� / adding the task to the queue
			tasks.emplace([task]() { (*task)(); });
		}

		// ����������� ������ �� ������� � ����� ������ / notifying one thread about the new task
		condition.notify_one();


		// ����������� future / returning the future
		return res;
	}

private:
	// �������, ����������� ������ ������� / function executed by each thread
	void WorkerThread(size_t index)
	{
		// ����������� ���� / infinite loop
		while (true) {
			// ������ ��� �������� ������ / object to store the task
			std::function<void()> task;

			{
				// ������ �������� ��� ������� � ������� / locking the mutex to access the queue
				std::unique_lock<std::mutex> lock(queue_mutex);
				// �������� ������� ��� ������� ����� / waiting for signal or tasks
				condition.wait(lock, [this] { return stop || !tasks.empty(); });

				// ���� ��������� � ������ ��������� / if stopped and tasks are done
				if (stop && tasks.front()) {
					// ����� �� ������ / exiting the thread
					return;
				}

				// ��������� ������ �� ������� / getting the task from the queue
				task = std::move(tasks.front());
				// �������� ������ �� ������� / removing the task from the queue
				tasks.pop();
			}

			// ���� ������ ���������� / if the task exists
			if (task) {
				// ���������� ������ / executing the task
				task();
			}
		}
	}

	// ������ ������� / vector of threads
	std::vector<std::thread> workers;
	// ������� ����� / queue of tasks
	std::queue<std::function<void()>> tasks;
	// ������� ��� ������������� ������� � ������� ����� / mutex to synchronize access to the task queue
	std::mutex queue_mutex;
	// �������� ���������� ��� ����������� ������� / condition variable for notifying threads
	std::condition_variable condition;
	// ���� ��������� ��� ������� / flag to stop the thread pool
	bool stop;
};