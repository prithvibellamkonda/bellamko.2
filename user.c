#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short  *array;
};

volatile sig_atomic_t terminate = 0;
int     shmid = 0;
int     semid = 0;
int*    shmem = NULL;
long long start_time = 0;

void sigterm_handler(int sig)
{
    terminate = 1;
}

void initialise()
{
    key_t key = ftok(".", 'a');
    if (key == -1)
    {
        perror("ftok");
        exit(1);
    }

    shmid = shmget(key, 0, 0777);
    if (shmid == -1)
    {
        perror("shmget");
        exit(1);
    }

    errno = 0;
    shmem = shmat(shmid, NULL, 0);
    if (errno)
    {
        perror("shmat");
        exit(1);
    }

    semid = semget(key, 0, 0777);
    if (semid == -1)
    {
        perror("semget");
        exit(1);
    }
}

long long read_clock()
{
    long long total_nano_sec = 0;
    struct sembuf buf;

    // lock shared memory
    buf.sem_num = 0;
    buf.sem_op = -1;
    buf.sem_flg = 0;
    semop(semid, &buf, 1);

    total_nano_sec += shmem[0] * 1000000000;
    total_nano_sec += shmem[1];

    // unlock shared memory
    buf.sem_num = 0;
    buf.sem_op = 1;
    buf.sem_flg = 0;
    semop(semid, &buf, 1);

    return total_nano_sec;
}

int main(int argc, char** argv)
{
    initialise();
    signal(SIGTERM, sigterm_handler);
    start_time = read_clock();

    int id      = atoi(argv[1]);
    int number  = atoi(argv[2]);

    int timeout = 0;
    int prime   = 1;

    if (number < 2 || (number != 2 && number % 2 == 0))
        prime = 0;

    for(int i=3; number > 2 && (i*i) <= number && !timeout; i+=2)
    {
        if (number % i == 0)
        {
            prime = 0;
            break;
        }

        if (terminate || read_clock() - start_time > 2000000)
        {
            timeout = 1;
        }
    }
    shmem[2 + id] = timeout ? -1 : (prime ? number : -number);
    shmdt(shmem);
}