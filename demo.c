#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <semaphore.h>
#include <fcntl.h>

// #define QUANTUM 3

int main(int argc, char **argv)
{

    int val;
    int i;

    // N is time
    int N = atoi(argv[1]);
    int round = atoi(argv[2]);
    int QUANTUM;

    // setting quantum
    if (round == 1)
    {
        QUANTUM = 3;
    }
    else
    {
        QUANTUM = 7;
    }

    // opening named semaphores
    sem_t *loop_sem = sem_open("/global", 0);
    sem_t *new_arrival = sem_open("/newArrival", 0);
    sem_t *process_re = sem_open("/processRe", 0);
    sem_getvalue(loop_sem, &val);

    int num;

    if (N < QUANTUM)
    {
        num = 0;
    }
    else
    {
        num = N - QUANTUM;
    }

    // loop and print number
    for (i = N; i > 0; i--)
    {
        // checking if demo needs to be stopped
        sem_getvalue(loop_sem, &val);

        // stop demo when quantum is reached
        if (i == num && i != 0)
        {
            // post new_arrival for scheduler to move ahead
            sem_post(new_arrival);
            return i;
        }
        if (val > 0)
        {
            // print if global is still 1
            printf("%d\n", i);
            sleep(1);
        }
        else
        {
            // stop if global is 0
            printf("closing demo\n");
            return i;
        }
    }

    // get value from global
    sem_getvalue(loop_sem, &val);

    // if this is the last process then change global back to 1
    if (val != 0)
    {
        sem_wait(loop_sem);
    }

    // if not, then schduler is waiting for something to reinsert so we move it along
    if (val == 0)
    {
        sem_post(process_re);
    }

    // close semaphore
    sem_close(loop_sem);

    return i;
    fflush(stdout);
}
