#include "main.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <string.h>

#define TRUE 1
#define FALSE 0
#define CMD_LEN 80

void cmd_cd();
void cmd_exit();

/*
 * Tokenize the command into a char
 */
void tokenize(char * command) {

}




/**
 * This function is where the proverbial magic sauce lives.
 */
void forker(const char * command) {

    pid_t pid;
    pid = fork();
    if ( pid == 0 ) {
        /* Child */
        char * args[1];
        args[0] = NULL;

        if(strlen(command) > 1) {
            if(execvp(command, args) == -1) {
                printf("Unknown Command: %s\n", command);
                exit(1);
            }
        }

        exit(1);

    } else if(pid > 0) {
        /* Parent */
        int status = 0;
        if (waitpid(pid, &status, 0) == -1) {
            perror("waitpid failed\n");
        } else {
            if(WIFEXITED(status)) {
                printf("Exited with status %d\n", WEXITSTATUS(status));
            } else if(WIFSIGNALED(status)) {
                printf("Exited through signal %d\n", WTERMSIG(status));
            } else {
                printf("Exited abnormally\n");
            }
        }

    } else {
        /* System fork err */
        printf("Fork failed");
        exit(0x667788);
    }
}

void remove_trailing_nl(char * str) {
    size_t len;
    len = strlen(str);

    str[len-1] = 0;
}

int handle_builtin(char * str) {

    if (strcmp(str, BI_CD)) {
        cmd_cd();
    } else if(strcmp(str, BI_EXIT)) {
        cmd_exit();
    }

    return 0;
}

void cmd_cd() {

}

void cmd_exit() {

}

int main(int argc, const char * argv[]) {

    int is_running = TRUE;
    char command[CMD_LEN];
    char hostname[CMD_LEN];

    gethostname(hostname, CMD_LEN);


    while (is_running) {

        /* Command prompt */
        printf("turtle-shell@%s %% ", hostname);
        fgets(command, CMD_LEN, stdin);
        remove_trailing_nl(command);


        /* Now the fun stuff */



        forker(command);

    }
}

