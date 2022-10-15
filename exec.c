#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <errno.h>
#include <fcntl.h>

#include "input_parsing.h"
#include "exec.h"

// exec pipe-less commands
int exec(char **cmds, int *sock2)
{

    pid_t pid;
    pid = fork();

    // child process
    if (pid == 0)
    {
        // dup output and error messages to socket
        dup2(*sock2, STDOUT_FILENO);
        dup2(*sock2, STDERR_FILENO);
        close(*sock2);

        // execute commands
        if (execvp(cmds[0], cmds) == -1)
        {
            perror("gssh: command not found");
            exit(EXIT_FAILURE);
        }
    }
    else if (pid > 0)
    {
        wait(NULL);
    }
    else
    {
        // fork failed
        perror("Fork failed");
    }

    return 1;
}

int execPipe(char **piped_array, int *sock2)
{
    /* gets the first and second elements of the piped_array
    which has been tokenized by pipe and splits each by the space delimiters into a list of
    strings  */
    char **first = tokenize(piped_array[0]);
    char **sec = tokenize(piped_array[1]);

    pid_t child;
    child = fork(); // create child process

    // child process
    if (child == 0)
    {

        pid_t child2;
        int fd[2]; // array of file descriptors for the pipe
        pipe(fd);  // create the pipe

        child2 = fork();

        if (child2 == 0)
        { // create a child to handle the first command

            // dup error messages to socket
            dup2(*sock2, STDERR_FILENO);
            close(*sock2);
            dup2(fd[1], 1); // close and redirect standard output to our pipe
            close(fd[1]);   // close the write end of the pipe
            close(fd[0]);   // close the read end of the pipe

            // checking for empty commands for parse error
            if (first[0] == NULL)
            {
                perror("gssh: parse error near `|\'");
                exit(EXIT_FAILURE);
            }

            // execute first command
            if (execvp(first[0], first) == -1)
            {
                perror("gssh: command not found");
                exit(EXIT_FAILURE); // exit with exit failure status since exec would have terminated abnormally if this part is reached
            }
        }
        else if (child2 > 0)
        { // in the context of child

            // dup output and error messages to socket
            dup2(*sock2, STDOUT_FILENO);
            dup2(*sock2, STDERR_FILENO);
            close(*sock2);
            dup2(fd[0], 0); // close and redirect standard input to our pipe
            close(fd[1]);   // close the write end of the pipe
            close(fd[0]);   // close the read end of the pipe

            // checking for empty commands for parse error
            if (sec[0] == NULL)
            {
                perror("gssh: parse error near `|\'");
                exit(EXIT_FAILURE);
            }

            // execute second command
            if (execvp(sec[0], sec) == -1)
            {
                perror("gssh: command not found");
                exit(EXIT_FAILURE); // exit with exit failure status since exec would have terminated abnormally if this part is reached
            }
        }
        else
        {
            // fork failed
            perror("Fork failed");
        }

        return 0;
    }
    else if (child > 0)
    {
        // parent process
        wait(NULL); // wait for the child process to terminate
    }
    else
    {
        // fork failed
        perror("Fork failed");
    }

    // free the dynamically allocated array of arrays
    free(first);
    free(sec);

    return 1;
}

