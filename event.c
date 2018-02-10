/*************************************************************************
  > File Name: event.c
  > Author: Ukey
  > Mail: gsl110809@gmail.com
  > Created Time: 2018年02月10日 星期六 21时21分20秒
 ************************************************************************/

#include "event.h"


void event_set(event_t *ev, int fd, void (*call_back)(int, int, void *), void *arg) {

	ev->ev_fd = fd;
	ev->ev_callback = call_back;
	ev->ev_arg = arg;
	ev->ev_events = 0;
	ev->ev_status = 0;

	return ;
}

void event_add(int ep_fd, int events, event_t *ev, int et_flag) {
	struct epoll_event ep_event = {0, {0}};
	int option;

	if (et_flag) {
		fcntl(ev->ev_fd, F_SETFL, O_NONBLOCK);
		events |= EPOLLET;
	}

	ep_event.data.fd = ev->ev_fd;
	ep_event.events = ev->ev_events = events;

	if (ev->ev_status == 1) {
		option = EPOLL_CTL_MOD;
	} else {
		option = EPOLL_CTL_ADD;
		ev->ev_status = 1;
	}

	if (epoll_ctl(ep_fd, option, ev->ev_fd, &ep_event) < 0) {
		perror("epoll ctl error");
		exit(-1);
	}
	return ;
}

void event_del(int ep_fd, event_t *ev) {

	int option;

	if (ev->ev_fd == -1)
		return ;

	option = EPOLL_CTL_DEL;

	if ((epoll_ctl(ep_fd, option, ev->ev_fd, 0)) < 0) {
		perror("del event error");
		return ;
	}
}
