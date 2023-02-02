// Jia Lin Sun
// 261052660

// README
// Hi! Here are a few notes on my shell:
// 1. Please separate arguments using a "space".
//    (Ex: echo hello, ls -l, ls > out.txt, ls | wc -l)
// 2. Sometimes, the ">>" does not print correctly at the start of each line.
//    Some are due to signal handling, others to the printing delay.
//    Please hit "enter" one more time if you want to see ">>".
// That's it, happy grading!


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <fcntl.h>

int fore_pid;   // pid of the foreground process
int jobs_table[100]; // job table (maximum 100 jobs) ONLY BACKGROUND JOBS ARE NEEDED
int job_num = 0;    // number of jobs running;

int getcmd(char *prompt, char *args[], int *background, int *redirection, int *piping);
static void signalHandler(int sig);

// Built-in functions
void echo(char *args[]);
void cd(char *args[]);
void pwd();
void custom_exit();
void fg(char *args[]);
void jobs();

// Helper functions
void addJob(int pid);
void removeJob(int pid);

int main(int argc, char *argv[]){

    printf("\n%s\n", "Welcome to sun_shell!");

    char *args[20];
    int bg;             // 0 = foreground, 1 = background
    int redirection;    // 0 = normal, 1 = redirected
    int piping;         // indicate index of '|' in the command
    int cnt;
    int child_status = 5;   // to collect child status -> 0 means child terminated

    


    while (1) {

        // Reset flags
        bg = 0;
        redirection = 0;
        piping = 0;
        fore_pid = 0;

        // Update jobs_table
        int job_check = 0;
        while (job_check < job_num) {
            int childpid = waitpid(jobs_table[job_check], &child_status, WNOHANG);

            if (child_status = 0 && childpid > 0){
                removeJob(childpid);
                child_status = 5;
            }
            else {
                job_check++;
            }
        }

        // Catch ^C and ^Z
        if (signal(SIGINT, signalHandler) == SIG_ERR) {
            printf("Cannot bind the signal handler for SIGINT");
            exit(1);
        }
        else if (signal(SIGTSTP, SIG_IGN) == SIG_ERR) {
            printf("Cannot bind the signal handler for SIGTSTP");    
            exit(1);
        }

        cnt = getcmd("\n>> ", args, &bg, &redirection, &piping);

        // Handle empty command
        if (cnt == 0) {
            continue;
        }

        // Set null pointer
        args[cnt] = NULL;

        // For built-in functions
        char *cmd = args[0];
        
        if (strcmp(cmd, "echo") == 0) {
            echo(args);
            continue;
        }
        else if (strcmp(cmd, "cd") == 0) {
            cd(args);
            continue;
        }
        else if (strcmp(cmd, "pwd") == 0) {
            pwd();
            continue;
        }
        else if (strcmp(cmd, "exit") == 0) {
            custom_exit();
            continue;
        }
        else if (strcmp(cmd, "fg") == 0) {
            fg(args);
            continue;
        }
        else if (strcmp(cmd, "jobs") == 0) {
            jobs();
            continue;
        }

        int pid = fork();

        if (pid == 0) {     // Child process

            // Child ignores ^C
            signal(SIGINT, SIG_IGN);

            if (piping != 0) {  // Command piping                
                // Set up first command
                char *first[piping + 1];
                for (int i = 0; i < piping; i++){
                    first[i] = args[i];
                }
                first[piping] = NULL;

                // Set up second command
                int len = cnt - piping;
                char *second[len];
                for (int i = 0; i < len - 1; i++) {
                    second[i] = args[i + piping + 1];
                }
                second[len - 1] = NULL;

                // Prepare pipe
                int fd[2];
                pipe(fd);

                int pid_pipe = fork();

                if (pid_pipe == 0) {    // First command
                    close(1);
                    int fd1 = dup(fd[1]);
                    close(fd[0]);

                    int status = execvp(first[0], first);
                    if (status < 0) {
                        printf("Exec failed\n");
                        exit(1);
                    }  
                    exit(0);
                }
                else if (pid == -1) {
                    printf("Fork failed\n");
                }
                else {  // Second command                  
                    waitpid(pid_pipe, NULL, 0);

                    close(0);
                    int fd0 = dup(fd[0]);
                    close(fd[1]);
                    
                    int status = execvp(second[0], second);
                    if (status < 0) {
                        printf("Exec failed\n");
                        exit(1);
                    }  
                    exit(0);
                }
            }
            else {
                if (redirection) {  // Set output redirection
                    close(1);
                    int fd = open(args[cnt - 1], O_CREAT|O_WRONLY|O_TRUNC, 0777);

                    args[cnt - 2] = '\0';
                    args[cnt - 1] = '\0';
                }

                int status = execvp(args[0], args);

                if (status < 0) {
                    printf("Exec failed\n");
                    exit(1);
                }  
                exit(0);
            }
            
        }
        else if (pid == -1) {

            printf("Fork failed\n");

        }
        else {  // parent
            
            if (bg == 0){
                fore_pid = pid;
                waitpid(pid, NULL, 0);
            }
            else {                
                addJob(pid);        // Add new background child to the job table
            }

        }

    }

}


