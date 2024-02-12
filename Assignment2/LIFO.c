#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <string.h>
#include <signal.h>
#include <semaphore.h>
#include <time.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <fcntl.h>

#define SIZE 20
#define NUMB_THREADS 10
#define PRODUCER_LOOPS 2

// printJob Strct
typedef struct printJob {
    pid_t pid;
    int jobSize;
    clock_t start;
    clock_t end;
} printJob;

int jobsConsumed;
double average = 0;

printJob totTime;
printJob avgTime;

printJob* buffer;
int *buffer_index;
int *jobMade = 0;

int num_prod;
int num_com;

pthread_t thread[NUMB_THREADS];
int thread_numb[NUMB_THREADS];

int flag = 0;

int key = 4759;
int shmid_buffer;
int shmid_empty;
int shmid_full;
int shmid_mutex;
int shmid_buffer_index;
int shmid_prod_done;
int shmid_jobMade;
int parent;

/* initially buffer will be empty.  full_sem
   will be initialized to buffer SIZE, which means
   SIZE number of producer threads can write to it.
   And empty_sem will be initialized to 0, so no
   consumer can read from buffer until a producer
   thread posts to empty_sem */

sem_t* buffer_mutex;
sem_t* full_sem; /* when 0, buffer is full */
sem_t* empty_sem; /* when 0, buffer is empty. Kind of
                    like an index for the buffer */
int *doneProd = 0;

clock_t start;
clock_t end;

void sigint_handler(int sig) {
    int l;

    for(l = 0;l < num_prod;l++){
        wait(NULL);
    }

    for(l = 0; l < num_com;l++){
        pthread_cancel(thread[l]);
    }

    sem_destroy(buffer_mutex);
    sem_destroy(full_sem);
    sem_destroy(empty_sem);

    shmdt(empty_sem);
    shmdt(full_sem);
    shmdt(buffer_mutex);
    shmdt(buffer);
    shmdt(buffer_index);
    shmdt(doneProd);
    shmdt(jobMade);

    shmctl(shmid_empty, IPC_RMID, NULL);
    shmctl(shmid_full, IPC_RMID, NULL);
    shmctl(shmid_mutex, IPC_RMID, NULL);
    shmctl(shmid_buffer, IPC_RMID, NULL);
    shmctl(shmid_buffer_index, IPC_RMID, NULL);
    shmctl(shmid_prod_done, IPC_RMID, NULL);
    shmctl(shmid_jobMade, IPC_RMID, NULL);

    printf("\n safe exit\n");
    exit(0);
}

void insertbuffer(printJob value) {
    if (*buffer_index < SIZE) {
        (*jobMade)++;
        buffer[(*buffer_index)++] = value;
    } else {
        printf("Buffer overflow\n");
    }
}

//gracefully after crlt c keep running then deallocate 
printJob dequeuebuffer() {
    printJob temp;
    if (*buffer_index > 0) {
        temp = buffer[--(*buffer_index)];
        temp.end = clock();
        clock_t timeDiff = temp.end - temp.start;
        double timeDiffSecs = (double)timeDiff / CLOCKS_PER_SEC;
        average = average + timeDiffSecs;
        return temp;
    } else {
        printf("Buffer underflow\n");
        exit(1);
    }
}

 //generate some random of jobs and run while there is some jobs between 0 and 20 
    /* possible race condition here. After this thread wakes up,
           another thread could aqcuire mutex before this one, and add to list.
           Then the list would be full again
           and when this thread tried to insert to buffer there would be
           a buffer overflow error */
void *producer(void *thread_n) {
    int thread_numb = *(int *)thread_n;
    printJob value;
    value.start = clock();
    int i=0;
    srand(getpid());
    int pro_loop = rand() % 20 + 1; 

    for(i; i < pro_loop;i++) {
        usleep(rand() % (1000000 - 100000 + 1) + 100000);
        value.jobSize = rand() % 901 + 100;
    
        sem_wait(full_sem);
        sem_wait(buffer_mutex);

        insertbuffer(value);
        
        printf("Producer [%d] added jobSize:[%d] to buffer\n", getpid(), value.jobSize);
        sem_post(buffer_mutex);
        sem_post(empty_sem);

        usleep(rand() % (1000000 - 100000 + 1) + 100000);

    } 
    pthread_exit(0);
}

void *consumer(void *thread_n) {
    int thread_numb = *(int *)thread_n;
    printJob value;
    jobsConsumed = 0;
    int i=0;

    while ((*doneProd) < num_prod || jobsConsumed < (*jobMade)) {
        sleep(1);
        sem_wait(empty_sem);
        sem_wait(buffer_mutex);

        value = dequeuebuffer();
        jobsConsumed++;
        
        printf("Consumer ID:[ %ld] dequeue pid[%d] jobSize:[%d] from buffer\n", pthread_self(),getpid(), value.jobSize);
        
        sem_post(buffer_mutex);
        sem_post(full_sem);
        
   }
}

double diff_timespec(const struct timespec *start, const struct timespec *finish) {
    return (finish->tv_sec - start->tv_sec) + (finish->tv_nsec - start->tv_nsec) / 1000000000.0;
}

