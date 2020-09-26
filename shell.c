#include<stdio.h>
#include<unistd.h>
#include<stdlib.h>
#include<string.h>
#include <sys/wait.h>


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
        command[i] = tok;
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



int execute(char **args){
    /*
        Executes the simple command without any pipes 
    */
    pid_t pid, wpid;
    int status;
        
    pid = fork();
    if(pid == 0){
        execvp(args[0], args);
        perror("exec_error");
        exit(EXIT_FAILURE);
    } else if(pid < 0){
        perror("err");
    }else{
        wpid = waitpid(pid, &status, WUNTRACED);
    }
    return 1;
}


int execute_pipe(char **args){
    /*
        inprogress...
    */
    pid_t pid, wpid;
    int status;
        
    pid = fork();
    if(pid == 0){
        execvp(args[0], args);
        perror("exec_error");
        exit(EXIT_FAILURE);
    } else if(pid < 0){
        perror("err");
    }else{
        wpid = waitpid(pid, &status, WUNTRACED);
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
    int status, pipeCount;

    do{
        input = get_input();
        pipeCount = count_pipe(input);

        if(pipeCount == 0){
            args = tokenize(input); 
            status = execute(args);
        }else{
            commands = parse_input(input);
            //to-do: execute piped commands
        }

        free(input);
        free(args);
    }while(1);
}

int main(int argc, char *argv[]){
    cmd_loop();
    return 0;
}