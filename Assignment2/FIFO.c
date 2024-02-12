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

#define SIZE 20
#define NUMB_THREADS 6
#define PRODUCER_LOOPS 2

// PrintJob struct
typedef struct printJob {
    pid_t pid;
    int jobSize;
    clock_t start;
    clock_t end;
} printJob;

int jobsConsumed;

// Variables for time tracking
double average = 0;

// Global variables for shared memory
printJob *buffer;
int *buffer_index;
int *jobMade;
int *in;
int *out;
int num_prod;
int num_com;

// Thread and semaphore arrays
pthread_t thread[NUMB_THREADS];
int thread_numb[NUMB_THREADS];
sem_t *buffer_mutex;
sem_t *full_sem;
sem_t *empty_sem;
int *doneProd;
int parent;

// Signal handler for SIGINT
void sigint_handler(int sig) {
    int l;

    for (l = 0; l < num_prod; l++) {
        wait(NULL);
    }

    for (l = 0; l < num_com; l++) {
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
    shmdt(in);
    shmdt(out);

    shmctl(shmid_empty, IPC_RMID, NULL);
    shmctl(shmid_full, IPC_RMID, NULL);
    shmctl(shmid_mutex, IPC_RMID, NULL);
    shmctl(shmid_buffer, IPC_RMID, NULL);
    shmctl(shmid_buffer_index, IPC_RMID, NULL);
    shmctl(shmid_prod_done, IPC_RMID, NULL);
    shmctl(shmid_jobMade, IPC_RMID, NULL);
    shmctl(shmid_in, IPC_RMID, NULL);
    shmctl(shmid_out, IPC_RMID, NULL);

    printf("\nSafe exit\n");
    exit(0);
}

// Function to insert a print job into the buffer
   /* full_sem is initialized to buffer size because SIZE number of
       producers can add one element to buffer each. They will wait
       semaphore each time, which will decrement semaphore value.
       empty_sem is initialized to 0, because buffer starts empty and
       consumer cannot take any element from it. They will have to wait
       until producer posts to that semaphore (increments semaphore
       value) */
void insertbuffer(printJob value) {
    if ((*in + 1) % SIZE == *out) {
        printf("Buffer overflow\n");
    } else {
        (*jobMade)++;
        if (*out == -1 && *in == -1) {
            *out = 0;
        }
        *in = (*in + 1) % SIZE;
        value.start = clock();
        buffer[*in] = value;
    }
}

// Function to dequeue a print job from the buffer
printJob dequeuebuffer() {
    printJob item;
    printJob temp = {0};
    if (*out == -1) {
        printf("Buffer underflow\n");
        item.jobSize = -1;
        return item;
    } else {
        item = buffer[*out];
        if (*out == *in) {
            *out = -1;
            *in = -1;
        } else {
            *out = (*out + 1) % SIZE;
        }
        temp.end = clock();
        clock_t timeDiff = temp.end - temp.start;
        double timeDiffSecs = (double)timeDiff / CLOCKS_PER_SEC;
        average = average + timeDiffSecs;
        return item;
    }
}

// Producer function
void *producer(void *thread_n) {
    int thread_numb = *(int *)thread_n;
    printJob value;
    int i = 0;
    srand(getpid());

    int pro_loop = rand() % 20 + 1;

    for (i; i < pro_loop; i++) {
        sleep(1);
        value.jobSize = rand() % 901 + 100;
        sem_wait(full_sem);
        sem_wait(buffer_mutex);
        insertbuffer(value);
        printf("Producer [%d] added jobSize:[%d] to buffer\n", getpid(), value.jobSize);
        sem_post(buffer_mutex);
        sem_post(empty_sem);
        usleep(rand() % (1000000 - 100000 + 1) + 100000);
    }
    (*doneProd)++;
    pthread_exit(0);
}

// Consumer function
void *consumer(void *thread_n) {
    int thread_numb = *(int *)thread_n;
    printJob value;
    jobsConsumed = 0;
    int i = 0;
    while ((*doneProd) < num_prod || jobsConsumed < (*jobMade)) {
        sleep(1);
        sem_wait(empty_sem);
        sem_wait(buffer_mutex);
        value = dequeuebuffer();
        if (value.jobSize == -1) {
            sem_post(buffer_mutex);
            sem_post(buffer_mutex);
            continue;
        }
        jobsConsumed++;
        printf("Consumer ID:[ %ld] dequeue pid:[%d], jobSize:[%d] from buffer\n", pthread_self(), getpid(), value.jobSize);
        sem_post(buffer_mutex);
        sem_post(full_sem);
    }
}

// Function to calculate time difference between timespec structs
double diff_timespec(const struct timespec *start, const struct timespec *finish) {
    return (finish->tv_sec - start->tv_sec) + (finish->tv_nsec - start->tv_nsec) / 1000000000.0;
}

int main(int argc, char *argv[]) {
    struct timeval finishTiming, startT;
    double totalTime;

    gettimeofday(&startT, NULL);
    signal(SIGINT, sigint_handler);

    // Shared memory initialization
    // (omitted for brevity)

    // Semaphore initialization
    sem_init(buffer_mutex, 1, 1);
    sem_init(full_sem, 1, SIZE);
    sem_init(empty_sem, 1, 0);

    num_prod = atoi(argv[1]);
    num_com = atoi(argv[2]);

    int i;
    // Create producer processes
    for (i = 0; i < num_prod; i++) {
        parent = fork();
        if (parent == 0) {
            producer(&i);
            exit(0);
        }
    }

    // Create consumer threads
    for (i = 0; i < num_com;) {
        thread_numb[i] = i;
        pthread_create(&thread[i], NULL, consumer, &thread_numb[i]);
        i++;
    }

    for (i = 0; i < num_prod; i++) {
        wait(NULL);
    }

    while (1) {
        int sem_value;
        sem_getvalue(full_sem, &sem_value);
        if (sem_value == SIZE) {
            break;
        }
    }

    for (i = 0; i < num_com; i++) {
        pthread_cancel(thread[i]);
    }

    gettimeofday(&finishTiming, NULL);
    totalTime = (finishTiming.tv_sec - startT.tv_sec) + (finishTiming.tv_sec - startT.tv_sec) / 1000000.0;
    printf("Total Execution Time: %f sec\n", totalTime);
    printf("Average Waiting Time: %f sec\n", (average / (*jobMade)));
    printf("Total jobs: %d\n", (*jobMade));

    // Semaphore and shared memory cleanup
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
    shmdt(in);
    shmdt(out);

    shmctl(shmid_empty, IPC_RMID, NULL);
    shmctl(shmid_full, IPC_RMID, NULL);
    shmctl(shmid_mutex, IPC_RMID, NULL);
    shmctl(shmid_buffer, IPC_RMID, NULL);
    shmctl(shmid_buffer_index, IPC_RMID, NULL);
    shmctl(shmid_prod_done, IPC_RMID, NULL);
    shmctl(shmid_jobMade, IPC_RMID, NULL);
    shmctl(shmid_in, IPC_RMID, NULL);
    shmctl(shmid_out, IPC_RMID, NULL);

    exit(0);
}
