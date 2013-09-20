#include <stdio.h>
#include <stdlib.h>
#define __USE_GNU
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <crypt.h>
#include <sched.h>
#include <sched.h>
#define MAXLENGTH 30
#define ALPHSTRING "12345678"
#define LENGTH 8
typedef char result_t[MAXLENGTH];

typedef enum run_mode_t{
	RM_SINGLE,
	RM_MULTI
} run_mode_t;

typedef enum brute_mode_t{
	BM_ITER,
	BM_REC
} brute_mode_t;

typedef struct task_t{
	result_t pass;
	int from;
	int to;
} task_t;

typedef struct queue_t{
	task_t task[16];
	int tail;
	int head;
	pthread_mutex_t tail_mutex,head_mutex;
	sem_t full_sem,empty_sem;
} queue_t;

typedef struct context_t{
	char *alph;
	char *hash;
	int alphLength;
	int length;
	int countMassive[MAXLENGTH];
	int complete;
	result_t result;
	enum run_mode_t run_mode;
	enum brute_mode_t brute_mode;
	queue_t queue;
	result_t password;
	struct crypt_data data_single;
} context_t;


int queue_push(task_t *task, context_t *context){
	sem_wait(&(context->queue.empty_sem));
	pthread_mutex_lock(&(context->queue.tail_mutex));
	context->queue.task[context->queue.tail]=*task;
	context->queue.tail++;
	if (context->queue.tail == sizeof(context->queue.task) / sizeof(context->queue.task[0]))
		context->queue.tail=0;
	pthread_mutex_unlock(&(context->queue.tail_mutex));
	sem_post(&(context->queue.full_sem));
	return 0;
}

void queue_init(context_t *context){
	context->queue.tail=0;
	context->queue.head=0;
	pthread_mutex_init(&context->queue.tail_mutex,NULL);
	pthread_mutex_init(&context->queue.head_mutex,NULL);
	sem_init(&context->queue.full_sem, 0, 0);
	sem_init(&context->queue.empty_sem, 0, sizeof(context->queue.task) / sizeof(context->queue.task[0]));
}

task_t queue_pop(context_t *context){
	task_t task;
	sem_wait(&(context->queue.full_sem));
	pthread_mutex_lock(&(context->queue.head_mutex));
	task = context->queue.task[context->queue.head];
	if (strcmp(task.pass, "") == 0){
		queue_push(&task, context);
		pthread_mutex_unlock(&(context->queue.head_mutex));
		sem_post(&(context->queue.empty_sem));
		pthread_exit(0);
	}
	context->queue.head++;
	if (context->queue.head == sizeof(context->queue.task) / sizeof(context->queue.task[0]))
		context->queue.head=0;
	pthread_mutex_unlock(&(context->queue.head_mutex));
	sem_post(&(context->queue.empty_sem));
	return task;
}

void process_args(int argc,char **argv, context_t *context){
	int c=getopt(argc, argv, "rism");
	while(c != -1){
		switch (c){
			case 'r': context->brute_mode = BM_REC;break;
			case 'i': context->brute_mode = BM_ITER;break;
			case 's': context->run_mode = RM_SINGLE;break;
			case 'm': context->run_mode = RM_MULTI; break;
  		}
		c=getopt(argc, argv, "rism");
	}
	context->hash = argv[optind];
}

int equelsHash(task_t *task, context_t *context){
	if (strcmp(context->hash, crypt_r(task->pass, context->hash, &context->data_single))==0)
	{
		context->complete = 1;
		return 1;
	}
	else return 0;
}

int check_run_mode(context_t *context){
	switch (context->run_mode){
		case RM_SINGLE: return 0;
		case RM_MULTI: return 1;
		default : return 0;
	};
}

void brute_rec(context_t *context, task_t *task, int count, int (*prob)(struct task_t*, struct context_t*)){
	if ((count < context->length) && (context->complete == 0)){
		int i;
		for(i = 0; i < context->alphLength; i++){
			task->pass[count] = context->alph[i];
			if (count == task->to - 1)
			{
				if ((int)prob(task, context) == 1) 
				{
					memcpy(context->password, task->pass, context->length);
					break;
				}
			}	
			else brute_rec(context, task, count + 1, prob);
		}
  	}
}

void brute_iter(context_t *context, task_t *task, int (*prob)(struct task_t*, struct context_t*)){
	int i;
	for(i = task->from; i < task->to; i++)
		context->countMassive[i] = 0;
	for(;;){
		if (context->countMassive[task->to - 1] >= context->alphLength){
			i = task->to-1;
			while (context->countMassive[i] >= context->alphLength - 1){
				if (i == task->from) return;
				context->countMassive[i] = 0;
				task->pass[i] = context->alph[0];
				i--;
			}
			context->countMassive[i]++;
			task->pass[i] = context->alph[context->countMassive[i]];
		}
		else{
			task->pass[task->to - 1] = context->alph[context->countMassive[task->to - 1]];
			context->countMassive[task->to - 1]++;
			if ((int) prob(task, context) == 1) 
			{
				memcpy(context->password, task->pass, context->length+1);
				return;
			}
			if (context->complete == 1) return;
		}
	}
}

void *thread_consumer(void *arg){
	context_t *context=(context_t*) arg;
	task_t task;
	while(1){
		task=queue_pop(context);
		task.from = task.to;
		task.to = context->length;
		brute_iter(context, &task, &equelsHash);
	}	
	pthread_exit(0);
}

void thread_creater(context_t *context, pthread_t threadIdCons[])
{
	int i;
	for(i = 0; i < sizeof(*threadIdCons) / sizeof(int); i++)
		pthread_create(&threadIdCons[i], NULL, &thread_consumer, context);
}

void thread_closer (context_t *context,  pthread_t threadIdCons[])
{
	int i;
	task_t *final_task = (task_t*)malloc(sizeof(task_t));
	memcpy(final_task->pass, "", 1);
	queue_push(final_task, context);
	for (i = 0;i < sizeof(*threadIdCons) / sizeof(int);i++)
		pthread_join(threadIdCons[i], NULL);
}

void producer(context_t *context)
{
	task_t task;
	task.from = 0;
	int j, nProcess = sysconf(_SC_NPROCESSORS_CONF);
	pthread_t threadIdCons[nProcess];
	for (j = 0; j < context->length; j++){
		context->result[j] = context->alph[0];
		context->countMassive[j] = 0;
	}
	context->result[context->length] = '\0';
	memcpy(task.pass, context->result, context->length+1);
	if (check_run_mode(context) == 1)
	{
		task.to = context->length - 2;
		thread_creater(context, threadIdCons);
	}
	else task.to = context->length;
	switch(context->brute_mode){
		case BM_REC:
			if (check_run_mode(context) == 1)
			{
				brute_rec(context, &task, 0, &queue_push);
				thread_closer(context, threadIdCons);
			}
			else 
				brute_rec(context, &task, 0, &equelsHash);
			break;
		case BM_ITER:
			if (check_run_mode(context) == 1)
			{
				brute_iter(context, &task, &queue_push);
				thread_closer(context, threadIdCons);
			}
			else 
				brute_iter(context, &task, &equelsHash);
			break;
	}
}

int main(int argc, char *argv[]){
	context_t context;
	context.data_single.initialized = 0;
	context.alph = ALPHSTRING;
	context.alphLength = strlen(context.alph);
	context.length = LENGTH;
	context.complete = 0;	
	queue_init(&context);
	process_args(argc, argv, &context);
	producer(&context);
	if (context.complete == 0)
		printf("password not found\n");
	else
  		printf("pass %s\n",context.password);
	return 0;
}
