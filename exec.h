#ifndef EXEC
#define EXEC

int exec(char**, int*); // function to execute pipe-less commands.
int execPipe(char**, int*); // function to execute commands with one pipe.
int execTwoPipes(char**, int*); // function to execute commands with two pipes.
int execThreePipes(char**, int*); // function to execute commands with three pipes.
int cd_command(char *, int *); // fucntion to execute the change directory command 

#endif