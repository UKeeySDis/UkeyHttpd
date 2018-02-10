/*************************************************************************
    > File Name: epoll.h
    > Author: Ukey
    > Mail: gsl110809@gmail.com
    > Created Time: 2018年02月08日 星期四 17时08分41秒
 ************************************************************************/

#ifndef __UKEY_EVENT_H_
#define __UKEY_EVENT_H_

#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>

typedef struct event_s event_t;
struct event_s {
	int ev_fd;
	void (*ev_callback)(int, int, void *ev_arg);
	void *ev_arg;
	int ev_events;
	int ev_status;
};

void event_set(event_t *ev, int fd, void (*call_back)(int, int, void *), void *arg); 

void event_add(int ep_fd, int events, event_t *ev, int et_flag);

void event_del(int ep_fd, event_t *ev); 

#endif
