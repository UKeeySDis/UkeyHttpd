/*************************************************************************
  > File Name: memory_pool.c
  > Author: Ukey
  > Mail: gsl110809@gmail.com
  > Created Time: 2018年02月07日 星期三 11时23分39秒
 ************************************************************************/


#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include "memory_pool.h"


static void *ukey_palloc_block(ukey_pool_t *pool, int size);
static void *ukey_palloc_large(ukey_pool_t *pool, int size);


ukey_pool_t *ukey_create_pool(int size) {
	ukey_pool_t *p;

	int page_size = getpagesize();
	int type_len = sizeof(ukey_pool_t);
	if (size <= type_len) {
		fprintf(stderr, "create too small memory pool\n");
		return NULL;
	}
	p = memalign(UKEY_POOL_ALIGNMENT, size);
	if (p == NULL) {
		return NULL;
	}

	p->small.last = (char *)p + type_len;
	p->small.end = (char *)p + size;
	p->small.next = NULL;
	p->small.failed = 0;

	size -= type_len;
	p->max = (size < page_size) ? size : page_size;
	page_size = p->max;
	p->current = p;
	p->large = NULL;

	return p;
}

void ukey_destroy_pool(ukey_pool_t *pool) {
	ukey_pool_t *p, *n;
	ukey_pool_large_t *l;

	for (l = pool->large; l; l = l->next) {
		if (l->alloc) {
			free(l->alloc);
			l->alloc = NULL;
		}
	}
	
	for (p = pool, n = pool->small.next; ; p = n, n = n->small.next) {
		free(p);
		if (n == NULL) {
			break;
		}
	}
}
void ukey_reset_pool(ukey_pool_t *pool) {
	ukey_pool_t *p;
	ukey_pool_large_t *l;

	for (l = pool->large; l; l = l->next) {
		if (l->alloc) {
			free(l->alloc);
			l->alloc = NULL;
		}	
	}

	pool->large = NULL;

	for (p = pool; p; p = p->small.next) {
		p->small.last = (char *)p + sizeof(ukey_pool_t);
	}
}

void *ukey_palloc(ukey_pool_t *pool, int size) {
	ukey_pool_t *p;
	char *m = NULL;

	if (size <= pool->max) {
		p = pool->current;

		do {
			m = (char *)((unsigned long)((p->small.last) + (UKEY_POOL_ALIGNMENT - 1)) & (unsigned long)~(UKEY_POOL_ALIGNMENT - 1));
			if ((int)(p->small.end - m) >= size) {
				p->small.last = m + size;
				return m;
			}
			p = p->small.next;
		} while (p);

		return ukey_palloc_block(pool, size);
	}

	return ukey_palloc_large(pool, size);
}

static void *ukey_palloc_block(ukey_pool_t *pool, int size) {
	char *m;
	int psize;
	ukey_pool_t *p, *new, *current;

	psize = (int)(pool->small.end - (char *)pool);

	m = memalign(UKEY_POOL_ALIGNMENT, psize);
	if (m == NULL) {
		return NULL;
	}

	new = (ukey_pool_t *)m;

	new->small.end = m + psize;
	new->small.next = NULL;
	new->small.failed = 0;

	m += sizeof(ukey_pool_data_t);
	m = (char *)((unsigned long)(m + (UKEY_POOL_ALIGNMENT - 1)) & (unsigned long)~(UKEY_POOL_ALIGNMENT - 1));
	new->small.last = m + size;

	current = pool->current;

	for (p = current; p->small.next; p = p->small.next) {
		if (p->small.failed++ > 4) {
			current = p->small.next;
		}
	}

	p->small.next = new;

	pool->current = current ? current : new;

	return m;
}

static void *ukey_palloc_large(ukey_pool_t *pool, int size) {
	void *p;
	int n = 0;
	ukey_pool_large_t *l;

	p = malloc(size);
	if (p == NULL) {
		return NULL;
	}

	for (l = pool->large; l; l = l->next) {
		if (l->alloc == NULL) {
			l->alloc = p;
			return p;
		}
	}
	l = ukey_palloc(pool, sizeof(ukey_pool_large_t));
	if (l == NULL) {
		free(p);
		p = NULL;
		return NULL;
	}

	l->alloc = p;
	l->next = pool->large;
	pool->large = l;

	return p;
}

void *ukey_pnalloc(ukey_pool_t *pool, int size) {
	char *m;
	ukey_pool_t *p;

	if (size <= pool->max) {
		p = pool->current;

		do {
			m = p->small.last;

			if ((int)(p->small.end - m) >= size) {
				p->small.last = m + size;

				return m;
			}
			p = p->small.next;
		} while (p);

		return ukey_palloc_block(pool, size);
	}
	return ukey_palloc_large(pool, size);
}

void *ukey_pcalloc(ukey_pool_t *pool, int size) {
	void *p;

	p = ukey_palloc(pool, size);
	if (p) {
		memset(p, 0, size);
	}
	return p;
}

int ukey_pfree(ukey_pool_t *pool, void *p) {
	ukey_pool_large_t *l;

	for (l = pool->large; l; l = l->next) {
		if (p == l->alloc) {
			free(p);
			l->alloc = NULL;
			return 0;
		}
	}
	return -1;
}
