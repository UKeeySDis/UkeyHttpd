CC = gcc
C_SOURCES = $(shell find . -name "*.c")
C_OBJECTS = $(patsubst %.c, %.o, $(C_SOURCES))
C_FLAGS = -lpthread -g 

all : ukey_httpd

ukey_httpd : $(C_OBJECTS)
	@$(CC) $^ -o $@ $(C_FLAGS)

%.o : %.c
	@$(CC) $< -c $(C_FLAGS)

.PHONY:

clean:
	@rm -f ukey_httpd
	@rm -f *.o
