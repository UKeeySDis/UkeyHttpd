/*************************************************************************
    > File Name: thread_pool.h
    > Author: Ukey
    > Mail: gsl110809@gmail.com
    > Created Time: 2017年06月08日 星期四 15时30分33秒
 ************************************************************************/

#ifndef __THREAD_POOL_H_
#define __THREAD_POOL_H_

typedef struct threadpool_t threadpool_t;

//创建线程池
threadpool_t *threadpool_create(int min_thread_num, int max_thread_num, int queue_max_size);

//向线程池中添加一个任务
int threadpool_add(threadpool_t *pool, void *(*func)(void *arg), void *arg);

//销毁线程池
int threadpool_destroy(threadpool_t *pool);



#endif
