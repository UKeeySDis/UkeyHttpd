/*************************************************************************
    > File Name: connect.c
    > Author: Ukey
    > Mail: gsl110809@gmail.com
    > Created Time: 2018年02月10日 星期六 21时47分36秒
 ************************************************************************/

#include "connection.h"

void conn_init(connections_t *c, int size) {	
	int i;
	for (i = 0; i < size; i++) {
		c[i].m_cpool = ukey_create_pool(1024);
		c[i].read = NULL;
		c[i].write = NULL;
	}
}

int conn_set(int fd, int events, void (*call_back)(int, int, void *)) {
	if (fd > g_manager.nconnections) {
		fprintf(stderr, "fd max limits\n");
		return -1;
	}

	connections_t *new_conn = &g_manager.conn[fd];
	if (new_conn == NULL) {
		fprintf(stderr, "conn set error\n");
		return -1;
	}
	event_t *new_ev = (event_t *)ukey_palloc(new_conn->m_cpool, sizeof(event_t));
	if (new_ev == NULL) {
		fprintf(stderr, "alloc from coon m_cpoll error\n");
		return -1;
	}
	void *arg = (void *)new_ev;
	event_set(new_ev, fd, call_back, arg);
	event_add(g_manager.ep_fd, events, new_ev, 1);
	if (events & EPOLLIN) {
		new_conn->read = new_ev;
	}
	if (events & EPOLLOUT) {
		new_conn->write = new_ev;
	}
	
	return 0;
}	

void conn_free(int fd) {
	
	if (fd == -1)
		return;
	connections_t *c = &g_manager.conn[fd];
	if (c == NULL)
		return ;
	if (c->read != NULL) {
		event_del(g_manager.ep_fd, c->read);
		event_set(c->read, -1, NULL, 0);
		
	} else if (c->write != NULL) {
		event_del(g_manager.ep_fd, c->write);
		event_set(c->write, -1, NULL, 0);
	}
}
