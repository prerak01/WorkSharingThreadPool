/*

			COTTON++ - Library based Work-Stealing concurrency platform
It supports very basic async-finish parallelism. It supports “flat finish” scopes.
A flat-finish scope is a single-level finish scope for all async tasks spawned within its scope
(i.e., an async task can recursively create new async tasks, but it can never spawn new finish
scopes)

								Implemented by-
							 Prerak Malik 2019378
							Navam Chaurasia 2019314

*/

#include "cotton.h"
#include "cotton-runtime.h"
#include <pthread.h>
#include <unistd.h>

const int QUEUE_SIZE = 10000;
//#define debug

#ifdef debug
float misses = 0;
float hits = 0;
#endif

// Global Variables
pthread_key_t globalkey;
pthread_mutex_t finish_mutex;
int totalThreads;
pthread_t *thread_store;
volatile int shutdown = 0, finish_counter;

// Class representing Deck/Deque
class Deque
{
	volatile int start, end, size;
	std::function<void()> **queue;
	pthread_mutex_t queue_mutex;

public:
	// Constructor for Deque class
	Deque()
	{
		size = 0;
		start = 0;
		end = 0;
		pthread_mutex_init(&queue_mutex, NULL);
		queue = (std::function<void()> **)malloc(sizeof(std::function<void()> *) * QUEUE_SIZE);
	}

	// Method to push task to its task queue by the thread from the tail
	void push(std::function<void()> *pointer)
	{	pthread_mutex_lock(&queue_mutex);
		if (size >= QUEUE_SIZE)
		{
			perror("queue overflow exiting..");
			exit(-1);
		}
		queue[end] = pointer;
		end = (end + 1) % QUEUE_SIZE;
		size++;
		pthread_mutex_unlock(&queue_mutex);
	}

	// Method to steal(pop) task from the head by some other thread
	std::function<void()> *FIFOpoll()
	{
		std::function<void()> *temp = (std::function<void()> *)malloc(sizeof(std::function<void()>));
		temp = NULL;
		pthread_mutex_lock(&queue_mutex);
		if (size > 0)
		{
			temp = *(queue + start);
			start = (start + 1) % QUEUE_SIZE;
			size--;
		}
		pthread_mutex_unlock(&queue_mutex);
		return temp;
	}

	// Method to pop task from the tail by the thread
	std::function<void()> *LIFOpoll()
	{
		std::function<void()> *temp = (std::function<void()> *)malloc(sizeof(std::function<void()>));
		temp = NULL;
		pthread_mutex_lock(&queue_mutex);
		if (size > 0)
		{
			end = (end - 1) % QUEUE_SIZE;
			temp = *(queue + end);
			size--;
		}
		pthread_mutex_unlock(&queue_mutex);
		return temp;
	}
};


// Class representing all the queues in a container dequeContainer
class dequeContainter
{
	Deque *store;

public:
// Parametrized Constructor of dequeContainer
	dequeContainter(int threads)
	{
		store = (Deque *)malloc(sizeof(Deque) * threads);
		for (int i = 0; i < threads; i++)
			*(store + i) = Deque();
	}

	dequeContainter() {}

// Method to push task to taskPool of thread with private key(which we get using getThread) 
	void push(std::function<void()> *task)
	{
		// printf("%d ",cottonruntime::getThread());
		(store + cottonruntime::getThread())->push(task);
	}

// Method to pop task from its own taskPool by the thread with private key(which we get using getThread) 
	std::function<void()> *poll()
	{
		return (store + cottonruntime::getThread())->LIFOpoll();
	}

// Method to steal a task from a taskPool of other threads by choosing a taskPool randomly
	std::function<void()> *steal()
	{
		int mine = cottonruntime::getThread();

		int toSteal = rand() % totalThreads;
		while (toSteal == mine)
		{
			toSteal = rand() % totalThreads;
		}
		return (store + toSteal)->FIFOpoll();
	}
};

