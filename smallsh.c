/*
Assignment 3: smallsh (Portfolio Project)
Author: Judah Jackson
OSU email: jacksjud@oregonstate.edu
GitHub: jacksjud
CS374 - Operating Systems 1 (OS1)
Date: 05-14-2024

Description:

Build Using:
gcc --std=gnu99 -o smallsh smallsh.c

Program goals:

    Provide a prompt for running commands
    Handle blank lines and comments, which are lines beginning with the # character
    Provide expansion for the variable $$
    Execute 3 commands exit, cd, and status via code built into the shell
    Execute other commands by creating new processes using a function from the exec family of functions
    Support input and output redirection
    Support running commands in foreground and background processes
    Implement custom handlers for 2 signals, SIGINT and SIGTSTP


Dev Notes:
* use printenv in terminal to see environment variables
* 

*/

/* Includes */
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

/* Definitions */
#define MAX_CHARS 2048
#define MAX_ARGS 512

/* Struct(s) */

typedef struct Command {
    char * command[MAX_ARGS + 1];
    short int skip;
    short int builtin;
    short int background;
    int argCount;
} Command;

/* Function Prototypes*/

Command * parseCommand();
char* replaceToken(char *, char *);
void execCommand(Command *);
void execExit();
void execCd(Command *);
void execStatus(Command *);
void execOther(Command *);
void freeCommand(Command *);
char * checkExpansion(char *, char *);

void test_replaceToken();


/* ----------------------------------------
    Function: 
///////////////////////////////////////////
Desc: 

Params:
---------------------------------------- */

/* ----------------------------------------
    Function: main
///////////////////////////////////////////

Desc: main function, handles program starting.
Creates infinite loop that keeps shell alive.

Params: N/A
---------------------------------------- */
int main(){

    //test_replaceToken();

    while(1){
        // Parse command 
        Command * command = parseCommand();
        if(command->skip){
            free(command);
            continue;
        }
        execCommand(command);
        freeCommand(command);
    }

    
    return 0;
}

/* ----------------------------------------
    Function: parseCommand
///////////////////////////////////////////
Desc: Prompts user and gets input, then takes
it and parses it, splitting it by spaces, and checks
other factors such as whether it's a comment or a blank
line, in which case it returns after marking it, and whether
the command is meant to be ran in the background. It also
checks if the command (first argument) is built in. 

Params: N/A
---------------------------------------- */
Command * parseCommand(){
    // Struct that represents a command 
    Command * command = (Command *)malloc(sizeof(Command));
    if (command == NULL) {
        fprintf(stderr, "Memory allocation failed.\n");
        exit(EXIT_FAILURE);
    }
    command->argCount = 0;                  // Initially no command arguments
    command->skip = 0;
    command->builtin = 0;
    command->background = 0;

    printf(": ");                           // Prompt user interaction, make sure it's output
    fflush(stdout);
    
    // Represents command given to shell in string form (max size)
    char commandStr[MAX_CHARS];
    fgets(commandStr, MAX_CHARS, stdin);        // Gets command and saves in string variable

    char * token = strtok(commandStr , " ");    // Gets token (argument) based on a space (' ') as the delimiter


    // Checks if comment or blank - only applicable to first argument (command)
    if( strcmp(token, "\n") == 0 || strncmp(token , "#", 1) == 0){
        command->skip = 1;
        return command;
    }

    // Continue until we reach the end (cannot be NULL, or ending character '\0')
    while(token != NULL && strcmp(token, "\0") != 0){
        // Check if token has variable that needs to be replaced
        char * position = strstr(token, "$$");
        if(position != NULL){
            replaceToken(token, position);
        }

        if(command->argCount >= MAX_ARGS){
            fprintf(stderr, "MAX ARGS REACHED == \n");
            exit(EXIT_FAILURE);
        }

        // Allocate memory for command arg
        command->command[command->argCount] = (char *)malloc(MAX_CHARS);
        if (command->command[command->argCount] == NULL) {
            fprintf(stderr, "Memory allocation failed.\n");
            exit(EXIT_FAILURE);
        }


        // Copies token to command argument array
        strncpy(command->command[command->argCount], token, MAX_ARGS - 1);
        //command->command[command->argCount][MAX_CHARS - 1] = '\0'; // Ensure null-termination
        command->argCount++;                    // Increment arg count
        token = strtok(NULL, " ");              // Update token to point to next
    }

    command->command[command->argCount] = (char *)malloc(2);
    strncpy(command->command[command->argCount], "\0", 2);
    command->argCount++;

    
    // Checks if meant to executed in background
    if(strcmp(command->command[command->argCount - 1], "&") == 0 || strcmp(command->command[command->argCount - 1], "&\n") == 0){
        printf("background! == %s", command->command[command->argCount - 1]);
        command->background = 1;
    }

    // Checks if built in command, give appropriate code
    if(strcmp(strtok(command->command[0] , "\n"), "exit") == 0){
        command->builtin = 1;
    }
    else if(strcmp(strtok(command->command[0], "\n"), "cd") == 0) {
        command->builtin = 2;
    }
    else if(strcmp(strtok(command->command[0], "\n"), "status") == 0) {
        command->builtin = 3;
    }
    return command;
}

