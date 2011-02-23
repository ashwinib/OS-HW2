#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include<string.h>
#include "board.h"

#define MAX_THREAD_NUM 200

#ifndef bool
    #define bool int
    #define false ((bool)0)
    #define true  ((bool)1)
#endif

void* hacker(void *arg);
void* serf(void *arg);

static pthread_mutex_t safe_mutex = PTHREAD_MUTEX_INITIALIZER; //total and safe
static pthread_mutex_t oar = PTHREAD_MUTEX_INITIALIZER;	//to row

static pthread_cond_t cond  = PTHREAD_COND_INITIALIZER;//Waiting to board
static pthread_cond_t condRow  = PTHREAD_COND_INITIALIZER;//Waiting to row
static int safe = 0, total =0 , convert =0;
static int allowableHackers = -1, allowableSerfs=-1; 

void pintf(char *buf){
int i=0;
while(buf[i]!='\0'){i++;}
write(1,buf,i);
}
/*void myprintf(char *s, int i) {
  char out[80];

  sprintf(out, s, i);
  write(1, out, strlen(out));

}*/

bool isSafeToBoard(void *arg){
	int isHacker = *(int*)(arg);

	pthread_mutex_lock(&safe_mutex);


	total++;safe+=isHacker;
	//if(alarmtw == true) 
	//	alarmtw = false;
	if(total<4){//LESS THAN FOUR DONT GET TO GO.

		pthread_mutex_unlock(&safe_mutex);
		return false;
	}
	else {pthread_mutex_unlock(&safe_mutex);return (isStillSafeToBoard(arg));}
	
	
}
/*	
Purpose : When its time to board and it is queried, this function
	decides who will board (based on total and safe) and sets 
	appropriate values(allowableHackers)
Policy :  If enough homogeneuos people then allow them.
	Else 2/3S2H or 
*/
int isStillSafeToBoard(void *arg){
	int isHacker = *(int*)(arg);

	//if(isHacker) pintf("\nHacker asks..");
	//else pintf("\nSerf asks..");
	pthread_mutex_lock(&safe_mutex);
		//myprintf("\nTotal = %d",total);myprintf("\tsafe = %d\n",safe);
		//myprintf("AllwdHack = %d",allowableHackers);myprintf("\tAllwdSerfs = %d\n",allowableSerfs);

	if(allowableHackers == -1&& allowableSerfs ==-1 ){//not scheduled so schedule;
		pintf("\n------------------------\n");
		if(safe >= 4){	
			//pintf("\nSetting hackers to4 \n");
			allowableHackers = 4;
			allowableSerfs = 0;
		}else if(total - safe >=4){
			allowableHackers = 0;
			allowableSerfs = 4;
		}else if(safe==3 && total-safe ==1){
			allowableHackers =-1;
			allowableSerfs = -1;
			pthread_mutex_unlock(&safe_mutex);
			return 0;
		}else if(total<4 ){
			allowableHackers = -1;
			allowableSerfs = -1;	
			pthread_mutex_unlock(&safe_mutex);
			return 0;	
		}else {	
			//pintf("\n.Simple schedule .\n");
			allowableSerfs=total-safe;
			allowableHackers= 4 - allowableSerfs;
			if(allowableHackers ==1)
				convert=1;
		}
	}
	//pintf("\n.Askin am i Safe ? .\n");
	if(isHacker)
		if(allowableHackers > 0){
			allowableHackers--;
			pthread_mutex_unlock(&safe_mutex);
			return 1;
		}
		else {	
			pthread_mutex_unlock(&safe_mutex);
			return 0;}
	else
		if(allowableSerfs > 0){
			allowableSerfs--;
			pthread_mutex_unlock(&safe_mutex);
			return 1;
		}

		else {	pthread_mutex_unlock(&safe_mutex);
			return 0;}
}

void* bl_here(void *arg,int i){
	int isHacker = *(int*)(arg);
	pthread_mutex_lock(&safe_mutex);
		//myprintf("\nTotal = %d",total);myprintf("\tsafe = %d\n",safe);
		//myprintf("AllwdHack = %d",allowableHackers);myprintf("\tAllwdSerfs = %d\n",allowableSerfs);
	if(i!=3){/*Leaves after deplaning and not because of waiting.*/
	if(isHacker)
		safe--;
	total--;
	}
	if(isHacker  && convert ==1 && i!=0){	
		int *try ; try = arg;
		*try = 0; //Hacker is recruited
		convert = 0;		
	}
	pthread_mutex_unlock(&safe_mutex);
	if(i==0)
	board(arg);
	else
	leave(arg);
}

