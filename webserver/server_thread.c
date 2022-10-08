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
pthread_mutex_t cache_lock;
pthread_cond_t cv_full;
pthread_cond_t cv_empty;

/* new data structure */
struct node{
	struct file_data *data;
	struct node *next;
};

struct master{
	int table_size;
	struct node **hash_table;
	struct node *LRU;
};

/* hash key function */
/* djb2 hash function found on http://www.cse.yorku.ca/~oz/hash.html#:~:text=If%20you%20just%20want%20to,K%26R%5B1%5D%2C%20etc. 
   Made small adjustment */
unsigned long hash(char *str, long table_size)
{
	unsigned long hash = 5381;
	int c;

	while ((c = *str++)){
		hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
	}

	return hash % table_size;
}

/* global variables */
int maximum_cache_size;
int available_cache_size;
struct master *master_table;

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

/* Lab 5 related functions */
/* when encounter cache hit, the file need to be put to the end of LRU list */
void update_LRU(struct file_data *file){
	/* move existing LRU to the back of the list */
	struct node *target = NULL;
	struct node *temp = master_table -> LRU;
	if (strcmp(temp -> data -> file_name, file -> file_name) == 0){
		target = temp;
		master_table -> LRU = temp -> next;
		temp = master_table -> LRU;
		target -> next = NULL;
		if (temp == NULL){
			/* only one node */
			master_table -> LRU = target;
			return;
		}else{
			while (temp -> next != NULL){
				temp = temp -> next;
			}
			temp -> next = target;
			return;
		}
	}else{
		while (temp -> next != NULL){
			if (strcmp(temp -> next -> data -> file_name, file -> file_name) == 0){
				target = temp -> next;
				temp -> next = temp -> next -> next;
        target -> next = NULL;
				break;
			}
      temp = temp->next;
		}
		while (temp -> next != NULL){
			temp = temp -> next;
		}
		temp -> next = target;
	}
}


void push_LRU(struct file_data *file){
	if (master_table -> LRU == NULL){
		struct node *new = (struct node *)malloc(sizeof(struct node));
		new -> data = file;
		new -> next = NULL;
		master_table -> LRU = new;
		return;
	}else{
		struct node *temp = master_table -> LRU;
		while (temp -> next != NULL){
			temp = temp -> next;
		}
		struct node *new = (struct node *)malloc(sizeof(struct node));
		new -> data = file;
		new -> next = NULL;
		temp -> next = new;
	}
}

/* cache lookup */
struct file_data *cache_lookup(struct file_data *file){
	unsigned long key = hash(file -> file_name, master_table -> table_size);
	if (master_table -> hash_table[key] == NULL){
		return NULL; /* cache miss */
	}else{
		struct node* temp = master_table -> hash_table[key]; /* points to the head of the list */
		while (temp != NULL){
			if (strcmp(file -> file_name, temp -> data -> file_name) == 0){
				update_LRU(file);	
				return temp -> data;
			}
			temp = temp -> next;
		}
	}
	return NULL;
}

/* cache evict */
void cache_evict(int required_size){
	while (available_cache_size < required_size){
		struct node *popped = master_table -> LRU;
		master_table -> LRU = popped -> next;
		popped -> next = NULL;
		available_cache_size += popped -> data -> file_size;
		/* get the node out of the hash table too */
		unsigned long key = hash(popped -> data -> file_name, master_table -> table_size);
		struct node* temp = master_table -> hash_table[key]; /* points to the head of the list */
		if (strcmp(temp -> data -> file_name, popped -> data -> file_name) == 0){
			/* head of the list */
			master_table -> hash_table[key] = temp -> next;
			free(temp);
			free(popped);
		}else{
			while (temp -> next != NULL){
				if (strcmp(temp -> next -> data -> file_name, popped -> data -> file_name) == 0){
					struct node *delete_node = temp -> next;
					temp -> next = delete_node -> next;
					delete_node -> next = NULL;
					break;
				}
				temp = temp -> next;
			}
			free(temp);
			free(popped);
		}
	}
}


/* cache insert */
void cache_insert(struct file_data *file){
	if (file -> file_size > maximum_cache_size){
		return;
	}
	struct file_data *target = NULL;
	target = cache_lookup(file);
	if (target != NULL){
		return;  /* other thread put the target into cache already */
	}else{
		if (file -> file_size > available_cache_size){
			cache_evict(file -> file_size);
		}
		unsigned long key = hash(file -> file_name, master_table -> table_size);
		if (master_table -> hash_table[key] == NULL){
			struct node *new = (struct node *)malloc(sizeof(struct node));
			new -> data = file;
			new -> next = NULL;
			master_table -> hash_table[key] = new;
			available_cache_size = available_cache_size - file -> file_size;
		}else{
			struct node* temp = master_table -> hash_table[key]; /* points to the head of the list */
			struct node* prev = NULL;
			while (temp != NULL){
				prev = temp;
				temp = temp -> next;
			}
			struct node *new = (struct node *)malloc(sizeof(struct node));
			new -> data = file;
			new -> next = NULL;
			prev -> next = new;
			available_cache_size = available_cache_size - file -> file_size;
		}
		push_LRU(file);
	}
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

	if(sv -> max_cache_size == 0){
	   /* read file, 
		* fills data->file_buf with the file contents,
		* data->file_size with file size. */
		ret = request_readfile(rq);
		if (ret == 0) { /* couldn't read file */
			goto out;
		}
		/* send file to client */
		request_sendfile(rq);
		request_destroy(rq);
		return;
	}else{
		struct file_data *target = NULL;
		if (sv -> max_cache_size > 0){
			pthread_mutex_lock(&cache_lock);
			target = cache_lookup(data);
			pthread_mutex_unlock(&cache_lock);
		}
		if (target){
			/* cache hit */
			request_set_data(rq, target);
			/* send file to client */
			request_sendfile(rq);
		}else{
			/* cache miss */
			ret = request_readfile(rq);
			if (ret == 0) { /* couldn't read file */
				goto out;
			}
			/* send file to client */
			request_sendfile(rq);
			/* put the new data into cache */
			pthread_mutex_lock(&cache_lock);
			cache_insert(data);
			pthread_mutex_unlock(&cache_lock);	
		}
	}
	
out:
	request_destroy(rq);
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
	pthread_mutex_init(&cache_lock, NULL);
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
		/* Lab 5: init server cache and limit its size to max_cache_size */
		if (max_cache_size > 0){
			maximum_cache_size = max_cache_size;
			available_cache_size = max_cache_size;
			master_table = malloc(sizeof(struct master));
			master_table -> table_size = 100000;
			master_table -> hash_table = malloc(sizeof(struct node*) * 100000);
			master_table -> LRU = NULL;
			for (int i = 0; i < 100000; i++){
				master_table -> hash_table[i] = NULL;
			}
		}
	}
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
