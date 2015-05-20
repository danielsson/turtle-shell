#define _XOPEN_SOURCE 500
#define TRUE 1
#define FALSE 0
#define PIPE_READ 0
#define PIPE_WRITE 1
#define CMD_LEN 80
#define ARGS_SIZE 5



#include "main.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <signal.h>
#include <errno.h>
#include <linux/limits.h>
#include <sys/time.h>
#include <sys/wait.h>




#define SIGDET TRUE

#define	timersub(tvp, uvp, vvp)						\
	do {								\
		(vvp)->tv_sec = (tvp)->tv_sec - (uvp)->tv_sec;		\
		(vvp)->tv_usec = (tvp)->tv_usec - (uvp)->tv_usec;	\
		if ((vvp)->tv_usec < 0) {				\
			(vvp)->tv_sec--;				\
			(vvp)->tv_usec += 1000000;			\
		}							\
	} while (0)


#ifdef __APPLE__
#define SAY_TURTLE FALSE
#else
#define SAY_TURTLE FALSE
#endif

#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_RESET   "\x1b[0m"
#define putz puts

extern char **environ;


int is_running = TRUE;

char last_dir[PATH_MAX] = {0};

/*
 * Forward Declarations
 */
void run_child(char *const *args, const char *command);
void setup_sigterm_handler();

/**
 * Count the number of arguments in a string.
 */
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
    printf(" \xE2\x8F\xB3  System time: %ld.%05ld\t", diff.tv_sec, diff.tv_usec);

    timersub(&after->ru_utime, &before->ru_utime, &diff);
    printf("User time: %ld.%05ld\t", diff.tv_sec, diff.tv_usec);
}


/**
 * Print the exit status from the specified status var. Also print
 * the system and user time for the terminated process.
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
            putz("Exited abnormally");
        }
    } else {
        print_time(before, after);
        printf("Exited with status %d\n", *status);
    }

}


/*
 * Splits a string into an array of strings in-place.
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
    putz("");
    if (dir != NULL) {
        while ((ep = readdir(dir))) {
            if(ep->d_type == 4) {
                printf(" \xF0\x9F\x8D\xB0  %s \n", ep->d_name);
            } else {
                printf(" \xF0\x9F\x8D\x93  %s \n", ep->d_name);
            }
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

    kill(-getpid(), SIGTERM);

    if (!SIGDET) {
        pid_t p;
        int status;
        while ((p = waitpid(-2, &status, WNOHANG)) != -1);
    }

    is_running = FALSE;

    printf("\xF0\x9F\x9A\x80  exiting\n");
}

/**
 * Execute the cd command.
 */
void cmd_cd(char *args[ARGS_SIZE]) {
    char current[PATH_MAX];
    getcwd((char *) &current, PATH_MAX);

    if (args[1] == NULL) {
        if (chdir(getenv("HOME")) == -1) {
            perror("Failed to chdir");
        }
        return;
    } else if (args[1][0] == '-' && last_dir[0] != 0) {
        chdir((const char *) &last_dir);
        return;
    } else if (args[1][0] == '-') {
        putz("No previous dir");
        return;
    }

    strcpy(last_dir, current);

    if (chdir(args[1]) == -1) {
        perror("Failed to chdir");
    }
}


/*
 * Helper function to create a pipe or print an error and die.
 */
void create_pipe(int p[2]) {
    if (pipe(p) == -1) {
        perror("ERROR CREATING PIPE");
        exit(EXIT_FAILURE);
    }
}

/**
 * Helper function to close a pipe end or print an error-
 */
void close_pipe(int fd) {
    if (close(fd) == -1) {
        fprintf(stderr, "Error closing: %i", fd);
        printf("errno: %i", errno);
        exit(EXIT_FAILURE);
    }
}

/**
 * Prints the environment to stdout.
 */
void print_environment() {
    char **pt = environ;

    do {
        putz(*pt);
    } while (*++pt);
}


/**
 * check that the return value is not an error.
 */
void check_return_value(int return_value, const char *msg) {
    if (return_value == -1) {
        perror(msg);
        exit(EXIT_FAILURE);
    }
}


/**
 * Helper function to close all ends of the specified pipes.
 */
void close_all_pipes(const int *fd_descriptor_env_sort, const int *fd_descriptor_sort_pager, const int *fd_descriptor_grep_sort) {
    close_pipe(fd_descriptor_env_sort[PIPE_WRITE]);
    close_pipe(fd_descriptor_env_sort[PIPE_READ]);

    close_pipe(fd_descriptor_sort_pager[PIPE_WRITE]);
    close_pipe(fd_descriptor_sort_pager[PIPE_READ]);

    close_pipe(fd_descriptor_grep_sort[PIPE_WRITE]);
    close_pipe(fd_descriptor_grep_sort[PIPE_READ]);
}


