#include "main.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#define TRUE 1
#define FALSE 0


void forker(const char * command) {

    int pid;
    pid = fork();
    if ( pid == 0 ) {
        /* Child */

    } else if(pid > 0) {
        /* Parent */

    } else {
        /* System fork err */
        printf("Fork failed");
        exit(0x667788);
    }
}

int main(int argc, const char * argv[]) {

    int is_running = TRUE;
    char command[70];
    char hostname[70];

    gethostname(hostname, 70);


    while (is_running) {

        /* Command prompt */
        printf("turtle-shell@%s %% ", hostname);
        fgets(command, 70, stdin);

        /* Now the fun stuff */
        forker(command);

    }
}

