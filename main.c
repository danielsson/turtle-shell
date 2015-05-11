#include "main.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <signal.h>

#define TRUE 1
#define FALSE 0
#define CMD_LEN 80
#define ARGS_SIZE 5

int is_running = TRUE;

int count_non_null(char * args[ARGS_SIZE]) {
    int count;
    for(count = 0; count < ARGS_SIZE && args[count] != NULL; count++);
    return count;
}

/*
 * Returns an array of strings
 */
void tokenize(char * command, char* into[ARGS_SIZE]) {
    char * ptr = command;
    int count = 1;

    into[0] = command;
    ptr++;

    while (*ptr != 0 && count < ARGS_SIZE) {
        if(*ptr == ' ') {
            *ptr = 0;
            into[count++] = ptr + sizeof(char);
        }

        ptr++;
    }
}

/**
 * This function is where the proverbial magic sauce lives.
 */
void forker(const char * args[ARGS_SIZE]) {
    const char * command = args[0];

    pid_t pid;
    pid = fork();
    if ( pid == 0 ) {
        /* Child */
        char * child_args[1];
        args[0] = NULL;

        if(strlen(command) > 1) {
            if(execvp(command, child_args) == -1) {
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
                puts("Exited abnormally");
            }
        }

    } else {
        /* System fork err */
        printf("Fork failed");
        exit(0xDEADBEEF);
    }
}

void remove_trailing_nl(char * str) {
    size_t len;
    len = strlen(str);

    str[len-1] = 0;
}

void cmd_ls() {
    DIR* dir;
    struct dirent *ep;
    dir = opendir("./");
    if ( dir != NULL ) {
        while ((ep = readdir(dir))) {
            puts(ep->d_name);
        }
        closedir(dir);
    } else {
        perror("Failed to read dir\n");
        exit(1);
    }
}

void cmd_exit() {
    printf("exiting\n");
    kill(-2, SIGTERM);
    is_running = FALSE;
}

void cmd_cd() {

}

int handle_builtin(char ** str) {

    if (strcmp(str[1], BI_CD) == 0) {
        cmd_cd();
        return TRUE;

    } else if(strcmp(str[1], BI_EXIT) == 0) {
        cmd_exit();
        return TRUE;
    } else if(strcmp(str[1], BI_LS) == 0) {
        cmd_ls();
        return TRUE;
    }

    return FALSE;
}

int main(int argc, const char * argv[]) {

    char command[CMD_LEN];
    char hostname[CMD_LEN];
    char *args[ARGS_SIZE];

    gethostname(hostname, CMD_LEN);
    strtok(hostname, ".");


    while (is_running) {
        int i;

        /* Command prompt */
        printf(" ❤ ❤ ❤ ");
        fgets(command, CMD_LEN, stdin);
        remove_trailing_nl(command);
        tokenize(command, args);

        /* Now the fun stuff */
        if (!handle_builtin(args))
            forker(args);

    }

    return 0;
}

