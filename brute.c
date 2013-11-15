#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#define __USE_GNU
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <crypt.h>
#include <signal.h>

#define TEST "aT12689"
#define MAXLENGTH 30
#define ALPHSTRING TEST
#define LENGTH 8
#define QUEUE_SIZE (8)
#define PORT 54321
#define SEND_STR_SIZE 300
#define SEND_JOB_TMPL "<msg>\n" \
"<type>MT_SEND_JOB</type>\n" \
"<args>\n" \
"<job>\n" \
"<job>\n" \
"<password>%s </password>\n" \
"<id>%d</id>\n" \
"<idx>%d</idx>\n" \
"<hash>%s </hash>\n" \
"<alphabet>%s </alphabet>\n" \
"<from>%d</from>\n" \
"<to>%d</to>\n" \
"</job>\n" \
"</job>\n" \
"</args>\n" \
"</msg>\n"
#define REPORT_RESULT_TMPL "<msg>\n" \
"<type>MT_REPORT_RESULTS</type>\n" \
"<args>\n" \
"<result>\n" \
"<result>\n" \
"<password>%s </passwdord>\n" \
"<id>%d</id>\n" \
"<idx>%d</idx>\n" \
"<password_found>%d</password_found>\n" \
"<mutex/>\n" \
"</result>\n" \
"</result>\n" \
"</args>\n" \
"</msg>\n"

typedef char result_t[MAXLENGTH];

typedef enum run_mode_t
  {
    RM_SINGLE,
    RM_MULTI,
    RM_SERVER,
    RM_CLIENT,
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
  int tail, head, size, cancel;
  pthread_mutex_t mutex_sem;
  pthread_cond_t full_sem, empty_sem;
} queue_t;

typedef struct context_t
{
  char *alph;
  char *hash;
  int alph_length;
  int length;
  int rev_count;
  int complete;
  result_t result;
  result_t password;
  enum run_mode_t run_mode;
  enum brute_mode_t brute_mode;
  queue_t queue;
  pthread_cond_t signal_sem;
  pthread_mutex_t mutex_sem;
} context_t;

typedef struct distributor_t
{
  struct sockaddr_in peer_addr;
  socklen_t peer_addr_size;
  int i32_socket_FD;
  context_t *context;
}distributor_t;

typedef struct client_info_t
{
  struct sockaddr_in st_client_addr;
  int i32_connect_FD;
  context_t *context;
  pthread_mutex_t peer_lock;
  pthread_cond_t peer_signal;
}client_info_t;

void queue_cancel(queue_t *queue)
{
  queue->cancel = !0;
  pthread_cond_broadcast(&queue->empty_sem);
  pthread_cond_broadcast(&queue->full_sem);
}

void rev(context_t *context)
{
  pthread_mutex_lock(&context->mutex_sem);
  context->rev_count++;
  pthread_mutex_unlock(&context->mutex_sem);
}

void unrev(context_t *context)
{
  pthread_mutex_lock(&context->mutex_sem);
  context->rev_count--;
  if (context->rev_count <= 0)
    pthread_cond_signal(&context->signal_sem);
  pthread_mutex_unlock(&context->mutex_sem);
}

int queue_push (task_t *task, queue_t * queue)
{
  pthread_mutex_lock (&queue->mutex_sem);
  printf("push undo -> %d\n", queue->size);
  while(queue->size == sizeof(queue->task) / sizeof(queue->task[0]) && queue->cancel == 0)
    {
      printf("push -> %d\n", queue->size);
      pthread_cond_wait(&queue->full_sem, &queue->mutex_sem);
    }
  if (queue->cancel != 0)
    {
      pthread_mutex_unlock(&queue->mutex_sem);
      pthread_cond_broadcast(&queue->empty_sem);
      return !0;
    }
  queue->task[queue->tail] = *task;
  queue->tail++;
  queue->size++;
  if (queue->tail == sizeof(queue->task) / sizeof(queue->task[0]))
    queue->tail = 0;
  pthread_cond_signal(&queue->empty_sem);
  pthread_mutex_unlock (&(queue->mutex_sem));
  return 0;
}

void queue_init (queue_t  * queue)
{
  queue->tail=0;
  queue->head=0;
  queue->size = 0;
  pthread_mutex_init(&queue->mutex_sem, NULL);
  pthread_cond_init(&queue->full_sem, NULL);
  pthread_cond_init(&queue->empty_sem, NULL);
}

