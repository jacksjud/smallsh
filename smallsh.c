/*
Author: Judah Jackson
OSU email: jacksjud@oregonstate.edu
GitHub: jacksjud

Assignment 3: smallsh (Portfolio Project)
CS374 - Operating Systems 1 (OS1)
Date: 05-14-2024

Description:

Build Using:
gcc --std=gnu99 -o smallsh smallsh.c

Program goals:

X   Provide a prompt for running commands
X   Handle blank lines and comments, which are lines beginning with the # character
X   Provide expansion for the variable $$
X   Execute 3 commands exit, cd, and status via code built into the shell
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
#include <fcntl.h>

/* Definitions */
#define MAX_CHARS 2048
#define MAX_ARGS 512

/* == DEBUG == */
// #define DEBUG

/* Struct(s) */

typedef struct Command {
    char * command[MAX_ARGS + 1];
    short int skip;
    short int builtin;
    short int background;
    int argCount;
} Command;

/* Function Prototypes*/
void handleSIGINT(int signal);
void configSIGINT();
void handleSIGTSTP(int signal);
void configSIGTSTP();
Command * parseCommand();
char* replaceToken(char *, char *);
void execCommand(Command *);
void inputRedirect(Command *);
void outputRedirect(Command *);
void execExit();
void execCd(Command *);
void execStatus(Command *);
void execOther(Command *);
void freeCommand(Command *);
char * checkExpansion(char *, char *);

void test_replaceToken();

/* Global Variables */
int lastForegroundStatus = 0;
int foregroundOnlyMode = 0;


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

    #ifdef DEBUG
    test_replaceToken();
    #endif
    configSIGINT();
    configSIGTSTP();


    while(1){
        // Parse command 
        Command * command = parseCommand();
        if(command->skip){
            freeCommand(command);
            continue;
        }
        execCommand(command);
        freeCommand(command);
    }
    return 0;
}

/* ----------------------------------------
    Function: handleSIGINT
///////////////////////////////////////////
Desc: Makes the shell, i.e., the parent process,
ignore SIGINT. Any children running as background 
processes ignore SIGINT. A child running as 
a foreground process will terminate itself when 
it receives SIGINT. The parent does not attempt to
terminate the foreground child process; instead 
the foreground child (if any) terminates 
itself on receipt of this signal. If a child 
foreground process is killed by a signal, the 
parent immediately prints out the number of 
the signal that killed it's foreground child process 
before prompting the user for the next command.

Params:
---------------------------------------- */
void handleSIGINT(int signal){
    if(signal == SIGINT){
        #ifdef DEBUG
        printf("\n == SIGINT ignored in parent process (shell): %d\n", getpid());
        fflush(stdout);
        #endif
    }
    printf("\n: ");                           // Prompt user interaction, make sure it's output
    fflush(stdout);
}

void configSIGINT(){
    // Initialize SIGINT_action struct to be empty
    struct sigaction SIGINT_action = {0};

    // Fill out the SIGINT_action struct
    // Register handleSIGINT as the signal handler
    SIGINT_action.sa_handler = handleSIGINT;
    // Block all catchable signals while handleSIGINT is running
    sigfillset(&SIGINT_action.sa_mask);
    // System auto restarts
    SIGINT_action.sa_flags = SA_RESTART;

    // Install our signal handler
    sigaction(SIGINT, &SIGINT_action, NULL);

    #ifdef DEBUG
    
    return;
    #endif
}


/* ----------------------------------------
    Function: handleSIGTSTP
///////////////////////////////////////////
Desc: A CTRL-Z command from the keyboard 
sends a SIGTSTP signal to the parent shell 
process and all children at the same time 
(this is a built-in part of Linux).A child, 
if any, running as a foreground process must 
ignore SIGTSTP. Any children running as 
background process must ignore SIGTSTP.
When the parent process running the shell
receives SIGTSTP The shell must display an 
informative message (see below) immediately 
if it's sitting at the prompt, or immediately 
after any currently running foreground process 
has terminated. The shell then enters a state 
where subsequent commands can no longer be 
run in the background. In this state, the & 
operator should simply be ignored, i.e., all 
such commands are run as if they were 
foreground processes. If the user sends SIGTSTP 
again, then your shell will. Display another 
informative message (see below) immediately
after any currently running foreground process 
terminates. The shell then returns back to the 
normal condition where the & operator is once 
again honored for subsequent commands, allowing 
them to be executed in the background. See the
example below for usage and the exact syntax 
which you must use for these two informative 
messages.

Params:
---------------------------------------- */
void handleSIGTSTP(int signal){
    if (signal == SIGTSTP) {
        if (foregroundOnlyMode == 0) {
            // Enter foreground-only mode
            foregroundOnlyMode = 1;
            printf("\nEntering foreground-only mode (& is now ignored)");
            fflush(stdout);

        } else {
            // Exit foreground-only mode
            foregroundOnlyMode = 0;
            printf("\nExiting foreground-only mode");
            fflush(stdout);
        }
    }
    printf("\n: ");                           // Prompt user interaction, make sure it's output
    fflush(stdout);
    return;
}

