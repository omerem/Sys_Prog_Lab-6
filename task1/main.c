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

#define RUNNING 1
#define SUSPENDED 0
#define TERMINATED  -1

typedef struct process{
    cmdLine* cmd;                         /* the parsed command line*/
    pid_t pid; 		                  /* the process id that is running the command*/
    int status;                           /* status of the process: RUNNING/SUSPENDED/TERMINATED */
    struct process *next;	                  /* next process in chain */
} process;


void execute(cmdLine *pCmdLine);
void pwd();
void addProcess(process** process_list, cmdLine* cmd,pid_t pid);
void printProcessList(process** process_list);
void terminatePid(struct process ** process_list , int pid);
void freeProcessList(struct process ** process_list);
void freeListHelper(struct process * process_list);
void updateProcessList(process **process_list);
void updateProcessStatus(process** process_list, int pid, int status);
void updateProcessStatusHelper(process* process_list, int pid, int status);
void deleteTerminated(process** process_list);
void delete (process* process_list);


void updateProcessStatus(process** process_list, int pid, int status)
{
    updateProcessStatusHelper(*process_list, pid, status);
}

void updateProcessStatusHelper(process* current, int pid, int status)
{
    if(current == NULL)
    {
        exit(1);
    }
    if(pid == current->pid)
    {
        current->status = status;
    }
    else
    {
        updateProcessStatusHelper(current->next, pid, status);
    }
}

void updateProcessList(process **process_list)
{
    int status;
    process * current = *process_list;
    while(current != NULL) {
        if (waitpid(current->pid, &status, WNOHANG) == -1) {
            current->status = TERMINATED;
        }
        current = current->next;
    }
}


void freeProcessList(struct process ** process_list)
{
    struct process * current = *process_list;
    freeListHelper(current);

}
void freeListHelper(struct process * current)
{
    if(current != NULL) {
        freeListHelper(current->next);
        freeCmdLines(current->cmd);
        free(current);
    }
}




void terminatePid(struct process ** process_list ,int pid)
{
    struct process * current = *process_list;

    if(current == NULL)
    {
        exit(1);
    }
    while (current->next != NULL)
    {
        if(current->pid == pid)
        {
            current->status = TERMINATED;
        }
        current = current->next;
    }
    exit(1);
}

void deleteTerminated(process** process_list)
{
    process * pos = *process_list;
    process * deleteMe;
    process * prev;
    if (pos == NULL)
    {
        return;
    }
    while (pos->status == TERMINATED)
    {
        deleteMe = pos;
        pos = pos->next;
        delete(deleteMe);

        if (pos == NULL)
        {
            *process_list = NULL;
            return;
        }
    }
    *process_list = pos;

    while(pos != NULL)
    {
        prev = pos;
        pos = pos->next;
        if(pos == NULL)
        {
            return;
        }
        while (pos->status == TERMINATED)
        {
            deleteMe = pos;
            pos = pos->next;
            delete(deleteMe);

            if (pos == NULL)
            {
                prev->next = NULL;
                return;
            }
        }
        prev->next = pos;

    }

}
void delete (process* proc)
{
    freeCmdLines(proc->cmd);
    free(proc);
}
void printProcessList(process** process_list)
{
    updateProcessList(process_list);
    process * current = *process_list;
    char * cmd;
    int i=1;
    int j;
    int cmdCharLength;
    int pad;
    int status;
    fprintf(stdout, "PID      cmd      status\n");


    while(current != NULL)
    {
        fprintf(stdout, "%d",current->pid);
        fprintf(stdout, "    ");

        cmd =  current->cmd->arguments[0];
        cmdCharLength = strlen(cmd);
        fprintf(stdout, "%s", cmd);
        pad = 9 - cmdCharLength;
        if(pad <= 0)
        {
            pad = 1;
        }
        for(j=0; j<pad; j++)
        {
            fprintf(stdout, " ");
        }
        status = current->status;
        if (status == RUNNING)
        {
            fprintf(stdout, "RUNNING");
        }
        else if (status == SUSPENDED)
        {
            fprintf(stdout, "SUSPENDED");
        }
        else if (status == TERMINATED)
        {
            fprintf(stdout, "TERMINATED");
        } else
        {
            fprintf(stdout, "NO STATUS");
        }
        fprintf(stdout, "\n");
        current = current->next;
        i++;
    }
    deleteTerminated(process_list);
}

