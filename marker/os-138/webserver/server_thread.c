#include "request.h"
#include "server_thread.h"
#include "common.h"

struct server {
	int nr_threads;
	int max_requests;
	int max_cache_size;
	int exiting;
	/* add any other parameters you need */
	pthread_t *worker_thread_list;
};

/* static functions */
int *buffer;
int in;
int out;
pthread_mutex_t buffer_lock;
pthread_cond_t cv_full;
pthread_cond_t cv_empty;

/* initialize file data */
static struct file_data *
file_data_init(void)
{
	struct file_data *data;

	data = Malloc(sizeof(struct file_data));
	data->file_name = NULL;
	data->file_buf = NULL;
	data->file_size = 0;
	return data;
}

/* free all file data */
static void
file_data_free(struct file_data *data)
{
	free(data->file_name);
	free(data->file_buf);
	free(data);
}

static void
do_server_request(struct server *sv, int connfd)
{
	int ret;
	struct request *rq;
	struct file_data *data;

	data = file_data_init();

	/* fill data->file_name with name of the file being requested */
	rq = request_init(connfd, data);
	if (!rq) {
		file_data_free(data);
		return;
	}
	/* read file, 
	 * fills data->file_buf with the file contents,
	 * data->file_size with file size. */
	ret = request_readfile(rq);
	if (ret == 0) { /* couldn't read file */
		goto out;
	}
	/* send file to client */
	request_sendfile(rq);
out:
	request_destroy(rq);
	file_data_free(data);
}

/* entry point functions */
void stub_function(struct server *sv){
	while (1){
		//using the notation of the code in F2-monitors slide 7 of producer-consumer with monitors
		pthread_mutex_lock(&buffer_lock);
		while (in == out){
			if (sv -> exiting){
				pthread_mutex_unlock(&buffer_lock);
				pthread_exit(0);
				return;
			}
			pthread_cond_wait(&cv_empty, &buffer_lock);
		} //empty

		int connfd = buffer[out];
		out = (out + 1) % (sv -> max_requests + 1);
		pthread_cond_broadcast(&cv_full);
		pthread_mutex_unlock(&buffer_lock);
		do_server_request(sv, connfd);

	}
}

struct server *
server_init(int nr_threads, int max_requests, int max_cache_size)
{
	struct server *sv;

	sv = Malloc(sizeof(struct server));
	sv->nr_threads = nr_threads;
	sv->max_requests = max_requests;
	sv->max_cache_size = max_cache_size;
	sv->exiting = 0;

	//added for Lab4
	in = 0;
	out = 0;
	pthread_mutex_init(&buffer_lock, NULL);
	pthread_cond_init(&cv_full, NULL);
	pthread_cond_init(&cv_empty, NULL);
	
	if (nr_threads > 0 || max_requests > 0 || max_cache_size > 0) {
		/* Lab 4: create queue of max_request size when max_requests > 0 */
		if (max_requests > 0){
			//to distinguish between empty and full add 1 to max_requests
			buffer = Malloc(sizeof(int) * (max_requests + 1));
		}else{
			buffer = NULL;
		}
		/* Lab 4: create worker threads when nr_threads > 0 */
		if (nr_threads > 0){
			sv -> worker_thread_list = Malloc(sizeof(pthread_t) * nr_threads);
			for (int i = 0; i < nr_threads; i++){
				pthread_create(&sv -> worker_thread_list[i], NULL, (void *)&stub_function, sv);
			}
		}
	}

	/* Lab 5: init server cache and limit its size to max_cache_size */

	return sv;
}

void
server_request(struct server *sv, int connfd)
{
	if (sv->nr_threads == 0) { /* no worker threads */
		do_server_request(sv, connfd);
	} else {
		/*  Save the relevant info in a buffer and have one of the
		 *  worker threads do the work. */
		//also use the notation of the code in F2-monitors slide 7 of producer-consumer with monitors
		pthread_mutex_lock(&buffer_lock);
		while ((in - out + sv -> max_requests + 1) % (sv -> max_requests + 1) == sv -> max_requests){
			pthread_cond_wait(&cv_full, &buffer_lock);
		} //full

		buffer[in] = connfd;
		in = (in + 1) % (sv -> max_requests + 1);
		pthread_cond_broadcast(&cv_empty);
		pthread_mutex_unlock(&buffer_lock);
		
	}
}

void
server_exit(struct server *sv)
{
	/* when using one or more worker threads, use sv->exiting to indicate to
	 * these threads that the server is exiting. make sure to call
	 * pthread_join in this function so that the main server thread waits
	 * for all the worker threads to exit before exiting. */
	sv->exiting = 1;
	//added for Lab4
	pthread_cond_broadcast(&cv_full);
	pthread_cond_broadcast(&cv_empty);
	for (int i = 0; i < sv -> nr_threads; i++){
		assert(!pthread_join(sv -> worker_thread_list[i], NULL));
	}
	/* make sure to free any allocated resources */
	free(sv -> worker_thread_list);
	free(buffer);
	free(sv);
}
