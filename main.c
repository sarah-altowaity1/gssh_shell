#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/wait.h>

#include "input_parsing.h"
#include "exec.h"

#define PORT 5793
#define NUM_CLIENTS 10

// mutex for inserting and deleting in the queue
pthread_mutex_t queue_mutex;

// named sem to start and stop demo program
sem_t *global;
// named sem to notify scheduler when there is a new program in the queue
sem_t *new_arrival;
// named sem to notify scheduler when a stopped process has been reinserted
sem_t *process_re;

// current size of priority queue
int size = 0;

// struct to hold the Request from the client
typedef struct Request
{
    char *command;
    int time;
    pthread_t threadid;
    int socket;
    sem_t *sem;
    int round;
} Request;

// helper function for priority queue
void swap(Request *a, Request *b)
{
    Request temp = *b;
    *b = *a;
    *a = temp;
}

// Function to heapify the array of requests
void heapify(Request array[], int size, int i)
{
    if (size == 1)
    {
        return;
    }
    else
    {
        // Find the shortest request, left child and right child
        int shortest = i;
        int l = 2 * i + 1;
        int r = 2 * i + 2;
        if (l < size && array[l].time < array[shortest].time)
            shortest = l;
        if (r < size && array[r].time < array[shortest].time)
            shortest = r;

        // Swap and continue heapifying if root is not shortets command
        if (shortest != i)
        {
            swap(&array[i], &array[shortest]);
            heapify(array, size, shortest);
        }
    }
}

// Function to insert an request into the queue
void insert(Request array[], Request newRequest)
{

    if (size == 0)
    {
        array[0] = newRequest;
        size += 1;
    }
    else
    {
        array[size] = newRequest;
        size += 1;
        for (int i = (size / 2) - 1; i >= 0; i--)
        {
            heapify(array, size, i);
        }
    }
}

// Function to delete an element from the queue
void delete (Request array[], pthread_t threadid)
{
    int i;
    for (i = 0; i < size; i++)
    {
        if (array[i].threadid == threadid)
            break;
    }

    swap(&array[i], &array[size - 1]);
    size -= 1;
    for (int i = (size / 2) - 1; i >= 0; i--)
    {
        heapify(array, size, i);
    }
}

Request queue[100];
// code to handle ctrl-c command and close the server
void serverExitHandler(int sig_num)
{
    printf("\n Exiting server. \n");
    sem_close(global);
    sem_close(new_arrival);
    sem_close(process_re);
    sem_unlink("/global");
    sem_unlink("/newArrival");
    sem_unlink("/processRe");
    sem_destroy(global);
    sem_destroy(new_arrival);
    sem_destroy(process_re);
    // clear the standard out
    fflush(stdout);
    exit(0);
}

// exec handler for normal commands
void runExec(char *line, int sock2)
{
    // status response
    int res = 1;

    // to store the different commands
    char **cmds;
    // all the arrays of strings separated by pipes
    char **piped_array;
    // number of cmds
    int no_cmds;

    // tokenize into commands and separate pipes
    piped_array = tokenizePipes(line, &no_cmds);

    // choosing the correct exec function depending on the number of pipes
    switch (no_cmds)
    {
    case 0:
        // if no commands, do nothing
        break;
    case 1:
        // separate by white space
        cmds = tokenize(line);

        // if user enters exit, close the socket connection
        if (strcmp(cmds[0], "exit") == 0)
        {
            close(sock2);
            break;
        }

        // change directory, but changes it for all clients
        else if ((strncmp(cmds[0], "cd", 2) == 0) && (strlen(cmds[0]) == 2))
        {
            res = cd_command(cmds[1], &sock2);
        }
        else if ((strncmp(cmds[0], "cat", 3) == 0) && (strlen(cmds[0]) == 3) && (cmds[1] == NULL))
        {
            break;
        }
        else
        {
            res = exec(cmds, &sock2);
        }

        // free the memory allocated to hold the single command
        free(cmds);
        break;
    case 2:
        // handle with one pipe
        res = execPipe(piped_array, &sock2);
        break;
    case 3:
        // handle with two pipes
        res = execTwoPipes(piped_array, &sock2);
        break;
    case 4:
        // handle with three pipes
        res = execThreePipes(piped_array, &sock2);
        break;
    }

    free(piped_array);
}

