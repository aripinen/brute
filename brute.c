#include <stdio.h>
#include <stdlib.h>
#define __USE_GNU
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <crypt.h>

#define TEST "qwertyuiopasdfghjklzxcvbnm"
#define MAXLENGTH 30
#define ALPHSTRING TEST
#define LENGTH 8
#define QUEUE_SIZE (8)

typedef char result_t[MAXLENGTH];

typedef enum run_mode_t
  {
    RM_SINGLE,
    RM_MULTI
  } run_mode_t;

typedef enum brute_mode_t
  {
    BM_ITER,
    BM_REC
  } brute_mode_t;

typedef struct task_t
{
  result_t pass;
  int from;
  int to;
} task_t;

typedef struct queue_t
{
  task_t task[QUEUE_SIZE];
  int tail;
  int head;
  pthread_mutex_t tail_mutex,head_mutex;
  sem_t full_sem,empty_sem;
} queue_t;

typedef struct context_t
{
  char *alph;
  char *hash;
  int alph_length;
  int length;
  int complete;
  result_t result;
  result_t password;
  enum run_mode_t run_mode;
  enum brute_mode_t brute_mode;
  queue_t queue;
} context_t;


void queue_push (task_t *task, queue_t * queue, struct crypt_data * cd)
{
  sem_wait (&(queue->empty_sem));
  pthread_mutex_lock (&(queue->tail_mutex));
  queue->task[queue->tail] = *task;
  queue->tail++;

  if (queue->tail == sizeof(queue->task) / sizeof(queue->task[0]))
    queue->tail = 0;

  pthread_mutex_unlock (&(queue->tail_mutex));
  sem_post (&(queue->full_sem));
}

void queue_init (queue_t  * queue)
{
  queue->tail=0;
  queue->head=0;
  pthread_mutex_init (&queue->tail_mutex,NULL);
  pthread_mutex_init (&queue->head_mutex,NULL);
  sem_init (&queue->full_sem, 0, 0);
  sem_init (&queue->empty_sem, 0, sizeof(queue->task) / sizeof(queue->task[0]));
}

void queue_pop (queue_t * queue, task_t * task)
{
  sem_wait (&(queue->full_sem));
  pthread_mutex_lock (&(queue->head_mutex));
  *task = queue->task[queue->head];
  queue->head++;

  if (queue->head == sizeof(queue->task) / sizeof(queue->task[0]))
    queue->head=0;

  pthread_mutex_unlock (&(queue->head_mutex));
  sem_post (&(queue->empty_sem));
}

  void process_args(int argc,char **argv, context_t *context){
    for (;;)
      {
	int c = getopt (argc, argv, "rism");

	if (c == -1)
	  break;

	switch (c)
	  {
	  case 'r': 
	    context->brute_mode = BM_REC;
	    break;
	  case 'i': 
	    context->brute_mode = BM_ITER;
	    break;
	  case 's': 
	    context->run_mode = RM_SINGLE;
	    break;
	  case 'm': 
	    context->run_mode = RM_MULTI; 
	    break;
	  }
      }

    context->hash = argv[optind];
  }

 int equels_hash (task_t *task, context_t *context, struct crypt_data * data_single)
 {
   if (strcmp (context->hash, crypt_r(task->pass, context->hash, data_single)) == 0)
     {
       memcpy (context->password, task->pass, context->length);
       context->complete = !0;
       return !0;
     }
   else 
     return 0;
}

int queue_push_transform(task_t *task, context_t *context, struct crypt_data * data_single)
{
  queue_push (task, &context->queue, data_single);
  return context->complete;
}

int brute_rec (context_t *context, task_t *task, int count, struct crypt_data * data_single,
	       int (*prob)(struct task_t*, struct context_t*, struct crypt_data*))
{
  if (count >= task->to)
    {
      if (prob (task, context, data_single)) 
	return (!0);
    }	
  else
    {
      int i;

      for(i = 0; i < context->alph_length; i++)
	{
	  task->pass[count] = context->alph[i];

	  if (brute_rec (context, task, count + 1, data_single, prob))
	    return (!0);
	}
    }
  return (0);
}

void brute_iter(context_t *context, task_t *task, struct crypt_data * data_single,
		int (*prob)(struct task_t*, struct context_t*, struct crypt_data*))
{
  int i;
  int count_massive[MAXLENGTH];

  for (i = task->from; i < task->to; i++)
    count_massive[i] = 0;

  for (;;) {
    if (prob (task, context, data_single))
      break;

    for (i = task->to - 1; (i >= task->from) && (count_massive[i] >= context->alph_length -1); --i)
      {
	count_massive[i] = 0;
	task->pass[i] = context->alph[0];
      }

    if (i < task->from)
      break;

    task->pass[i] = context->alph[++count_massive[i]];
  }
}

void *thread_consumer(void *arg)
{
  struct crypt_data data_single;
  data_single.initialized = 0;
  context_t *context=(context_t*) arg;
  task_t task;

  while(1)
    {
      queue_pop (&context->queue, &task);

      if (strcmp (task.pass, "") == 0)
       {
	 queue_push (&task, &context->queue, &data_single);
	 return 0;
       }

      task.from = task.to;
      task.to = context->length;
      brute_iter (context, &task, &data_single, &equels_hash);
    } 
}

void brute_single(context_t *context, task_t *task)
{
  task->from = 0;
  task->to = context->length;
  struct crypt_data data_single;
  data_single.initialized = 0;
  
  if (context->brute_mode == BM_REC)
    brute_rec (context, task, 0, &data_single, &equels_hash);
  else
    brute_iter (context, task, &data_single, &equels_hash);
}

void brute_multi(context_t *context, task_t *task)
{
  task->from = 0;
  task->to = context->length - 2;
  struct crypt_data data_single;
  data_single.initialized = 0;
  int i, nProcess = sysconf(_SC_NPROCESSORS_CONF);
  pthread_t threadIdCons[nProcess];

  for(i = 0; i < nProcess; i++)
    pthread_create (&threadIdCons[i], NULL, &thread_consumer, context);

  if (context->brute_mode == BM_REC)
    brute_rec (context, task, 0, &data_single, &queue_push_transform);
  else
    brute_iter (context, task, &data_single, &queue_push_transform);

  task_t final_task;
  strcpy (final_task.pass, "");
  final_task.from = context->length + 1;
  queue_push (&final_task, &context->queue, &data_single);
  
  for (i = 0; i < nProcess; i++)
    pthread_join (threadIdCons[i], NULL);
}

void producer(context_t *context)
{
  task_t task;
  int i;

  for (i = 0; i < context->length; i++)
    task.pass[i] = context->alph[0];
  task.pass[context->length] = '\0';

  if (context->run_mode == RM_SINGLE)
    brute_single (context, &task);
  else 
    brute_multi (context, &task);
}

int main(int argc, char *argv[])
{
  context_t context = {
  .alph = ALPHSTRING,
  .complete = 0,
  .length = 4,
  };
  context.alph_length = strlen(context.alph);
  queue_init (&context.queue);
  process_args (argc, argv, &context);
  producer (&context);

  if (context.complete == 0)
    printf("password not found\n");
  else
    printf("pass %s\n",context.password);
  return 0;
}