int execTwoPipes(char **piped_array, int *sock2)
{
    /* gets the the three elements (commands) of the piped_array
    which has been tokenized by pipe and splits each by the space delimiters into a list of
    strings  */
    char **first = tokenize(piped_array[0]);
    char **sec = tokenize(piped_array[1]);
    char **third = tokenize(piped_array[2]);

    pid_t child;
    child = fork(); // create a child

    // child process
    if (child == 0)
    {
        // initialize and create child and pipes.
        pid_t child2;
        int fd[2];
        int fd2[2];
        pipe(fd);
        pipe(fd2);

        child2 = fork();

        if (child2 == 0)
        { // child2 process

            // dup error messages to socket
            dup2(*sock2, STDERR_FILENO);
            close(*sock2);
            dup2(fd[1], 1); // close and redirect standard output to write the output of the first command to the write end of the first pipe
            // close both ends of both pipes
            close(fd[1]);
            close(fd[0]);
            close(fd2[0]);
            close(fd2[1]);

            // checking for empty commands for parse error
            if (first[0] == NULL)
            {
                perror("gssh: parse error near `|\'");
                exit(EXIT_FAILURE);
            }

            // execute first command
            if (execvp(first[0], first) == -1)
            {
                perror("gssh: command not found");
            }
            exit(EXIT_FAILURE);
        }
        else if (child2 > 0)
        {                          // context of child1 as parent of child2
            pid_t child3 = fork(); // create a child

            if (child3 == 0)
            { // context of child3

                // dup error messages to socket
                dup2(*sock2, STDERR_FILENO);
                close(*sock2);
                dup2(fd[0], 0);  // close and redirect standard input to the read end of the first pipe; child3 will read the output of the first command
                dup2(fd2[1], 1); // close and redirect standard output to the write end of the second pipe; child3 will write the output of the second command to the write end of the second pipe
                // close both ends of both pipes
                close(fd[1]);
                close(fd2[0]);
                close(fd[0]);
                close(fd2[1]);

                // checking for empty commands for parse error
                if (sec[0] == NULL)
                {
                    perror("gssh: parse error near `|\'");
                    exit(EXIT_FAILURE);
                }

                // execute second command
                if (execvp(sec[0], sec) == -1)
                {
                    perror("gssh: command not found");
                }

                exit(EXIT_FAILURE);
            }
            else if (child3 > 0)
            { // context of child2 as the parent of child 3
                // dup output and error messages to socket
                dup2(*sock2, STDOUT_FILENO);
                dup2(*sock2, STDERR_FILENO);
                close(*sock2);
                dup2(fd2[0], 0); // close and redirect standard input to the read end of the second pipe; the third command will execute using input read from fd2[0]
                // close both ends of both pipes
                close(fd[1]);
                close(fd2[1]);
                close(fd[0]);
                close(fd2[0]);

                // checking for empty commands for parse error
                if (third[0] == NULL)
                {
                    perror("gssh: parse error near `|\'");
                    exit(EXIT_FAILURE);
                }

                // execute third command
                if (execvp(third[0], third) == -1)
                {
                    perror("gssh: command not found");
                }

                exit(EXIT_FAILURE);
            }

            else
            {

                // fork failed
                perror("Fork failed");
            }
        }
        else
        {
            // fork failed
            perror("Fork failed");
        }

        return 0;
    }
    else if (child > 0)
    {
        // parent process
        wait(NULL);
    }
    else
    {
        // fork failed
        perror("Fork failed");
    }

    // free the dynamically allocated array of arrays
    free(first);
    free(sec);
    free(third);

    return 1;
}