int getcmd(char *prompt, char *args[], int *background, int *redirection, int *piping) {

    int length, i = 0;
    char *token, *loc;
    char *line = NULL;
    size_t linecap = 0;

    // Clear previous value
    memset(args, '\0', 20);

    printf("%s", prompt);
    length = getline(&line, &linecap, stdin);

    if (length <= 0) {
        exit(-1);
    }

    // Check if background is specified..
    if ((loc = index(line, '&')) != NULL) {
        printf("Background\n");
        *background = 1;
        *loc = ' ';
    } 
    else {
        *background = 0;
    }

    while ((token = strsep(&line, " \t\n")) != NULL) {

        for (int j = 0; j < strlen(token); j++) {
            if (token[j] <= 32){
                token[j] = '\0';
            }
            if (token[j] == 62) {
                *redirection = 1;
            }
            if (token[j] == 124) {
                *piping = i;
            }
        }

        if (strlen(token) > 0){
            args[i++] = token;
        }

    }
    return i;

}

// Signal handler for ^C

static void signalHandler(int sig){

    if (fore_pid != 0){
        printf("\nForeground process killed: %d\n", fore_pid);
        kill(fore_pid, SIGTERM);
        fore_pid = 0;
    }
    else {
        printf("\nNothing to kill\n");
    }

}

// Helper functions

void addJob(int pid) {
    // Append new job to the end of array

    jobs_table[job_num] = pid;
    job_num++;

}

void removeJob(int pid) {
    // Remove job, fill in the empty space

    bool exist = false;
    for (int i = 0; i < job_num; i++) {
        if (jobs_table[i] == pid) {
            for (int j = i; j < job_num - 1; j++) {
                jobs_table[j] = jobs_table[j+1];
            }
            job_num--;
            exist = true;
            break;
        }
    }

    if (!exist) {
        printf("Not a valid pid\n");
    }

}


// Built-in functions

void echo(char *args[]){
    // Can take:
    // 1. No argument -> prints nothing
    // 2. One argument -> prints the argument
    // 3. Multiple argumetns -> prints the first argument

    if (args[1] == NULL){
        printf("\n");
    }
    else {
        printf("%s\n", args[1]);
    }

}

void cd(char *args[]){
    // Prints directory before change and after change

    printf("From:\n");
    pwd();
    chdir(args[1]);
    printf("To:\n");
    pwd();

}

void pwd(){
    // Prints the present working directory

    char path[1024];
    printf("%s\n", getcwd(path, 1024));

}

void custom_exit() {
    // Exit sun_shell after cleaning all background jobs

    if (job_num != 0) {
        for (int i = 0; i < job_num; i++) {
            kill(jobs_table[i], SIGKILL);
        }
    }
    printf("\nThank you for using sun_shell!\n\n");
    exit(0);

}

void fg(char *args[]) {
    // Can take (if at least one job is in the background):
    // 1. No argument -> the last job will put in the foreground
    // 2. One argument -> the job at the specified index will be put in the foreground
    // If no job in the background, do nothing

    if (job_num == 0) {
        printf("No job in the background\n");
    }
    else {
        if (args[1] == NULL) {
            int pid = jobs_table[job_num - 1];
            printf("Job number %d (pid: %d) is put in the foreground\n", job_num - 1, pid);
            removeJob(pid);
            fore_pid = pid;
            waitpid(pid, NULL, 0);
        }
        else if (atoi(args[1]) >= 0 && atoi(args[1]) < job_num) {
            int pid = jobs_table[atoi(args[1])];
            printf("Job number %s (pid: %d) is put in the foreground\n", args[1], pid);
            removeJob(pid);
            fore_pid = pid;
            waitpid(pid, NULL, 0);
        }
        else {
            printf("Not a valid pid\n");
        }
    }

}

void jobs() {
    // Prints all processes

    if (job_num == 0){
        printf("Nothing running\n");
    }
    else {
        printf("Job Table:\n");
        for (int i = 0; i < job_num; i++){
            printf("%d\t%d\n", i, jobs_table[i]);
        }
    }

}