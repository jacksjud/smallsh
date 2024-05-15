/*
Assignment 3: smallsh (Portfolio Project)
Author: Judah Jackson
OSU email: jacksjud@oregonstate.edu
GitHub: jacksjud
CS374 - Operating Systems 1 (OS1)
Date: 05-14-2024

Description:

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
    char command[MAX_ARGS];
    short int skip;
    short int builtin;
    short int background;
    int argCount;

} Command;

/* Function Prototypes*/

Command * parseCommand();
void execCommand(Command *);
void freeCommand(Command *);
char * checkExpansion(char *, char *);
char* replaceToken(char *, char *);
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

    test_replaceToken();

    // while(1){
    //     // Parse command 
    //     Command * command = parseCommand();
    //     if(command->skip){
    //         free(command);
    //         continue;
    //     }
    //     execCommand(command);
    // }

    
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
    command->argCount = 0;                  // Initially no command arguments

    printf(": ");                           // Prompt user interaction, make sure it's output
    fflush(stdout);

    // Represents command given to shell in string form (max size)
    char commandStr[MAX_CHARS];
    fgets(commandStr, MAX_CHARS, stdin);    // Gets command and saves in string variable

    char * token = strtok(commandStr , " ");// Gets token (argument) based on a space (' ') as the delimiter
    
    // Checks if comment or blank
    if(token[0] == "#" || token[0] == "\n"){
        command->skip = 1;
        return command;
    }
        
    

    // Continue until we reach the end (cannot be NULL, or ending character '\0')
    while(token != NULL || strcmp(token, "\0" != 0)){
        
        char * position = strstr(token, "$$");
        if(position != NULL){
        checkExpansion(token, position);
        }
        // Copies token to command argument array
        strncpy(command->command[command->argCount], token, strlen(token));
        command->argCount++;                // Increment arg count
        token = strtok(NULL, " ");          // Update token to point to next
    }
    
    // Checks if meant to executed in background
    if(command->command[command->argCount] == "&"){
        command->background = 1;
    }

    // Checks if built in command
    if(command->command[0] == "exit" || command->command[0] == "cd" || command->command[0] == "status"){
        command->builtin = 1;
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



    freeCommand(command);
}

/* ----------------------------------------
    Function: 
///////////////////////////////////////////
Desc: 

Params:
---------------------------------------- */
char* replaceToken(char *token, char *substring) {
    char pid_str[20]; // Buffer to hold the string representation of the process ID
    sprintf(pid_str, "%d", getpid()); // Convert process ID to string

    // Calculate the length of the substring to be replaced
    int sub_len = strlen(substring);
    int pid_len = strlen(pid_str);

    // Calculate the length of the new token
    int new_len = strlen(token) - sub_len + pid_len;

    // Shift the rest of the string to make space for the PID
    memmove(substring + pid_len, substring + sub_len, strlen(substring + sub_len) + 1);

    // Copy the PID string into the token
    strncpy(substring, pid_str, pid_len);

    char * pos = strsts(token , "$$");
    if(pos != NULL){
        return replaceToken(token, pos);
    }
    return token;
}
// Tester function 
void test_replaceToken() {
    char token[] = "TEST$$TEST";
    char *substring = strstr(token, "$$");
    if (substring != NULL) {
        printf("Original token: %s\n", token);
        char *new_token = replaceToken(token, substring);
        printf("Modified token: %s\n", new_token);
    } else {
        printf("Substring not found\n");
    }
}