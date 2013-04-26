#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <crypt.h>
#include <sched.h>
#define _GNU_SOURCE
#define MAXLENGTH 30
#define ALPHSTRING "stic"
typedef char password_t[MAXLENGTH];

typedef enum run_mode_t{
  RM_REC,
  RM_ITER
};

typedef struct task_t{
  char hash[20];
  password_t pass;
} task_t; 

typedef struct context_t{
  char *alph;
  char *hash;
  int alphLength;
  int length;
  password_t result;
  enum run_mode_t run_mode;
} context_t;

typedef struct queue_t{
  task_t task[16];
  int tail;
  int head;
  pthread_mutex_t tail_mutex,head_mutex;
  sem_t full_sem,empty_sem;
  } queue_t;
queue_t queue;
password_t password;

void queue_push(queue_t *queue,task_t *task){
  sem_wait(&queue->empty_sem);
  pthread_mutex_lock(&queue->tail_mutex);
  queue->task[queue->tail]=*task;
  int i;
  for (i=queue->head;i<=queue->tail;i++)
  // printf("%s ",queue->task[i].pass);
  printf("%s ",task->pass);
  printf("\n");
  queue->tail++;
  if (queue->tail==sizeof(queue->task)/sizeof(queue->task[0]))
    queue->tail=0;
  pthread_mutex_unlock(&queue->tail_mutex);
  sem_post(&queue->full_sem);
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
  sem_wait(&queue->full_sem);
  pthread_mutex_lock(&queue->head_mutex);
  task=queue->task[queue->head];
  if (task.pass==NULL){
      queue_push(queue,&task);
      pthread_exit(0);
    }
  queue->head++;
  if (queue->head==sizeof(queue->task)/sizeof(queue->task[0]))
    queue->head=0;
  pthread_mutex_unlock(&queue->head_mutex);
  sem_post(&queue->empty_sem);
  return task;
}

enum run_mode_t process_args(int argc,char **argv){
  int c=getopt(argc,argv,"r:i:");
  switch (c){
  case 'r': return RM_REC;
  case 'i': return RM_ITER;
  }
}

int equelsHash(task_t *task){ 
  //if (strcmp(task->hash, crypt(task->pass,task->hash))==0)
    return 1;
    // else return 0;
    }

void brute_rec(context_t *context, int count){
  if (count<context->length){
    int i;
    for(i=0;i<context->alphLength;i++){
      context->result[count]=context->alph[i];
      if (count==context->length-1 && equelsHash(context))
	printf("%s\n",context->result);
      else
	brute_rec(context,count+1);
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
        task_t *task=(task_t*)malloc(sizeof(task_t));
	if (password!=NULL){
	 queue_push(&queue,task);
	 break;
        }
     	context->result[0]=context->alph[countMassive[0]];
      	countMassive[0]++;
       	context->result[context->length]='\0';
	printf("%s\n",context->result);
        memcpy(task->pass,context->result,context->length);
       //printf("%s\n",task->hash);
       queue_push(&queue,task);
     }
  }
}

void *thread_consumer(void *arg){
  context_t *context=(context_t*) arg;
  while(1){
    task_t *task;
    *task=queue_pop(&queue);
    if(equelsHash(&task)){
      strncpy(password,&task->pass,&context->length);
      }
    free(task);
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
  switch(context->run_mode){
  case RM_REC:
    brute_rec(context,0);
    break;
  case RM_ITER:
    brute_iter(context,&countMassive);
    break;
  }
  pthread_exit(0);
}

int main(int argc, char *argv[]){
  context_t context;
  context.alph=ALPHSTRING;
  context.alphLength=strlen(context.alph);
  context.length=4;
  int i,count,nProcess,threadIdProd,countMassive[MAXLENGTH],threadIdCons[5],check=1;
  nProcess=sysconf(_SC_NPROCESSORS_CONF);
  queue_init(&queue);
  context.run_mode=process_args(argc,argv);
  extern char *optarg;
  context.hash=optarg;
  pthread_create(&threadIdProd,NULL,&thread_produsser,&context);
  for (i=0;i<nProcess;i++)
    pthread_create(&threadIdCons[i],NULL,&thread_consumer,&context);
  pthread_join(threadIdProd,NULL);
   for (i=0;i<nProcess;i++)
   pthread_cancel(&threadIdCons[i]);
  return 0;
}

