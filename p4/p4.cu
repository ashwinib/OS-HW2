#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <cuda.h>
#include <time.h>
#include <sys/types.h>
#include <sys/time.h>
#define VERBOSE 1
__global__ void checkMatchOnDevice(char *fileBuffer, char* searchString, int* matchArray, int numBytes,size_t searchSize,int* matchStartArray, int* matchEndArray)
{
  extern __shared__ int sdata[];
  int idx = blockIdx.x*blockDim.x + threadIdx.x;
  int rangeStart;
  int rangeEnd; 
  rangeStart = idx*numBytes;
  rangeEnd = rangeStart + numBytes;
  int i,j;
  int numMatches;
  int foundMatch;
  int firstMatch = 1;
  int firstMatchIndex = -1;
  int lastMatchIndex = -1;
  unsigned int tid = threadIdx.x; 

  for(numMatches=0, i = rangeStart; i < rangeEnd; i++)
  {
    foundMatch = 1;
    for(j = 0; j < searchSize; j++)
    {
      int index = i+j;
      if(fileBuffer[index] != searchString[j])
      {
        foundMatch = 0;
        break;
      }
    }
    if(foundMatch)
    {
      numMatches++;
      if(firstMatch)
      {
	firstMatchIndex = i;
        firstMatch = 0;
      }
      lastMatchIndex = i+searchSize;
      i+=searchSize-1;
    }
  }
  matchStartArray[idx] = firstMatchIndex;
  matchEndArray[idx] = lastMatchIndex;
  //matchArray[idx] = numMatches;
  sdata[tid] = numMatches;

  __syncthreads();
  for(unsigned int s=1;s<blockDim.x;s *= 2){
    int index = s*2*tid;
    if((index+s) <  blockDim.x){
	    sdata[index] += sdata[index + s];
    }
    __syncthreads();
  }
  if(tid==0) matchArray[blockIdx.x]=sdata[0];  
}
__global__ void cumulateOnDevice(int* matchArray, int noOfThreads, int* outArray)
{
  extern __shared__ int sdata2[];
  unsigned int tid = threadIdx.x;
  unsigned int i = blockIdx.x * blockDim.x + threadIdx.x;
  unsigned int lim;// = (noOfThreads > blockDim.x) ? blockDim.x:noOfThreads;
  if(i<noOfThreads){
	sdata2[tid] = matchArray[i];
	__syncthreads();

	for(unsigned int s=1;s<blockDim.x;s*=2){
	  int index = s*2*tid;
	
	//__syncthreads();
	  if((index+s) < noOfThreads){
	    sdata2[index] += sdata2[index+s];
	  }
	 __syncthreads();
	}
	if(tid == 0) matchArray[blockIdx.x] = sdata2[0];
  }
}
int main(int argc, char *argv[])
{
  struct timeval cpuStart,cpuEnd;
  char* searchString = (char*)malloc(sizeof(char*)*80);
  char* fileBuffer = (char*)malloc(sizeof(char*)*10000000);
  int nBlocks;
  int threadsPerBlock;
  if(argc != 4)
  {
    printf("Usage: stringSearch \"Key\" numBlocks threadsPerBlock < inputFile\n");
    exit(0);
  }
  else
  {
    searchString = argv[1];
    nBlocks = atoi(argv[2]);
    threadsPerBlock = atoi(argv[3]);
    #ifdef VERBOSE
    printf("Search String: %s\n",searchString);
    #endif
    int ptr;
    for(ptr = 0; !feof(stdin);)
    {
      ptr+= fread(&(fileBuffer[ptr]),1,1,stdin);
    }
    char *deviceFileBuffer;                 // pointer to device memory
    char *deviceSearchBuffer;                 // pointer to device memory
    int *matchArray;
    int *outArray;
    int *hostMatchArray;
    int *hostMatchStartArray;
    int *hostMatchEndArray;
    int *matchStartArray;
    int *matchEndArray;
    int fileSize = ptr;
    //printf("FileSize: %d",strlen(fileBuffer));
    size_t searchSize = strlen(searchString);
    cudaEvent_t start, stop; 
    float time; 
    cudaEventCreate(&start); 
    cudaEventCreate(&stop);
    cudaThreadSynchronize();
    gettimeofday(&cpuStart, NULL);
    // allocate file buffer space on device 
    cudaMalloc((void **) &deviceFileBuffer, fileSize);
    // allocate search string space on device 
    cudaMalloc((void **) &deviceSearchBuffer, searchSize);
    // copy data from host to device
    cudaMemcpy(deviceFileBuffer, fileBuffer, fileSize, cudaMemcpyHostToDevice);
    cudaMemcpy(deviceSearchBuffer, searchString, searchSize, cudaMemcpyHostToDevice);
    // do calculation on device:
    // Part 1 of 2. Compute execution configuration
    size_t numThreads = nBlocks*threadsPerBlock;
    int numBytesPerThread = fileSize/numThreads; 
    //Allocate match array space on device
    cudaMalloc((void **) &matchArray, sizeof(int)*numThreads);
    cudaMalloc((void **) &outArray, sizeof(int)*numThreads);
    hostMatchArray = (int*)malloc(sizeof(int)*numThreads);
    cudaMalloc((void **) &matchStartArray, sizeof(int)*numThreads);
    hostMatchStartArray = (int*)malloc(sizeof(int)*numThreads);
    cudaMalloc((void **) &matchEndArray, sizeof(int)*numThreads);
    hostMatchEndArray = (int*)malloc(sizeof(int)*numThreads);
    //Init array to 0
    int i;
    for(i = 0; i < numThreads; i++)
    {
      hostMatchArray[i] = 0; 
      hostMatchStartArray[i] = -1; 
      hostMatchEndArray[i] = -1; 
    }
    cudaMemcpy(matchArray, hostMatchArray, numThreads, cudaMemcpyHostToDevice);
    cudaMemcpy(matchStartArray, hostMatchStartArray, numThreads, cudaMemcpyHostToDevice);
    cudaMemcpy(matchEndArray, hostMatchEndArray, numThreads, cudaMemcpyHostToDevice);
    //printf("Number of threads:%d, Number of blocks:%d, Num Threads Per Block:%d, Num Bytes Per Thread:%d\n",numThreads,nBlocks,threadsPerBlock,numBytesPerThread);
    // Part 2 of 2. Call call checkMatchOnDevice kernel
    cudaEventRecord( start, 0 );
    checkMatchOnDevice <<< nBlocks, threadsPerBlock >>> (deviceFileBuffer, deviceSearchBuffer, matchArray,numBytesPerThread,searchSize,matchStartArray,matchEndArray);
    int newNBlocks = (nBlocks/threadsPerBlock)+1;printf("\nNew Blocks:%d",nBlocks);
    cudaThreadSynchronize();
    cumulateOnDevice <<< newNBlocks, threadsPerBlock ,threadsPerBlock * sizeof(int)>>> (matchArray,nBlocks,outArray);
    //cudaMemcpy(matchArray, outArray, sizeof(int)*numThreads, cudaMemcpyDeviceToDevice);
    cudaEventRecord( stop, 0 ); 
    cudaEventSynchronize( stop ); 
    cudaEventElapsedTime( &time, start, stop ); 
    cudaEventDestroy( start ); 
    cudaEventDestroy( stop ); 
    cudaThreadSynchronize();
    // Retrieve result from device and store in host array
    cudaMemcpy(hostMatchArray, matchArray, sizeof(int)*numThreads, cudaMemcpyDeviceToHost);
    cudaMemcpy(hostMatchStartArray, matchStartArray, sizeof(int)*numThreads, cudaMemcpyDeviceToHost);
    cudaMemcpy(hostMatchEndArray, matchEndArray, sizeof(int)*numThreads, cudaMemcpyDeviceToHost);
    int total = 0;
    for(i = 0; i < numThreads; i++)
    {
	total += hostMatchArray[i];
        printf("%d)%d\n",i,hostMatchArray[i]);
    }
	total = hostMatchArray [0];
    //Overlap check, commented out for hw2  
/*  for(i = 0; i < numThreads; i++)
    {
	if(hostMatchEndArray[i] != -1 && hostMatchStartArray[i+1] != -1)
	{
		if(hostMatchEndArray[i] - hostMatchStartArray[i+1] < 0)
			total--;	
	}
	//printf("%d)%d\n",i,hostMatchStartArray[i]);
	//printf("start:%d,end:%d\n",hostMatchStartArray[i],hostMatchEndArray[i]);
    }*/
    
    gettimeofday(&cpuEnd, NULL);
   // float totalTime = (cpuEnd - cpuStart);
    //printf("Number of threads:%d, Number of blocks:%d, Num Threads Per Block:%d, Num Bytes Per Thread:%d\n",numThreads,nBlocks,threadsPerBlock,numBytesPerThread);
    //printf("numOfThread: %4d matchCount: %4d CPUrunningTime: %8ld\n", blocksize, thread_num, sum, q.tv_usec - p.tv_usec + (q.tv_sec-p.tv_sec)*1000000);
    #ifdef VERBOSE
    printf("Completed Successfully! Number of blocks:%d Number of threads per block:%d Num Threads: %d Matches:%d CPU Time:%8ld GPU Time:%f\n\n",nBlocks,threadsPerBlock,numThreads,total,cpuEnd.tv_usec - cpuStart.tv_usec + (cpuEnd.tv_sec - cpuStart.tv_sec ),time);
    #else
    printf("%d %f\n\n",numThreads,time);
    //printf("%d %8ld\n\n",numThreads,cpuEnd.tv_usec - cpuStart.tv_usec + (cpuEnd.tv_usec - cpuStart.tv_usec) );
    #endif
    cudaFree(matchArray);
    cudaFree(outArray);
  }
  
}
