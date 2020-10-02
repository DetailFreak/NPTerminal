#include<stdio.h>
#include<unistd.h>
#include<stdlib.h>
#include<string.h>
#include<ctype.h>
#include<sys/wait.h>

#define STDIN 0
#define STDOUT 1

#define DEBUG 3

char* get_input(){
    char *buf;
    size_t size = 1024;
    buf = (char*) malloc(size);
    printf("$ ");
    fflush(stdout);
    if (getline(&buf, &size, stdin) == -1){
            exit(EXIT_FAILURE);
    }
    return buf;
}


char **tokenize(char *line){
    /*
    returns the tokens in a command seperated by space
    
    input: individual command with args
    
    output: tokenized version of the command

    */
    char *tok;
    char **tokens = malloc(64 * sizeof(char*));
    char *sep = " ";
    int index = 0;

    tok = strtok(line, sep);

    while(tok != NULL){
        char *newline = strchr(tok, '\n');
        if(newline)
            *newline=0;
        tokens[index] = tok;
        index++;

        tok = strtok(NULL, sep);
    }
    tokens[index] = NULL;
    return tokens;
}

char **parse_input(char *input){
    /* 
    
    returns the list of commands (seperated by pipe in the input)
    
    input: user input from getline
    
    output: set of commands

    */
    char *tok;
    char **command = malloc(64 * sizeof(char *));
    char *sep = "|";
    int i = 0;

    tok = strtok(input, sep);

    while(tok != NULL){
        char *newline = strchr(tok, '\n');
        if(newline)
            *newline=0;
        // command[i] = malloc(strlen(tok) + 2);
        command[i] = tok;
        // strcpy(command[i], tok);
        // strcat(command[i], "\n");
        // printf("%s\t",  command[i]);
        i++;
        tok = strtok(NULL, sep);
    }
    command[i] = NULL;
    return command;
}


int count_pipe(char *input){
    /*
        returns the number of pipes present in the input
    */
    int count = 0;
    for (int i = 0; i < strlen(input)-1; i++) { 
        if(input[i] ==  '|'){
            count++;
        }
    }
    return count;
}

void close_safe(int fd){
    if(close(fd) == -1){
        perror("close");
    } else {
        #if DEBUG > 2
        fprintf(stderr, "( -x- ) %d closed fd (%d)\n", getpid(), fd);
        #endif
    }
}

void dup2_safe(int fd1, int fd2){
    if(dup2(fd1, fd2) == -1){
        perror("dup2");
    } else {
        #if DEBUG > 2
        fprintf(stderr, "( dup ) %d dup2ed fd (%d, %d)\n", getpid(), fd1, fd2);
        #endif
    }
}

void pipe_safe(int *fd){
    if (pipe(fd) == -1){
        perror("pipe");
    } else {
        #if DEBUG > 1
        printf("( ==> ) %d pipped fd (%d, %d)\n", getpid(), fd[0], fd[1]);
        #endif
    }
}

int **init_pipes(int numPipes){
    // printf("=====> Initializing %d pipe(s) for pid %d\n", numPipes, getpid());
    int **pipes = malloc(numPipes*sizeof(int*));
    for(int i = 0; i<numPipes; ++i){
        pipes[i] = malloc(2*sizeof(int));
        pipe_safe(pipes[i]);
    } 
    // printf("=====> Done\n");
    return pipes;
}

int execute_pipe(char **args, int *inputPipe, int *outputPipe){
    /*
        Generic execution command with/without pipes
        in case pipes are used:
            0. Closes unneeded pipe ends.
            1. it duplicates the pipefd to STDIN/OUT

    */    
    int childPid = fork();
    if (childPid == 0){
        #if DEBUG > 1
        printf("( --< ) %d forked child (%d, \"%s\")\n", getppid(), getpid(), args[0]);
        #endif
        if (inputPipe){
            close_safe(inputPipe[1]);
            dup2_safe(inputPipe[0], STDIN);
        }
        if (outputPipe){
            close_safe(outputPipe[0]);
            dup2_safe(outputPipe[1], STDOUT);
        }
        execvp(args[0], args);
        perror("exec_error");
        exit(EXIT_FAILURE);
    } 
    else if (childPid < 0){
        perror("fork");
    }
    return childPid;
}

int execute(char **args){
    #if DEBUG
    printf("[Executing \"%s\"]\n",args[0]);
    #endif
    int status = execute_pipe(args, NULL, NULL); 
    #if DEBUG
    printf("[Done]\n",args[0]);
    #endif
    return status;
}

char *remove_space(char *str){
    /*
        Removes whitespace from both ends of a string, 
        this function modifies the existing string.
    */
    int start = 0, i;
    int end = strlen(str) - 1;
    while (isspace((unsigned char) str[start])){
        start++;
    }
    while ((end >= start) && isspace((unsigned char) str[end])){
        end--;
    }
    for (i = start; i <= end; i++){
        str[i - start] = str[i];
    }
    str[i - start] = '\0';
    return str;
}

int execute_multipipe(char **commands, int pipeCount){
    /*
        Executes each command using pipes as follows:
        0. Initializes all pipes.
        1. Waits for previous process (if any) to complete.
        2. Strips whitespace and tokenizes command to be executed.
        3. Executes the command by passing either a pipe or NULL to execute_pipe.
        4. closes last pipe fds.
    */
    #if DEBUG
    printf("[Executing multipipe, pipe(s):%d]\n", pipeCount);
    #endif
    int  ** pipes = init_pipes(pipeCount);
    char ** args  = tokenize(remove_space(commands[0]));
    int     pid   = execute_pipe(args, NULL, pipes[0]);
    free(args);
    for(int i = 1; i < pipeCount; ++i){
        waitpid(pid, NULL, 0);
        args = tokenize(remove_space(commands[i]));
        pid  = execute_pipe(args, pipes[i-1], pipes[i]);
        close_safe(pipes[i-1][0]);
        close_safe(pipes[i-1][1]);
        // free(pipes[i-1]);
    }
    waitpid(pid, NULL,0);
    args = tokenize(remove_space(commands[pipeCount]));
    pid  = execute_pipe(args, pipes[pipeCount - 1], NULL);
    close_safe(pipes[pipeCount - 1][0]);
    close_safe(pipes[pipeCount - 1][1]);
    //free(args);
    // free(pipes[pipeCount-1]);
    // free(pipes);
    waitpid(pid, NULL, 0);
    #if DEBUG
    printf("[Done]\n");
    #endif
    return 1;
}


void cmd_loop(){
    /*
    1.  read input
    2.  parse the string and tokenize
    3.  check tokens 
    4.  execute
    */
    char *input;
    char **args;
    char **commands;
    int status, pipeCount;

    do{
        input = get_input();
        pipeCount = count_pipe(input);

        if(pipeCount == 0){
            args = tokenize(input); 
            status = execute(args);
            free(args);
        }else{
            commands = parse_input(input);
            status = execute_multipipe(commands, pipeCount);
            free(commands);
        }
        free(input);
    }while(1);
}



int main(int argc, char *argv[]){
    cmd_loop();
    return 0;
}