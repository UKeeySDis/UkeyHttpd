/*************************************************************************
    > File Name: connect.h
    > Author: Ukey
    > Mail: gsl110809@gmail.com
    > Created Time: 2018年02月10日 星期六 13时13分06秒
 ************************************************************************/
#ifndef __UKEY_CONNECTION_H_
#define __UKEY_CONNECTION_H_

#include "event.h"
#include "memory_pool.h"
#include "thread_pool.h"

typedef struct connections_s connections_t;
struct connections_s {
	event_t *read;
	event_t *write;

	ukey_pool_t *m_cpool;
};

typedef struct manager_s manager_t;
struct manager_s {
	int ep_fd;
	struct epoll_event *ep_events;
	int nep_events;
	
	connections_t *conn;
	int nconnections;

	threadpool_t *t_pool;
	ukey_pool_t *m_pool;
	
	pthread_mutex_t conn_lock;
};


struct manager_s g_manager;

void conn_init(connections_t *c, int size);

int conn_set(int fd, int events, void (*call_back)(int, int, void *)); 

void conn_free(int fd);


#endif