void* hacker(void *arg)
{
	int rc,rct;
	bool iWasRower = false;
	int isHacker = *(int*)(arg);
	struct timeval tv;
        struct timespec ts;
	rc =  gettimeofday(&tv, NULL);
        ts.tv_sec = tv.tv_sec + 3;
        ts.tv_nsec = 0;
	//pintf("\nInside hacker");

	/*BOARDING LOGIC STARTS HERE*/
	rc = isSafeToBoard(arg);
	if( rc == 0){//NOT SAFE
waittag:	/*DEBUG*///pintf("Going in Wait");
		/*pthread_mutex_lock(&safe_mutex);
		pthread_cond_wait(&cond,&safe_mutex);		
		pthread_mutex_unlock(&safe_mutex);*/
		//pintf("\nAwakeNow");
		//if(alarmtw == true){//
			//if(isTimeToWait(arg)){//Not all do timed wait.
		pthread_mutex_lock(&safe_mutex);
		rct = pthread_cond_timedwait(&cond,&safe_mutex,&ts);
		pthread_mutex_unlock(&safe_mutex);
	        if (rct == ETIMEDOUT) {
			//pintf("\nWait timed out!\n");
			bl_here(arg,1);
			pthread_exit(NULL);
		}	


tryboarding:	if((rc = isStillSafeToBoard(arg))==1)
			bl_here(arg,0);
		else if(rc == 0) goto waittag;
	}
	else if(rc == 1){
		bl_here(arg,0);
		//pintf("\nBroadcasting for others to board\n");
		pthread_cond_broadcast(&cond);
	}

	/*ROWING LOGIC STARTS HERE*/
	
	if(peopleOnBoard() != 4 ){
		//pintf("\nWaiting to Row\n");
		pthread_mutex_lock(&oar);
		pthread_cond_wait(&condRow,&oar);
		pthread_mutex_unlock(&oar);
		//pintf("\nNot competing now to Row\n");
	}
	else{
		pthread_mutex_lock(&oar);
		rowBoat(arg);	
		iWasRower = true;
		pthread_mutex_unlock(&oar);
		//pintf("\nBroadcasting to deplane\n");
		pthread_cond_broadcast(&condRow);//wake people to deplane
	}	
	

	/*DEPLANING*/
	
	if(iWasRower){
		while(peopleOnBoard()!=1){
			pthread_cond_broadcast(&condRow);
		}
		deplane(arg);
	}
	else{
		deplane(arg);	
		//pintf("\nBroadcasting to rower to deplane\n");
		pthread_cond_broadcast(&condRow);//Tell rower to stop waiting
	}
	
	
	if(iWasRower){
		iWasRower=false;
		pthread_mutex_lock(&safe_mutex);//Explicitly scheduling
		allowableHackers=-1;
		allowableSerfs=-1;
		total++;
		if(isHacker) safe++;
		pthread_mutex_unlock(&safe_mutex);
		//pintf("\nBroadcasting to board\n");
		pthread_cond_broadcast(&cond);//wake people to start boarding again
		goto tryboarding;}

	bl_here(arg,3);

	pthread_exit(NULL);
}

void* serf(void *arg)
{
	//pintf("\nInside serf");
	hacker(arg);
	//pthread_exit(NULL);
}

void initialize(void)
{
	allowableHackers = -1; allowableSerfs=-1; 
	safe = 0; total =0;
}

int main(int argc, char** argv)
{
	pthread_t th[MAX_THREAD_NUM];
	int th_arg[MAX_THREAD_NUM];
	int i;
	int numOfHacker, numOfSerf;

	while(1) {
		pintf("\nEnter no of hackers\n");
		scanf("%d%d", &numOfHacker, &numOfSerf);

		if ( numOfHacker == 0 && numOfSerf == 0 )
			break;

		pintf("\nEnter hackers\n");
		for ( i=0 ; i<(numOfHacker+numOfSerf) ; i++ )
			scanf("%d", &(th_arg[i]));

		initialize();

		for ( i=0 ; i<(numOfHacker+numOfSerf) ; i++ ) {
			if ( th_arg[i] == 1 )
				pthread_create(&(th[i]), NULL, hacker, (void*)(&th_arg[i]));
			else
				pthread_create(&(th[i]), NULL, serf, (void*)(&th_arg[i]));
		}

		for ( i=0 ; i<(numOfHacker+numOfSerf) ; i++ )
			pthread_join(th[i], NULL);
	}

	return 0;
}

