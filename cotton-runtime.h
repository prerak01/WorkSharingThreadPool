namespace cottonruntime{
	
	// creates n-1 threads
	void spawn(int n); 

	// joins n-1 threads 
	void join(int n);
	
	// function executed by each thread spawned
	void *routine(void*p);

	// excutes a task by popping it from the taskpool(queue) 
	void executeTask();

	// returns the private key of thread
	int getThread();
}