/* ----------------------------------------
    Function: execCommand
///////////////////////////////////////////
Desc: 

Params:
---------------------------------------- */

void execCommand(Command * command){

    // If command meant to be ran in background
    if(command->background){
        /* 
        We want this to execute, but we don't want
        the parent to wait, either way it's going to fork
        */
    }
    
    pid_t spawnpid = -5;
	int childStatus;
    int childPid;
    // If fork is successful, the value of spawnpid will be 0 in the child, the child's pid in the parent
    spawnpid = fork();
    switch (spawnpid){
        case -1:
            perror("fork() failed!");
            exit(1);
        case 0:
            // spawnpid is 0 in the child
            printf("I am the child. My pid  = %d\n", getpid());
            switch(command->builtin){
                case 1:
                    freeCommand(command);
                    execExit();
                    break;
                case 2:
                    execCd(command);
                    break;
                case 3:
                    execStatus(command);
                    break;
                default:
                    execOther(command);
                    break;
            }
            break;
        default:
            // spawnpid is the pid of the child
            printf("I am the parent. My pid  = %d\n", getpid());
            // If command meant to be ran in background
            if(command->background){
                /* 
                We want this to execute, but we don't want
                the parent to wait, either way it's going to fork
                */
                printf("Command meant for background - parent returning: %d\n", getpid());
                return;
            }
            childPid = waitpid(spawnpid , &childStatus, 0);
            printf("Parent's waiting is done as the child with pid %d exited\n", childPid);
            break;
    }
}



/* ----------------------------------------
    Function: execExit
///////////////////////////////////////////
Desc: Kills any other processes or jobs that
the shell has started before it terminates itself.

Params: N/A
---------------------------------------- */
void execExit(){
    kill(0, SIGTERM);       // Send signal to all processes 
    exit(EXIT_SUCCESS);     // Exit shell
}


/* ----------------------------------------
    Function: execCd
///////////////////////////////////////////
Desc: Changes the working directory of smallsh.
If no arguments are given, it changes to the 
directory specified in the HOME environment variable 

Params:
command: Command *
---------------------------------------- */
void execCd(Command * command){
    // Check if cd has no other arguments, in which case cd HOME
    if(strcmp(command->command[1], "\n") == 0 || strcmp(command->command[1], "\0") == 0){ 
        chdir(getenv("HOME"));
        system("ls");
        return;
    }
    if(chdir(strtok(command->command[1], "\n")) != 0){
        perror("Error with chdir");
    }
    system("ls");
}


/* ----------------------------------------
    Function: execStatus
///////////////////////////////////////////
Desc: Prints out either the exit status or the 
terminating signal of the last foreground process 
ran by the shell. If ran before any foreground 
command is run, then it simply returns the exit 
status 0.

Params: 
command: Command *
---------------------------------------- */
void execStatus(Command * command){

}


/* ----------------------------------------
    Function: execOther
///////////////////////////////////////////
Desc: 

Params:
---------------------------------------- */
void execOther(Command * command){

}


void freeCommand(Command * command){
    for(int i = 0; i < command->argCount ; i++){
        free(command->command[i]);
    }
    free(command);
}

/* ----------------------------------------
    Function: replaceToken
///////////////////////////////////////////
Desc: Takes pointer, token, and replaces the location
specified by the substring pointer with the pid.
It checks for multiple occurances.

Params:
token: char * , string to change.
substring: char * , part of string to change
---------------------------------------- */
char* replaceToken(char *token, char *substring){
    char pidStr[20];                        // String representation for process ID (pid)

    sprintf(pidStr, "%d", getpid());        // Convert process ID to string

    int pidLen = strlen(pidStr);            // Gets length of pid now that it's in string form.

    int restLen = strlen(substring + 2);    // Calculate the length of the rest of the token after the substring


    /*
    Easy to get confused here, this is what's happening:
    destination =   'substring + pidLen' where the characters after the $$ are being copied (memory address)
    source      =   'substring + 2'  directly after the $$, what is being copied over (memory address)
    size        =   'restLen + 1' this is the number of bytes to copy over , aka size of the remaining chars
    */
    memmove(substring + pidLen, substring + 2, restLen + 1);

    
    strncpy(substring, pidStr, pidLen);     // Copy the process ID string into the token, in the new space we just created.

    // Check again to see if there are anymore, if so, recurse.
    char * newSubstring = strstr(token, "$$");
    if (newSubstring != NULL) {
        replaceToken(token, newSubstring);
    }
}


// Tester function 
void test_replaceToken() {
    char token[] = "TEST$$TEST$$$";
    char *substring = strstr(token, "$$");
    if (substring != NULL) {
        printf("Original token: %s\n", token);
        replaceToken(token, substring);
        printf("Modified token: %s\n", token);
    } else {
        printf("Substring not found\n");
    }
}