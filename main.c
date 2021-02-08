/*
 * main.c
 *
 *  Created on: Sep 26, 2020
 *      Author: slavius
 */


#include <stdio.h>
#include <threads.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

/////////////// CORES /////////////////////
#define __USE_GNU
#include <sched.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>

#define print_error_then_terminate(en, msg) \
  do { errno = en; perror(msg); exit(EXIT_FAILURE); } while (0)
#define print_perror_then_terminate(msg) \
  do { perror(msg); exit(EXIT_FAILURE); } while (0)
//////////////// CORES ////////////////////

typedef struct
{
	int begin;
	int end;
	int core_id;
}thrd_arg_t;

typedef struct
{
	thrd_arg_t arg;
	pthread_t thrd;
}thrd_arr_t;

int *num_arr = NULL; /* Array with numbers for sorting */
mtx_t mtx;

void* parallel_max_finding(void *arg); /* Prototype of the thread function */

/* for c11 threads.h (most of them are not used currently) */
void m_mtx_init(mtx_t *__mutex, int __type);
void m_mtx_lock(mtx_t *__mutex);
void m_mtx_unlock(mtx_t *__mutex);
void m_thrd_create(thrd_t *__thr, thrd_start_t __func, void *__arg);
void m_thrd_join(thrd_t __thr, int *__res);


int main()
{
	////////// CORES ////////////////
	const pthread_t pid = pthread_self();
	cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    int core_cnt = 0;
    CPU_SET(core_cnt, &cpuset);
    /* Attach main thread to core 0 */
    int s = pthread_setaffinity_np(pid, sizeof(cpu_set_t), &cpuset);
    if (s != 0)
    {
    	print_error_then_terminate(s, "pthread_setaffinity_np");
    }
	////////// CORES ////////////////


	/* Variables for time bench */
	clock_t start, end;
	double cpu_time_used_1, cpu_time_used_2;

	int num_arr_size = 0u;
	int block_size = 0u;
	int thrd_arr_size = 0u;
	void* result; /* Threads return values */
	int max_simple = 0u; /* Final max element */


	/* Initialize thread creation attributes */
	pthread_attr_t attr;
	const int attr_init_result = pthread_attr_init(&attr);
	if (attr_init_result != 0)
	{
		print_error_then_terminate(attr_init_result, "pthread_attr_init");
	}

	thrd_arr_t* thrd_arr = NULL;


	/* Initialize mutex */
	m_mtx_init(&mtx, mtx_plain);

	/* Input of the array and block sizes */
	printf("Set the size of the numbers array: ");
	scanf("%d", &num_arr_size);

	printf("Set the size of the block: ");
	scanf("%d", &block_size);

	if (block_size > num_arr_size)
	{
		printf("ERROR: invalid block size\n");
	}

	/* Allocate array for numbers */
	num_arr = (int*)malloc(sizeof(int) * num_arr_size);

	if (num_arr == NULL)
	{
		printf("ERROR: memory allocation was failed\n");
	}

	/* Initialize numbers array by random numbers */
	//printf("Initial array:\n");
	srand(time(NULL));
	for (int i = 0u; i < num_arr_size; i++)
	{
//		num_arr[i] = (rand() % (100u - 0u + 1u)) + 0u;
		num_arr[i] = rand();
		//printf("%d ", num_arr[i]);
	}
	printf("\n");

	/////////  SEARCH WITHOUT THREADS   /////////////
	start = clock();

	for (int i = 0; i < num_arr_size; i++)
	{
		if (num_arr[i] > max_simple)
		{
			max_simple = num_arr[i];
		}
	}
	printf("\nMax element: %d ", max_simple);

	end = clock();
	cpu_time_used_1 = ((double)(end - start)) / CLOCKS_PER_SEC;

	printf("\nSearch without threads: %fsec", cpu_time_used_1);
    ///////////  SEARCH WITHOUT THREADS END //////////////////////////


	start = clock();

	/* Calculate treads number */
	thrd_arr_size = (num_arr_size % block_size) ? (num_arr_size / block_size + 1) : (num_arr_size / block_size);

	/* Allocate array for threads */
	thrd_arr = (thrd_arr_t*)malloc(sizeof(thrd_arr_t) * thrd_arr_size);

	if (thrd_arr == NULL)
	{
		printf("ERROR: memory allocation was failed\n");
	}

	/* Create threads */
	for (int i = 0u; i < thrd_arr_size; i++)
	{
		thrd_arr[i].arg.begin = i * block_size;
		if ( (i * block_size + block_size) < num_arr_size )
		{
			thrd_arr[i].arg.end = i * block_size + block_size - 1u;
		}
		else
		{
			thrd_arr[i].arg.end = num_arr_size - 1u;
		}

		thrd_arr[i].arg.core_id = i;
		int create_result = pthread_create(&thrd_arr[i].thrd, &attr, &parallel_max_finding, (void*)&thrd_arr[i].arg);
		if (create_result != 0)
		{
			print_error_then_terminate(create_result, "pthread_create");
		}
	}

	/* Block main till all threads are finished */
	for (int i = 0u; i < thrd_arr_size; i++)
	{
		pthread_join(thrd_arr[i].thrd, &result);
		num_arr[i] = *(int *)result;
	}

//	printf("Array with max elements:\n");
//	for (int i = 0u; i < thrd_arr_size; i++)
//	{
//		printf("%d ", num_arr[i]);
//	}

	/* Find maximum element */
	max_simple = 0;
	for (int i = 0; i < thrd_arr_size; i++)
	{
		if (num_arr[i] > max_simple)
		{
			max_simple = num_arr[i];
		}
	}
	printf("\nMax element: %d ", max_simple);

	mtx_destroy(&mtx);

	end = clock();
	cpu_time_used_2 = ((double)(end - start)) / CLOCKS_PER_SEC;
	printf("\nSearch with threads: %fsec", cpu_time_used_2);

	getchar(); getchar();
	return 0;
}

