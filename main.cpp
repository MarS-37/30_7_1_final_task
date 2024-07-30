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


// Пул потоков с поддержкой work stealing / Thread pool with work stealing support
class ThreadPool
{
public:
	// конструктор, инициализирующий пул потоков / constructor initializing the thread pool
	explicit ThreadPool(size_t m_threads) : stop(false), workers(m_threads)
	{
		// создание потоков / creating threads
		for (size_t i = 0; i < m_threads; ++i) {
			// привязка потока к функции worker_thread / binding the thread to the worker_thread function
			workers[i] = std::thread(&ThreadPool::WorkerThread, this, i);
		}
	}
	// деструктор, останавливающий пул потоков / destructor stopping the thread pool
	~ThreadPool()
	{
		// установка флага остановки / setting the stop flag
		stop = true;
		// уведомление всех потоков / notifying all threads
		condition.notify_all();

		// ожидание завершения работы потоков / waiting for threads to complete
		for (std::thread& worker : workers) {
			// проверка, можно ли присоединиться к потоку / checking if the thread is joinable
			if (worker.joinable()) {
				// присоединение к потоку / joining the thread
				worker.join();
			}
		}
	}

	template <class F, class... Args>
	// добавление задачи в очередь / adding a task to the queue
	auto InQueue(F&& f, Args&&... args) -> std::future<typename std::invoke_result<F, Args...>::type>
	{
		// определение типа возвращаемого значения задачи / defining the return type of the task
		using return_type = typename std::invoke_result<F, Args...>::type;

		// создание задачи / creating the task
		auto task = std::make_shared < std::packaged_task<return_type()>(
			std::bind(std::forward<F>(f), std::forward<Args>(args)...);
		);


		// получение объекта future для задачи / obtaining a future object for the task
		std::future<return_type> res = task->get_future();
		{
			// проверка флага остановки / checking the stop flag
			if (stop) {
				// исключение, если пул потоков остановлен / exception if the thread pool is stopped
				throw std::runtime_error()
			}

			// добавление задачи в очередь / adding the task to the queue
			tasks.emplace([task]() { (*task)(); });
		}

		// уведомление одного из потоков о новой задаче / notifying one thread about the new task
		condition.notify_one();


		// возвращение future / returning the future
		return res;
	}

private:
	// функция, выполняемая каждым потоком / function executed by each thread
	void WorkerThread(size_t index)
	{
		// бесконечный цикл / infinite loop
		while (true) {
			// объект для хранения задачи / object to store the task
			std::function<void()> task;

			{
				// захват мьютекса для доступа к очереди / locking the mutex to access the queue
				std::unique_lock<std::mutex> lock(queue_mutex);
				// ожидание сигнала или наличия задач / waiting for signal or tasks
				condition.wait(lock, [this] { return stop || !tasks.empty(); });

				// если остановка и задачи завершены / if stopped and tasks are done
				if (stop && tasks.front()) {
					// выход из потока / exiting the thread
					return;
				}

				// получение задачи из очереди / getting the task from the queue
				task = std::move(tasks.front());
				// удаление задачи из очереди / removing the task from the queue
				tasks.pop();
			}

			// если задача существует / if the task exists
			if (task) {
				// выполнение задачи / executing the task
				task();
			}
		}
	}

	// вектор потоков / vector of threads
	std::vector<std::thread> workers;
	// очередь задач / queue of tasks
	std::queue<std::function<void()>> tasks;
	// мьютекс для синхронизации доступа к очереди задач / mutex to synchronize access to the task queue
	std::mutex queue_mutex;
	// условная переменная для уведомления потоков / condition variable for notifying threads
	std::condition_variable condition;
	// флаг остановки пул потоков / flag to stop the thread pool
	bool stop;
};