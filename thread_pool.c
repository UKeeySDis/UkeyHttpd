/*************************************************************************
    > File Name: thread_pool.c
    > Author: Ukey
    > Mail: gsl110809@gmail.com
    > Created Time: 2017年06月08日 星期四 15时37分50秒
 ************************************************************************/

#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <stdlib.h>
#include "thread_pool.h"

#define DEFAULT_TIME	10	//10s检测一次
#define MIN_WAIT_TASK_NUM	10	//如果queue_size > MIN_WAIT_TASK_NUM 则添加新线程到线程池中
#define DEFAULT_THREAD_VARY	10	//每次创建和销毁线程的个数
#define TRUE	1
#define FALSE	0

typedef struct 
{
	void *(*func)(void *);	//函数指针,用于回调函数
	void *arg;				//函数指针的参数
}threadpool_task_t;

struct threadpool_t
{
	pthread_mutex_t struct_lock;	//该锁用于锁本结构体
	pthread_mutex_t thread_counter;	//记录忙状态线程个数的锁
	pthread_cond_t queue_not_full;	///当任务队列满时,让添加任务的线程阻塞
	pthread_cond_t queue_not_empty;	//任务队列里不为空时,通知等待任务的线程
	pthread_t *threads;				//存放线程池中每个线程的线程id
	pthread_t adjust_tid;		//存放管理线程的tid
	threadpool_task_t *task_queue;	//任务队列

	int min_thread_num;				//线程池中最小线程数
	int max_thread_num;				//线程池中最大线程数
	int live_thread_num;			//线程池中存活的线程数
	int busy_thread_num;			//线程池中处于忙碌状态的线程数
	int wait_exit_thread_num;		//线程池中即将销毁的线程数

	int queue_front;				//任务队列的队首下标
	int queue_rear;					//任务队列的队尾下标
	int queue_size;					//任务队列中实际的任务数
	int queue_max_size;				//任务队列的最大可容纳任务数

	int shutdown;					//线程池的使用状态,为true则可用,为false则不可用
};

//工作线程的函数
static void *threadpool_thread(void *threadpool);

//管理线程的函数
static void *adjust_thread(void *threadpool);

//检测一个线程是否存活
static int is_thread_alive(pthread_t tid);
static int threadpool_free(threadpool_t *pool);

//获取线程池中的线程数
//static int threadpool_total_num(threadpool_t *pool);

//获取线程池中忙的线程数
//static int threadpool_busy_num(threadpool_t *pool);


threadpool_t *threadpool_create(int min_thread_num, int max_thread_num, int queue_max_size)
{
	int i;
	threadpool_t *pool = NULL;
	do
	{
		if((pool = (threadpool_t *)malloc(sizeof(threadpool_t))) == NULL)
		{
			fprintf(stderr, "malloc threadpool failed\n");
			break;
		}
		pool->min_thread_num = min_thread_num;
		pool->max_thread_num = max_thread_num;
		pool->busy_thread_num = 0;
		pool->live_thread_num = 0;
		pool->queue_size = 0;
		pool->queue_max_size = queue_max_size;
		pool->queue_front = 0;
		pool->queue_rear = 0;
		pool->shutdown = FALSE;

		//给工作线程数组申请空间
		pool->threads = (pthread_t *)malloc(sizeof(pthread_t) * max_thread_num);
		if(pool->threads == NULL)
		{
			fprintf(stderr, "malloc threads fail\n");
			break;
		}
		memset(pool->threads, 0, sizeof(pthread_t) * max_thread_num);

		//给任务队列申请空间
		pool->task_queue = (threadpool_task_t *)malloc(sizeof(threadpool_task_t) * queue_max_size);
		if(pool->task_queue == NULL)
		{
			fprintf(stderr, "malloc task_queue fail");
			break;
		}

		//初始化互斥锁和条件变量
		if(pthread_mutex_init(&(pool->struct_lock), NULL) != 0 || pthread_mutex_init(&(pool->thread_counter), NULL) != 0 || pthread_cond_init(&(pool->queue_not_empty), NULL) != 0 || pthread_cond_init(&(pool->queue_not_full), NULL) != 0)
		{
			fprintf(stderr, "init lock/cond failed");
			break;
		}
		//启动min_thread_num个工作线程
		for(i = 0; i < min_thread_num; i++)
		{
			pthread_create(&(pool->threads[i]), NULL, threadpool_thread, (void *)pool);
		//	printf("start thread 0x%x\n", (unsigned int)pool->threads[i]);
		}
		pthread_create(&(pool->adjust_tid), NULL, adjust_thread, (void *)pool);
		return pool;
	}while(0);

	//前面的代码调用失败,释放pool存储空间
	threadpool_free(pool);
	return NULL;
}

