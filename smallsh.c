/*
Author: Judah Jackson
OSU email: jacksjud@oregonstate.edu
GitHub: jacksjud

Assignment 3: smallsh (Portfolio Project)
CS374 - Operating Systems 1 (OS1)
Date: 05-14-2024

Description:
A personal shell that implements a subset of features from
well known shells, like bash. More exact program goals are
listed below.

Build Using:
gcc --std=gnu99 -o smallsh smallsh.c

Program goals:
X   Provide a prompt for running commands
X   Handle blank lines and comments, which are lines beginning with the # character
X   Provide expansion for the variable $$
X   Execute 3 commands exit, cd, and status via code built into the shell
X   Execute other commands by creating new processes using a function from the exec family of functions
X   Support input and output redirection
X   Support running commands in foreground and background processes
X   Implement custom handlers for 2 signals, SIGINT and SIGTSTP
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
void handleSIGTSTP(int signal);
void configSIGS();
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
int foregroundProcessRunning = 0;



/* == DEBUG == */
// #define DEBUG
// #define DEBUG_SIGINT
// #define DEBUG_TOKEN


/* ----------------------------------------
    Function: main
===========================================

Desc: main function, handles program starting.
Creates infinite loop that keeps shell alive.

Params: N/A
---------------------------------------- */
int main(){
    // Debugging - Note: prefix 'DEBUG' meant for debugging, no functionality
    #ifdef DEBUG_TOKEN
    test_replaceToken();
    #endif

    // Configure the signals
    configSIGS();

    // Infinite loop
    while(1){
        // Parse command 
        Command * command = parseCommand();
        // Determine if we skip 
        if(command->skip){
            freeCommand(command);
            continue;
        }
        // Execute the command
        execCommand(command);
        // Free the command
        freeCommand(command);
    }
    return 0;
}

/* ----------------------------------------
    Function: handleSIGCHLD
===========================================

Desc: Handler function used for providing
status and termination signals.

Params: 
signal: int, signal to handle
---------------------------------------- */
void handleSIGCHLD(int signal) {
    pid_t childPid;
    int childStatus;

    // While valid
    while ((childPid = waitpid(-1, &childStatus, WNOHANG)) > 0) {
        // As long as there is a foreground process
        if (!foregroundProcessRunning) {
            // Check child status, print approrpiate info.
            if (WIFEXITED(childStatus)) {
                printf("\nBackground PID %d is done: exit value %d\n", childPid, WEXITSTATUS(childStatus));
            } else if (WIFSIGNALED(childStatus)) {
                printf("\nBackground PID %d is done: terminated by signal %d\n", childPid, WTERMSIG(childStatus));
            }
            // User prompt
            printf(": ");
            fflush(stdout);
        }
    }
}

/* ----------------------------------------
    Function: handleSIGINT
===========================================
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
Prints appropriate newline or newline + user prompt
depending on whether there is a foregorund process
actively running.

Params:
signal: int, signal to handle
---------------------------------------- */
void handleSIGINT(int signal){
    if (signal == SIGINT) {

        if (foregroundProcessRunning) {
            // A foreground process is running, the child process should handle its own termination
            printf("\n");
            fflush(stdout);
        } else {
            // No foreground process is running, print a new prompt
            printf("\n: ");
            fflush(stdout);
        }
    }
}


/* ----------------------------------------
    Function: configSIGS
===========================================
Desc: Configures signals with appropriate
handlers.

Params: N/A
---------------------------------------- */
void configSIGS(){
// SIGINT
    // Initialize SIGINT_action struct to be empty
    struct sigaction SIGINT_action = {0};
    // Set handler function
    SIGINT_action.sa_handler = handleSIGINT;
    // Block all catchable signals while handleSIGINT is running
    sigfillset(&SIGINT_action.sa_mask);
    // System auto restarts
    SIGINT_action.sa_flags = SA_RESTART;
    // Install signal handler
    sigaction(SIGINT, &SIGINT_action, NULL);

// SIGTSTP
    // Initialize SIGTSTP_action struct to be emtpy
    struct sigaction SIGTSTP_action = {0};
    // Set handler function
    SIGTSTP_action.sa_handler = handleSIGTSTP;
    // Block all catchable signals while handleSIGTSTP is running
    sigfillset(&SIGTSTP_action.sa_mask);
    // System calls auto restart
    SIGTSTP_action.sa_flags = SA_RESTART;
    // Install signal handler
    sigaction(SIGTSTP, &SIGTSTP_action, NULL);

// SIGCHLD
    // Initialize SIGCHLD_action struct to be emtpy
    struct sigaction SIGCHLD_action = {0};
    // Set handler function 
    SIGCHLD_action.sa_handler = handleSIGCHLD;
    // Block all catchable signals while handleSIGCHLD is running
    sigfillset(&SIGCHLD_action.sa_mask);
    // System calls auto restart and SIGCHLD won't generate where appropriate
    SIGCHLD_action.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    // Install signal handler
    sigaction(SIGCHLD, &SIGCHLD_action, NULL);
}


