#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdbool.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <sys/shm.h>
#include <string.h>
#include <semaphore.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>

#define SEM_NAME "/mis"    //name of semaphore
#define BOUND 

void exitfuncCtrlC(int sig);


//Structs used throughout program
typedef struct resource{
  int maxInstances;               //amount of instances if shareable.  Only one if it is  not shareable
  int instancesReleased;
}res;


typedef struct processBlock{
  int empty;
  int quit;    
  pid_t pid;
  int request;      //flag to let oss know it is requesting a resource 
  int myRes[20];    //spot to see what resources a process has
                    //ex: if process[7] has 2 x R4 & 1 x R18   ->   shmPtr->PCB[x].myRes[3] = 2 & myRes[17] = 1 (the rest will be zero) 
}PCB;

typedef struct bitVec{
  int available;
}BV;

typedef struct ShmData{             //struct used to hold the seconds, nanoseconds, and shmMsg and reference in shared memory
  int seconds;
  int nanoseconds;
  int currentProcessCount;
  int timeNPS;
  int timeNPNS;
  int timeHolder;
  struct processBlock PCB[18];
  struct bitVec BV[18];
  struct resource res[20];
}shmData;




shmData shm;
shmData *shmPtr;
FILE* file_ptr;
char logFile[] = "logFile";         //set the name of the default log file


int main(int argc, char *argv[]){

	int shmKey = 3690;
	int id;
  int processIndex = atoi(argv[1]);
  //file_ptr = fopen(logFile, "a");
  //fprintf(file_ptr,"processIndex in child: %d\n",processIndex);

  if (signal(SIGINT, exitfuncCtrlC) == SIG_ERR) {
       printf("SIGINT error\n");
       exit(1);
   }

	 if ((id = shmget(shmKey,sizeof(shm), IPC_CREAT | 0666)) < 0){
       perror("SHMGET:user");
       exit(1);
   }

   if((shmPtr = shmat(id, NULL, 0)) == (shmData *) -1){
       perror("SHMAT");
       exit(1);
   }


     sem_t *semaphore = sem_open(SEM_NAME, O_RDWR);
    if (semaphore == SEM_FAILED) {
        //perror("sem_open(3) failed in child\n");
        fprintf(file_ptr,"ERROR: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    

    //shmPtr->PCB[processIndex].pid = getpid();



  


    

    int notDone = 1;
    int count = 0;
    time_t t;
     /* Intializes random number generator */
      srand((unsigned) time(&t));

    while(notDone){
      

      if (sem_wait(semaphore) < 0) {
          perror("sem_wait(3) failed on child");
          continue;
      }


     //  //Critical section 
      file_ptr = fopen(logFile, "a");

      //fprintf(file_ptr,"Child with pid: %d has control of the semaphore\n", getpid());
      shmPtr->PCB[processIndex].quit = 1;

       

      //fprintf(file_ptr,"\n\tchild with pid: %d & index: %d & time %d.%d is exiting\n", getpid(), processIndex, shmPtr->seconds, shmPtr->nanoseconds);

      fclose(file_ptr);

      





      if (sem_post(semaphore) < 0) {
            perror("sem_post(3) error on child");
      }

      
      int check = rand()%40;


      if(check == 7){
         notDone=0;

         //deallocate resources here


          exit(0);
        
      }    
      

      count++;

    }

    if (sem_close(semaphore) < 0)
            perror("sem_close(3) failed");



   shmdt(shmPtr);
   return 0;

}

void exitfuncCtrlC(int sig){

    fprintf( stderr, "Child %ld is dying from parent\n", (long)getpid());
    shmdt(shmPtr);
    sem_unlink(SEM_NAME);
    exit(1);
}