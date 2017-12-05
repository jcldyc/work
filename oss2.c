#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdbool.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <sys/shm.h>
#include <string.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <fcntl.h>           /* For O_* constants */
#include <sys/stat.h>        /* For mode constants */
#include <semaphore.h>
#include <errno.h>
#include <time.h>

#define msAdded 110
#define maxTotalProc 100
#define billion 1000000000
#define million 1000000
#define addedNS 220000000


// semaphore globals
#define SEM_NAME "/aa"
#define SEM_PERMS (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)
#define INITIAL_VALUE 1
#define CHILD_PROGRAM "./user"

//function declarations

void childProcess(int vacantIndex);
void ctrlPlusC(int sig);


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
  int resourceWanted;
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

int main(int argc, char *argv[]){
  int option;
  char logFile[] = "logFile";         //set the name of the default log file
  int custLF = 0;
  int runIt = 1;                      //flag to determine if program should run.
  FILE* file_ptr;
  pid_t pid;
  int shmKey = 3690;			                //this  is the key used to identify the shared memory

  shmPtr = &shm;			                 //points the shared memory pointer to teh address in shared memory
  int id;
  clock_t start_t = clock();
  


  //----------------------getOpt-------------------------------------------------------------------
    while((option=getopt(argc, argv, "hlv:")) != -1){
      switch(option){
        case 'h':               //this option shows all the arguments available
          runIt = 0;            //how to only asked.  Set to 0 *report to log file*
          printf(" \t./oss: \n\t\t[-l (Log File Name)] \t Custom Logfile name\n \t [-v] \t Turn verbose off. ");
          break;

        case 'l':               //option to name the logfile used
          custLF = 1;

          if(custLF){
            file_ptr = fopen(optarg, "w");
            fclose(file_ptr);
            file_ptr = fopen(optarg, "a");
            printf("\tLog File used: %s\n", optarg);
          }
          break;
          case 'v':
            printf("versobse is on \n");
            break;
        default:
          printf("\tno option used \n");
          break;
      }
    }

    //create the log file specified by user
  //otherwise, use the default log file name : logFile

  if(!custLF && runIt){
    file_ptr = fopen(logFile, "w");
    fclose(file_ptr);
    //file_ptr = fopen(logFile, "a");
    printf("\tLog File name: %s\n", logFile);
  }
  printf("-----------------------------------------------------------------------\n\n");

  if (signal(SIGINT, ctrlPlusC) == SIG_ERR) {
        printf("SIGINT error\n");
        exit(1);
    }





  if(runIt){

    //printf("size of shm: %d\n", (int)sizeof(shm));


    ///only creates the shared memory if the program runs through for(runIt)
      id = shmget(shmKey,sizeof(shm), IPC_CREAT | 0666);
      if (id < 0){
        perror("SHMGET PARENT");
        exit(1);
      }

      shmPtr = shmat(id, NULL, 0);
      if(shmPtr == (shmData *) -1){
        perror("SHMAT");
        exit(1);
      }


     /* We initialize the semaphore counter to 1 (INITIAL_VALUE) */
       sem_t *semaphore = sem_open(SEM_NAME, O_CREAT | O_EXCL, SEM_PERMS, INITIAL_VALUE);

       if (semaphore == SEM_FAILED) {
           perror("sem_open(3) error");
           exit(EXIT_FAILURE);
       }

          /* Close the semaphore as we won't be using it in the parent process */
       if (sem_close(semaphore) < 0) {
           perror("sem_close(3) failed");
           /* We ignore possible sem_unlink(3) errors here */
           sem_unlink(SEM_NAME);
           exit(EXIT_FAILURE);
       }



























       //------------------the beginning of the main loop------------------------


      pid_t pids[18];
      int i;
      int n = 18;
      int needMoreProcesses = 1;
      shmPtr->currentProcessCount = 0;
      time_t t;

       /* Intializes random number generator */
      srand((unsigned) time(&t));
      int shareable = (rand()%3)+3;

      for(int q = 0; q<20; q++){              //initializes all resources maxInstances to 1;
        shmPtr->res[q].maxInstances = 1;
        shmPtr->PCB[q].request = 0;
      }

      for(int qq = 0; qq < shareable; qq++){              //initialize random resources (around 20 +/- 5%) with a random # of instances
        int ran = rand()%20;
        if(shmPtr->res[ran].maxInstances == 1){
          shmPtr->res[ran].maxInstances = (rand()%10) + 2;
        }else if(shmPtr->res[ran].maxInstances != 1){
          qq = qq -1;
        }
      }

      file_ptr = fopen(logFile, "a");
      shmPtr->res[5].maxInstances = 1;
      for(int xx = 0; xx < 20; xx++){
        fprintf(file_ptr, "resource: %d, \t maxInstances: %d\n",xx,shmPtr->res[xx].maxInstances);
      }
      fclose(file_ptr);







      //set all bitVecotr.available = 1 to show it is available

      for(int x=0;x<18; x++){
        shmPtr->BV[x].available=1;
        shmPtr->PCB[x].quit=0;
      }
      shmPtr->seconds = 0;
      shmPtr->nanoseconds = 0;
      shmPtr->timeNPS = 0;
      shmPtr->timeNPNS = (rand()%500000000) +1000000+shmPtr->nanoseconds;     //gets the NS until next process created
      int jC = 0;   //just a checking varr

          

//-------------------------------------------------------------------------------------------------------------------




      while(needMoreProcesses){

        int bvVacant = 0; //flag set to check if the BitVecotr has an open spot
        int f = 1;
        int vacantSpot = 0; //this is where the new process can be placed
        int newProcess = 0;
        int aJC = 0;  

        //file_ptr = fopen(logFile, "a");

        // fprintf(file_ptr,"\tnew process time: \t%d.%d\n", shmPtr->timeNPS, shmPtr->timeNPNS);
        // fprintf(file_ptr,"\ttime:             \t%d.%d\n", shmPtr->seconds, shmPtr->nanoseconds);
        // fprintf(file_ptr,"\tprocess count: \t%d\n", shmPtr->currentProcessCount);
        // fprintf(file_ptr,"\t..........................................................................\n");
        // fclose(file_ptr);


        // //setting shmData to easier vars to use
        // int seconds = shmPtr->seconds, nanoseconds = shmPtr->nanoseconds, timeNPS = shmPtr->timeNPS, timeNPNS = shmPtr->timeNPNS;



        //check if child process is terminating on it's own
        for(int x = 0; x<18; x++){
          if(shmPtr->PCB[x].quit){
            shmPtr->PCB[x].quit = 0;
            shmPtr->BV[x].available = 1;
            
            file_ptr = fopen(logFile, "a");

            fprintf(file_ptr,"child process in BV[%d] is exiting\n", x);
            fclose(file_ptr);
          }
        }




        //resource management
        for(int xs = 0; xs <20; xs++){

          file_ptr = fopen(logFile, "a");

          fprintf(file_ptr,"PCB[%d] = resource wanted: %d\n", xs, shmPtr->PCB[xs].resourceWanted);
          fclose(file_ptr);
        }


      




         //check if the time against the newProcess time
        //if so, create a new timeNPS & NPNS
        if(shmPtr->seconds > shmPtr->timeNPS || (shmPtr->seconds == shmPtr->timeNPS && shmPtr->nanoseconds > shmPtr->timeNPNS)){
          //newProcess =1;


          //checks to see if there is an open spot for a new process
          while(f){
            if(aJC >17){
              f=0;
            }else if(aJC < 18){
              if(shmPtr->BV[aJC].available){
                bvVacant=1;         //if available == 1 that means there is a spot open so bvVacant = 1
                vacantSpot = aJC;
                f=0;

                      //create a new process here
                if(bvVacant && shmPtr->currentProcessCount < 100){
                  shmPtr->BV[vacantSpot].available = 0;
                  

                  shmPtr->PCB[vacantSpot].pid = fork();


                  if (shmPtr->PCB[vacantSpot].pid == 0){
                    
                    childProcess(vacantSpot);
                    printf("error spawning child\n");
                    exit(0);
                  }else if(shmPtr->PCB[vacantSpot].pid > 0){
                    //parent stuff
                    file_ptr = fopen(logFile, "a");
                    fprintf(file_ptr, "\n\n\t\tnew process made at time: %d.%d \t vacantSpot: %d\n" ,shmPtr->seconds, shmPtr->nanoseconds,vacantSpot);
                    fclose(file_ptr);
                    shmPtr->BV[vacantSpot].available = 0;
                    shmPtr->currentProcessCount = shmPtr->currentProcessCount + 1;

                  }else if(shmPtr->PCB[vacantSpot].pid < 0){
                    perror("fork");
                    abort();
                  }

                }


              }
            }
            aJC++;
          }
         
          


          shmPtr->timeNPS = shmPtr->seconds;
          shmPtr->timeNPNS = shmPtr->nanoseconds;
          int temp = shmPtr->timeNPNS;
          shmPtr->timeNPNS += (rand()%500000000) +1000000;     //gets the NS until next process created
          if((shmPtr->timeNPNS % billion) != shmPtr->timeNPNS){        //determines whether to increase seconds or MS
            shmPtr->timeNPS++;                          
            shmPtr->timeNPNS = shmPtr->timeNPNS % billion;
          }

        }


        



        //Deadlock detection




        
       
        printf("\n\n");



      //add time to the clock
      if((shmPtr->nanoseconds % billion) != shmPtr->nanoseconds){        //determines whether to increase seconds or MS
        shmPtr->seconds++;                          
        shmPtr->nanoseconds = shmPtr->nanoseconds % billion;
      }else{
        shmPtr->nanoseconds += addedNS;
      }



      if(shmPtr->currentProcessCount >= 100){
        file_ptr = fopen(logFile, "a");
        fprintf(file_ptr, "CurrentProcessCount is greater than 100: %d\n",shmPtr->currentProcessCount);
        fclose(file_ptr);
        needMoreProcesses=0;
       // fprintf(file_ptr,"processs count greater than 40");
      }
      
//      sleep(1);

      }//needMoreProcesses end


























     

    runIt=0;
  }//runIt end




  //----------------------------------------------------------------------------------------------------
  if (sem_unlink(SEM_NAME) < 0)
      perror("sem_unlink(3) failed");

  shmdt(shmPtr);

  return 0;

}







void childProcess(int cIndex){

    char workingDir[256];
    getcwd(workingDir, sizeof(workingDir));
    char placeToExec[1024];
    strcpy(placeToExec, workingDir);
    strcat(placeToExec, "/user2");

    char index[10];
    snprintf(index, 10, "%d", cIndex);

    char *args[]={"./user2", index, NULL};                //this passes the arguments needed to exec the child  process

    execvp("./user2", args);
    printf("Error while executing ./user2 for child %d\n", getpid());
    printf("ERROR: %s\n", strerror(errno));
    exit(1);
}






void ctrlPlusC(int sig){

    fprintf( stderr, "Child %ld is dying from parent\n", (long)getpid());
    shmdt(shmPtr);
    sem_unlink(SEM_NAME);
    exit(1);
}




