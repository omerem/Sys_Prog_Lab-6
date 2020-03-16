#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <limits.h>
#include "LineParser.h"
#include <string.h>
#include <wait.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>


void execute(cmdLine *pCmdLine);
void pwd();

void pwd()
{
    char cwd[256];
    getcwd(cwd, sizeof(cwd));
    printf("%s$", cwd);
}


void execute(cmdLine *pCmdLine)
{
    if (execvp((pCmdLine->arguments[0]),  pCmdLine->arguments ) < 0)
    {
        perror("Error: ");
        exit(errno);
    }
    fclose(stdin);
}


int main(int argc, char **argv)
{
    int debug = 0;
    int i=0;
    int status;
    int cd;
    int pipefd[2];
    for(i=0;i<argc;i++)
    {
        if (strcmp(argv[i],"-d")==0)
        {
            debug = 1;
        }
    }
    int child=0;
    int child1 = 0;
    int child2 = 0;
    char blocking1 = '0';
    char blocking2 = '0';



    printf("Starting the program\n");
    char input[2048];
    cmdLine * parsedCLines;
    int pid;

    while (1) {
        if(child == 0)
        {
            pwd();
            fgets(input, 2048, stdin);
        }
        if (strcmp(input, "quit\n")==0)
        {
            exit(0);
        }
        if (child == 0)
        {
            parsedCLines = parseCmdLines(input);
        }
        if(parsedCLines->next != NULL)
        {
            child = 1;
            if (pipe(pipefd) == -1)
            {
                exit(1);
            }
        }

        pid = fork();
        if (child == 1)
        {
            child1 = pid;
            blocking1 = parsedCLines->blocking;
        }
        else if (child == 2)
        {
            child2 = pid;
            blocking2 = parsedCLines->blocking;
        }

        if (pid < 0) {
            freeCmdLines(parsedCLines);
            exit(1);
        }

        if (debug) {
            fprintf(stderr, "PID = %d", pid);
            fprintf(stderr, "\nExecuting Command = %s", parsedCLines->arguments[0]);
            fprintf(stderr, "\n");
        }
        if (strcmp(parsedCLines->arguments[0], "cd") == 0)
        {
            cd = 1;
        }
        else
        {
            cd = 0;
        }
        if((cd == 1) && (pid != 0)) // quarter 1 - PARENT process
        {
            status = chdir(parsedCLines->arguments[1]);
            if (status == -1) // cd failed
            {
                perror("Error: ");
                freeCmdLines(parsedCLines);
                kill(-1, SIGKILL);
                exit(errno);
            }
        }
        else if ((cd == 0) && (pid != 0)) // quarter 2 - PARENT process
        {
            if(child == 0)
            {
                waitpid(pid, &status, !parsedCLines->blocking);
            }
            else if (child == 1)
            {
                close(pipefd[1]);
            }
            else // child == 2
            {
                close(pipefd[0]);


                waitpid(child1, &status, !blocking1);
                waitpid(child2, &status, !blocking2);
            }
        }
        else if ((cd == 1) && (pid == 0)) // quarter 3 - CHILD process
        {
            freeCmdLines(parsedCLines);
            _exit(0);
        }
        else if ((cd  == 0) && (pid == 0)) // quarter 4 - CHILD process
        {
            if(child == 0)
            {
                if (parsedCLines->inputRedirect != NULL) {
                    close(STDIN_FILENO);
                    open(parsedCLines->inputRedirect, O_RDWR, 0777);
                }
                if (parsedCLines->outputRedirect != NULL) {
                    close(STDOUT_FILENO);
                    open(parsedCLines->outputRedirect, O_RDWR | O_CREAT, 0777);
                }

                execute(parsedCLines);
                freeCmdLines(parsedCLines);
                _exit(errno);
            }
            else if (child == 1)
            {
                close(STDOUT_FILENO);
                if (dup(pipefd[1]) < 0)
                {
                    exit(1);
                }
                close(pipefd[1]);
                execute(parsedCLines);
                close(pipefd[1]);
                freeCmdLines(parsedCLines);

            }
            else // child == 2
            {
                close(STDIN_FILENO);
                dup(pipefd[0]);
                close(pipefd[0]);
                execute(parsedCLines);
                freeCmdLines(parsedCLines);
            }
        } // end quarter 4

        if(child == 1)
        {
            parsedCLines = parsedCLines->next;
            child = 2;
        }
        else if (child == 2)
        {
            child = 0;
        }

    }// end while

    return 0;
}
