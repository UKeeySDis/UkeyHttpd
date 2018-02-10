/*************************************************************************
  > File Name: main.c
  > Author: Ukey
  > Mail: gsl110809@gmail.com
  > Created Time: 2018年02月10日 星期六 22时05分43秒
 ************************************************************************/

#include "connection.h"
#include "httpd.h"

int main(int argc, char *argv[])
{

	if (manager_init() != 0) {
		fprintf(stderr, "manager init error\n");
		return -1;;
	}

	struct sockaddr_in server_addr;
	int listen_fd;
	int ep_fd = g_manager.ep_fd;

	if((listen_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		perror("socket error");
		exit(1);
	}
	if (conn_set(listen_fd, EPOLLIN, (void *)add_conn) < 0) {
		fprintf(stderr, "listen fd:conn set error\n");
		exit(-1);
	}

	bzero(&server_addr, sizeof(server_addr));

	//设置端口复用
	int flag = 1;
	setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port = htons(SERV_PORT);

	bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));
	listen(listen_fd, 20);


	while(1)
	{
		int event_count = epoll_wait(ep_fd, g_manager.ep_events, g_manager.nep_events, -1);
		if(event_count < 0)
		{
			perror("epoll wait error");
			exit(1);
		}
		int i = 0;
		for(; i < event_count; i++)
		{
			int fd = g_manager.ep_events[i].data.fd;
			int events = g_manager.ep_events[i].events;
			connections_t *ev_conn = &g_manager.conn[fd];

			if (ev_conn == NULL)
				continue;
			if (events & EPOLLIN) {
				if (ev_conn->read) {
					event_t *read_ev = ev_conn->read;
					if (read_ev->ev_callback) 
						read_ev->ev_callback(read_ev->ev_fd, read_ev->ev_events, read_ev->ev_arg);
				}
			}
			if (events & EPOLLOUT) {
				if (ev_conn->write) {
					event_t *write_ev= ev_conn->write;
					write_ev->ev_callback(write_ev->ev_fd, write_ev->ev_events, write_ev->ev_arg);
				}
			}
			/* 需要扩展epoll_event数组 */
			if (event_count == g_manager.nep_events) {
				if (epev_ralloc() < 0) {
					fprintf(stderr, "epoll event realloc error\n");
					exit(-1);
				}
			}
		}
	}

	close(listen_fd);
	manager_destroy();
	return 0;
}


