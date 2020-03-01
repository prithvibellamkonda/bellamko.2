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


#define NANO    1000000000

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short  *array;
};


volatile sig_atomic_t terminate = 0;

int     t = 4;          // total number of process
int     n = 4;          // total number of process, initially same as above t
int     s = 2;          // max process any any moment
int     r = 0;          // number of running process
int     b = 2;          // start of sequence to check prime for
int     inc = 1;        // increment

int     semid = 0;      // semaphore id
int     shmid = 0;      // shared memory id
int*    shmem = NULL;   // pointer to shared memory

union semun arg;

long long read_clock();

void initialise()
{
    key_t key = ftok(".", 'a');
    if (key == -1)
    {
        perror("ftok");
        exit(1);
    }

    // get shared memory for t child processes and 2 clock variables
    shmid = shmget(key, sizeof(int) * (t + 2), IPC_CREAT | 0777);
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

    for (int i = 0; i < t + 2; ++i)
    {
        shmem[i] = 0;
    }

    // get semaphore
    semid = semget(key, 1, IPC_CREAT | 0777);
    if (semid == -1)
    {
        perror("semget");
        exit(1);
    }

    // initialize semaphore to 1
    arg.val = 1;
    if (semctl(semid, 0, SETVAL, arg) == -1)
    {
        perror("semctl");
        exit(1);
    }
}

void cleanup()
{
    semctl(semid, 0, IPC_RMID);
    shmdt(shmem);
    shmctl(shmid, IPC_RMID, NULL);
}

void handler(int sig)
{
    terminate = 1;
}

char* int_to_string(int num)
{
    char* buffer = malloc(20);
    sprintf(buffer, "%d", num);
    return buffer;
}

void launch_child(int id, int prime)
{
    char* argv[] = {"user", int_to_string(id), int_to_string(prime), NULL};
    pid_t pid;
    switch ((pid = fork()))
    {
        case 0:
            execv("user", argv);
            perror("execv");
            exit(1);
        case -1:
            perror("fork");
            exit(1);
        default:
            break;
    }
    long long tm = read_clock();
    int sec = (int) tm / 1000000000;
    int nan = (int) tm % 1000000000;
    printf("process %d: started   : %d s : %d ns\n", pid, sec, nan);

    free(argv[1]);
    free(argv[2]);
    n--;
    r++;
}

void poll_terminated_childs()
{
    int pid;
    int status;
    do {
        pid = waitpid(WAIT_ANY, &status, WNOHANG | WUNTRACED);
        if (pid > 0)
        {
            long long tm = read_clock();
            int sec = (int) tm / 1000000000;
            int nan = (int) tm % 1000000000;
            printf("process %d: terminated: %d s : %d ns\n", pid, sec, nan);
            --r;
        }
    } while (pid > 0 || (pid == -1 && errno == EINTR));
}

void terminate_all_child()
{
    kill(0, SIGTERM);
    while (r)
    {
        poll_terminated_childs();
    }
}

void increment_clock(int nanoseconds)
{
    static long long total_nano_sec = 0;
    total_nano_sec += nanoseconds;
    struct sembuf buf;

    // lock shared memory
    buf.sem_num = 0;
    buf.sem_op = -1;
    buf.sem_flg = 0;
    semop(semid, &buf, 1);

    shmem[0] = (int) (total_nano_sec / 1000000000);
    shmem[1] = (int) (total_nano_sec % 1000000000);

    // unlock shared memory
    buf.sem_num = 0;
    buf.sem_op = 1;
    buf.sem_flg = 0;
    semop(semid, &buf, 1);
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


void print_usage(const char* name)
{
    printf("usage: %s [-n number] [-s number] [-b number] [-i number] [-o path]\n", name);
    printf("-n      number of child process to launch. (default = 4)\n");
    printf("-s      max number of child process to run concurrently. (default = 2)\n");
    printf("-b      first number to check primality of. (min, default = 2)\n");
    printf("-i      increment value to get next number. (default = 1)\n");
    printf("-o      path to file, where output will be stored. (default = stdout)\n");
}

void parse_arguments(int argc, char** argv)
{
    int opt = 0;
    while ((opt = getopt(argc, argv, "hn:s:b:i:o:")) != -1) {
        switch (opt) {
            case 'h':
                print_usage(argv[0]);
                exit(0);
                break;
            case 'n':
                n = atoi(optarg);
                t = n;
                break;
            case 's':
                s = atoi(optarg);
                s = s < 20 ? s : 20;
                break;
            case 'b':
                b = atoi(optarg);
                b = b > 2 ? b : 2;
                break;
            case 'i':
                inc = atoi(optarg);
                break;
            case 'o':
                freopen(optarg, "w", stdout);
                break;
            default:
                print_usage(argv[0]);
                exit(1);
        }
    }
}


int main(int argc, char** argv)
{
    parse_arguments(argc, argv);

    initialise();

    signal(SIGALRM, handler);       // will get launched after 2 seconds
    signal(SIGINT,  handler);       // control c

    // parent sends all the process in its process group this signal to notify them to terminate
    // even itself, so ignore this signal in parent.
    signal(SIGTERM, SIG_IGN);

    // triggers SIGALRM signal after 2 seconds
    alarm(2);

    int id = 0;
    while (!terminate && (n || r))
    {
        increment_clock(10000);
        if (n && r < s)
        {
            launch_child(id, b + (inc * id));
            ++id;
        }
        poll_terminated_childs();
    }

    terminate_all_child();
    printf("\ncurrent clock: %d s : %d ns\n", (int)(read_clock() / NANO), (int)(read_clock() % NANO));

    // print all prime numbers
    for (int i = 0; i < t; ++i)
    {
        int v = shmem[2 + i];
        int p = b + (inc * i);
        if (v == p)
        {
            printf("%d: prime\n", p);
        }
    }
    putchar('\n');

    // print all non-prime numbers
    for (int i = 0; i < t; ++i)
    {
        int v = shmem[2 + i];
        int p = b + (inc * i);
        if (v == -p)
        {
            printf("%d: not prime\n", p);
        }
    }
    putchar('\n');

    // print all numbers which could not be evaluated.
    for (int i = 0; i < t; ++i)
    {
        int v = shmem[2 + i];
        int p = b + (inc * i);
        if (v == 0 || v == -1)
        {
            printf("%d: timeout\n", p);
        }
    }
    putchar('\n');

    cleanup();
}