int execThreePipes(char **piped_array, int *sock2)
{
    /* gets the four elements (commands) of the piped_array
    which has been tokenized by pipe and splits each by the space delimiters into a list of
    strings  */
    char **first = tokenize(piped_array[0]);
    char **sec = tokenize(piped_array[1]);
    char **third = tokenize(piped_array[2]);
    char **fourth = tokenize(piped_array[3]);

    pid_t child;
    child = fork(); // create a child from within which execution of commands will occur

    // child process
    if (child == 0)
    {
        // initialize and create a child and three pipes
        pid_t child2;
        int fd[2];
        int fd2[2];
        int fd3[2];
        pipe(fd);
        pipe(fd2);
        pipe(fd3);

        child2 = fork();

        if (child2 == 0)
        {
            // dup error messages to socket
            dup2(*sock2, STDERR_FILENO);
            close(*sock2);
            dup2(fd[1], 1); // close and redirect standard output to the first pipe
            // close both read and write ends of the three pipes
            close(fd[1]);
            close(fd[0]);
            close(fd2[0]);
            close(fd2[1]);
            close(fd3[0]);
            close(fd3[1]);

            // checking for empty commands for parse error
            if (first[0] == NULL)
            {
                perror("gssh: parse error near `|\'");
                exit(EXIT_FAILURE);
            }

            // execute first command
            if (execvp(first[0], first) == -1)
            {
                perror("gssh: command not found");
            }
            exit(EXIT_FAILURE);
        }
        else if (child2 > 0)
        {                          // context of child as the parent of child2
            pid_t child3 = fork(); // create a third child to execute the second command

            if (child3 == 0)
            {
                // dup error messages to socket
                dup2(*sock2, STDERR_FILENO);
                close(*sock2);
                dup2(fd[0], 0);  // close and redirect the standard input to read from the output of our first pipe (first command).
                dup2(fd2[1], 1); // close and redirect the standard output to write to our second pipe
                // close both the read and write ends of the pipes
                close(fd[1]);
                close(fd2[0]);
                close(fd[0]);
                close(fd2[1]);
                close(fd3[0]);
                close(fd3[1]);

                // checking for empty commands for parse error
                if (sec[0] == NULL)
                {
                    perror("gssh: parse error near `|\'");
                    exit(EXIT_FAILURE);
                }

                // execute second command
                if (execvp(sec[0], sec) == -1)
                {
                    perror("gssh: command not found");
                }

                exit(EXIT_FAILURE);
            }
            else if (child3 > 0)
            { // context of child2 as the parent of child3

                pid_t child4 = fork(); // create a fourth child to handle the third command

                if (child4 == 0)
                { // context of child4
                    // dup error messages to socket
                    dup2(*sock2, STDERR_FILENO);
                    close(*sock2);
                    dup2(fd2[0], 0); // close and redirect the standard input to our second pipe to read the output of child3 (second command)
                    dup2(fd3[1], 1);
                    close(fd[1]);
                    close(fd2[1]);
                    close(fd[0]);
                    close(fd2[0]);
                    close(fd3[0]);
                    close(fd3[1]);

                    // checking for empty commands for parse error
                    if (third[0] == NULL)
                    {
                        perror("gssh: parse error near `|\'");
                        exit(EXIT_FAILURE);
                    }

                    // execute third command
                    if (execvp(third[0], third) == -1)
                    {
                        perror("gssh: command not found");
                    }
                    exit(EXIT_FAILURE);
                }

                else if (child4 > 0)
                { // context of child3 as the parent of child4
                    // dup output and error messages to socket
                    dup2(*sock2, STDOUT_FILENO);
                    dup2(*sock2, STDERR_FILENO);
                    close(*sock2);
                    dup2(fd3[0], 0); // close and redirect standard input to the read end of the third pipe to read the output of the third command
                    // close both ends of all pipes
                    close(fd[1]);
                    close(fd2[1]);
                    close(fd[0]);
                    close(fd2[0]);
                    close(fd3[0]);
                    close(fd3[1]);

                    // checking for empty commands for parse error
                    if (fourth[0] == NULL)
                    {
                        perror("gssh: parse error near `|\'");
                        exit(EXIT_FAILURE);
                    }

                    // execute fourth command
                    if (execvp(fourth[0], fourth) == -1)
                    {
                        perror("gssh: command not found");
                    }
                    exit(EXIT_FAILURE);
                }
                else
                {

                    // fork failed
                    perror("Fork failed");
                }
            }

            else
            {

                // fork failed
                perror("Fork failed");
            }
        }
        else
        {
            // fork failed
            perror("Fork failed");
        }

        return 0;
    }
    else if (child > 0)
    {
        // parent process
        wait(NULL);
    }
    else
    {
        // fork failed
        perror("Fork failed");
    }
    // free dynamically allocated arrays of strings
    free(first);
    free(sec);
    free(third);
    free(fourth);

    return 1;
}

int cd_command(char *path, int *sock)
{

    // open file to check validity of path
    FILE *path_check = fopen(path, "r");

    // if path is null, dup the standard error and display the error message in client
    if (path_check == NULL)
    {
        dup2(*sock, STDERR_FILENO);
        perror("cd: ");
    }
    // otherwise exectute the change directory function
    else
    {
        chdir(path);
    }

    // close the file path
    fclose(path_check);

    return 1;
}