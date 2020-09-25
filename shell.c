#include<stdio.h>
#include<unistd.h>
#include<stdlib.h>
#include<string.h>
#include <sys/wait.h>


char* get_input(){
    char *buf;
    size_t size = 1024;
    buf = (char*) malloc(size);
    int bytes_read;
    printf("$ ");
    if (getline(&buf, &size, stdin) == -1){
            exit(EXIT_FAILURE);
    }
    return buf; 
}


char **tokenize(char *line){
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


int execute(char **args){
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
    int status;

    do{
        input = get_input();
        args = tokenize(input); 
        status = execute(args);
        free(input);
        free(args);
    }while(1);
}

int main(int argc, char *argv[]){
    cmd_loop();
    return 0;
}