# Shell

This is the shell implementation for the Operating Systems class (CS-3010). This project has been implemented in 4 phases.

This project was done by Gautham Dinesh and Sarah Al-Towaity.

## Phase 1 - Local Shell

For the first phase, we had to implement a local shell that can run at least 15 command line commands and make use of one pipe, two pipes and three pipes.

### Steps

1. Input

- Read and store the input from the user with the read_input() function in input_parsing.c

2. Tokenization

- Tokenize the stored input by separating using white space to get the command and the arguments as well as any flags with the tokeinze() function in input_parsing.c

3. Execution

- Pass the tokenized input to `execvp` and run the command using the exec() function in exec.c

4. Tokenization with pipes

- Before separating by space, we separate the input t using pipes and count the number of pipes using the tokenizePipes() function in input_parsing.c
- Pass the commands into the execution function depending on the number of pipes

5. Execution with one, two and three pipes

- We created three different commands to handle the different number of pipes and these would be chosen based on that using the execPipe(), execTwoPipes() and execThreePipes() functions in exec.c

### Test cases

#### 15 commands

1. mkdir
2. ls
3. cat
4. sort
5. grep
6. echo
7. cp
8. rm
9. mv
10. wc
11. date
12. touch
13. rmdir
14. pwd
15. ps
16. man
17. ping
18. tee

#### Pipes

1. ls | grep in
2. cat main.c | grep exec
3. cat Makefile | grep main | wc
4. ls | grep main | tee main.txt
5. cat Makefile | grep main | grep gcc | wc
6. ls | grep main | tee main.txt | wc
