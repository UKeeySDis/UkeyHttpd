CC = gcc
CFLAGS = -lpthread -g -Wall

all : ukey_httpd

ukey_httpd : thread_pool.o ukey_httpd.o
	@$(CC) $^ -o $@ $(CFLAGS)

%.o : %.c
	@$(CC) $< -c $(CFLAGS)

.PHONY:

clean:
	@rm -f ukey_httpd
	@rm -f *.o