// run the demo program
Request execDemo(Request demoCmd)
{

    int status;
    pid_t pid;
    pid = fork();

    // child process
    if (pid == 0)
    {

        // run the demo command with the given argument
        if (execvp("./demo", tokenize(demoCmd.command)) == -1)
        {
            perror("gssh: command not found");
            exit(EXIT_FAILURE);
        }
    }
    else if (pid > 0)
    {
        // parent process
        // get the return time from the stopped command
        wait(&status);
        int time = WEXITSTATUS(status);

        char sending[1024];

        // if request is completed, inform the client
        if (time == 0)
        {
            snprintf(sending, 1024, "Request completed");
            send(demoCmd.socket, sending, strlen(sending), 0);
        }

        // create a new request with new time and round
        Request newCmd;
        newCmd.socket = demoCmd.socket;
        newCmd.threadid = demoCmd.threadid;
        char *cmd;
        cmd = (char *)malloc(20);
        int newRound = demoCmd.round + 1;
        snprintf(cmd, 20, "./demo %d %d", time, newRound);
        newCmd.time = time;
        newCmd.command = cmd;
        newCmd.sem = demoCmd.sem;
        newCmd.round = newRound;

        // return the new command to the clientHandler
        return newCmd;
    }
    else
    {
        // fork failed
        perror("Fork failed");
        exit(EXIT_FAILURE);
    }
}

// thread handler function for client
void *clientHandler(void *socket)
{

    // local sem to control the execution of the request
    sem_t local;
    sem_init(&local, 0, 0);

    // derefence the socket and free memory
    int sock2 = *(int *)socket;
    free(socket);

    // buffer to receive the input from the user
    char line[1024];
    while (1)
    {

        // reset the buffer
        memset(line, 0, 1024 * sizeof(char));

        // read client input into line
        int n = recv(sock2, line, sizeof(line), 0);

        // setting null for end of the input
        line[n] = '\0';

        // create Request with the command received
        Request demoCmd;
        Request job;
        job.round = 1;
        job.socket = sock2;
        job.threadid = pthread_self();
        job.sem = &local;
        char *cmd;
        cmd = (char *)malloc(20);
        strcpy(cmd, line);

        // boolean for normal vs demo command
        int normal = 0;

        // set time for demo command
        if (strncmp("./demo", cmd, 6) == 0)
        {
            job.command = strcat(cmd, " 1");
            char **demoParams = tokenize(line);
            job.time = atoi(demoParams[1]); // error with no time
        }
        // exit the client
        else if (strncmp("exit", cmd, 4) == 0)
        {
            close(sock2);
            pthread_exit(NULL);
        }
        // if normal command, set time for 1
        else
        {
            job.command = cmd;
            job.time = 1;
            normal = 1;
        }

        demoCmd = job;
        int repeat = 1;
        int firstRun = 1;

        // if the Request does not finish, then repeat this
        while (repeat)
        {
            // insert in queue
            int value;
            // lock while adding
            pthread_mutex_lock(&queue_mutex);
            insert(queue, demoCmd);
            pthread_mutex_unlock(&queue_mutex);

            // if this command is repeating then post the scheduler when it's reinserted
            if (!firstRun)
            {
                sem_post(process_re);
            }
            // post the scheduler for a new arrival process
            else
            {
                sem_post(new_arrival);
            }

            // wait for scheduler to post it
            sem_wait(&local);
            // it is no longer the first run
            firstRun = 0;

            if (normal)
            {
                repeat = 0;
                runExec(demoCmd.command, demoCmd.socket);
                pthread_mutex_lock(&queue_mutex);
                delete (queue, demoCmd.threadid);
                pthread_mutex_unlock(&queue_mutex);

                if (size > 0)
                {
                    sem_post(new_arrival);
                }
            }
            // demo program
            else
            {
                // process returned with a new time
                Request new_process = execDemo(demoCmd);
                if (new_process.time == 0)
                {
                    repeat = 0;
                }

                pthread_mutex_lock(&queue_mutex);
                delete (queue, demoCmd.threadid);
                pthread_mutex_unlock(&queue_mutex);
                demoCmd = new_process;

                // if there is something in the queue and we are not running this request again...
                if (size > 0 && !repeat)
                {
                    int waiting;
                    sem_getvalue(new_arrival, &waiting);

                    int reinsert;
                    sem_getvalue(process_re, &reinsert);

                    // if the scheduler is waiting on a new process then post it so it moves on
                    if (waiting == 0)
                    {
                        sem_post(new_arrival);
                    }
                }
            }
        }
    }

    // exit the thread
    // sem_destroy(local);
    pthread_exit(NULL);
}