void configSIGTSTP(){
    struct sigaction SIGTSTP_action = {0};
    SIGTSTP_action.sa_handler = handleSIGTSTP;
    // Block all catchable signals while handleSIGINT is running
    sigfillset(&SIGTSTP_action.sa_mask);
    // System calls auto restart
    SIGTSTP_action.sa_flags = SA_RESTART;
    sigaction(SIGTSTP, &SIGTSTP_action, NULL);
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
    if(command == NULL) {
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
    if(fgets(commandStr, MAX_CHARS, stdin) == NULL) {
        freeCommand(command);
        exit(EXIT_FAILURE); // Exit on read failure
    }
    commandStr[strcspn(commandStr, "\n")] = '\0'; // Remove newline

    char * token = strtok(commandStr , " ");    // Gets token (argument) based on a space (' ') as the delimiter

    // Check if line is skippable, mark and return if so
    if (token == NULL || token[0] == '#') {
        command->skip = 1;
        return command;
    }

    // Continue until we reach the end (cannot be NULL, or ending character '\0')
    while(token != NULL){
        if(command->argCount >= MAX_ARGS) {
            fprintf(stderr, "MAX ARGS REACHED\n");
            freeCommand(command);
            exit(EXIT_FAILURE);
        }

        // Allocate memory for command arg
        command->command[command->argCount] = (char *)malloc(MAX_CHARS);
        if(command->command[command->argCount] == NULL) {
            fprintf(stderr, "Memory allocation failed.\n");
            freeCommand(command);
            exit(EXIT_FAILURE);
        }

        // Check if token has variable that needs to be replaced
        char * position = strstr(token, "$$");
        if(position != NULL){
            replaceToken(token, position);
        }

        // Copies token to command argument array
        strncpy(command->command[command->argCount], token, MAX_ARGS - 1);

        //command->command[command->argCount][MAX_CHARS - 1] = '\0'; // Ensure null-termination
        command->argCount++;                    // Increment arg count
        token = strtok(NULL, " ");              // Update token to point to next
    }

    // Null-terminate the command array
    command->command[command->argCount] = NULL;

    // Check if background command - look for '&' symbol
    if (command->argCount > 0 && strcmp(command->command[command->argCount - 1], "&") == 0 && foregroundOnlyMode != 1){
        command->background = 1;                            // Mark as background
        free(command->command[command->argCount - 1]);      // Deallocate that part of the command 
        command->command[command->argCount - 1] = NULL;     // Make old command new pointer to NULL
        command->argCount--;                                // Decrease argCount
    }

    // Checks if built in command, give appropriate code
    if(strcmp(strtok(command->command[0] , "\n"), "exit") == 0){
        command->builtin = 1;
    } else if(strcmp(strtok(command->command[0], "\n"), "cd") == 0) {
        command->builtin = 2;
    } else if(strcmp(strtok(command->command[0], "\n"), "status") == 0) {
        command->builtin = 3;
    }
    return command;
}

/* ----------------------------------------
    Function: execCommand
///////////////////////////////////////////
Desc: Determines which command is being
executed, calls approrpiate function, such
as one of the built in functions, or the
function that handles everything else.

Params:
command: Command * , command to process
---------------------------------------- */
void execCommand(Command * command){    

    inputRedirect(command);
    outputRedirect(command);

    // Check if command is built in, if so switch to appropriate
    if(command->builtin){
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
        }
    } else {
        // If it's not built in, pass to function to use exec family 
        execOther(command);
    }
}