/* ----------------------------------------
    Function: handleSIGTSTP
===========================================
Desc: Handler function for SIGTSTP, displays
appropriate message, changes foregroundOnlyMode
variable, and reprints the user prompt.

Params: 
signal: int, signal to be handled
---------------------------------------- */
void handleSIGTSTP(int signal) {
    // Make sure valid signal
    if (signal == SIGTSTP) {
        // Print appropriate message
        if (foregroundOnlyMode == 0) {
            // Enter foreground-only mode
            foregroundOnlyMode = 1;
            printf("\nEntering foreground-only mode (& is now ignored)\n");
            fflush(stdout);
        } else {
            // Exit foreground-only mode
            foregroundOnlyMode = 0;
            printf("\nExiting foreground-only mode\n");
            fflush(stdout);
        }
    }
    // Check if user prompt needs to be made again
    if (!foregroundProcessRunning) {
        printf(": ");
        fflush(stdout);
    }
}


/* ----------------------------------------
    Function: parseCommand
===========================================
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
    // Default values for the command
    command->argCount = 0;                  // Initially no command arguments
    command->skip = 0;
    command->builtin = 0;
    command->background = 0;

    printf(": ");                           // Prompt user interaction, make sure it's output
    fflush(stdout);
    
    // Represents command given to shell in string form (max size)
    char commandStr[MAX_CHARS];
    // Gets command as string
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
    if (command->argCount > 0 && strcmp(command->command[command->argCount - 1], "&") == 0){
        // If in foregroundOnlyMode, skip marking it, but do everything else.
        if(foregroundOnlyMode != 1){
            command->background = 1;                            // Mark as background
        }
        free(command->command[command->argCount - 1]);      // Deallocate that part of the command 
        command->command[command->argCount - 1] = NULL;     // Make old command new pointer to NULL
        command->argCount--;                                // Decrease argCount
    }

    // Checks if built in command, give appropriate code
    // Compares first command argument (stripped of newline chars) for builtin
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
===========================================
Desc: Determines which command is being
executed, calls approrpiate function, such
as one of the built in functions, or the
function that handles everything else.

Params:
command: Command * , command to process
---------------------------------------- */
void execCommand(Command * command){    

    // To execute command, fork, parent runs shell
    pid_t spawnpid = fork();
    if (spawnpid == -1) {
        perror("fork() failed!");
        exit(1);
    } else if (spawnpid == 0) {
        // Child executes command 
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
    } else {
        // Parent checks if meant to be background
        if (!command->background) {
            foregroundProcessRunning = 1;
            // If not meant to be in background then update the last foreground status
            int childStatus;
            // Wait and get child status, shell does not return to user control until done.
            waitpid(spawnpid, &childStatus, 0);
            lastForegroundStatus = childStatus; // Update the lastForegroundStatus variable
            // If the child is signaled to stop, print info
            if (WIFSIGNALED(childStatus)) {
                int termSignal = WTERMSIG(childStatus);
                printf("\nTerminated by signal %d\n", termSignal);
                fflush(stdout);
            }
            foregroundProcessRunning = 0;
        } else {
            // Otherwise print the background pid
            printf("Background PID: %d\n", spawnpid);
            fflush(stdout);
        }
    }

    
}


