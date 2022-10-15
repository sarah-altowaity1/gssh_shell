#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/time.h>
#include <signal.h>

#include "input_parsing.h"

#define PORT 5793

// Size of each chunk of data received
#define CHUNK_SIZE 512

// client socket
int sock = 0;

// handler for ctrl-c to close the socket and exit client
void clientExitHandler(int sig_num)
{
  send(sock, "exit", strlen("exit"), 0);
  close(sock);
  printf("\nExiting client. \n");
  fflush(stdout);
  exit(0);
}

/*
  Non-blocking socket to receive data in chunks.
  Credits to Silver Moon for the function: https://www.binarytides.com/receive-full-data-with-recv-socket-function-in-c/
*/
int recv_timeout(int s, float timeout)
{
  // total size of data received and size received on reach iteration
  int size_recv, total_size = 0;
  // structure to store the time
  struct timeval begin, now;
  // buffer for storing the received data
  char chunk[CHUNK_SIZE];
  // time difference between receiving data and current time
  double timediff;

  // make socket non blocking
  // fcntl(s, F_SETFL, O_NONBLOCK);

  // beginning time
  gettimeofday(&begin, NULL);

  while (1)
  {
    // current time
    gettimeofday(&now, NULL);

    // time elapsed in seconds
    timediff = (now.tv_sec - begin.tv_sec) + 1e-6 * (now.tv_usec - begin.tv_usec);

    // if you got some data, then break after timeout
    if (total_size > 0 && timediff > timeout)
    {
      break;
    }

    // if you got no data at all, wait twice the timeout
    else if (timediff > timeout * 2)
    {
      break;
    }

    memset(chunk, 0, CHUNK_SIZE); // clear the variable
    if ((size_recv = recv(s, chunk, CHUNK_SIZE, MSG_DONTWAIT)) < 0)
    {
      // if nothing was received then we want to wait for 0.1s before trying again
      usleep(100000);
    }
    else
    {
      total_size += size_recv;
      // print the received data
      printf("%s", chunk);
      // reset beginning time
      gettimeofday(&begin, NULL);
    }
  }

  // clear the standard out
  fflush(stdout);
  return total_size;
}

int main()
{
  // handle ctrl-c to exit the client
  if (signal(SIGINT, clientExitHandler) == SIG_ERR)
  {
    perror("signal");
    exit(1);
  }

  // Structure to store the remote server IP address and port.
  struct sockaddr_in serv_addr;

  /* Creating socket file descriptor with communication: domain of internet protocol
  version 4, type of SOCK_STREAM for reliable/conneciton oriented communication,
  and protocol of internet*/
  if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) // Checking for failed socket creation.
  {
    printf("\n Socket creation error \n");
    return -1;
  }

  // Setting the address to connect socket to server.
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(PORT);

  // Convert IPv4 and IPv6 addresses from text to binary form and set the ip
  if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0)
  {
    printf("\nInvalid address/ Address not supported \n");
    return -1;
  }
  // Connect the socket with the adddress and establish connnection with the server.
  if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
  {
    printf("\nConnection Failed \n");
    return -1;
  }

  char received[1024]; // string to receive the message
  while (1)
  {
    // Display shell prompt.
    char *line;
    printf("gssh$ ");

    // Read input line into buffer.
    line = read_input();

    // if there is no input, do nothing and read input again
    if (strlen(line) == 0)
    {
      continue;
    }

    // Detecting and handling the exit command
    if (strcmp(line, "exit") == 0)
    {
      send(sock, "exit", strlen("exit"), 0);
      close(sock);
      printf("\n Exiting client. \n");
      fflush(stdout);
      break;
    }

    // Sending the command to the server for handling.
    send(sock, line, strlen(line), 0);

    if (strncmp(line, "./demo", 6) == 0)
    {
      printf("sending demo\n");
      memset(received, 0, 1024);
      int n = recv(sock, received, 4096, 0);
      received[n] = '\0';
      printf("%s\n", received);
    }
    else
    {
      int total_recv = recv_timeout(sock, 2);
    }

    // receiving response in chunks

    // clear the standard out
    fflush(stdout);
    fflush(stderr);
    free(line); // Deallocate dynamically allocated buffer holding user input.
  }

  close(sock); // close the socket/end the conection
}