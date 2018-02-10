/*************************************************************************
  > File Name: ukey_httpd.c
  > Author: Ukey
  > Mail: gsl110809@gmail.com
  > Created Time: 2017年06月07日 星期三 19时04分58秒
 ************************************************************************/

#include "httpd.h"

int epev_ralloc() {
	int nep_events = g_manager.nep_events;
	struct epoll_event 	*old_events = g_manager.ep_events;
	ukey_pool_t *pool = g_manager.m_pool;
	ukey_pool_large_t *old_large = NULL;
	struct epoll_event *temp = NULL;
	nep_events *= 2;

	for (old_large = pool->large; old_large; old_large = old_large->next) {
		if (old_large->alloc == (void *)old_events) {
			break;
		}
	}
	/* 未找到,返回-1 */
	if (old_large == NULL) {
		return -1;
	}

	temp = realloc(g_manager.ep_events, nep_events * sizeof(struct epoll_event));
	if (temp == NULL) {
		fprintf(stderr, "realloc event error\n");
		return -1;
	}

	old_large->alloc = temp;
	g_manager.ep_events = temp;
	g_manager.nep_events = nep_events;

	return 0;
}

void *accept_connect(void *arg)
{
	int *sock_fd = (int *)arg;
	struct sockaddr_in client_addr;
	socklen_t client_len;
	client_len = sizeof(client_addr);
	int new_fd = accept(*sock_fd, (struct sockaddr* )&client_addr, &client_len);
	if (new_fd < 0) {
		perror("accept error");
	}

	conn_set(new_fd, EPOLLIN, (void *)process_conn);
	
	return NULL;
}

//按行获取浏览器的request
static int get_line(int sock_fd, char *buf, int size)
{
	int i = 0, n = 0;
	char c = '\0';
	while((i < size) && (c != '\n'))
	{
		n = 0;
		while (n <= 0) {
			n = recv(sock_fd, &c, 1, 0);
			if (n == -1) {
				if (errno != EAGAIN || errno != EWOULDBLOCK) {
					break;
				}
			}
		}
		//printf("--------------c:%c\n", c);
		if (n <= 0) {
			perror("recv error:");
		}
		if(n > 0)
		{
			//这里主要的目的是读取请求数据时将http请求的回车换行符在我们的buf中换成换行
			if(c == '\r')
			{
				//MSG_PEEK的作用是读取数据后,但是并不将读取的数据清出缓冲区,也就是说下次读的时候还在
				n = recv(sock_fd, &c, 1, MSG_PEEK);
				if((n > 0) && (c == '\n'))
				{
					int ret = 0;
					while (ret <= 0) {
						ret = recv(sock_fd, &c, 1, 0);
						if (ret == -1) {
							if (errno != EAGAIN || errno != EWOULDBLOCK)
								break;
						}
					}
				}
				else
				{
					c = '\n';
				}
			}
			buf[i] = c;
			i++;
		}
		else
			break;
	}
	buf[i] = '\0';
	return i;
}
static void send_headers(int client_fd)
{
	char buf[BUFF_SIZE];
	sprintf(buf, "HTTP/1.0 200 OK\r\nServer: ukey_httpd/1.0\r\nContent-Type:text/html\r\n\r\n");
	send(client_fd, buf, strlen(buf), 0);
}

static void not_found(int client_fd)
{
	char buf[BUFF_SIZE];
	sprintf(buf, "HTTP/1.0 404 NOT FOUND\r\nServer: ukey_httpd/1.0\r\nContent-Type:text/html\r\n\r\n");
	send(client_fd, buf, strlen(buf), 0);
	sprintf(buf, "<HTML><TITLE>404 not found</TITLE>\r\n<BODY><P>server can't find the source which you request\r\n</P></BODY></HTML>\r\n");
	send(client_fd, buf, strlen(buf), 0);
}

static void permission_denied(int client_fd)
{
	char buf[BUFF_SIZE];
	sprintf(buf, "HTTP/1.0 403 PERMISSION DENIED\r\nServer: ukey_httpd/1.0\r\nContent-Type:text/html\r\n\r\n");
	send(client_fd, buf, strlen(buf), 0);
	sprintf(buf, "<HTML><TITLE>403 permission denied</TITLE>\r\n<BODY><P>you don't have enough permissions to view this file\r\n</P></BODY></HTML>\r\n");
	send(client_fd, buf, strlen(buf), 0);
}


