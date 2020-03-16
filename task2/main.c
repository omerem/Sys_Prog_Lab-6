#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <wait.h>
#include <errno.h>
#include <signal.h>


int main(int argc, char **argv) {
    int i;

    int status1;
    int status2;
    char * arg0A = "ls";
    char * arg1A = "-l";
    int debug = 1;
    char * const argumentsA[256]  = {arg0A, arg1A};
    char * arg0B = "tail";
    char * arg1B = "-n";
    char * arg2B = "2";
    int child1, child2;

    char * const argumentsB[256]  = {arg0B, arg1B, arg2B};

    int do4 = 1;
    int do7 = 1;
    int do8 = 1;

    for(i=0;i<argc;i++)
    {
        if (strcmp(argv[i],"-d")==0)
        {
            debug = 1;
        }
    }


    int pipefd[2];


    if (pipe(pipefd) == -1)
    {
        exit(1);
    }
    if(debug)
    {
        fprintf(stderr,"(parent_process>forking…)\n");
    }
    child1 = fork();
    if(debug && (child1 != 0))
    {
        fprintf(stderr,"(parent_process>created process with id: %d)\n",child1);
    }

    if (child1 == 0)
    {
        if(debug)
        {
            fprintf(stderr,"(child1>redirecting stdout to the write end of the pipe…)\n");
        }
        close(STDOUT_FILENO);
        dup(pipefd[1]);

        close(pipefd[1]);
        if(debug)
        {
            fprintf(stderr,"(child1>going to execute cmd: ls…)\n");
        }
        execvp(argumentsA[0], argumentsA);
    }
    else
    {
        if (do4) {
            if (debug) {
                fprintf(stderr, "(parent_process>closing the write end of the pipe…)\n");
            }
            close(pipefd[1]);
        }
    }
    if(debug)
    {
        fprintf(stderr,"(parent_process>forking…)\n");
    }
    child2 = fork();
    if(debug && (child2 != 0))
    {
        fprintf(stderr,"(parent_process>created process with id: %d)\n",child2);
    }

    if(child2 == 0) // child2
    {
        if(debug)
        {
            fprintf(stderr,"(child2>redirecting stdin to the read end of the pipe…)\n");
        }
        close(STDIN_FILENO);
        dup(pipefd[0]);

        close(pipefd[0]);

        if(debug)
        {
            fprintf(stderr,"(child1>going to execute cmd: tail…)\n");
        }
        execvp(argumentsB[0], argumentsB);
    }
    else // parent of child2
    {
        if(do7) {
            if (debug) {
                fprintf(stderr, "(parent_process>closing the read end of the pipe…)\n");
            }
            close(pipefd[0]);
        }
    }
    if(do8) {
        if (debug) {
            fprintf(stderr, "(parent_process>waiting for child processes to terminate…)\n");
        }
        waitpid(child1, &status1, 0);
        waitpid(child2, &status2, 0);
    }


    if(debug)
    {
        fprintf(stderr,"(parent_process>exiting…)\n");
    }
    exit(0);
}
