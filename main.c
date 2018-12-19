#include <stdio.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/wait.h>
#include "ezipc.h"

void male();
void female();
void quitHandler(int);
//7 SMs, 6 semaphores
int mutex1, stallSemaphore, open, activeMenInQueue, activeWomenInQueue,queueAccess;

char *menInside, *womenInside, *menWaitingOutside, *womenWaitingOutside, *totalMen, *totalWomen;

int main(int argc, char* argv[]) {
    unsigned int lineNumber=1;
    int t1=0;
    int t2=0;
    int pid=1;
    int numStalls=(int)argv[1];
    char line[10];
    char *gender;
    char finalGender[10];

    SETUP();
    mutex1=SEMAPHORE(SEM_BIN,1);
    activeMenInQueue=SEMAPHORE(SEM_BIN,1);
    activeWomenInQueue=SEMAPHORE(SEM_BIN,1);
    open=SEMAPHORE(SEM_BIN,1);

    menInside=SHARED_MEMORY(20);
    womenInside=SHARED_MEMORY(20);
    totalMen=SHARED_MEMORY(2);
    totalWomen=SHARED_MEMORY(2);
    menWaitingOutside=SHARED_MEMORY(2);
    womenWaitingOutside=SHARED_MEMORY(2);

    printf("ENTER NUMBER OF STALLS\n");
    fgets(line,10,stdin);
    numStalls=line[0]-'0';
    stallSemaphore=(SEM_CNT, numStalls);

    *totalMen=0;
    *totalWomen=0;
    *menInside=0;
    *womenInside=0;
    *menWaitingOutside=0;
    *womenWaitingOutside=0;

    printf("NUMSTALLS:%d\n",numStalls);
    FILE *input = fopen("unisex.txt","r");
    if (input==NULL){
        perror("Error: File not opened\n");
        exit(-1);
    }

    //gets first number - used in FOR loop
    fgets(line, 10, input);
    int numCustomers=line[0]-'0';
    fgets(line, 10, input);

    for(int i=0;i<numCustomers;i++){
        t2=*strtok(line, " ")-'0';
        gender=strtok(NULL, " ");
        strcpy(finalGender,gender);
        //when reading file in, do sleep(t1-t0);, then update t0=t1;
        sleep(t2-t1);
        printf("Customer %d, Gender:%c arrives at time %d\n",lineNumber, finalGender[0], t2);
        if(strchr(&finalGender[0],'M')!=NULL){
            pid=fork();
            if(pid==0){
                //printf("RUNNING MALE\n");
                male(lineNumber);
                exit(0);
            }
        }
        else{
            pid=fork();
            if(pid==0){
                //printf("RUNNING Female\n");
                female(lineNumber);
                exit(0);
            }
        }
        t1=t2;
        lineNumber++;
        fgets(line, 10, input);
    }
    signal(SIGINT,quitHandler);
    pause();//handle end - waits for sigint from user
}
void male(unsigned int id){
    int customerID=id;//sets ID for print statements
    //checks if women are in bathroom
    if(*womenInside>0||*womenWaitingOutside!=0){
        //if so, adds 1 to menWaitingOutside, requests bathroom access

        P(mutex1);
        //First male in line gets access to queue and requests bathroom access
        *menWaitingOutside=*menWaitingOutside+(char)1;
        V(mutex1);
        printf("***Man %d is waiting outside bathroom, there are %d men and %d women outside \n", customerID,*menWaitingOutside,
               *womenWaitingOutside);
        P(activeMenInQueue);
        if(*menWaitingOutside==1){
            P(queueAccess);
            P(open);
        }
        V(activeMenInQueue);

        //ENTERING BATHROOM
        P(mutex1);
        printf("@@@ Customer %d moving  from line to bathroom and is male. there are currently %d men in the bathroom\n",
               customerID, *menInside);
        //LEAVES LINE, returns from queue access
        *menWaitingOutside=*menWaitingOutside-(char)1;
        if(*menWaitingOutside==0){
            V(queueAccess);
        }
        V(mutex1);
    }
        //protecting count of men in bathroom
        P(mutex1);
        *menInside=*menInside+(char)1;
        printf("&&& Customer %d entered and is male. there are currently %d men in the bathroom\n", customerID, *menInside);
        V(mutex1);

        //using bathroom;
        P(stallSemaphore);
        sleep(1);//using bathroom
        V(stallSemaphore);
        //sleep((customerID*3)%4+1);//doing other things

        P(mutex1);
        ++*totalMen;
        *menInside=*menInside-(char)1;
        printf("!!! Customer %d left and is male. there are currently %d men in the bathroom\n", customerID, *menInside);
        V(mutex1);
    //if last man leaves bathroom,
    if(*menInside==0){
        V(open);
    }
}
void female(unsigned int id){
    unsigned int sleepTime= (id*3)%4+1;//sets ID for print statements
    //checks if women are in bathroom
    if(*menInside>0||*menWaitingOutside!=0){
        //if so, adds 1 to womenWaitingOutside, requests bathroom access, FIXME: needs to block other males from entering

        P(mutex1);
        //First female in line gets access to queue and requests bathroom access
        *womenWaitingOutside=*womenWaitingOutside+(char)1;
        V(mutex1);
        printf("### Woman %d is waiting outside bathroom, there are %d men and %d women outside \n", id,*menWaitingOutside,
               *womenWaitingOutside);
        P(activeWomenInQueue);
        if(*womenWaitingOutside==1){
            P(queueAccess);
            P(open);
        }
        V(activeWomenInQueue);

        //ENTERING BATHROOM
        P(mutex1);
        printf("@@@ Customer %d moving from line to bathroom and is female. there are currently %d women in the bathroom\n",
               id, *womenInside);
        //LEAVES LINE, returns from queue access
        *womenWaitingOutside=*womenWaitingOutside-(char)1;
        if(*womenWaitingOutside==0){
            V(queueAccess);
        }
        V(mutex1);
        //FIXME: ADD BATHROOM USE HERE
    }
    //protecting count of women in bathroom
    P(mutex1);
    *womenInside=*womenInside+(char)1;
    printf("&&& Customer %d entered bathroom and is female. there are currently %d women in the bathroom\n", id, *womenInside);
    V(mutex1);
    //using bathroom;
    P(stallSemaphore);
    sleep(1);//using bathroom
    V(stallSemaphore);

    P(mutex1);
    ++*totalWomen;
    *womenInside=*womenInside-(char)1;
    //printf("TESTING %d\n",*womenInside);
    printf("!!! Customer %d left and is female. there are currently %d women in the bathroom\n", id, *womenInside);
    V(mutex1);
    //}
    //if last woman leaves bathroom,
    if(*womenInside==0){
        V(open);
    }
}

void use_bathroom(int customerID, char* gender, int inBathroom){
    printf("Customer %d entered, is %s, there are currently %d people in the bathroom\n", customerID, gender, inBathroom);
    sleep((customerID*3)%4+1);
    printf("Customer %d exited, is %s, there are currently %d people in the bathroom\n", customerID, gender, inBathroom);
}

void quitHandler(int num){
    printf("\n@@@   PROGRAM TERMINATED   @@@\n      %d MEN and %d WOMEN used the bathroom",*totalMen, *totalWomen);
    exit(0);
}
/*void femaleReaders(){

        P(mutex2);
        if (++*womenCount==1){
            P(open);
        }
        V(mutex2);
        P(stallSemaphore);
        //use bathroom(customerID, "Female", customers_in_bathroom);
        V(stallSemaphore);
        P(mutex2);
        if (--*==0){
            V(open);
        }
        V(mutex2);
}*/;