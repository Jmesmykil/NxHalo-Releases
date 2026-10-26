#pragma once

/* Real locks, on pthreads: the Switch port of Halo: Combat Evolved links
this compiler into the game and compiles shaders on several threads at once
(its deko3d renderer's background shader cache), which the fake header this
replaces made impossible. devkitA64's newlib has pthreads through libnx. */

#include <pthread.h>

typedef pthread_mutex_t mtx_t;

enum
{
	mtx_plain = 0,
	mtx_recursive = 1,
	mtx_timed = 2
};

static inline int mtx_init(mtx_t *mutex, int type)
{
	(void)type;
	return pthread_mutex_init(mutex, NULL);
}

static inline int mtx_destroy(mtx_t *mutex)
{
	return pthread_mutex_destroy(mutex);
}

static inline int mtx_lock(mtx_t *mutex)
{
	return pthread_mutex_lock(mutex);
}

static inline int mtx_unlock(mtx_t *mutex)
{
	return pthread_mutex_unlock(mutex);
}

#define _MTX_INITIALIZER_NP PTHREAD_MUTEX_INITIALIZER
