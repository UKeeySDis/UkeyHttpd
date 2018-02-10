/*************************************************************************
    > File Name: memory_pool.h
    > Author: Ukey
    > Mail: gsl110809@gmail.com
    > Created Time: 2018年02月07日 星期三 11时01分40秒
 ************************************************************************/
#ifndef _UKEY_MEMORY_POOL_H_INCLUDED_
#define _UKEY_MEMORY_POOL_H_INCLUDED_

//#define UKEY_MAX_ALLOC_FROM_POOL 4095
#define UKEY_POOL_ALIGNMENT 16

typedef struct ukey_pool_large_s ukey_pool_large_t;
typedef struct ukey_pool_data_s ukey_pool_data_t;
typedef struct ukey_pool_s ukey_pool_t;


struct ukey_pool_large_s {
	ukey_pool_large_t *next;
	void *alloc;
};

struct ukey_pool_data_s {
	char *last;
	char *end;
	ukey_pool_t *next;
	int failed;
};

struct ukey_pool_s {
	ukey_pool_data_t small;

	int	max;

	ukey_pool_t *current;

	ukey_pool_large_t *large;
};

ukey_pool_t *ukey_create_pool(int size);
void ukey_destroy_pool(ukey_pool_t *pool);
void ukey_reset_pool(ukey_pool_t *pool);

void *ukey_alloc(int size);
void *ukey_calloc(int size);

void *ukey_palloc(ukey_pool_t *pool, int size);
void *ukey_pnalloc(ukey_pool_t *pool, int size);
void *ukey_pcalloc(ukey_pool_t *pool, int size);
//void *ukey_pmemalign(ukey_pool_t *pool, int size, int alignment);
int  ukey_pfree(ukey_pool_t *pool, void *p);


#endif