int queue_pop (queue_t * queue, task_t * task)
{
  pthread_mutex_lock (&(queue->mutex_sem));
  while(queue->size == 0 && queue->cancel == 0)
    pthread_cond_wait(&queue->empty_sem, &queue->mutex_sem);
  if (queue->cancel != 0)
    {
      pthread_mutex_unlock(&queue->mutex_sem);
      pthread_cond_broadcast(&queue->full_sem);
      pthread_cond_broadcast(&queue->empty_sem);
      return !0;
    }
  *task = queue->task[queue->head];
  queue->head++;
  queue->size--;
  printf("hash -> %s\n", task->pass);
  printf("%d\n", queue->size);
  if (queue->head == sizeof(queue->task) / sizeof(queue->task[0]))
    queue->head=0;
  pthread_cond_signal(&queue->full_sem);
  pthread_mutex_unlock (&(queue->mutex_sem));
  return 0;
}

  void process_args(int argc,char **argv, context_t *context){
    for (;;)
      {
	int c = getopt (argc, argv, "rismhc");

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
	  case 'c':
	    context->run_mode = RM_CLIENT;
	    break;
	  case 'h':
	    context->run_mode = RM_SERVER;
	    break;
	  }
      }

    context->hash = argv[optind];
    printf("%s\n", context->hash);
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
  if (queue_push (task, &context->queue) == 0)
    rev(context);
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
      if (queue_pop (&context->queue, &task) != 0)
	pthread_exit(0);

      task.from = task.to;
      task.to = context->length;
      brute_iter (context, &task, &data_single, &equels_hash);
      unrev(context);
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
  
  pthread_mutex_lock(&context->mutex_sem);
  if(context->rev_count != 0)
    pthread_cond_wait(&context->signal_sem, &context->mutex_sem);
  pthread_mutex_unlock(&context->mutex_sem);
  queue_cancel(&context->queue);
}

void *thread_sender(void *arg)
{
  client_info_t *client_info = (client_info_t*) arg;
  pthread_cond_broadcast(&client_info->peer_signal);
  printf("enter sender\n");
  for(;;)
    {
      char str_in[SEND_STR_SIZE], str_out[SEND_STR_SIZE];
      task_t task;
      if (queue_pop(&client_info->context->queue, &task) != 0)
	{
	  pthread_exit(0);
	}
      //printf("take job\n");
      sprintf(str_out, SEND_JOB_TMPL, task.pass, 0,  0, client_info->context->hash, client_info->context->alph, task.to, client_info->context->length);
      printf("write job into str %s\n", str_out);
      uint32_t length = strlen (str_out);
      
      if (write(client_info->i32_connect_FD, &length, sizeof(uint32_t)) == -1)
	{
	  queue_push(&task, &client_info->context->queue);
	  pthread_exit(0);
	}
      //printf("send length\n");
      if (write(client_info->i32_connect_FD, str_out, length) == -1)
	{
	  queue_push(&task, &client_info->context->queue);
	  pthread_exit(0);
	}
      printf("send job %s\n", str_out);
      if (read (client_info->i32_connect_FD, &length, sizeof(uint32_t)) == -1)
	{
	  queue_push(&task, &client_info->context->queue);
	  pthread_exit(0);
	}
      //printf("get result length %d\n", length);
      if (read (client_info->i32_connect_FD, &str_in, length) == -1)
	{
	  queue_push(&task, &client_info->context->queue);
	  pthread_exit(0);
	}
      printf("get result str %s\n", str_in);
      int id, check_result;
      sscanf(str_in, REPORT_RESULT_TMPL, str_out, &id, &id, &check_result);
      //printf("result = %s\n", str_out);
      if (check_result != 0)
	{
	  printf("beda s checkom\n");
	  client_info->context->complete = !0;
	  queue_cancel(&client_info->context->queue);
	  memcpy(client_info->context->password, str_out, client_info->context->length);
	  unrev(client_info->context);
	  pthread_exit(0);
	}
      unrev(client_info->context);
      //printf("rev %d\n", client_info->context->rev_count);
    }
}

void *client_distributor(void *arg)
{
  distributor_t *distributor = (distributor_t*) arg;
  pthread_t thread_id;
  signal(SIGPIPE, SIG_IGN);

   for(;;) 
      {
 	memset (&distributor->peer_addr, 0, sizeof(distributor->peer_addr));
        int i32_connect_FD = accept (distributor->i32_socket_FD, (struct sockaddr*)&distributor->peer_addr, &distributor->peer_addr_size);
   
        if (i32_connect_FD < 0) 
	{
	  continue;
	  //printf("Error in enter desc");
	  //close(distributor->i32_socket_FD);
	  //exit( EXIT_FAILURE );
        }
	client_info_t client_info;
	pthread_mutex_init (&client_info.peer_lock, NULL);
	pthread_mutex_lock (&client_info.peer_lock);
	pthread_cond_init(&client_info.peer_signal, NULL);
	client_info.st_client_addr = distributor->peer_addr;
	client_info.context = distributor->context;
	client_info.i32_connect_FD = i32_connect_FD;
	pthread_create (&thread_id, NULL, &thread_sender, &client_info);
	pthread_cond_wait(&client_info.peer_signal, &client_info.peer_lock);
	pthread_mutex_unlock(&client_info.peer_lock);
    }
}

