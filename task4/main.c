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



typedef struct pair
{
    char * name;
    char * value;
    struct pair * next;
    struct cmdLine * cmd;
} pair;


void execute(cmdLine *pCmdLine);
void pwd();
struct pair * findPair (struct pair ** list, char * name);
struct pair * getPrevious(struct pair ** list, struct pair * pos);

void pwd()
{
    char cwd[256];
    getcwd(cwd, sizeof(cwd));
    printf("%s$ ", cwd);
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

struct pair * findPair (struct pair ** list, char * name)
{
    if (list == NULL)
    {
        return NULL;
    }
    struct  pair * pos = *list;
    while (pos != NULL)
    {
        if (strcmp(pos->name, name) == 0)
        {
            return pos;
        } else{
            pos = pos->next;
        }
    }
    return NULL;
}

struct pair * getPrevious(struct pair ** list, struct pair * pos)
{
    if (*list == NULL)
    {
        fprintf(stderr, "ERROR finding previous");
        exit(1);
    }
    struct pair * cur = *list;
    if (cur == pos)
    {
        return NULL;
    }
    while(cur->next != NULL)
    {
        if(cur->next == pos)
        {
            return cur;
        }
        cur = cur->next;
    }
    exit(1); //UNREACHABLE
}



int main(int argc, char **argv)
{

    int status;
    int cd;
    int set;
    int vars;
    int delete;
    int pipefd[2];
    int i;
    int child=0;
    int child1 = 0;
    int child2 = 0;
    char blocking1 = '0';
    char blocking2 = '0';
    int specialActions = 0;
    struct pair * ezer = NULL;
    struct pair ** list = &ezer;

    char * name;
    char * value;

    printf("Starting the program\n");
    char input[2048];
    cmdLine * parsedCLines;
    int pid;

    while (1) {
        struct  pair * pos;
        cd = 0;
        set = 0;
        vars = 0;
        delete = 0;
        specialActions = 0;
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
            for (i=1; i < parsedCLines->argCount; i++)
            {
                if(strncmp(parsedCLines->arguments[i], "$", 1) == 0)
                {
                    char * var = parsedCLines->arguments[i];
                    memmove(var, var+1, strlen(var));
                    pos = findPair(list, var);
                    if (pos == NULL)
                    {
                        fprintf(stderr, "ERROR: Variable \"%s\" not found", var);
                        exit(1);
                        break;
                    }
                    if (replaceCmdArg(parsedCLines, i, pos->value) == 0)
                    {
                        fprintf(stderr, "ERROR REPLACING VARIABLE");
                        exit(1);
                    }
                }
            }
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

        if (strcmp(parsedCLines->arguments[0], "cd") == 0)
        {
            cd = 1;
            specialActions = 1;

        }
        else if (strcmp(parsedCLines->arguments[0], "set") == 0)
        {
            set = 1;
            specialActions = 1;
        }
        else if (strcmp(parsedCLines->arguments[0], "vars") == 0)
        {
            vars = 1;
            specialActions = 1;
        }
        else if (strcmp(parsedCLines->arguments[0], "delete") == 0)
        {
            delete = 1;
            specialActions = 1;
        }

        if((specialActions == 1) && (pid != 0)) // quarter 1 - PARENT process
        {

            if (cd == 1) {
                if(strncmp(parsedCLines->arguments[1],"~", 1) == 0)
                {
                    const char * homeDir = getenv("HOME");
                    chdir(homeDir);
                }
                else {
                    status = chdir(parsedCLines->arguments[1]);

                    if (status == -1) // cd failed
                    {
                        perror("Error: ");
                        freeCmdLines(parsedCLines);
                        kill(-1, SIGKILL);
                        exit(errno);
                    }
                }
            }
            else if(set == 1)
            {
                name = parsedCLines->arguments[1];
                value = parsedCLines->arguments[2];

                pos = findPair(list, name);

                if (pos != NULL)
                {
                    pos->value = value;
                }
                else // pos == NULL
                {
                    pos = (struct pair *)malloc(sizeof(struct pair));

                    pos->name = name;
                    pos->value = value;
                    if (list == NULL)
                    {
                        *list = pos;
                    }
                    else
                    {
                        pos->next = *list;
                        *list = pos;
                    }
                }
            }
            else if (vars == 1)
            {
                if (list != NULL)
                {
                    pos = *list;
                    printf("Name:      Value:\n");
                    while (pos != NULL)
                    {
                        printf("%s          %s\n",pos->name, pos->value);
                        pos = pos->next;
                    }
                }
            }
            else if (delete == 1)
            {
                char * var = parsedCLines->arguments[1];
                pos = findPair(list,var);
                if (pos == NULL)
                {
                    fprintf(stderr, "ERROR: Variable \"%s\" not found", var);
                    break;
                } else{
                    struct pair * prev = getPrevious(list, pos);
                    if(prev == NULL)
                    {
                        *list = pos->next;
                    } else{
                        prev->next = pos->next;
                    }
                    free(pos);
                    freeCmdLines(pos->cmd);
                }
            }
        }
        else if ((specialActions == 0) && (pid != 0)) // quarter 2 - PARENT process
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
        else if ((specialActions == 1) && (pid == 0)) // quarter 3 - CHILD process
        {
            freeCmdLines(parsedCLines);
            _exit(0);
        }
        else if ((specialActions  == 0) && (pid == 0)) // quarter 4 - CHILD process
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
