#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#define MAX_THREAD_NUM 500
#define MAX_TEXTSIZE (1<<20)
static pthread_barrier_t level_barr;
static pthread_barrier_t main_barr;
int noOfThreads;
int sharedData[500];

struct th_arg_s
{
	char *buf;
	char *str;
	int thread_id;
	int start;
	int end;
	int count;
	int first;
	int last;
};

void myprintf(char *s, int i) {
  char out[80];
  sprintf(out, s, i);
  write(1, out, strlen(out));
}

void __string_match(void* arg)
{
extern int sharedData[500];
	struct th_arg_s *th_arg = (struct th_arg_s*)(arg);
	int i;
	int len = strlen(th_arg->str);
	int count;
	int first, last;

	count = 0;
	first = -1;
	last = -1;
	for ( i = th_arg->start ; i<th_arg->end ; i++ ) {
		if ( strncmp(&(th_arg->buf[i]), th_arg->str, len) == 0 ) {
			count++;
			if ( first < 0 ) first = i;
			i += (len-1);
			last = i;
		}
	}

	th_arg->count = count;
	th_arg->first = first;
	th_arg->last = last;
unsigned int s;
    sharedData[th_arg->thread_id] = count;

    for(s=1; s < noOfThreads; s *= 2)
   		 {

   	 pthread_barrier_wait(&level_barr);
     	if (th_arg->thread_id % (2*s) == 0 && (th_arg->thread_id + s) < noOfThreads)
 		     	{

	         	sharedData[th_arg->thread_id] += sharedData[th_arg->thread_id + s];

   		     	}

   		 }
	if(th_arg->thread_id == 0)
   		 {
   		    		 printf("Final Data = %d",sharedData[0]);
   		 }
	//pthread_barrier_wait(&main_barr);
}

void* string_match(void* arg)
{
		__string_match(arg);
}

int main(int argc, char** argv)
{
	pthread_t th[MAX_THREAD_NUM];
	struct th_arg_s th_arg[MAX_THREAD_NUM];
	char *buf;
	int ptr;
	int textsize;
	int blocksize;
	int thread_num;
	int i;
	int sum;
	struct timeval p,q;

	if ( (buf = (char*)malloc(MAX_TEXTSIZE)) == NULL )
		goto out;

	for ( ptr = 0 ; !feof(stdin) ; )
		ptr += fread(&(buf[ptr]), 1, 1, stdin);

	textsize = ptr;

	for ( thread_num = 1 ; thread_num <= MAX_THREAD_NUM ; thread_num++) {
		gettimeofday(&p, NULL);
		noOfThreads = thread_num;
		blocksize = (textsize / thread_num) + (textsize % thread_num ? 1:0);

		pthread_barrier_init(&level_barr,NULL,thread_num);
		//pthread_barrier_init(&main_barr,NULL,(thread_num+1));
		myprintf("\nCreating Threads %d",thread_num);
		for ( i=0 ; i<thread_num ; i++ ) {
			th_arg[i].buf = buf;
			th_arg[i].str = argv[1];
			th_arg[i].thread_id = i;
			th_arg[i].start = i*blocksize;
			if ( i == thread_num-1 )
				th_arg[i].end = textsize;
			else
				th_arg[i].end = (i+1)*blocksize;
			th_arg[i].count = 0;

			pthread_create(&(th[i]), NULL, string_match, &(th_arg[i]));
		}
		myprintf("\njoining",0);
		for ( sum=0, i=0 ; i<thread_num ; i++ ) {
			pthread_join(th[i], NULL);
			if ( i>0 && th_arg[i].count > 0 && th_arg[i-1].last >= th_arg[i].first ) {
				th_arg[i].start = th_arg[i-1].last + 1;
				th_arg[i].count = 0;

				__string_match((void*)(&(th_arg[i])));
			}
			//sum += th_arg[i].count;
		}
		myprintf("Main going in wait%d",thread_num+1);
		//pthread_barrier_wait(&main_barr);
		sum= sharedData[0];
		gettimeofday(&q, NULL);
		printf("blockSize: %8d numOfThread: %4d matchCount: %3d runningTime: %8ld\n", 
				blocksize, thread_num, sum, q.tv_usec - p.tv_usec + (q.tv_sec-p.tv_sec)*1000000);
	}
	
	free(buf);
out:
	return 0;
}