void brute_server(context_t *context, task_t *task)
{
  task->from = 0;
  task->to = context->length - 2;
  struct sockaddr_in st_sock_addr, peer_addr;
  socklen_t peer_addr_size = sizeof(struct sockaddr_in);
  pthread_t  thread_id;
  int i32_socket_FD = socket( PF_INET, SOCK_STREAM, IPPROTO_TCP );
  if ( i32_socket_FD == -1 ) 
    {
      printf("Error in creat desc");
      exit( EXIT_FAILURE );
    }
 
  memset( &st_sock_addr, 0, sizeof( st_sock_addr ) );
  
  st_sock_addr.sin_family = PF_INET;
  st_sock_addr.sin_port = htons (PORT);
  st_sock_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  
  if ( bind( i32_socket_FD, (struct sockaddr *)&st_sock_addr, sizeof( st_sock_addr ) ) == -1)
    {
      printf("Error in bind");
      close( i32_socket_FD );
      exit( EXIT_FAILURE );
    }
  
  if ( listen (i32_socket_FD, 10 ) == -1 ) 
    {
      printf("Error in listen");
      close (i32_socket_FD );
      exit (EXIT_FAILURE );
    }
  
  distributor_t distributor;
  distributor.peer_addr = peer_addr;
  distributor.peer_addr_size = peer_addr_size;
  distributor.i32_socket_FD = i32_socket_FD;
  distributor.context = context;
  pthread_create(&thread_id, NULL, &client_distributor, &distributor);
  struct crypt_data data_single;
  data_single.initialized = 0;
  brute_iter(context, task, &data_single, &queue_push_transform);
  
  pthread_mutex_lock(&context->mutex_sem);
  if(context->rev_count != 0 && context->complete == 0)
    pthread_cond_wait(&context->signal_sem, &context->mutex_sem);
  pthread_mutex_unlock(&context->mutex_sem);
  queue_cancel(&context->queue);
  shutdown(i32_socket_FD, SHUT_RDWR);
  close(i32_socket_FD);
}

int brute_client(context_t *context)
{
  struct crypt_data data_single;
  data_single.initialized = 0;
  struct sockaddr_in st_sock_addr;
  uint32_t length;
  int i32_socket_FD = socket( PF_INET, SOCK_STREAM, IPPROTO_TCP ), id;
  printf("call\n");
  if ( i32_socket_FD == -1 )
    {
      printf("Error in socket\n");
      return EXIT_FAILURE;
    }
  
  memset( &st_sock_addr, 0, sizeof( st_sock_addr ) );

  st_sock_addr.sin_family = PF_INET;
  st_sock_addr.sin_port = htons( 54321 );
  st_sock_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  
  if ( connect( i32_socket_FD, ( const void* )&st_sock_addr, sizeof(st_sock_addr)) == -1 )
    {
      printf("Can't connect to server\n");
      close( i32_socket_FD );
      return EXIT_FAILURE;
    }
 
  printf("client enter\n");
  
  for(;;)
    {
      task_t task;
      char hash[SEND_STR_SIZE], alph[SEND_STR_SIZE];
      context->complete = 0;
      //printf("ololo");
      char str_in[SEND_STR_SIZE], str_out[SEND_STR_SIZE];
      
      if (read(i32_socket_FD, &length, sizeof(length)) == -1)
	return EXIT_FAILURE;
      
      if (read(i32_socket_FD, &str_in, length) == -1)
	return EXIT_FAILURE;
      printf("length - %d\n", length);
      sscanf(str_in, SEND_JOB_TMPL, task.pass, &id, &id, hash, alph, &task.from, &task.to);
      context->length = strlen(task.pass);
      context->hash = hash;
      context->alph = alph;
      printf("%s\n", str_in);
      printf("%s %s %d\n", task.pass, context->alph, task.to);

      brute_iter(context, &task, &data_single, &equels_hash);
      
      sprintf (str_out, REPORT_RESULT_TMPL, context->password, 0, 0, context->complete);
      length = strlen (str_out);
      printf("%s\n", str_out);
      if (write (i32_socket_FD, &length, sizeof(length)) == -1)
	return EXIT_FAILURE;
      
      if (write (i32_socket_FD, str_out, length) == -1)
	return EXIT_FAILURE;
    }
}

void producer(context_t *context)
{
  task_t task;
  int i;

  for (i = 0; i < context->length; i++)
    task.pass[i] = context->alph[0];
  task.pass[context->length] = '\0';

  switch (context->run_mode)
    {
    case (RM_SINGLE):
      {
	brute_single (context, &task);
	break;
      }
    case (RM_MULTI):
      {
	brute_multi (context, &task);
	break;
      }
    case (RM_SERVER):
      {
	brute_server(context, &task);
	break;
      }
    case (RM_CLIENT):
      {
	brute_client(context);
	break;
      }
    default :
      {
	brute_multi(context, &task);
	break;
      }
    }
}

int main(int argc, char *argv[])
{
  context_t context = {
  .alph = ALPHSTRING,
  .complete = 0,
  .length = LENGTH,
  .rev_count = 0,
  };
  context.alph_length = strlen(context.alph);
  queue_init (&context.queue);
  pthread_mutex_init(&context.mutex_sem, NULL);
  pthread_cond_init(&context.signal_sem, NULL);
  process_args (argc, argv, &context);
  producer (&context);

  if (context.complete == 0)
    printf("password not found\n");
  else
    printf("pass %s\n",context.password);
  return 0;
}
