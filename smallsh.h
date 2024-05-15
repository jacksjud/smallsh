#ifndef SMALLSH_H
#define SMALLSH_H

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#define MAX_CHARS 2048
#define MAX_ARGS 512

typedef struct Command {
    char command[MAX_ARGS];
    short int skip;
    short int builtin;
    short int background;
    int argCount;
} Command;

Command * parseCommand();
void execCommand(Command *);
void freeCommand(Command *);
char * checkExpansion(char *, char *);
char* replaceToken(char *, char *);
void test_replaceToken();

#endif /* SMALLSH_H */
