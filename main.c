#include "main.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <signal.h>
#include <errno.h>
#include <sys/time.h>

#define TRUE 1
#define FALSE 0
#define READ 0
#define WRITE 1
#define CMD_LEN 80
#define ARGS_SIZE 5

#define SIGDET FALSE

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"


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
    printf(" \xE2\x8F\xB3  System time: %ld.%05i\t", diff.tv_sec, diff.tv_usec);

    timersub(&after->ru_utime, &before->ru_utime, &diff);
    printf("User time: %ld.%05i\t", diff.tv_sec, diff.tv_usec);
}

/**
 * Print the exit status from the specified status var and print it and timings.
 */
void handle_status(struct rusage *before, struct rusage *after, int *status) {
    if (*status != 0) {
        if (WIFEXITED((*status))) {
            print_time(before, after);
            printf("Exited with status %d\n", WEXITSTATUS((*status)));


        } else if (WIFSIGNALED((*status))) {
            print_time(before, after);
            printf("Exited through signal %d\n", WTERMSIG((*status)));


        } else {
            print_time(before, after);
            puts("Exited abnormally");
        }
    } else {
        print_time(before, after);
        printf("Exited with status %d\n", *status);
    }

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

    if (str[len - 1] == '\n')
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
            printf(" \xF0\x9F\x8D\x93  %s \n", ep->d_name);
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
    kill(-2, SIGKILL);

    if (!SIGDET) {
        pid_t p;
        int status;
        while ((p = waitpid(-2, &status, WNOHANG)) != -1);
    }

    is_running = FALSE;

    printf("exiting\n");
}

/**
 * Execute the cd command.
 */
void cmd_cd(char *args[ARGS_SIZE]) {
    if (chdir(args[1]) == -1) {
        perror("Failed to chdir");
    }
}

void create_pipe(int p[2]) {
    if(pipe(p) == -1) {
        perror("ERROR CREATING PIPE");
    }
}

void close_pipe(int p[2]) {
    if(close(p[0]) == -1) {
        perror("ERROR CLOSING READ");
    }
    if(close(p[1]) == -1) {
        perror("ERROR CLOSING WRITE");
    }
}

/*
 * PIPING, looked at this source: http://stackoverflow.com/questions/16174187/cant-pipe-to-less1
 */

void cmd_check_env(char * args[ARGS_SIZE]) {
    int p_1[2], p_2[2], p_3[2], p_4[2];
    int pid_1, pid_2, pid_3, pid_4;
    char * argsLess[] = {"less", NULL};

    pid_1 = fork();

    create_pipe(p_1);
    create_pipe(p_2);

    if(pid_1 == 0) {
        if(dup2(p_1[WRITE], STDOUT_FILENO) == -1) {
            perror("ERROR, DUP2");
        }
        close_pipe(p_1);
        close_pipe(p_2);

        if (execvp("sort", args) == -1){
            perror("EXECUTION ERROR");
        }

    } else if(pid_1 == -1){
        perror("FORK ERROR");
    } else {
        pid_3 = fork();

        if(pid_3 == 0) {
            if (dup2(p_2[READ], STDIN_FILENO) == -1) {
                perror("ERROR, DUP2");
            }
        } else if(pid_3 == -1) {
            perror("FORK ERROR");
        }

        close_pipe(p_2);
        close_pipe(p_1);
        waitpid(pid_3, NULL, 0);



        if (execvp(argsLess[0], argsLess) == -1){
            perror("ERROR IN PAGER");
        }
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
    } else if (strcmp(args[0], BI_CHKENV) == 0) {
        cmd_check_env(args);
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

        sighold(SIGCHLD);
        if (waitpid(pid, &status, 0) == -1) {
            printf("waitpid failed %d\n", errno);
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
        sigrelse(SIGCHLD);

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

    if (strcmp(args[0], BI_LS) == 0) {
        cmd_ls();
    } else {
        if (execvp(command, args) == -1) {
            printf("Unknown Command: %s\n", command);

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
    pid_t p;

    getrusage(RUSAGE_CHILDREN, &before);
    p = waitpid(-1, &status, WNOHANG);
    getrusage(RUSAGE_CHILDREN, &after);

    if(p > 0) {
        printf("Terminated in background: \n");
        handle_status(&before, &after, &status);
    }

}


void clean_up_after_children(int signal_number, siginfo_t *info, void *context) {

    sighold(SIGCHLD);
    if (signal_number == SIGCHLD) {

        int status;
        struct rusage before;
        struct rusage after;
        pid_t p;

        getrusage(RUSAGE_CHILDREN, &before);
        while ((p = waitpid(-1, &status, WNOHANG)) != -1 && p != 0) {
            getrusage(RUSAGE_CHILDREN, &after);
            handle_status(&before, &after, &status);

            getrusage(RUSAGE_CHILDREN, &before);
        }
    }
    sigrelse(SIGCHLD);
}

void setup_signal_handler() {
    struct sigaction sigchld_action;
    memset (&sigchld_action, 0, sizeof(sigchld_action));
    sigchld_action.sa_sigaction = &clean_up_after_children;
    sigchld_action.sa_flags &= SA_SIGINFO | SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sigchld_action, NULL);
}


int main(int argc, char *argv[]) {

    char command[CMD_LEN];
    int len;


    while (is_running) {
        char *args[ARGS_SIZE] = {0};
        memset(command, 0, CMD_LEN);

        if (SIGDET)
            setup_signal_handler();
        else
            poll_background_children();

        /* Command prompt */
        printf(" \xF0\x9F\x90\xA2  " ANSI_COLOR_GREEN);
        fgets(command, CMD_LEN, stdin);



        remove_trailing_nl(command);
        tokenize(command, args);


        puts(ANSI_COLOR_RESET);

        /* Now the fun stuff */

        len = count_non_null(args);

        if (*(args[len - 1]) == '&') {
            args[len - 1] = 0;
            background(args);


        } else if (!handle_builtin(args) && strlen(args[0]) != 0) {
            foreground(args);
        }

    }

    return 0;
}