void* parallel_max_finding(void *arg)
{
	thrd_arg_t *thrd_arg = (thrd_arg_t*)arg;

	////////// CORES ////////////////
	const pthread_t pid = pthread_self();
	cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    int core_cnt = thrd_arg->core_id%2;
    CPU_SET(core_cnt, &cpuset);
    int s = pthread_setaffinity_np(pid, sizeof(cpu_set_t), &cpuset);
    if (s != 0)
    {
    	print_error_then_terminate(s, "pthread_setaffinity_np");
    }

    /* Check the actual affinity mask assigned to the thread */
    s = pthread_getaffinity_np(pid, sizeof(cpu_set_t), &cpuset);
    if (s != 0) {
    	print_error_then_terminate(s, "pthread_getaffinity_np");
    }

//	    printf("\n\nSet returned by pthread_getaffinity_np() contained:\n");
//	    for (int j = 0; j < CPU_SETSIZE; j++)
//	    {
//	        if (CPU_ISSET(j, &cpuset))
//	        {
//	            fprintf(stderr,"%d CPU %d\n",core_cnt, j);
//	        }
//	    }

    ////////// CORES ////////////////


	int* max = (int *)malloc(sizeof(int));
	*max = 0;

	for (int i = thrd_arg->begin; i <= thrd_arg->end; i++)
	{
		if (num_arr[i] > *max)
		{
			m_mtx_lock(&mtx);
			*max = num_arr[i];
			m_mtx_unlock(&mtx);
		}
	}

	return (void*)max;
}




/* for c11 threads.h (most of them are not used currently) */
void m_mtx_init(mtx_t *__mutex, int __type)
{
	if (mtx_init(__mutex, __type) != thrd_success)
	{
		printf("ERROR: mutex was not initialized\n");
	}
}

void m_mtx_lock(mtx_t *__mutex)
{
	if (mtx_lock(__mutex) != thrd_success)
	{
		printf("ERROR: mutex was not locked\n");
	}
}

void m_mtx_unlock(mtx_t *__mutex)
{
	if (mtx_unlock(__mutex) != thrd_success)
	{
		printf("ERROR: mutex was not unlocked\n");
	}
}

void m_thrd_create(thrd_t *__thr, thrd_start_t __func, void *__arg)
{
	if (thrd_create(__thr, __func, __arg) != thrd_success)
	{
		printf("ERROR: thread was not created\n");
	}
}

void m_thrd_join(thrd_t __thr, int *__res)
{
	if (thrd_join(__thr, __res) != thrd_success)
	{
		printf("ERROR: thread was not joined\n");
	}
}
