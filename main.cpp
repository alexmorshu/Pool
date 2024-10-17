#include <thread>
#include <iostream>
#include <vector>
#include <cstddef>
#include <random>

struct Data
{
	std::byte* destination;
	std::size_t size;
};

class Pool
{
public: 
	Pool(const unsigned int cores): threads(cores), flag(ATOMIC_FLAG_INIT), num(0)
	{	
		for(unsigned int i = 0; i < cores; i++)
		{
			threads[i] = std::move(std::thread(Pool::run, this, i, cores));
		}	
	}

	Pool(): Pool(std::thread::hardware_concurrency())
	{}


	~Pool()
	{
		for(std::thread& thread : threads)
		{
			if(thread.joinable())
			{
				thread.detach();
			}
		}
	}

	static void run(Pool* pool, const unsigned int n, const unsigned int max)
	{
		while(true)
		{
			pool->flag.wait(false, std::memory_order_acquire);
			if(pool->function == nullptr)
				return;
			pool->function(pool->args, n, max);
			std::size_t s = pool->num.fetch_add(1, std::memory_order_relaxed);
			if((s + 1) == max)
			{
				pool->num.store(0, std::memory_order_relaxed);
				pool->flag.clear(std::memory_order_release);
				pool->flag.notify_one();
			}
			else
			{
				while(!pool->flag.test(std::memory_order_relaxed));
				std::atomic_thread_fence(std::memory_order_release);
			}
		}
	}
	
	template<class TArgs>
	void make(void (*function)(TArgs*,const unsigned int n, const unsigned int max),TArgs* args)
	{
		this->function = 
			reinterpret_cast<void (*)(void*, const unsigned int, const unsigned int)>(function);
		this->args = args;
		this->flag.test_and_set(std::memory_order_release);
		this->flag.notify_all();
	}

	void join()
	{
		for(std::thread& thread : threads)
		{
			if(thread.joinable())
			{
				thread.join();
			}
		}
	}

	bool isWork()
	{
		return flag.test(std::memory_order_acquire);
	}

	void wait()
	{	
		this->flag.wait(true, std::memory_order_acquire);
	}

	void stop()
	{
		this->wait();
		this->function = nullptr;
		this->flag.test_and_set(std::memory_order_release);
		this->flag.notify_all();
	}

private:
	std::atomic_flag flag;
	std::vector<std::thread> threads;
	void (* function)(void* ptr, const unsigned int n, const unsigned int max);
	void* args;
	std::atomic<std::size_t> num;
};

void fill(std::byte* ptr, std::size_t size)
{
	std::mt19937 gen(std::time(nullptr));
	for(std::size_t i = 0; i < size; i++)
	{
		*(ptr + i) = std::byte(gen()%256);
	}	
}

void funtion(Data* data, const unsigned int n, const unsigned int max)
{
	std::mt19937 gen(std::time(nullptr));
	std::size_t div = (data->size)/max;
	std::size_t ptr = n*div;
	if((n+1) == max)
		div = data->size - ptr;
	fill(data->destination + ptr, div);
}
	


int main()
{
	Data data;
	data.destination = new(std::align_val_t(64)) std::byte[1024ull];
	data.size = 1024ull;

	Pool pool(6);

	pool.make(funtion, &data);
	pool.wait();
	pool.stop();
}