/* ----------------------------------------
    Function: inputRedirect
///////////////////////////////////////////
Desc: Handles input redirects so shell can
handle the special character '<'

Params: command: Command *, command to search
---------------------------------------- */
void inputRedirect(Command * command){
    int inputFile = -1;
    // Goes through all commands
    for(int i = 0; command->command[i] != NULL; i++){
        // Looks for '<'
        if(strcmp(command->command[i], "<") == 0) {
            // If input file is not null
            if(command->command[i + 1] != NULL) {
                inputFile = open(command->command[i + 1], O_RDONLY);
                // If open fails, print warning and exit
                if(inputFile == -1) {
                    perror("failed to open input file");
                    return;
                    exit(EXIT_FAILURE);
                }
                // Actual redirection using our new file
                dup2(inputFile, STDIN_FILENO);
                close(inputFile);
                // Shift command arguments to remove '<' and input file name
                for(int j = i; command->command[j] != NULL; j++){
                    command->command[j] = command->command[j + 2];
                }
            // If input file name is null, print error
            } else{
                fprintf(stderr, "Syntax error: missing input file name after '<'\n");
                exit(EXIT_FAILURE);
            }
        }
    }
}

/* ----------------------------------------
    Function: outputRedirect
///////////////////////////////////////////
Desc: Handles output redirects so shell can
handle the special character '>'

Params: command: Command *, command to search
---------------------------------------- */
void outputRedirect(Command * command){
    int outputFile = -1;
    // Goes through all commands
    for (int i = 0; command->command[i] != NULL; i++) {
        // Looks for '>'
        if(strcmp(command->command[i], ">") == 0) {
            // If input file is not null
            if (command->command[i + 1] != NULL) {
                outputFile = open(command->command[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0666);
                // If open fails, print warning and exit
                if (outputFile == -1) {
                    perror("failed to open output file");
                    exit(EXIT_FAILURE);
                }
                // Actual redirection using our new file
                dup2(outputFile, STDOUT_FILENO);
                close(outputFile);
                // Shift command arguments to remove ">" and output file name
                for (int j = i; command->command[j] != NULL; j++) {
                    command->command[j] = command->command[j + 2];
                }
            // If input file name is null, print error
            } else {
                fprintf(stderr, "Syntax error: missing output file name after '>'\n");
                exit(EXIT_FAILURE);
            }
        }
    }
}

/* ----------------------------------------
    Function: execExit
===========================================
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
===========================================
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
===========================================
Desc: Prints out either the exit status or the 
terminating signal of the last foreground process 
ran by the shell. If ran before any foreground 
command is run, then it simply returns the exit 
status 0.

Params: 
command: Command *
---------------------------------------- */
void execStatus(Command * command){
    // Check last foreground process exit status
    if (WIFEXITED(lastForegroundStatus)) {
        // If exited normally, print exist status
        printf("Exit status: %d\n", WEXITSTATUS(lastForegroundStatus));
    // Check if terminated by signal
    } else if (WIFSIGNALED(lastForegroundStatus)) {
        // If so, print appropriate signal number
        printf("Terminating signal: %d\n", WTERMSIG(lastForegroundStatus));
    // If neither, then the process got terminated in some other way (?)
    } else {
        printf("Unknown termination status\n");
    }
    // Flush output
    fflush(stdout);
}


/* ----------------------------------------
    Function: execOther
===========================================
Desc: Uses the execvp function to execute
all commands that are not "cd", "exit", or
"status".

Params:
command: Command * , command to be executed
---------------------------------------- */
void execOther(Command * command){

    // Command is executed
    execvp(command->command[0], command->command);
    // If done correctly this will never be executed
    perror("execvp");
    exit(1);
}


/* ----------------------------------------
    Function: freeCommand
===========================================
Desc: Frees allocated memory for each command
given to the smallsh CLT.

Params: 
command: Command * , command to free
---------------------------------------- */
void freeCommand(Command * command){
    // Goes through all commands
    for(int i = 0; i < command->argCount ; i++){
        #ifdef DEBUG_MEM
            printf("Freeing command[%d]: %s\n", i, command->command[i]);
            fflush(stdout);
        #endif
        // Free individual command
        free(command->command[i]);
    }
    // Free command struct mem alloc
    free(command);
}

/* ----------------------------------------
    Function: replaceToken
===========================================
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


// Tester function - Simply for debugging
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