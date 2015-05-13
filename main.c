#include "main.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/errno.h>

#define TRUE 1
#define FALSE 0
#define CMD_LEN 80
#define ARGS_SIZE 5

int is_running = TRUE;

void run_child(char *const *args, const char *command);

int count_non_null(char *args[ARGS_SIZE]) {
    int count;
    for (count = 0; count < ARGS_SIZE && args[count] != NULL; count++);
    return count;
}

/**
 * Print the system and user time for a terminated process.
 */
void print_time(struct rusage *before, struct rusage *after) {
    struct timeval diff;

    timersub(&after->ru_stime, &before->ru_stime, &diff);
    printf("System time: %ld.%05i\n", diff.tv_sec, diff.tv_usec);

    timersub(&after->ru_utime, &before->ru_utime, &diff);
    printf("User time: %ld.%05i\n", diff.tv_sec, diff.tv_usec);
}


/*
 * Splits a string into an array of strings with single space as delimiter.
 */
void tokenize(char *command, char *into[ARGS_SIZE]) {
    char *ptr = command;
    int count = 1;

    into[0] = command;
    ptr++;

    while (*ptr != 0 && count < ARGS_SIZE) {
        if (*ptr == ' ') {
            *ptr = 0;
            into[count++] = ptr + sizeof(char);
        }

        ptr++;
    }
}

/**
 * Remove the nl character from the arguments.
 */
void remove_trailing_nl(char *str) {
    size_t len;
    len = strlen(str);

    str[len - 1] = 0;
}

/**
 * Execute the ls command.
 */
void cmd_ls() {
    DIR *dir;
    struct dirent *ep;
    dir = opendir("./");
    if (dir != NULL) {
        while ((ep = readdir(dir))) {
            puts(ep->d_name);
        }
        closedir(dir);
    } else {
        perror("Failed to read dir\n");
        exit(EXIT_FAILURE);
    }
    exit(EXIT_SUCCESS);
}

/**
 * Execute the exit command.
 */
void cmd_exit() {
    printf("exiting\n");
    kill(-2, SIGTERM);
    is_running = FALSE;
}

/**
 * Execute the cd command.
 */
void cmd_cd(char *args[ARGS_SIZE]) {
    if (chdir(args[1]) == -1) {
        perror("Failed to chdir");
    }
}

/**
 * Handle the built in commands: cd and exit.
 */
int handle_builtin(char *args[ARGS_SIZE]) {

    if (strcmp(args[0], BI_CD) == 0) {
        cmd_cd(args);
        return TRUE;

    } else if (strcmp(args[0], BI_EXIT) == 0) {
        cmd_exit();
        return TRUE;
    }

    return FALSE;
}


/**
 * This function is where the proverbial magic sauce lives.
 */
void foreground(char *args[ARGS_SIZE]) {
    char *command = args[0];

    pid_t pid;
    pid = fork();
    if (pid == 0) {
        /* Child */
        run_child(args, command);

    } else if (pid > 0) {
        /* Parent */
        int status = 0;
        struct rusage before;
        struct rusage after;

        if (getrusage(RUSAGE_CHILDREN, &before) == -1) {
            perror("Catastrophic failure");
            exit(EXIT_FAILURE);
        }

        if (waitpid(pid, &status, 0) == -1) {
            perror("waitpid failed\n");
        } else {
            getrusage(RUSAGE_CHILDREN, &after);

            print_time(&before, &after);

            if (WIFEXITED(status)) {
                printf("Exited with status %d\n", WEXITSTATUS(status));
            } else if (WIFSIGNALED(status)) {
                printf("Exited through signal %d\n", WTERMSIG(status));
            } else {
                puts("Exited abnormally");
            }
        }

    } else {
        /* System fork err */
        printf("Fork failed");
        exit(0xCC);
    }
}

/**
 * Start a background process.
 */
void background(char *args[ARGS_SIZE]) {

    char *command = args[0];

    pid_t pid;
    pid = fork();

    if (pid == 0) {
        run_child(args, command);
        return;

    } else if (pid < 0) {
        /* System fork err */
        printf("Fork failed");
        exit(0xCC);
    }
}

/**
 * Start a child process.
 */
void run_child(char *const *args, const char *command) {/* Child */
    int i;
    if (strcmp(args[0], BI_LS) == 0) {
        cmd_ls();
    } else {
        if (execvp(command, args) == -1) {
            printf("Unknown Command: %s\n", command);

            for (i = 0; i < ARGS_SIZE; i++) {
                puts(args[i]);
            }

            exit(EXIT_FAILURE);
        }
    }

    exit(99);
}

/**
 * Poll for any terminated background processes.
 */

void poll_background_children() {
    struct rusage before;
    struct rusage after;

    int status = 0;

    getrusage(RUSAGE_CHILDREN, &before);
    waitpid(-1, &status, WNOHANG);
    printf("Status: %d \n", status);
    getrusage(RUSAGE_CHILDREN, &after);

    if (status != 0) {
        if (WIFEXITED(status)) {
            print_time(&before, &after);
            printf("Exited with status %d\n", WEXITSTATUS(status));


        } else if (WIFSIGNALED(status)) {
            print_time(&before, &after);
            printf("Exited through signal %d\n", WTERMSIG(status));


        } else {
            print_time(&before, &after);
            puts("Exited abnormally");

        }
    }

    printf("Status2: %d \n", status);

}


int main(int argc, char *argv[]) {

    char command[CMD_LEN];
    int len;

    while (is_running) {

        char *args[ARGS_SIZE] = {0};

        poll_background_children();

        /* Command prompt */
        printf(" ❤ ❤ ❤ ");
        fgets(command, CMD_LEN, stdin);
        remove_trailing_nl(command);
        tokenize(command, args);

        /* Now the fun stuff */

        len = count_non_null(args);
        if (*(args[len - 1]) == '&') {
            (args[len - 1]) = 0;
            background(args);

        } else if (!handle_builtin(args)) {
            foreground(args);
        }

    }

    return 0;
}