static void serve_file(int client_fd, const char *file_name)
{
	int n = 1, fd;
	char buf[BUFF_SIZE] = {0};
	struct stat s;
	while((n > 0) && strcmp(buf, "\n"))
	{
		n = get_line(client_fd, buf, sizeof(buf) - 1);
	}
	//判断文件是否存在
	if((access(file_name, F_OK)) == -1)
	{
		not_found(client_fd);
	}
	else
	{
		fd = open(file_name, O_RDONLY);
		//权限不足
		if(errno == EACCES)
		{
			permission_denied(client_fd);
		}
		//否则回送静态文本数据
		else
		{
			//使用mmap将文件映射到内存中
			fstat(fd, &s);
			int file_size = s.st_size;
			char *p = (char *)mmap(NULL, file_size, PROT_READ, MAP_SHARED, fd, 0);
			if(p == MAP_FAILED)
			{
				perror("mmap error");
				exit(1);
			}
			send_headers(client_fd);
			send(client_fd, p, file_size, 0);
			munmap(p, file_size);
		}
	}
	close(fd);
}

static void method_not_implemented(int client)
{
	char buf[BUFF_SIZE];

	sprintf(buf, "HTTP/1.0 501 Method Not Implemented\r\nServer: ukey_httpd/1.0\r\nContent-Type:text/html\r\n\r\n");
	send(client, buf, strlen(buf), 0);

	sprintf(buf, "<HTML><HEAD><TITLE>Method Not Implemented\r\n</TITLE></HEAD>\r\n<BODY><P>request method not support\r\n</P></BODY></HTML>\r\n");
	send(client, buf, strlen(buf), 0);
}


static void bad_request(int client_fd)
{
	char buf[BUFF_SIZE];
	sprintf(buf, "HTTP/1.0 400 BAD REQUEST\r\nServer: ukey_httpd/1.0\r\nContent-type:text/html\r\n\r\n");
	send(client_fd, buf, strlen(buf), 0);
	sprintf(buf, "<HTML><HEAD><TITLE>400 BAD REQUEST</TITLE></HEAD><BODY><P>this is a bad request for server</P></BODY></HTML>");
	send(client_fd, buf, strlen(buf), 0);
}

static void cannot_execute(int client_fd)
{
	char buf[BUFF_SIZE];
	sprintf(buf, "HTTP/1.0 500 Server error\r\nServer: ukey_httpd/1.0\r\nContent-type:text/html\r\n\r\n");
	send(client_fd, buf, strlen(buf), 0);
	sprintf(buf, "<HTML><HEAD><TITLE>500 SERVER ERROR</TITLE></HEAD><BODY><P>SERVER ERROR</P></BODY></HTML>");
	send(client_fd, buf, strlen(buf), 0);
}

static void execute_cgi(int client_fd, const char *path, const char *method, const char *query_string)
{

	char buf[BUFF_SIZE], c;
	int cgi_output[2];
	int cgi_input[2];
	int i, status;
	int n = 1, content_length = -1;
	pid_t pid;

	//GET请求
	if(!strcasecmp(method, "GET"))
	{
		while((n > 0) && strcmp(buf, "\n"))
		{
			n = get_line(client_fd, buf, sizeof(buf) - 1);
		}
	}
	//POST请求
	else
	{
		n = get_line(client_fd, buf, sizeof(buf) - 1);
		while((n > 0) && (strcmp(buf, "\n")))
		{
			buf[15] = '\0';
			if(!strcasecmp(buf, "Content-Length:"))
			{
				content_length = atoi(&(buf[16]));
			}
			n = get_line(client_fd, buf, sizeof(buf) - 1);
		}
		if(content_length == -1)
		{
			bad_request(client_fd);
			return ;
		}
	}

	if(pipe(cgi_output) < 0)
	{
		cannot_execute(client_fd);
		return ;
	}
	if(pipe(cgi_input) < 0)
	{
		cannot_execute(client_fd);
		return ;
	}
	sprintf(buf, "HTTP/1.0 200 OK\r\n");
	send(client_fd, buf, strlen(buf), 0);

	if((pid = fork()) < 0)
	{
		cannot_execute(client_fd);
		return ;
	}
	if(pid == 0)
	{
		char meth_env[ARG_SIZE];
		char query_env[ARG_SIZE];
		char length_env[ARG_SIZE];

		close(cgi_output[0]);
		close(cgi_input[1]);
		if(dup2(cgi_output[1], STDOUT_FILENO) == -1)
		{
			perror("dup2 out error");
			exit(1);
		}
		if(dup2(cgi_input[0], STDIN_FILENO) == -1)
		{
			perror("dup2 in error");
			exit(1);
		}
		sprintf(meth_env, "REQUEST_METHOD=%s", method);
		//设置环境变量
		putenv(meth_env);
		if(!strcasecmp(method, "GET"))
		{
			sprintf(query_env, "QUERY_STRING=%s", query_string);
			putenv(query_env);
		}
		else
		{
			sprintf(length_env, "CONTENT_LENGTH=%d", content_length);
			putenv(length_env);
		}
		execl(path, path, (char *)0);
		exit(0);
	}
	else
	{
		close(cgi_output[1]);
		close(cgi_input[0]);
		if(!strcasecmp(method, "POST"))
		{
			for(i = 0; i < content_length; i++)
			{
				recv(client_fd, &c, 1, 0);
				write(cgi_input[1], &c, 1);
			}
		}
		while(read(cgi_output[0], &c, 1) > 0)
		{
			send(client_fd, &c, 1, 0);
		}
		close(cgi_output[0]);
		close(cgi_input[1]);
		waitpid(pid, &status, 0);
	}
}


