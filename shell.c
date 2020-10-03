#include<stdio.h>
#include<unistd.h>
#include<stdlib.h>
#include<string.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h> 
#include <sys/stat.h> 
#include <sys/types.h> 
#include <ctype.h>
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

char **parse_input(char *input, char *sep){
    /* 
    
    returns the list of commands (seperated by pipe in the input)
    
    input: user input from getline
    
    output: set of commands

    */
    char *tok;
    char **command = malloc(64 * sizeof(char *));
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


char *remove_space(char *str){

    int i, start = 0;
    int end = strlen(str) - 1;

    while (isspace((unsigned char) str[start]))
        start++;
    while ((end >= start) && isspace((unsigned char) str[end]))
        end--;

    for (i = start; i <= end; i++)
        str[i - start] = str[i];
    str[i - start] = '\0';

    return str;
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


int execute_2Pipe(char **args){

    for(int i=0; i<2; i++){
        args[i] = remove_space(args[i]);
        // printf("%s \n", args[i]);
    }

    int fd; 

	// FIFO file path 
	char * myfifo = "/tmp/myfifo"; 
	
    if (mkfifo ("/tmp/myfifo", S_IRUSR| S_IWUSR) < 0) {
        printf("err: %d fifo create \n", errno);
        return -1;
    }

    pid_t pid, wpid, pid1, pid2, pid3, pid4;
    char **cmd1, **cmd2, **cmd3;

    char *test = malloc(256 * sizeof(char *));


    char **rhs_commands = parse_input(args[1], ",");
    pid1 = fork();
    if(pid1 == 0){
        int fd;
        puts("test comment 1");
        fd = open("/tmp/myfifo", O_RDONLY);
        dup2(fd, STDIN_FILENO);
        close(fd);

        // read(fd, test, 100);
        // printf("\n === %s === \n", test);

        cmd2 = tokenize(rhs_commands[0]);
        // printf("test comment 4 - %s", cmd2[0]);

        execvp(cmd2[0], cmd2);


        perror("exec_error cmd2");
        exit(EXIT_FAILURE);
    }else if(pid1 < 0){
        perror("exec_error cmd2");
    }else{
        //parent
    }



    int p1_fd[2];

    if(pipe(p1_fd) == -1){
        printf("error p1 \n");
    }

    pid = fork();

    if(pid == 0){
        cmd1 = tokenize(args[0]);
        puts("test comment 2");
    
        close(p1_fd[0]);
        dup2(p1_fd[1], STDOUT_FILENO);
        close(p1_fd[1]);

        execvp(cmd1[0], cmd1);
        perror("exec_error : cmd1");
        exit(EXIT_FAILURE);
    } else if(pid < 0){
        perror("err fork1 : ");
    }else{
        //parent
    }



    int p2_fd[2];
    if(pipe(p2_fd) == -1) {
        printf("error p2_fd \n");
    }

    pid2 = fork();
    if(pid2 == 0){
        puts("test comment 3.");

        close(p1_fd[1]);
        //duplicate the read end of pipe 1 to standard input
        if(dup2(p1_fd[0], STDIN_FILENO) == -1){
            printf("err: %d dup pipe 1 output", errno);
        } 


        //close the fd as its already duplicated
        close(p1_fd[0]);

        //close the read end of pipe 2
        close(p2_fd[0]);
        //duplicate the write end of pipe 2 to standard output
        dup2(p2_fd[1], STDOUT_FILENO);
        //close write end
        close(p2_fd[1]);

        // puts("test comment 3.1");

        char *tee_args[]={"tee", "/tmp/myfifo", NULL};
        execvp(tee_args[0], tee_args);
        // execlp("tee", "tee", "/tmp/myfifo", NULL);
        perror("error tee : ");

    }else if(pid2 < 0){
        perror("error tee1 : ");
    }else{
        //parent
    }

    close(p1_fd[0]);
    close(p1_fd[1]);
    
    wait(NULL);
    wait(NULL);
    wait(NULL);

    pid3 = fork();
    if(pid3 == 0){
        close(p2_fd[1]);
        dup2(p2_fd[0], STDIN_FILENO);
        close(p2_fd[0]);

        cmd3 = tokenize(rhs_commands[1]);

        execvp(cmd3[0], cmd3);
        printf("error cmd3 \n");
    }else if(pid3 < 0){
        perror("error cmd3 : ");
    }else{
        //
    }

    pid4 = fork();

    if(pid4 == 0){
        // puts("clearing pipe...");
        execlp("rm", "rm", "-r", "/tmp/myfifo", NULL);
        printf("err: %d FIFO delete \n", errno);
    }else if(pid4 < 0){

    }else{
        //
    }

    return 1;
}



int execute_3Pipe(char **args){

    for(int i=0; i<2; i++){
        args[i] = remove_space(args[i]);
        // printf("======= %s =========== \n", args[i]);
    }

    int fd; 

	// FIFO file path 
	char * fifo1 = "/tmp/fifo1"; 
	char * fifo2 = "/tmp/fifo2"; 
	
    if (mkfifo ("/tmp/fifo1", S_IRUSR| S_IWUSR) < 0) {
        printf("err: %d fifo1 create \n", errno);
        return -1;
    }
    if (mkfifo ("/tmp/fifo2", S_IRUSR| S_IWUSR) < 0) {
        printf("err: %d fifo2 create \n", errno);
        return -1;
    }

    pid_t pid, wpid, pid1, pid2, pid3, pid4, pid5, pid6;
    char **cmd1, **cmd2, **cmd3, **cmd4;

    // char *test = malloc(256 * sizeof(char *));

    char **rhs_commands = parse_input(args[1], ",");
    // pid1 = fork();

    switch (fork())
    {
    case -1:
        printf("fork error %d: cmd2", errno);

    case 0:{
        int fd;
        puts("test comment 1");
        fd = open("/tmp/fifo1", O_RDONLY);
        dup2(fd, STDIN_FILENO);
        close(fd);

        // read(fd, test, 100);
        // printf("\n === %s === \n", test);

        cmd2 = tokenize(rhs_commands[0]);
        printf("test comment 4 - %s", cmd2[0]);

        execvp(cmd2[0], cmd2);


        perror("exec_error cmd2");
        exit(EXIT_FAILURE);
    }    
    default:
        break;
    }


    switch (fork())
    {
    case -1:
        printf("fork error %d: cmd3", errno);
    case 0:{
        int fd;
        puts("test comment 2");
        fd = open("/tmp/fifo2", O_RDONLY);
        dup2(fd, STDIN_FILENO);
        close(fd);

        // read(fd, test, 100);
        // printf("\n === %s === \n", test);

        cmd3 = tokenize(rhs_commands[1]);
        // printf("test comment 4 - %s", cmd3[0]);

        execvp(cmd3[0], cmd3);


        perror("exec_error cmd3");
        exit(EXIT_FAILURE);
    }    
    default:
        break;
    }


    int p1_fd[2];

    if(pipe(p1_fd) == -1){
        printf("error p1 \n");
    }

    pid = fork();

    if(pid == 0){
        cmd1 = tokenize(args[0]);
        puts("test comment 2");
    
        close(p1_fd[0]);
        dup2(p1_fd[1], STDOUT_FILENO);
        close(p1_fd[1]);

        execvp(cmd1[0], cmd1);
        perror("exec_error : cmd1");
        exit(EXIT_FAILURE);
    } else if(pid < 0){
        perror("err fork1 : ");
    }else{
        //parent
    }


    int p2_fd[2];
    if(pipe(p2_fd) == -1) {
        printf("error p2_fd \n");
    }

    pid2 = fork();
    if(pid2 == 0){
        puts("test comment 3.");

        close(p1_fd[1]);
        //duplicate the read end of pipe 1 to standard input
        if(dup2(p1_fd[0], STDIN_FILENO) == -1){
            printf("err: %d dup pipe 1 output", errno);
        } 


        //close the fd as its already duplicated
        close(p1_fd[0]);

        //close the read end of pipe 2
        close(p2_fd[0]);
        //duplicate the write end of pipe 2 to standard output
        dup2(p2_fd[1], STDOUT_FILENO);
        //close write end
        close(p2_fd[1]);

        // puts("test comment 3.1");

        char *tee_args[]={"tee", "/tmp/fifo1",  "/tmp/fifo2", NULL};
        execvp(tee_args[0], tee_args);
        perror("error tee : ");

    }else if(pid2 < 0){
        perror("error tee1 : ");
    }else{
        //parent
    }

    close(p1_fd[0]);
    close(p1_fd[1]);
    
    wait(NULL);
    wait(NULL);
    wait(NULL);

    pid3 = fork();
    if(pid3 == 0){
        close(p2_fd[1]);
        dup2(p2_fd[0], STDIN_FILENO);
        close(p2_fd[0]);

        cmd3 = tokenize(rhs_commands[1]);

        execvp(cmd3[0], cmd3);
        printf("error cmd3 \n");
    }else if(pid3 < 0){
        perror("error cmd3 : ");
    }else{
        //
    }

    switch (fork())
    {
    case -1:
        printf("error %d in rm fork - 1", errno);
        break;
    case 0: {
        execlp("rm", "rm", "-r", "/tmp/fifo1", NULL);
        printf("err: %d FIFO delete 1 \n", errno);
    }
    default:
        break;
    }


    switch (fork())
    {
    case -1:
        printf("error %d in rm fork - 2", errno);
        break;
    case 0: {
        execlp("rm", "rm", "-r", "/tmp/fifo2", NULL);
        printf("err: %d FIFO delete 2 \n", errno);
    }
    default:
        break;
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

        // check in the reverse order
        if(strstr(input, "|||") != NULL){
            //triple pipe
            commands = parse_input(input, "|||");
            execute_3Pipe(commands);    
        }else if(strstr(input, "||") != NULL){
            //double pipe
            commands = parse_input(input, "||");
            execute_2Pipe(commands);
        }else if(strstr(input, "|") != NULL){
            // single pipe
            pipeCount = count_pipe(input);
            if(pipeCount == 0){
                args = tokenize(input); 
                status = execute(args);
            }else if(pipeCount > 0){
                commands = parse_input(input, "|");
                //to-do: multiple pipes
            }
        }else{
            args = tokenize(input); 
            status = execute(args);
        }
         
        free(input);
        free(args);
        // free(commands);
    }while(1);
}

int main(int argc, char *argv[]){
    cmd_loop();
    return 0;
}