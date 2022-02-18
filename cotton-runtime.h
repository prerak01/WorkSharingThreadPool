namespace cottonruntime{

	void spawn(int i); 
	// creates a thread using ith element from array thread_store
	void *routine(void*);
	// passed as function to pthread_create function
	void lock(pthread_mutex_t*m);
	void unlock(pthread_mutex_t*m);
	void executeTask();
}