void addProcess(process** process_list, cmdLine* cmd, pid_t pid)
{
    struct process * newProcess = (struct process *)malloc(sizeof(struct process));
    newProcess->cmd = cmd;
    newProcess->pid = pid;
    newProcess->status = RUNNING;
    newProcess->next = NULL;

    struct process * current1 = *process_list;
    if(current1 == NULL)
    {
        *process_list = newProcess;
    }
    else
    {
        struct process *current2 = current1->next;
        while (current2 != NULL)
        {
            current1 = current1->next;
            current2 = current2->next;
        }
        current1->next = newProcess;
    }
}
void pwd()
{
    char cwd[256];
    getcwd(cwd, sizeof(cwd));
    printf("%s$ ", cwd);
}

void execute(cmdLine *pCmdLine)
{
    if(pCmdLine->inputRedirect != NULL)
    {
        close(STDIN_FILENO);
        open(pCmdLine->inputRedirect, O_RDWR, 0777);
    }
    if(pCmdLine->outputRedirect != NULL)
    {
        close (STDOUT_FILENO);
        open(pCmdLine->outputRedirect, O_RDWR | O_CREAT, 0777);
    }


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
    int procs;
    pid_t n;
    int toWake, toKill, toSuspend;
    for(i=0;i<argc;i++)
    {
        if (strcmp(argv[i],"-d")==0)
        {
            debug = 1;
        }
    }

    struct process * pos = (struct process *)malloc(sizeof(struct process));
    struct process ** process_list = &pos;
    free(pos);
    * process_list = NULL;

    printf("Starting the program\n");
    char input[2048];
    cmdLine * parsedCLines;
    int pid;
    int specialProc;
    while (1) {
        cd = 0;
        procs = 0;
        toWake = 0;
        toKill = 0;
        toSuspend = 0;
        specialProc = 0;
        pwd();
        fgets(input, 2048, stdin);
        if (strcmp(input, "quit\n")==0)
        {
            freeProcessList(process_list);
            exit(0);
        }
        parsedCLines = parseCmdLines(input);

        pid = fork();
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
            specialProc = 1;
        }
        else if (strcmp(parsedCLines->arguments[0], "procs") == 0)
        {
            procs = 1;
            specialProc = 1;
        }
        else if (strcmp(parsedCLines->arguments[0], "suspend") == 0)
        {
            toSuspend = 1;
            specialProc = 1;
        }
        else if (strcmp(parsedCLines->arguments[0], "kill") == 0)
        {
            toKill = 1;
            specialProc = 1;
        }
        else if (strcmp(parsedCLines->arguments[0], "wake") == 0)
        {
            toWake = 1;
            specialProc = 1;
        }

        if((specialProc == 1) && (pid != 0)) // quarter 1
        {
            if (cd == 1) {
                status = chdir(parsedCLines->arguments[1]);
                if (status == -1) // cd failed
                {
                    perror("Error: ");
                    freeCmdLines(parsedCLines);
                    kill(-1, SIGKILL);
                    exit(errno);
                }
            }
            else if (procs == 1)
            {
                printProcessList(process_list);
            }
            else if (toKill == 1)
            {
                n = (pid_t ) atoi(parsedCLines->arguments[1]);
                status = kill(n, SIGKILL);
                if (status == 0)
                {
                    fprintf(stderr, "Looper handling SIGINT\n");
                    updateProcessStatus(process_list,n,TERMINATED);
                }
                else{
                    exit(1);
                }
            }
            else if (toWake == 1)
            {
                n = (pid_t ) atoi(parsedCLines->arguments[1]);
                status = kill(n, SIGCONT);
                if (status == 0)
                {
                    fprintf(stderr, "Looper handling SIGCONT\n");
                    updateProcessStatus(process_list,n,RUNNING);
                } else{
                    exit(1);
                }
            }
            else if (toSuspend == 1)
            {
                n = (pid_t ) atoi(parsedCLines->arguments[1]);
                status = kill(n, SIGTSTP);
                if (status == 0)
                {
                    fprintf(stderr, "Looper handling SIGTSTP\n");
                    updateProcessStatus(process_list,n,SUSPENDED);
                } else{
                    perror("Error: ");
                    exit(errno);
                }
            }



            freeCmdLines(parsedCLines);
        }
        else if ((specialProc == 0) && (pid != 0)) // quarter 2
        {
            addProcess(process_list, parsedCLines, pid);
            waitpid(pid, &status,!parsedCLines->blocking);
        }
        else if ((specialProc == 1) && (pid == 0)) // quarter 3
        {
            freeCmdLines(parsedCLines);
            _exit(0);
        }
        else if ((specialProc  == 0) && (pid == 0)) // qurter 4
        {
            execute(parsedCLines);
            freeCmdLines(parsedCLines);
            _exit(errno);
        }


    }

    return 0;
}