int main(int argc, char *argv[]) {
    struct timeval finishTiming , startT;
    double totalTime;
    gettimeofday(&startT,NULL);

    signal(SIGINT, sigint_handler);

    if ((shmid_buffer = shmget(key, 20 * sizeof(printJob), IPC_CREAT | 0600)) < 0) {
        perror("Failed to get Shared Memory Segment for buffer");
        exit(1);
    }
    if ((buffer = (printJob*) shmat(shmid_buffer, NULL, 0)) == (printJob*) -1) {
        perror("Failed to attach Shared Memory Segment for buffer");
        exit(1);
    }

    if ((shmid_empty = shmget(key + 3, sizeof(sem_t), IPC_CREAT | 0600)) < 0) {
        perror("Failed to get Shared Memory Segment for empty");
        exit(1);
    }
    if ((empty_sem = shmat(shmid_empty, NULL, 0)) == (sem_t*) -1) {
        perror("Failed to attach Shared Memory Segment for empty");
        exit(1);
    }

    if ((shmid_full = shmget(key + 4, sizeof(sem_t), IPC_CREAT | 0600)) < 0) {
        perror("Failed to get Shared Memory Segment for full");
        exit(1);
    }
    if ((full_sem =  shmat(shmid_full, NULL, 0)) == (sem_t*) -1) {
        perror("Failed to attach Shared Memory Segment for full");
        exit(1);
    }

    if ((shmid_mutex = shmget(key + 5, sizeof(sem_t), IPC_CREAT | 0600)) < 0) {
        perror("Failed to get Shared Memory Segment for mutex");
        exit(1);
    }
    if ((buffer_mutex = (sem_t*) shmat(shmid_mutex, NULL, 0)) == (sem_t*) -1) {
        perror("Failed to attach Shared Memory Segment for mutex");
        exit(1);
    }

    if ((shmid_buffer_index = shmget(key + 7, sizeof(int), IPC_CREAT | 0600)) < 0) {
        perror("Failed to get Shared Memory Segment for total_jobs");
    }

    if ((buffer_index =  shmat(shmid_buffer_index, NULL, 0)) == (int*) -1) {
        perror("Failed to attach Shared Memory Segment for totalJobs");
        exit(1);
    }

    if ((shmid_prod_done = shmget(key + 8, sizeof(int), IPC_CREAT | 0600)) < 0) {
        perror("Failed to get Shared Memory Segment for total_jobs");
    }

    if ((doneProd =  shmat(shmid_prod_done, NULL, 0)) == (int*) -1) {
        perror("Failed to attach Shared Memory Segment for totalJobs");
        exit(1);
    }

    if ((shmid_jobMade = shmget(key + 9, sizeof(int), IPC_CREAT | 0600)) < 0) {
        perror("Failed to get Shared Memory Segment for total_jobs");
    }

    if ((jobMade =  shmat(shmid_jobMade, NULL, 0)) == (int*) -1) {
        perror("Failed to attach Shared Memory Segment for totalJobs");
        exit(1);
    }

    *buffer_index = 0; 

    sem_init(buffer_mutex,1,1);

    sem_init(full_sem, 1, SIZE);
    sem_init(empty_sem, 1, 0);

    num_prod = atoi(argv[1]);  
    num_com = atoi(argv[2]); 

    int i;
    for (i = 0; i < num_prod; i++) { 
        parent = fork();
        if (parent == 0){
            producer(&i);
            exit(0);
        }
    }

    for(i = 0; i < num_com; ) {
        thread_numb[i] = i;
        pthread_create(&thread[i], NULL, consumer, &thread_numb[i]);
        i++;
    }

    for(i = 0; i < num_prod; i++){
        wait(NULL);
    }
//Waiting until buffer is full
    while(1){
        int sem_value;
        sem_getvalue(full_sem,&sem_value);
        if(sem_value == SIZE){
            break;
        }
    }

    for (i = 0; i < num_com; i++){
        pthread_cancel(thread[i]);
    }

    gettimeofday(&finishTiming,NULL);
    totalTime =  (finishTiming.tv_sec - startT.tv_sec) + (finishTiming.tv_sec - startT.tv_sec) / 1000000.0;
    printf("Total Execution Time: %f sec\n", totalTime);
    printf("Average Waiting Time: %f sec\n",(average/ (*jobMade) ));
    printf("Total jobs: %d\n", (*jobMade));

    sem_destroy(buffer_mutex);
    sem_destroy(full_sem);
    sem_destroy(empty_sem);

    shmdt(empty_sem);
    shmdt(full_sem);
    shmdt(buffer_mutex);
    shmdt(buffer);
    shmdt(buffer_index);
    shmdt(doneProd);
    shmdt(jobMade);
//removing shared mem
    shmctl(shmid_empty, IPC_RMID, NULL);
    shmctl(shmid_full, IPC_RMID, NULL);
    shmctl(shmid_mutex, IPC_RMID, NULL);
    shmctl(shmid_buffer, IPC_RMID, NULL);
    shmctl(shmid_buffer_index, IPC_RMID, NULL);
    shmctl(shmid_prod_done, IPC_RMID, NULL);
    shmctl(shmid_jobMade, IPC_RMID, NULL);

    exit(0);
}