dequeContainter deque;

// Initialising all the data structures and variables
void cotton::init_runtime()
{
	pthread_mutex_init(&finish_mutex, NULL);
	srand(time(0));

	if (getenv("COTTON_WORKERS") != NULL)
		totalThreads = atoi(getenv("COTTON_WORKERS"));
	else
		totalThreads = 1;

	printf("total threads %d \n\n", totalThreads);

	// initializing container deque
	deque = dequeContainter(totalThreads);

	// binding threads to particular value
	pthread_key_create(&globalkey, NULL);
	int *p = (int *)malloc(sizeof(int));
	*p = 0;
	pthread_setspecific(globalkey, (void *)p);

	thread_store = (pthread_t *)malloc(sizeof(pthread_t) * (totalThreads - 1));

	// spawn will also pass values which need to be assigned to each thread
	cottonruntime::spawn(totalThreads);
}

// Helper function to get the private key of the thread
int cottonruntime::getThread()
{
	return *((int *)pthread_getspecific(globalkey));
}

// Helper Function to spawn n-1 threads
void cottonruntime::spawn(int n)
{
	for (int i = 1; i < totalThreads; i++)
	{
		int *p = (int *)malloc(sizeof(int));
		*p = i;
		pthread_create(thread_store + i - 1, NULL, routine, (void *)p);
	}
}

// Starting the finish scope
void cotton::start_finish()
{
	// Setting the finish_counter to 0
	pthread_mutex_lock(&finish_mutex);
	finish_counter = 0;
	pthread_mutex_unlock(&finish_mutex);
}

// Ending the finish scope
void cotton::end_finish()
{	// while loop continues until finish counter > 0
	while (finish_counter != 0)
	{// Thread executes a task by poping it from its taskpool 
	// or stealing it from the taskpools of other threads
		cottonruntime::executeTask();
	}
}

// Destroying locks, condition variables, joining threads 
void cotton::finalize_runtime()
{	// shutdown is set to 1 so that no more threads execute routine()
	shutdown = 1;
	cottonruntime::join(totalThreads);
	pthread_mutex_destroy(&finish_mutex);
	// printf("Hit percentage %f\n", hits / (misses + hits));
}

// Asynchronous function to copy task to heap, getting pointer to that task and saving it to queue
void cotton::async(std::function<void()> &&lambda)
{
	pthread_mutex_lock(&finish_mutex);
	finish_counter++;
	pthread_mutex_unlock(&finish_mutex);

	// Allocation of memory in heap
	std::function<void()> *pointer = (std::function<void()> *)malloc(sizeof(lambda));
	memcpy(pointer, &lambda, sizeof(lambda));

	// Adding a task to its taskpool by the thread( push task to queue)
	deque.push(pointer);
}

// Helper Function to join n-1 threads
void cottonruntime::join(int n)
{
	for (int i = 0; i < n - 1; i++)
		pthread_join(*(thread_store + i), NULL);
}

// Function to be run by each thread
void *cottonruntime::routine(void *p)
{
	// Assigning key to thread
	pthread_setspecific(globalkey, p);

	while (shutdown != 1)
	{
		cottonruntime::executeTask();
	}

	return NULL;
}

// Function to execute task by a thread by taking it from his taskpool or from taskpools of other threads
void cottonruntime::executeTask()
{
	//Thread first tries to pop task from his own threadpool
	std::function<void()> *p = deque.poll(); 

	//If his taskpool is empty, thread tries to steal from taskpools of other threads 
	if (p == NULL)
		p = deque.steal();
	if (p == NULL)
	{
#ifdef debug
		misses++;
#endif

		return;
	}

#ifdef debug
	hits++;
#endif

// Running task inside variable task
	std::function<void()> task = *p;
	task();

	free(p);

	pthread_mutex_lock(&finish_mutex);
	finish_counter--;
	pthread_mutex_unlock(&finish_mutex);
}
