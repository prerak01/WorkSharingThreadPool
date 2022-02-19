namespace cottonruntime{
	
	// creates n-1 threads
	void spawn(int n); 

	// joins n-1 threads 
	void join(int n);
	
	// function executed by each thread spawned
	void *routine(void*);

	// locks the mutex
	void lock(pthread_mutex_t*m);
	
	// unlocks the mutex
	void unlock(pthread_mutex_t*m);

	// excutes a task by popping it from the taskpool(queue) 
	void executeTask();
}