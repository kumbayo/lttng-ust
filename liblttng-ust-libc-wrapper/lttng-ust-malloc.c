/*
 * Copyright (C) 2009  Pierre-Marc Fournier
 * Copyright (C) 2011  Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

#define _GNU_SOURCE
#include <dlfcn.h>
#include <sys/types.h>
#include <stdio.h>
#include <pthread.h>

#define TRACEPOINT_DEFINE
#define TRACEPOINT_CREATE_PROBES
#include "ust_libc.h"

#define STATIC_CALLOC_LEN 4096
static char static_calloc_buf[STATIC_CALLOC_LEN];
static size_t static_calloc_buf_offset;
static pthread_mutex_t static_calloc_mutex = PTHREAD_MUTEX_INITIALIZER;

static void *static_calloc(size_t nmemb, size_t size)
{
	size_t prev_offset;

	pthread_mutex_lock(&static_calloc_mutex);
	if (nmemb * size > sizeof(static_calloc_buf) - static_calloc_buf_offset) {
		pthread_mutex_unlock(&static_calloc_mutex);
		return NULL;
	}
	prev_offset = static_calloc_buf_offset;
	static_calloc_buf_offset += nmemb * size;
	pthread_mutex_unlock(&static_calloc_mutex);
	return &static_calloc_buf[prev_offset];
}

void *malloc(size_t size)
{
	static void *(*plibc_malloc)(size_t size);
	void *retval;

	if (plibc_malloc == NULL) {
		plibc_malloc = dlsym(RTLD_NEXT, "malloc");
		if (plibc_malloc == NULL) {
			fprintf(stderr, "mallocwrap: unable to find malloc\n");
			return NULL;
		}
	}
	retval = plibc_malloc(size);
	tracepoint(ust_libc, malloc, size, retval);
	return retval;
}

void free(void *ptr)
{
	static void (*plibc_free)(void *ptr);

	/* Check whether the memory was allocated with
	 * static_calloc, in which case there is nothing
	 * to free.
	 */
	if ((char *)ptr >= static_calloc_buf &&
	    (char *)ptr < static_calloc_buf + STATIC_CALLOC_LEN) {
		return;
	}

	if (plibc_free == NULL) {
		plibc_free = dlsym(RTLD_NEXT, "free");
		if (plibc_free == NULL) {
			fprintf(stderr, "mallocwrap: unable to find free\n");
			return;
		}
	}
	tracepoint(ust_libc, free, ptr);
	plibc_free(ptr);
}

void *calloc(size_t nmemb, size_t size)
{
	static void *(*volatile plibc_calloc)(size_t nmemb, size_t size);
	void *retval;

	if (plibc_calloc == NULL) {
		/*
		 * Temporarily redirect to static_calloc,
		 * until the dlsym lookup has completed.
		 */
		plibc_calloc = static_calloc;
		plibc_calloc = dlsym(RTLD_NEXT, "calloc");
		if (plibc_calloc == NULL) {
			fprintf(stderr, "callocwrap: unable to find calloc\n");
			return NULL;
		}
	}
	retval = plibc_calloc(nmemb, size);
	tracepoint(ust_libc, calloc, nmemb, size, retval);
	return retval;
}

void *realloc(void *ptr, size_t size)
{
	static void *(*plibc_realloc)(void *ptr, size_t size);
	void *retval;

	if (plibc_realloc == NULL) {
		plibc_realloc = dlsym(RTLD_NEXT, "realloc");
		if (plibc_realloc == NULL) {
			fprintf(stderr, "reallocwrap: unable to find realloc\n");
			return NULL;
		}
	}
	retval = plibc_realloc(ptr, size);
	tracepoint(ust_libc, realloc, ptr, size, retval);
	return retval;
}