/**
 * Wait for a child to terminate.
 */
void wait_for_child() {
    int status;
    pid_t child_pid;

    child_pid = wait(&status);
    check_return_value(child_pid, "Wait failed.");

    if (WIFEXITED((status))) {
        int child_status = WEXITSTATUS((status));

        if (0 != child_status) /* child had problems */
        {
            fprintf(stderr, "Child (pid %ld) failed with exit code %d\n",
                    (long int) child_pid, child_status);
        }
    } else {
        if (WIFSIGNALED((status))) /* child-processen avbröts av signal */
        {
            int child_signal = WTERMSIG((status));
            fprintf(stderr, "Child (pid %ld) was terminated by signal no. %d\n",
                    (long int) child_pid, child_signal);
        }
    }
}


/**
 * execute the checkEnv command.
 */
void cmd_check_env(char *args[ARGS_SIZE]) {
    int fd_descriptor_env_sort[2];
    int fd_descriptor_sort_pager[2];
    int fd_descriptor_grep_sort[2];
    pid_t env_child, sort_child, grep_child, pager_child;
    int grep = FALSE;
    int return_value;

    sighold(SIGCHLD);

    create_pipe(fd_descriptor_env_sort);
    create_pipe(fd_descriptor_sort_pager);
    create_pipe(fd_descriptor_grep_sort);
    env_child = fork();

    if (env_child == 0) {
        return_value = dup2(fd_descriptor_env_sort[PIPE_WRITE], STDOUT_FILENO);
        check_return_value(return_value, "Error: cannot dup2 1");

        close_all_pipes(fd_descriptor_env_sort, fd_descriptor_sort_pager, fd_descriptor_grep_sort);

        print_environment();
        exit(EXIT_SUCCESS);

    } else if (env_child == -1) {
        perror("Error forking.");
        exit(EXIT_FAILURE);
    }

    if(args[1] != NULL) {
        grep = TRUE;
        grep_child = fork();

        if(grep_child == 0) {
            return_value = dup2(fd_descriptor_env_sort[PIPE_READ], STDIN_FILENO);
            check_return_value(return_value, "Error: cannot dup2 2");

            return_value = dup2(fd_descriptor_grep_sort[PIPE_WRITE], STDOUT_FILENO);
            check_return_value(return_value, "Error: cannot dup2 3");

            close_all_pipes(fd_descriptor_env_sort, fd_descriptor_sort_pager, fd_descriptor_grep_sort);

            args[0] = "grep";

            return_value = execvp(args[0], args);
            check_return_value(return_value, "Error: execution failed.");
        } else if(grep_child == -1){
            perror("Error forking.");
            exit(EXIT_FAILURE);
        }
    }

    sort_child = fork();

    if (sort_child == 0) {
        char * argp[] = {"sort", NULL};

        if(grep == TRUE) {
            return_value = dup2(fd_descriptor_grep_sort[PIPE_READ], STDIN_FILENO);
            check_return_value(return_value, "Error: cannot dup2 3");
        } else {
            return_value = dup2(fd_descriptor_env_sort[PIPE_READ], STDIN_FILENO);
            check_return_value(return_value, "Error: cannot dup2 4");
        }
        return_value = dup2(fd_descriptor_sort_pager[PIPE_WRITE], STDOUT_FILENO);
        check_return_value(return_value, "Error: cannot dup2 5");

        close_all_pipes(fd_descriptor_env_sort, fd_descriptor_sort_pager, fd_descriptor_grep_sort);

        args[0] = "sort";

        return_value = execvp(args[0], argp);
        check_return_value(return_value, "Error: execution failed.");

    } else if (sort_child == -1) {
        perror("Error forking.");
        exit(EXIT_FAILURE);
    }

    pager_child = fork();

    if (pager_child == 0) {
        char * argp[] = {"less", NULL};
        return_value = dup2(fd_descriptor_sort_pager[PIPE_READ], STDIN_FILENO);
        check_return_value(return_value, "Error: cannot dup2 6");

        close_all_pipes(fd_descriptor_env_sort, fd_descriptor_sort_pager, fd_descriptor_grep_sort);

        if(getenv("PAGER") != NULL) {
            argp[0] = getenv("PAGER");
        }

        return_value = execvp(argp[0], argp);
        check_return_value(return_value, "Error: execution failed.");

    } else if (pager_child == -1) {
        perror("Error forking.");
        exit(EXIT_FAILURE);
    }

    close_all_pipes(fd_descriptor_env_sort, fd_descriptor_sort_pager, fd_descriptor_grep_sort);

    wait_for_child();
    wait_for_child();
    wait_for_child();
    if(grep == TRUE) wait_for_child();

    sigrelse(SIGCHLD);
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
 * Start a process in the foreground and wait for the process to finish.
 */
void foreground(char *args[ARGS_SIZE]) {

    char *command = args[0];

    pid_t group_id = getpid();
    pid_t pid;
    pid = fork();
    if (pid == 0) {
        /* Child */
        setpgid(0, group_id);
        run_child(args, command);

    } else if (pid > 0) {
        /* Parent */
        int status = 0;
        struct rusage before;
        struct rusage after;

        setpgid(pid, group_id);

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
                putz("Exited abnormally");
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
    pid_t group_id = getpid();
    int return_value = 0;

    signal(SIGINT, SIG_IGN);

    pid = fork();

    if (pid == 0) {
        return_value = setpgid(0, group_id);
        check_return_value(return_value, "FAILED DUDE");

        run_child(args, command);
        return;

    } else if (pid > 0) {
        return_value = setpgid(pid, group_id);
        check_return_value(return_value, "FAILED DUDE2");
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

    if (p > 0) {
        printf("Terminated in background: \n");
        handle_status(&before, &after, &status);
    }

}


/**
 * Detect terminated processes by signals and handle the exit status.
 */
void clean_up_after_children(int signal_number, siginfo_t *info, void *context) {
    int status;
    struct rusage before;
    struct rusage after;
    pid_t p;

    if (signal_number == SIGCHLD) {
        sighold(SIGCHLD);

        getrusage(RUSAGE_CHILDREN, &before);
        while ((p = waitpid(-1, &status, WNOHANG)) != -1 && p != 0) {
            getrusage(RUSAGE_CHILDREN, &after);
            printf("Terminated in background: \n");
            handle_status(&before, &after, &status);

            getrusage(RUSAGE_CHILDREN, &before);
        }

        sigrelse(SIGCHLD);
    }
}

/**
 * Set up a signal handler.
 */
void setup_signal_handler() {
    struct sigaction sigchld_action;
    memset (&sigchld_action, 0, sizeof(sigchld_action));
    sigchld_action.sa_sigaction = &clean_up_after_children;
    sigchld_action.sa_flags &= SA_SIGINFO | SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sigchld_action, NULL);
}

static void handle_interrupt(int sig) {
    /* THIS IS WHERE THE MAGIC HAPPENS */
	/* Do noting, just let SIGINT pass. */
}


/**
 * Setup a signal handler for SIGINT
 */
void setup_interrupt_signal_handler() {
    struct sigaction sa;
    sa.sa_handler = handle_interrupt;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP; /* Restart functions if
                                 interrupted by handler */
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction failed.");
        exit(EXIT_FAILURE);
    }

}


/** Handle SIGTERM, do not let ourselfs commit suicide. */
static void handle_sigterm(int sig) {
    int status;
    struct rusage before;
    struct rusage after;


    if(sig == SIGTERM) {
        sighold(SIGTERM);

        getrusage(RUSAGE_CHILDREN, &before);
        if(waitpid(-1, &status, WNOHANG) == -1) return;
        while(waitpid(-1, &status, WNOHANG) == 0) {
            getrusage(RUSAGE_CHILDREN, &before);
        }
        getrusage(RUSAGE_CHILDREN, &after);
        printf("Terminated in background by exit in foreground: \n");
        handle_status(&before, &after, &status);

        sigrelse(SIGTERM);
    }
}


/**
 * Setup the handler for SIGTERM
 */
void setup_sigterm_handler() {
    struct sigaction sa;
    sa.sa_handler = handle_sigterm;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART; /* Restart functions if
                                 interrupted by handler */
    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        perror("sigaction failed.");
        exit(EXIT_FAILURE);
    }
}


/**
 * Get arguments from standard in and execute the commands. e
 */
int main(int argc, char *argv[]) {

    char command[CMD_LEN];
    int len;

    if (SIGDET)
        setup_signal_handler();

    setup_sigterm_handler();

    while (is_running) {
        char *args[ARGS_SIZE] = {0};
        memset(command, 0, CMD_LEN);


        if(!SIGDET) poll_background_children();

        setup_interrupt_signal_handler();

        /* Command prompt */
        if (SAY_TURTLE) system("say \"turtle\"");
        printf(" \xF0\x9F\x90\xA2  " ANSI_COLOR_GREEN);
        fgets(command, CMD_LEN, stdin);

        remove_trailing_nl(command);
        tokenize(command, args);

        putz(ANSI_COLOR_RESET);

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