void inputRedirect(Command * command){
    int inputFile = -1;
    for(int i = 0; command->command[i] != NULL; i++){
        if(strcmp(command->command[i], "<") == 0) {
            if(command->command[i + 1] != NULL) {
                inputFile = open(command->command[i + 1], O_RDONLY);
                if(inputFile == -1) {
                    perror("failed to open input file");
                    return;
                    exit(EXIT_FAILURE);
                }
                dup2(inputFile, STDIN_FILENO);
                close(inputFile);
                // Shift command arguments to remove "<" and input file name
                for(int j = i; command->command[j] != NULL; j++){
                    command->command[j] = command->command[j + 2];
                }
            } else{
                fprintf(stderr, "Syntax error: missing input file name after '<'\n");
                exit(EXIT_FAILURE);
            }
        }
    }
}


void outputRedirect(Command * command){
    int outputFile = -1;
    for (int i = 0; command->command[i] != NULL; i++) {
        if(strcmp(command->command[i], ">") == 0) {
            if (command->command[i + 1] != NULL) {
                outputFile = open(command->command[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0666);
                if (outputFile == -1) {
                    perror("failed to open output file");
                    exit(EXIT_FAILURE);
                }
                dup2(outputFile, STDOUT_FILENO);
                close(outputFile);
                // Shift command arguments to remove ">" and output file name
                for (int j = i; command->command[j] != NULL; j++) {
                    command->command[j] = command->command[j + 2];
                }
            } else {
                fprintf(stderr, "Syntax error: missing output file name after '>'\n");
                exit(EXIT_FAILURE);
            }
        }
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
    sleep(1);
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
    /* Problem: when running: 'cd &', we get an error.*/


    // Check if cd has no other arguments, in which case cd HOME
    if(command->argCount < 2){ 
        chdir(getenv("HOME"));
        #ifdef DEBUG
        system("ls");
        #endif
        return;
    } else {
        if(chdir(strtok(command->command[1], "\n")) != 0){
            perror("Error with chdir");
    }
    }
    #ifdef DEBUG
    system("ls");
    #endif
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
    if (WIFEXITED(lastForegroundStatus)) {
        printf("Exit status: %d\n", WEXITSTATUS(lastForegroundStatus));
    } else if (WIFSIGNALED(lastForegroundStatus)) {
        printf("Terminating signal: %d\n", WTERMSIG(lastForegroundStatus));
    } else {
        printf("Unknown termination status\n");
    }
    fflush(stdout);
}


/* ----------------------------------------
    Function: execOther
///////////////////////////////////////////
Desc: Uses the execvp function to execute
all commands that are not "cd", "exit", or
"status".

Params:
command: Command * , command to be executed
---------------------------------------- */
void execOther(Command * command){

    /* ERROR: Currently, only execOther commands
    use fork(), this is not optimal. We'll want
    to have it forked in the execCommand() function
    (I think..?)*/

    pid_t spawnpid = fork();
    if (spawnpid == -1) {
        perror("fork() failed!");
        exit(1);
    } else if (spawnpid == 0) {
        // Child executes command 
        execvp(command->command[0], command->command);
        perror("execvp");
        exit(1);
    } else {
        // Parent checks if meant to be background
        if (!command->background) {
            // If not meant to be in background then update the last foreground status
            int childStatus;
            waitpid(spawnpid, &childStatus, 0);
            lastForegroundStatus = childStatus; // Update the lastForegroundStatus variable
        } else {
            // Otherwise print the background pid
            printf("Background PID: %d\n", spawnpid);
            fflush(stdout);
        }
    }
}

/* ----------------------------------------
    Function: freeCommand
///////////////////////////////////////////
Desc: Frees allocated memory for each command
given to the smallsh CLT.

Params: 
command: Command * , command to free
---------------------------------------- */
void freeCommand(Command * command){
    for(int i = 0; i < command->argCount ; i++){
        #ifdef DEBUG
            printf("Freeing command[%d]: %s\n", i, command->command[i]);
            fflush(stdout);
        #endif
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
    // String representation for process ID (pid)
    char pidStr[20];                        
    // Convert process ID to string
    sprintf(pidStr, "%d", getpid());        
    // Gets length of pid now that it's in string form.
    int pidLen = strlen(pidStr);            
    // Calculate the length of the rest of the token after the substring
    int restLen = strlen(substring + 2);    

    /*
    Easy to get confused here, this is what's happening:
    destination =   'substring + pidLen' where the characters after the $$ are being copied (memory address)
    source      =   'substring + 2'  directly after the $$, what is being copied over (memory address)
    size        =   'restLen + 1' this is the number of bytes to copy over , aka size of the remaining chars
    */
    memmove(substring + pidLen, substring + 2, restLen + 1);

    // Copy the process ID string into the token, in the new space we just created.
    strncpy(substring, pidStr, pidLen);     

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