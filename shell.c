#include<stdio.h>
#include<unistd.h>
#include<stdlib.h>
#include<string.h>
#include <sys/wait.h>

#define STDIN 0
#define STDOUT 1

char* get_input(){
    char *buf;
    size_t size = 1024;
    buf = (char*) malloc(size);
    printf("$ ");
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



int execute(char **args, int *inputPipe, int *outputPipe){
    /*
        Executes the simple command without any pipes 
    */
    pid_t pid, wpid;
    int status;
        
    pid = fork();
    if(pid == 0){
        
        if (inputPipe != NULL){
            close(inputPipe[1]);
            if (dup2(inputPipe[0], STDIN) == -1){
                perror("dup2");
            } close(inputPipe[0]);
        }
        if (outputPipe != NULL){
            close(outputPipe[0]);
            if (dup2(outputPipe[1], STDOUT) == -1){
                perror("dup2");
            } close(outputPipe[1]);
        }
        printf("executing %s\n", args[0]);
        execvp(args[0], args);
        perror("exec_error");
        exit(EXIT_FAILURE);
    } else if(pid < 0){
        perror("fork");
    }else{
        wpid = waitpid(pid, &status, WUNTRACED);
    }
    return 1;
}


int** init_pipes(int pipeCount){
    int ** pfds = malloc(pipeCount * sizeof(int *));
    for(int i = 0 ; i < pipeCount; ++i){
        pfds[i] = malloc(2 * sizeof(int));
        if(pipe(pfds[i]) == -1){
            perror("pipe");
        }
    }
    return pfds;
}

int execute_pipe(char **commands, int **fds, int pipeCount){
    /*
        inprogress...
    */
    printf("in execute_pipe, pipe(s) = %d\n", pipeCount);
    // need to pass '-' as input for stdinputw
    for(int i = 0; i <= pipeCount; ++i){
        char** args = tokenize(commands[i]);
        int *inputPipe = NULL, *outputPipe = NULL;
        
        if(i == 0){
            outputPipe = fds[0];
        } else if(i == pipeCount){
            inputPipe = fds[i-1];
        } else {
            inputPipe = fds[i-1];
            outputPipe = fds[i];
        }

        execute(args, inputPipe, outputPipe);
        
        // free(args);
   }
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
    int status, pipeCount, **pipefds;

    do{
        input = get_input();
        pipeCount = count_pipe(input);

        if(pipeCount == 0){
            args = tokenize(input); 
            status = execute(args, NULL, NULL);
        }else{
            printf("pipe(s) : %d\n", pipeCount);
            commands = parse_input(input);
            pipefds = init_pipes(pipeCount);
            status = execute_pipe(commands, pipefds, pipeCount);
            free(commands);
            free(pipefds);
        }

        free(input);
        free(args);
    }while(1);
}



int main(int argc, char *argv[]){
    cmd_loop();
    return 0;
}