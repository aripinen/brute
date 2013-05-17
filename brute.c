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
#define ALPHSTRING "csit"
typedef char result_t[MAXLENGTH];

typedef enum run_mode_t{
	RM_SINGLE,
	RM_MULTI
};

typedef enum brute_mode_t{
	BM_ITER,
	BM_REC
};

typedef struct task_t{
	char hash[20];
	result_t pass;
} task_t;

typedef struct context_t{
	char *alph;
	char *hash;
	int alphLength;
	int length;
	result_t result;
	enum run_mode_t run_mode;
	enum brute_mode_t brute_mode;
} context_t;

typedef struct queue_t{
	task_t task[16];
	int tail;
	int head;
	pthread_mutex_t tail_mutex,head_mutex;
	sem_t full_sem,empty_sem;
} queue_t;
queue_t queue;
result_t password;
struct crypt_data data_single;

void queue_push(queue_t *queue,task_t *task){
	sem_wait(&(queue->empty_sem));
	pthread_mutex_lock(&(queue->tail_mutex));
	queue->task[queue->tail]=*task;
	int i;
	queue->tail++;
	if (queue->tail==sizeof(queue->task)/sizeof(queue->task[0]))
		queue->tail=0;
	pthread_mutex_unlock(&(queue->tail_mutex));
	sem_post(&(queue->full_sem));
}

void queue_init(queue_t *queue){
	queue->tail=0;
	queue->head=0;
	pthread_mutex_init(&queue->tail_mutex,NULL);
	pthread_mutex_init(&queue->head_mutex,NULL);
	sem_init(&queue->full_sem,0,0);
	sem_init(&queue->empty_sem,0,sizeof(queue->task)/sizeof(queue->task[0]));
}

task_t queue_pop(queue_t *queue){
	task_t task;
	sem_wait(&(queue->full_sem));
	pthread_mutex_lock(&(queue->head_mutex));
	task=queue->task[queue->head];
	if (strcmp(task.pass,"")==0){
		queue_push(queue,&task);
	pthread_exit(0);
	}
	queue->head++;
	if (queue->head==sizeof(queue->task)/sizeof(queue->task[0]))
		queue->head=0;
	pthread_mutex_unlock(&(queue->head_mutex));
	sem_post(&(queue->empty_sem));
	return task;
}

void process_args(int argc,char **argv,context_t *context){
	int c=getopt(argc,argv,"rism");
	while(c!=-1){
		switch (c){
			case 'r': context->brute_mode =BM_REC;break;
			case 'i': context->brute_mode = BM_ITER;break;
			case 's': context->run_mode = RM_SINGLE;break;
			case 'm': context->run_mode = RM_MULTI; break;
  		}
		c=getopt(argc,argv,"rism");
	}
	context->hash=argv[optind];
}

int equelsHash(result_t *result, char *hash, struct crypt_data *data){
	if (strcmp(hash, crypt_r(result,hash,data))==0)
		return 1;
	else return 0;
}

int check_run_mode(context_t *context){
	switch (context->run_mode){
		case RM_SINGLE: return 0;
		case RM_MULTI: return 1;
	}
}

void brute_rec(context_t *context, int count){
	if (count<context->length){
		int i;
		for(i=0;i<context->alphLength;i++){
			context->result[count]=context->alph[i];
			if (count==context->length-1){
				if (check_run_mode(context)==1){
					task_t *task=(task_t*)malloc(sizeof(task_t));
					if (strcmp(password,"")!=0){
						queue_push(&queue,task);
						pthread_exit(0);
					}
					memcpy(task->pass,context->result,context->length);
					memcpy(task->hash,context->hash,20);
					queue_push(&queue,task);
				}
				else{
					if (equelsHash(context->result,context->hash,&data_single)){
						memcpy(password,context->result,context->length);
						return;
					}
				}
			}	
			else brute_rec(context,count+1);
		}
  	}
}

void brute_iter(context_t *context, int *countMassive){
	int i;
	for(;;){
		if (countMassive[0]>=context->alphLength){
			i=0;
			while (countMassive[i]>=context->alphLength-1){
				if (i>context->length-2) return;
					countMassive[i]=0;
				context->result[i]=context->alph[0];
				i++;
			}
			countMassive[i]++;
			context->result[i]=context->alph[countMassive[i]];
		}
		else{
			context->result[0]=context->alph[countMassive[0]];
			countMassive[0]++;
			context->result[context->length]='\0';
			if (check_run_mode(context)==1){
				task_t *task=(task_t*)malloc(sizeof(task_t));
				if (strcmp(password,"")!=0){
					queue_push(&queue,task);
					break;
				}
				memcpy(task->pass,context->result,context->length);
				memcpy(task->hash,context->hash,20);
				queue_push(&queue,task);
			}
			else{
				if (equelsHash(context->result,context->hash,&data_single)){
					memcpy(password,context->result,context->length);
					break;
				}
			}	
		}
	}
}

void *thread_consumer(void *arg){
	context_t *context=(context_t*) arg;
	struct crypt_data data;
	data.initialized=0;
	int i;
	while(1){
		task_t task[5];
		for (i=0;i<5;i++)
			task[i]=queue_pop(&queue);
		for (i=0;i<5;i++)
			if(equelsHash(&task[i].pass,&task[i].hash,&data)){
				memcpy(password,task[i].pass,context->length);
			}
	}
	pthread_exit(0);
}

void *thread_produsser(void *arg){
	context_t *context=(context_t*) arg;
	int j,countMassive[MAXLENGTH];
	for (j=0;j<context->length;j++){
		context->result[j]=context->alph[0];
		countMassive[j]=0;
	}
	context->result[context->length]='\0';
	switch(context->brute_mode){
		case BM_REC:
			brute_rec(context,0);
			break;
		case BM_ITER:
			brute_iter(context,&countMassive);
			break;
	}
	if (strcmp(password,"")==0){
		task_t *task=(task_t*)malloc(sizeof(task_t));
		queue_push(&queue,task);
	}
	pthread_exit(0);
}

int main(int argc, char *argv[]){
	data_single.initialized=0;
	context_t context;
	context.alph=ALPHSTRING;
	context.alphLength=strlen(context.alph);
	context.length=4;
	int i,count,nProcess,threadIdProd,countMassive[MAXLENGTH],threadIdCons[5],check=1;
	nProcess=sysconf(_SC_NPROCESSORS_CONF);
	queue_init(&queue);
	process_args(argc,argv,&context);
	if (check_run_mode(&context)==1){
		pthread_create(&threadIdProd,NULL,&thread_produsser,&context);
		for (i=0;i<nProcess;i++)
			pthread_create(&threadIdCons[i],NULL,&thread_consumer,&context);
		pthread_join(threadIdProd,NULL);
		for (i=0;i<nProcess;i++)
			pthread_join(&threadIdCons[i],NULL);
	}
	else{
		int j,countMassive[MAXLENGTH];
		for (j=0;j<context.length;j++){
			context.result[j]=context.alph[0];
			countMassive[j]=0;
		}
		switch(context.brute_mode){
			case BM_REC:
				brute_rec(&context,0);
				break;
			case BM_ITER:
				brute_iter(&context,&countMassive);
				break;
		}
	}
	if (strcmp(password,"")==0)
		printf("password not found\n");
	else
  		printf("pass %s\n",password);
	return 0;
}
