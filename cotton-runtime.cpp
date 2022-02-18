#include "cotton.h"
#include "cotton-runtime.h"
#include<pthread.h>
#include<unistd.h>

const int QUEUE_SIZE=999999;
#define debug

/* 
default number of threads is 2
*/
volatile int shutdown=0;
pthread_mutex_t finish_mutex,queue_mutex;
int threads;
pthread_t *thread_store;
pthread_cond_t cond;
std::function<void()> *queue[QUEUE_SIZE];
int size,start,end;
volatile int finish_counter;


void cotton::init_runtime(){
	pthread_mutex_init(&queue_mutex,NULL);
	pthread_mutex_init(&finish_mutex,NULL);
	pthread_cond_init(&cond,NULL);
	//pthread_mutex_init(&cond_mutex,NULL);
	if(	getenv("COTTON_WORKERS")!=NULL)
		threads=atoi(getenv("COTTON_WORKERS"));
	else
		threads=2;
	printf("total threads %d\n",threads);
	size=0;
	start=0;
	end=0;
	// storing pthread_t for 1 less (excluding main thread)
	thread_store=(pthread_t*)malloc(sizeof(pthread_t)*(threads-1));
	

	// creating threads-1 new threads
	cottonruntime::spawn(threads);
	
	

}

void cotton::start_finish(){
	cottonruntime::lock(&finish_mutex);
	finish_counter=0;
	cottonruntime::unlock(&finish_mutex);
}
void cotton::end_finish(){
	while(finish_counter!=0){
		
		cottonruntime::executeTask();
		
		
	}

}
void cotton::finalize_runtime(){
	
	
	shutdown=1;
	//joining
	pthread_cond_broadcast(&cond);
	for(int i=0;i<threads-1;i++)
		pthread_join(*(thread_store+i),NULL);
	
	pthread_mutex_destroy(&finish_mutex);
	pthread_mutex_destroy(&queue_mutex);
	pthread_cond_destroy(&cond);
	//pthread_mutex_destroy(&cond_mutex);
}

void cotton::async(std::function<void()> &&lambda){
	cottonruntime::lock(&finish_mutex);
	finish_counter++;
	cottonruntime::unlock(&finish_mutex);

	std::function<void()> *pointer=(std::function<void()>*)malloc(sizeof(lambda));
	memcpy(pointer,&lambda,sizeof(lambda));

	cottonruntime::lock(&queue_mutex);
	if(size==QUEUE_SIZE){
		perror("queue overflow exiting");
		exit(-1);
	}
	//printf("increment\n");
	size++;
	queue[end]=pointer;
	end++;
	end%=QUEUE_SIZE;
	pthread_cond_signal(&cond);
	cottonruntime::unlock(&queue_mutex);

	

	
	

}


void cottonruntime::spawn(int n){
	
	for(int i=0;i<n-1;i++)
		pthread_create(thread_store+i,NULL,routine,NULL);
}

void *cottonruntime::routine(void *){

	while(shutdown!=1){
		pthread_mutex_lock(&queue_mutex);
		while(size==0 && shutdown==0)
			pthread_cond_wait(&cond,&queue_mutex);
		pthread_mutex_unlock(&queue_mutex);
		cottonruntime::executeTask();
		
		
	}
	

		

	return NULL;
}
void cottonruntime::lock(pthread_mutex_t *m){
	pthread_mutex_lock(m);
}
void cottonruntime::unlock(pthread_mutex_t*m){
	pthread_mutex_unlock(m);
}
void cottonruntime::executeTask(){
	std::function<void()> *p;
	std::function<void()> temp;

	lock(&queue_mutex);
	if(size==0){
		temp=NULL;
	}
	else{
		temp=*queue[start];
		p=queue[start];

		start++;
		size--;
		start%=QUEUE_SIZE;
	}
	
	unlock(&queue_mutex);

	
	if(temp==NULL)
		return;

	lock(&finish_mutex);
	//printf("decrement\n");
	finish_counter--;
	unlock(&finish_mutex);


	temp();
	free(p);

	


}