void *work_request(void *arg)
{
	int *work_fd = (int*)arg;
	char buf[BUFF_SIZE];
	char method[ARG_SIZE];
	char url[ARG_SIZE];
	char path[512];
	char *query_string = NULL;
	size_t n, i = 0, j = 0;
	int cgi_flag = 0;
	struct stat s;
	n = get_line(*work_fd, buf, sizeof(buf) - 1);

	while((i < sizeof(method) - 1) && (!isspace(buf[i])))
	{
		method[i] = buf[i];
		i++;
	}
	method[i] = '\0';
	j = i;

	if(strcasecmp(method, "GET") && strcasecmp(method, "POST"))
	{
		method_not_implemented(*work_fd);
		return  NULL;
	}

	//跳过空格字符
	while(isspace(buf[j]) && (j < n))
	{
		j++;
	}
	i = 0;
	//获取url
	while(!isspace(buf[j]) && (i < sizeof(url) - 1) && (j < n))
	{
		url[i] = buf[j];
		i++;
		j++;
	}
	url[i] = '\0';


	if(!strcasecmp(method, "POST"))
	{
		cgi_flag = 1;
	}
	else
	{
		query_string = url;
		while((*query_string != '?') && (*query_string != '\0'))
		{
			query_string++;
		}
		if(*query_string == '?')
		{
			cgi_flag = 1;
			*query_string = '\0';
			query_string++;
		}
	}
	sprintf(path, "htdocs%s", url);
	if(path[strlen(path) - 1] == '/')
	{
		strcat(path, "index.html");
	}
	if(stat(path, &s) == -1)
	{
		while((n > 0) && strcmp(buf, "\n"))
		{
			n = get_line(*work_fd, buf, sizeof(buf) - 1);
			not_found(*work_fd);
		}
	}
	else
	{
		if(S_ISDIR(s.st_mode))
		{
			strcat(path, "/index.html");
		}
		if((s.st_mode & S_IXUSR) || (s.st_mode & S_IXGRP) || (s.st_mode & S_IXOTH))
		{
			cgi_flag = 1;
		}
		if(!cgi_flag)
		{
			serve_file(*work_fd, path);
		}
		else
		{
			execute_cgi(*work_fd, path, method, query_string);
		}
	}

	pthread_mutex_lock(&(g_manager.conn_lock));
	conn_free(*work_fd);
	close(*work_fd);
	pthread_mutex_unlock(&(g_manager.conn_lock));

	return NULL;
}


void add_conn(int listen_fd, int events, void *arg) {
	int *sock_fd = (int *)ukey_palloc(g_manager.m_pool, sizeof(int));


	*sock_fd = listen_fd;
	threadpool_add(g_manager.t_pool, accept_connect, (void *)sock_fd);
}

void process_conn(int sock_fd, int events, void *arg) {


	int *fd = (int *)ukey_palloc(g_manager.m_pool, sizeof(int));
	*fd = sock_fd;
	threadpool_add(g_manager.t_pool, work_request, (void *)fd);	
}

int manager_init() {
	g_manager.m_pool = ukey_create_pool(2048);
	g_manager.t_pool = threadpool_create(50, 1024, 1024); 
	g_manager.conn = (connections_t *)ukey_palloc(g_manager.m_pool, sizeof(connections_t ) * 1024);
	g_manager.nconnections = 1024;
	conn_init(g_manager.conn, g_manager.nconnections);
	g_manager.ep_fd = epoll_create(EPOLL_SIZE);
	if (g_manager.ep_fd < 0) {
		perror("epoll create error");
		return -1;
	}
	g_manager.ep_events = (struct epoll_event *)ukey_palloc(g_manager.m_pool, sizeof(struct epoll_event) * 4096);
	g_manager.nep_events = 4096;
	pthread_mutex_init(&(g_manager.conn_lock), NULL);

	return 0;
}

void manager_destroy() {
	close(g_manager.ep_fd);
	int i;
	for (i = 0; i < g_manager.nconnections; i++) {
		ukey_destroy_pool(g_manager.conn[i].m_cpool);
	}
	threadpool_destroy(g_manager.t_pool);
	ukey_destroy_pool(g_manager.m_pool);
}