//向线程池中添加一个任务
int threadpool_add(threadpool_t *pool, void *(*func)(void *arg), void *arg)
{
	pthread_mutex_lock(&(pool->struct_lock));

	//如果任务队列已满,则阻塞等待
	while((pool->queue_size == pool->queue_max_size) && (!pool->shutdown))
	{
		pthread_cond_wait(&(pool->queue_not_full), &(pool->struct_lock));
	}
	if(pool->shutdown)
	{
		pthread_mutex_unlock(&(pool->struct_lock));
	}
	//清空工作线程调用的回调函数的参数
	if(pool->task_queue[pool->queue_rear].arg != NULL)
	{
		free(pool->task_queue[pool->queue_rear].arg);
		pool->task_queue[pool->queue_rear].arg = NULL;
	}
	//添加任务到任务队列里
	pool->task_queue[pool->queue_rear].func = func;
	pool->task_queue[pool->queue_rear].arg = arg;
	pool->queue_rear = (pool->queue_rear + 1) % pool->queue_max_size;	//队尾指针移动,模拟环形队列
	pool->queue_size++;

	//添加完任务之后,由于队列不为空,唤醒线程池中等待处理任务的线程
	pthread_cond_signal(&(pool->queue_not_empty));
	pthread_mutex_unlock(&(pool->struct_lock));
	
	return 0;
}

//线程池中工作线程执行的函数
static void *threadpool_thread(void *threadpool)
{
	threadpool_t *pool = (threadpool_t *)threadpool;
	threadpool_task_t task;
	while(TRUE)
	{
		pthread_mutex_lock(&(pool->struct_lock));
		//如果没有任务的话,则阻塞在该条件变量上
		while((pool->queue_size == 0) && (!pool->shutdown))
		{
			//printf("thread 0x%x is waiting\n", (unsigned int)pthread_self());
			pthread_cond_wait(&(pool->queue_not_empty), &(pool->struct_lock));
			//清除指定数量的空闲线程,如果要结束的线程个数大于0,则结束该线程
			if(pool->wait_exit_thread_num > 0)
			{
				pool->wait_exit_thread_num--;
				//如果线程池里线程个数大于最小值则可结束当前线程
				if(pool->live_thread_num > pool->min_thread_num)
				{
					printf("thread 0x%x is exiting\n", (unsigned int)pthread_self());
					pool->live_thread_num--;
					pthread_mutex_unlock(&(pool->struct_lock));
					pthread_exit(NULL);
				}
			}
		}
		//如果shutdown置为true,则关闭线程池中每个线程
		if(pool->shutdown)
		{
			pthread_mutex_unlock(&(pool->struct_lock));
			//printf("thread 0x%x is exiting\n", (unsigned int)pthread_self());
			pthread_exit(NULL);
		}
		//从任务队列中获取任务
		task.func = pool->task_queue[pool->queue_front].func;
		task.arg = pool->task_queue[pool->queue_front].arg;
		pool->queue_front = (pool->queue_front + 1) % pool->queue_max_size;
		pool->queue_size--;

		//由于消耗了一个任务,所以通知可以有新的任务添加进来
		pthread_cond_broadcast(&(pool->queue_not_full));
		
		//任务取出后,释放线程池锁
		pthread_mutex_unlock(&(pool->struct_lock));
		//执行任务
		printf("thread 0x%x start working\n", (unsigned int)pthread_self());
		pthread_mutex_lock(&(pool->thread_counter));
		pool->busy_thread_num++;
		pthread_mutex_unlock(&(pool->thread_counter));
		task.func(task.arg);
		
		//任务处理结束
		printf("thread 0x%x end working\n", (unsigned int)pthread_self());
		pthread_mutex_lock(&(pool->thread_counter));
		pool->busy_thread_num--;
		pthread_mutex_unlock(&(pool->thread_counter));
	}
	pthread_exit(NULL);
}

