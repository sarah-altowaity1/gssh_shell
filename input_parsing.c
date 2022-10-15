#include "input_parsing.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define DELIMITERS " \t\n" 
#define PIPE "|"

char* read_input(){
    size_t size = 64; // initialize the size of the buf array 
    char* buf = (char*)malloc(size); // dynamically allocate space 

    int i = 0; // variable used to access the elements of array buf 
    while (true){ 
        char c = getchar(); // read a character from the user 
        if ((c == EOF) || (c == '\n')){ // return the array when the return key is hit or the EOF character is read from the input stream
            buf[i] = '\0'; // add the null character to the character string
            return buf;
        }
        buf[i] = c; // store character in buf
        i++;
        if (i >= size){ // dynamically resize the array if user input exceeds the allocated array size
            size = size * 2;
            buf = realloc(buf, size);
            if (buf == NULL){ // detect and handle allocation errors by exiting the program 
                perror("gssh: error in allocation.");
                exit(EXIT_FAILURE);
            }
        }

    }
}

char** tokenize(char* buf){
    // allocate memory for an array of strings to hold the tokens
    size_t size = 64;
    char** token_arr = malloc(size * sizeof(char*)); 

    char *curr = strtok(buf, DELIMITERS); // get first token 

    int i = 0;
    while (curr != NULL)
    {
        remove_special_chars(curr);
        token_arr[i] = curr; // store token in token_array 
        i++; 
        curr = strtok(NULL, DELIMITERS); // get the subsequent token 
        if (i >= size){ // dynamically expand the memory allocated for the token_array if the number of tokens exceed the allocated memory
            size = size * 2;
            token_arr = realloc(token_arr,size);
        }
        if (token_arr == NULL){ // detect and handle errors in execution by printing an error message and exiting the program 
            perror("gssh: error in allocation.");
            exit(EXIT_FAILURE);
        }
    }
    token_arr[i] = NULL; // add the NULL pointer as the last element of the token_arr

    return token_arr;
}

char** tokenizePipes(char* buf, int* no_cmds){
   // allocate memory for an array of strings to hold the pipe separated strings
    size_t size = 64;
    char** token_arr = malloc(size * sizeof(char*)); 

    char *curr = strtok(buf, PIPE); // get first token 

    int i = 0;
    while (curr != NULL)
    {
        token_arr[i] = curr; // store token in token_array 
        i++; 
        curr = strtok(NULL, PIPE); // get the subsequent token 
        if (i >= size){ // dynamically expand the memory allocated for the token_array if the number of tokens exceed the allocated memory
            size = size * 2;
            token_arr = realloc(token_arr,size);
        }
        if (token_arr == NULL){ // detect and handle errors in execution by printing an error message and exiting the program 
            perror("gssh: error in allocation.");
            exit(EXIT_FAILURE);
        }
    }
    token_arr[i] = NULL; // add the NULL pointer as the last element of the token_arr

    *no_cmds = i;
    return token_arr; 
}

void remove_special_chars(char * str){
    for(int i = 0; str[i] != '\0'; i++){
        // while((str[i] < 'a' || str[i] > 'z') && (str[i] < 'A' || str[i] > 'Z') && (str[i] != '\0') && (str[i] != '.') && (str[i] != '-')){
            if(str[i] == '\'' || str[i] == '\"'){
            int j;
            for(j = i; str[j] != '\0'; j++){
                str[j] = str[j+1];
            }
            str[j] = '\0';
        }
    }
}