void *scheduler(void *arg)
{
    // demo program sem
    int global_value;
    // reinsert sem
    int pr;
    int sched_val;
    int current = -1;

    while (1)
    {

        // if there is something in queue then check the semaphores
        if (size > 0)
        {

            sem_getvalue(new_arrival, &sched_val);
            sem_wait(new_arrival);
            int ret_val = sem_getvalue(global, &global_value);
            Request new_process;

            // if demo is currently running
            if (global_value == 1)
            {
                // stop demo
                sem_wait(global);
                sem_getvalue(process_re, &pr);

                // wait for process to reinsert
                sem_wait(process_re);

                // if this is the first process running
                if (current == -1)
                {
                    new_process = queue[0];
                }
                // if it's the same process that is running, and there is more than one, pick the next one
                else if (current == queue[0].socket && size > 1)
                {
                    new_process = queue[1];
                    if (size > 2)
                    {
                        if (queue[2].time < queue[1].time)
                        {
                            new_process = queue[2];
                        }
                    }
                }
                // otherwise run the only one in the queue
                else
                {
                    new_process = queue[0];
                }

                // only do this if the incoming process is not normal
                if (strncmp("./demo", new_process.command, 6) == 0)
                {
                    // allow the demo program to run
                    sem_post(global);
                }
                // allow the Request to run
                sem_post(new_process.sem);
            }

            // if demo is not running
            if (global_value == 0)
            {
                new_process = queue[0];
                if (strncmp("./demo", new_process.command, 6) == 0)
                {
                    sem_post(global);
                }
                // allow the Request to run
                sem_post(new_process.sem);
            }
            // set the current process
            current = new_process.socket;
            printf("\nCurrent scheduled request with burst time %d running for round %d run on socket %d\n", new_process.time, new_process.round, new_process.socket);
        }
    }
    exit(EXIT_FAILURE);
}

void gss_loop()
{
    // call scheduler to run
    pthread_t sched_id;
    pthread_attr_t sched_attr;

    if (pthread_mutex_init(&queue_mutex, NULL) != 0)
    {
        printf("\n mutex init has failed\n");
        exit(EXIT_FAILURE);
    }

    // set attributes for scheduler thread
    if (pthread_attr_init(&sched_attr) != 0)
    {
        perror("pthread_attr_init");
        exit(1);
    }

    // create detached scheduler thread
    if (pthread_attr_setdetachstate(&sched_attr, PTHREAD_CREATE_DETACHED) != 0)
    {
        perror("pthread_attr_setdetachstate");
        exit(1);
    }

    // create detached thread
    if ((pthread_create(&sched_id, &sched_attr, scheduler, NULL)) != 0)
    { // check for error when creating thread
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    // ctrl-c handler to exit the server
    if (signal(SIGINT, serverExitHandler) == SIG_ERR)
    {
        perror("signal");
        exit(1);
    }

    // creating server and client receiver socket variables
    int sock1, sock2;

    // creating thread and its attribute
    pthread_t tid;
    pthread_attr_t pthread_attr;

    // address properties
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    /* Creating socket file descriptor with communication: domain of internet protocol
    version 4, type of SOCK_STREAM for reliable/conneciton oriented communication,
    and protocol of internet*/
    if ((sock1 = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    { // checking if the socket creation failed
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // reset the server address
    memset(&address, 0, addrlen);
    address.sin_family = AF_INET;                // setting Internet protocol version 4
    address.sin_addr.s_addr = htonl(INADDR_ANY); // converting unsigned integer hostlong to network byte order
    address.sin_port = htons(PORT);              // converting unsigned short integer hostshort to network byte order

    if (bind(sock1, (struct sockaddr *)&address, addrlen) < 0)
    { // binding the socket to the address specified in sockaddr_in address
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(sock1, NUM_CLIENTS) < 0)
    { // checking that the number of clients does not exceed 10
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    // set attributes for thread
    if (pthread_attr_init(&pthread_attr) != 0)
    {
        perror("pthread_attr_init");
        exit(1);
    }

    // create detached thread
    if (pthread_attr_setdetachstate(&pthread_attr, PTHREAD_CREATE_DETACHED) != 0)
    {
        perror("pthread_attr_setdetachstate");
        exit(1);
    }

    while (1)
    {

        if ((sock2 = accept(sock1, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0)
        { // socket to accept incoming client connection
            perror("Accept failed");
            exit(EXIT_FAILURE);
        }

        // allocate dynamic memory to pass the socket to thread
        int *socket = (int *)malloc(sizeof(int));
        *socket = sock2;

        // create detached thread
        if ((pthread_create(&tid, &pthread_attr, clientHandler, socket)) != 0)
        { // check for error when creating thread
            perror("pthread_create");
            continue;
        }
    }
    // close server socket before exiting
    close(sock1);
}

// main loop to run the server shell
int main(int argc, char **argv)
{

    int val;
    // initialize all the named semaphores
    sem_unlink("/global");
    sem_unlink("/newArrival");
    sem_unlink("/processRe");
    global = sem_open("/global", O_CREAT, 0644, 0);
    new_arrival = sem_open("/newArrival", O_CREAT, 0644, 0);
    process_re = sem_open("/processRe", O_CREAT, 0644, 0);

    gss_loop();

    // destroy all the semaphores
    sem_close(global);
    sem_close(new_arrival);
    sem_close(process_re);
    sem_unlink("/global");
    sem_unlink("/newArrival");
    sem_unlink("/processRe");
    sem_destroy(global);
    sem_destroy(new_arrival);
    sem_destroy(process_re);

    return 0;
}