//管理线程
static void *adjust_thread(void *threadpool)
{
	int i;
	threadpool_t *pool = (threadpool_t *)threadpool;
	while(!pool->shutdown)
	{
		//定时
		sleep(DEFAULT_TIME);
		pthread_mutex_lock(&(pool->struct_lock));
		int queue_size = pool->queue_size;
		int live_thread_num = pool->live_thread_num;
		pthread_mutex_unlock(&(pool->struct_lock));
		pthread_mutex_lock(&(pool->thread_counter));
		int busy_thread_num = pool->busy_thread_num;
		pthread_mutex_unlock(&(pool->thread_counter));
		//当任务数大于最小线程池个数并且存活的线程数少于最大线程个数时创建新线程
		if(queue_size >= MIN_WAIT_TASK_NUM && live_thread_num < pool->max_thread_num)
		{
			int add =0;
			pthread_mutex_lock(&(pool->struct_lock));
			//一次增加DEFAULT_THREAD个线程
			for(i = 0; i < pool->max_thread_num && add < DEFAULT_THREAD_VARY && pool->live_thread_num < pool->max_thread_num; i++)
			{
				if(pool->threads[i] == 0 || !is_thread_alive(pool->threads[i]))
				{
					pthread_create(&(pool->threads[i]), NULL, threadpool_thread, (void *)pool);
					add++;
					pool->live_thread_num++;
				}
			}
			pthread_mutex_unlock(&(pool->struct_lock));
		}
		//只要忙碌的线程*2小于存活的线程数并且存活的线程数大于最小的线程数时就销毁多余的空闲线程
		if((busy_thread_num * 2) < live_thread_num && live_thread_num > pool->min_thread_num)
		{
			//一次销毁DEFAULT_THREAD个线程
			pthread_mutex_lock(&(pool->struct_lock));
			//要销毁的线程数设置为DEFAULT_THREAD_VARY(10)
			pool->wait_exit_thread_num = DEFAULT_THREAD_VARY;
			pthread_mutex_unlock(&(pool->struct_lock));
			for(i = 0; i < DEFAULT_THREAD_VARY; i++)
			{
				pthread_cond_signal(&(pool->queue_not_empty));
			}
		}
	}
	return NULL;
}

int threadpool_destroy(threadpool_t *pool)
{
	int i;
	if(pool == NULL)
	{
		return -1;
	}
	pool->shutdown = TRUE;
	//销毁管理线程
	pthread_join(pool->adjust_tid, NULL);
	for(i = 0; i < pool->live_thread_num; i++)
	{
		//通知所有空闲线程
		pthread_cond_broadcast(&(pool->queue_not_empty));
	}
	for(i = 0; i < pool->live_thread_num; i++)
	{
		pthread_join(pool->threads[i], NULL);
	}
	threadpool_free(pool);
	return 0;
}

static int threadpool_free(threadpool_t *pool)
{
	if(pool == NULL)
	{
		return -1;
	}
	if(pool->task_queue)
	{
		free(pool->task_queue);
	}
	if(pool->threads)
	{
		free(pool->threads);
		pthread_mutex_lock(&(pool->struct_lock));
		pthread_mutex_destroy(&(pool->struct_lock));
		pthread_mutex_lock(&(pool->thread_counter));
		pthread_mutex_destroy(&(pool->thread_counter));
		pthread_cond_destroy(&(pool->queue_not_empty));
		pthread_cond_destroy(&(pool->queue_not_full));
	}
	free(pool);
	pool = NULL;
	return 0;
}

/*static int threadpool_total_num(threadpool_t *pool)
{
	int thread_num = -1;
	pthread_mutex_lock(&(pool->struct_lock));
	thread_num = pool->live_thread_num;
	pthread_mutex_unlock(&(pool->struct_lock));
	return thread_num;
}*/ 

/*static int threadpool_busy_num(threadpool_t *pool)
{
	int busy_num = -1;
	pthread_mutex_lock(&(pool->thread_counter));
	busy_num = pool->busy_thread_num;
	pthread_mutex_unlock(&(pool->thread_counter));
	return busy_num;
}*/

static int is_thread_alive(pthread_t tid)
{
	//用pthread_kill函数向该线程发送0号信号
	//返回ESRCH代表信号不存在
	//返回EINVAL代表信号不合法
	int ret = pthread_kill(tid, 0);
	if(ret == ESRCH)
	{
		return FALSE;
	}
	return TRUE;
}

	
