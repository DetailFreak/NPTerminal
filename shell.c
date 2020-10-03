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


/*
    Set debug level:
    0 => no logs
    1 => show pid and status
    2 => show pids, status and forks
    3 => show pids, status, forks and fd related calls
*/
#define DEBUG 3

#define STDIN 0
#define STDOUT 1

#define NULL_ENDS 1
#define NO_NULL_ENDS 0

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

int **init_pipes(int numPipes, int type){
    // printf("=====> Initializing %d pipe(s) for pid %d\n", numPipes, getpid());
    numPipes = numPipes + 2*type;
    int **pipes = malloc((numPipes) *sizeof(int*));
    pipes[0] = NULL;
    pipes[numPipes- 1] = NULL;
    for(int i = type; i<numPipes-type; ++i){
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
            1. Duplicates the pipefd to STDIN/OUT

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

void print_status(int pid, int status, const char* command){
    if(WIFEXITED(status))
        printf("<SUCCESS> %d \"%s\" returned normally\n", pid, command);
    if(WIFSIGNALED(status))
        printf("<SIGNALLED> %d \"%s\" terminated by signal\n", pid, command);
}

int execute(char **args){
    #if DEBUG
    printf("[Executing \"%s\"]\n",args[0]);
    #endif

    int pid, status;
    
    pid = execute_pipe(args, NULL, NULL); 
    waitpid(pid, &status, 0);
    print_status(pid, status, args[0]);

    #if DEBUG
    printf("[Done]\n");
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
        0. Initializes all pipes. Two NULL pipes at the ends for cleaner code.
        1. Waits for previous process (if any) to complete.
        2. Strips whitespace and tokenizes command to be executed.
        3. Executes the command by passing either a pipe or NULL to execute_pipe.
        4. closes last pipe fds.
    */
    #if DEBUG
    printf("[Executing multipipe, #pipe(s) = %d]\n", pipeCount);
    #endif

    char ** args;
    int     pid, status, commandCount = pipeCount + 1;
    // Null ends option will create  extra dummy (null) start and end pipes.
    int  ** pipes = init_pipes(pipeCount, NULL_ENDS);
    
    for(int i = 0; i < commandCount; ++i){
        args = tokenize(remove_space(commands[i]));
        pid = execute_pipe(args, pipes[i], pipes[i + 1]);
        if (pipes[i]) {
            close_safe(pipes[i][0]);
            close_safe(pipes[i][1]);
            free(pipes[i]);
        }
        waitpid(pid, &status, 0);
        print_status(pid, status, args[0]);

        free(args);
    }
    free(pipes);
    
    #if DEBUG
    printf("[Done]\n");
    #endif

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
    close(p2_fd[0]);
    close(p2_fd[1]);
    wait(NULL);
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
    close(p2_fd[0]);
    close(p2_fd[1]);
    wait(NULL);
    wait(NULL);
    wait(NULL);
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
        if(*(input = get_input()) == '\n') continue;
        // check in the reverse order
        if(strstr(input, "|||") != NULL){
            //triple pipe
            commands = parse_input(input, "|||");
            execute_3Pipe(commands);
            free(commands);    
        }else if(strstr(input, "||") != NULL){
            //double pipe
            commands = parse_input(input, "||");
            execute_2Pipe(commands);
            free(commands);
        }else if(strstr(input, "|") != NULL){
            // single pipe
            pipeCount = count_pipe(input);
            if(pipeCount == 0){
                args = tokenize(input); 
                status = execute(args);
                free(args);
                
            }else if(pipeCount > 0){
                commands = parse_input(input, "|");
                status = execute_multipipe(commands, pipeCount);
                free(commands);
            }
        }else{
            args = tokenize(input); 
            status = execute(args);
            free(args);
        }
         
        free(input);
        // free(commands);
    }while(1);
}

int main(int argc, char *argv[]){
    cmd_loop();
    return 0;
}