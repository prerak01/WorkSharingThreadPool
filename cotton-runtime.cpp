/*

			COTTON - Library based Work-Sharing concurrency platform
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

// Queue size is set to QUEUE_SIZE
const int QUEUE_SIZE = 999999;

#define debug

class myQueue{
	// Initializing variables for our taskPool(queue)
	
	public:
		std::function<void()> *queue[QUEUE_SIZE];
		
		int size = 0;
		int start = 0;
		int end = 0;

		void push_task_to_runtime(std::function<void()> *pointer){
			// Adding task to queue
			// cottonruntime::lock(&queue_mutex);
			if (size == QUEUE_SIZE)
			{
				perror("queue overflow exiting");
				exit(-1);
			}	
			size++;
			queue[end] = pointer;
			end++;
			end %= QUEUE_SIZE;
			// pthread_cond_signal(&cond);
			// cottonruntime::unlock(&queue_mutex);
		}

		std::function<void()> pop_task_to_runtime(){
			std::function<void()> task;
			// cottonruntime::lock(&queue_mutex);

			// If queue is empty then task is set to NULL and the thread returns
			if (size == 0)
			{
				task = NULL;
			}
			else
			{
				// Task is taken from the front of the taskpool (queue) and stored in task
				task = *queue[start];

				start++;
				size--;
				start %= QUEUE_SIZE;
			}
			// cottonruntime::unlock(&queue_mutex);

			return task;
	}
};


// Global variables

volatile int shutdown = 0;
pthread_mutex_t finish_mutex, queue_mutex;
int threads;
pthread_t *thread_store;
pthread_cond_t cond;
// std::function<void()> *queue[QUEUE_SIZE];
// int size, start, end;
volatile int finish_counter;
myQueue queue;

// Initialising all the data structures and variables
void cotton::init_runtime()
{
	// Initialization of mutex and condition variables
	pthread_mutex_init(&queue_mutex, NULL);
	pthread_mutex_init(&finish_mutex, NULL);
	pthread_cond_init(&cond, NULL);

	/*
	default number of threads is 1
	*/
	if (getenv("COTTON_WORKERS") != NULL)
		threads = atoi(getenv("COTTON_WORKERS"));
	else
		threads = 1;
	printf("total threads %d\n", threads);

	// storing pthread_t for 1 less (excluding main thread)
	thread_store = (pthread_t *)malloc(sizeof(pthread_t) * (threads - 1));

	// creating threads-1 new threads
	cottonruntime::spawn(threads);
}

// Starting the finish scope
void cotton::start_finish()
{ // Setting the finish_counter to 0
	cottonruntime::lock(&finish_mutex);
	finish_counter = 0;
	cottonruntime::unlock(&finish_mutex);
}

// Ending the finish scope
void cotton::end_finish()
{ // while loop continues until finish counter > 0 i.e. until the taskpool is non-empty
	while (finish_counter != 0)
	{ // Thread executes a task by poping it from the taskpool
		cottonruntime::executeTask();
	}
}

// Destroying locks, condition variables, joining threads and waking up any awaiting threads
 void cotton::finalize_runtime()
{ // shutdown is set to 1 so that no more threads execute routine()
	shutdown = 1;
	pthread_cond_broadcast(&cond);

	cottonruntime::join(threads);

	// Destroying the mutex and cond variables
	pthread_mutex_destroy(&finish_mutex);
	pthread_mutex_destroy(&queue_mutex);
	pthread_cond_destroy(&cond);
}

// Asynchronous function to copy task to heap, getting pointer to that task and saving it to queue
void cotton::async(std::function<void()> &&lambda)
{
	cottonruntime::lock(&finish_mutex);
	finish_counter++;
	cottonruntime::unlock(&finish_mutex);

	// Allocation of memory in heap
	std::function<void()> *pointer = (std::function<void()> *)malloc(sizeof(lambda));
	memcpy(pointer, &lambda, sizeof(lambda));

	/*
	Circular FIFO Queue implemented using arrays with both push and pop operations in O(1) time

	*/
	// Adding task to taskpool ( push task to queue)

	// Use of mutex to avoid data races
	cottonruntime::lock(&queue_mutex);
	queue.push_task_to_runtime(pointer);
	pthread_cond_signal(&cond);
	cottonruntime::unlock(&queue_mutex);

}

// Helper Function to spawn n-1 threads
void cottonruntime::spawn(int n)
{
	for (int i = 0; i < n - 1; i++)
		pthread_create(thread_store + i, NULL, routine, NULL);
}

// Helper Function to join n-1 threads
void cottonruntime::join(int n)
{
	for (int i = 0; i < n - 1; i++)
		pthread_join(*(thread_store + i), NULL);
}

// Function to be run by each thread
void *cottonruntime::routine(void *)
{
	// Loop until shutdown is 1
	while (shutdown != 1)
	{
		// Use of mutex locks to avoid data race
		// pthread_mutex_lock(&queue_mutex);
		// while (queue.size == 0 && shutdown == 0)
		// {
		// 	pthread_cond_wait(&cond, &queue_mutex);
		// }
		// pthread_mutex_unlock(&queue_mutex);
		// Thread executes a task by poping it from the taskpool
		cottonruntime::executeTask();
	}

	return NULL;
}

// Helper function to lock a mutex
void cottonruntime::lock(pthread_mutex_t *m)
{
	pthread_mutex_lock(m);
}

// Helper function to unlock a mutex
void cottonruntime::unlock(pthread_mutex_t *m)
{
	pthread_mutex_unlock(m);
}

/*
	Function to execute task by a thread by taking it from the taskpool
*/
void cottonruntime::executeTask()
{
	std::function<void()> task;
	
	cottonruntime::lock(&queue_mutex);
	task=queue.pop_task_to_runtime();
	cottonruntime::unlock(&queue_mutex);	

	if (task == NULL)
		return;

	// Execution of task stored in task
	task();

	lock(&finish_mutex);
	finish_counter--;
	unlock(&finish_mutex);

}
