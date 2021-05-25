#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>


//Global state variable for FG/BG status 
int foregroundOnly = 0;


/* Background Process Struct
*   Creating a struct for the process ID's to be stored in a double-linked list. I decided to use this data structure 
*   because I wanted the ability to iterate through the list, pull completed processes out of the middle and not have 
*   to deal with a gap in the remaining process list like an array would have
*/
struct backgroundProcess
{
    pid_t processID;
    struct backgroundProcess* next;
    struct backgroundProcess* prev;
};


/* Handle_CTLZ - SIGTSTP  signal handler function
*   Inputs: signal number
*   Outputs: None
* 
*   Purpose: To re-define the behavior of this signal from the prior behavour of an interrupt that would end the program,
*   to a toggle for turning foreground only mode on and off.
* 
*   Procedure
*   This function will be called when the keyboard interrupt signal SIGTSTP (CTL-Z) is recieved by the shell the global. 
*   The function uses the global variable defined and initialized above to track the state of foreground only status and 
*   will restart the prior (interrupted) function upon completion. 
*/
void handle_CTLZ(int signo) {
    //If user input includes the ^Z, change the val of Foreground only mode
    switch (foregroundOnly) {
    case 0:
        foregroundOnly = 1;
        char* startFgOnly = "Entering foreground-only mode (& is now ignored)";
        write(1, startFgOnly, 49);
        fflush(stdout);
        break;
    default:
        foregroundOnly = 0;
        char* endFgOnly = "Exiting foreground-only mode";
        write(1, endFgOnly, 28);
        fflush(stdout);
        break;
    }
}

/* removeBackgroundProcess - Process list (structure) cleanup
*   Inputs: Target Process structure to remove from list
*   Outputs: None
* 
*   Purpose: To remove a process structuve recieved as an argument from the linked list of active process structures.
*
*   Procedure:
*   When called, the function will deploy pointers to the next process and the prior process in the list, remove the focus process's 
*   link by connecting the two pointer processes to one another, and then the struct for the focus process will be freed.
*/
void removeBackgroundProcess(struct backgroundProcess* focusProcess) {
    // Remove a completed or killed process from the list of running background processes
    struct backgroundProcess* restOfList = focusProcess->next;
    struct backgroundProcess* previousProcess = focusProcess->prev;
    if (restOfList == NULL) {
        previousProcess->next = NULL;
    }
    else {
        previousProcess->next = restOfList;
        restOfList->prev = previousProcess;
    }
    free(focusProcess);
}

/* CleanupBackgroundProcesses 
*   Inputs: Head Node of the background process linkedList
*   Outputs: None
* 
*   Purpose: To remove a given process from the linked list of active processes once it has completed running. 
* 
*   Procedure:
*   When called, the function will iterate through the linked list of processes, checking each for an exit condition with a non-blocking wait.
*   If the wait returns 0 (the process is stll running) the check will return 0 and that process will be left in the list. If it returns a non-zero status,
*   the function will print a text alert to the user stating the exit condition and value or signal recieved on exit, then call the removeBackgroundProcess
*   function to remove it from the linkedList before moving on
*/
void cleanupBackgroundProcesses(struct backgroundProcess* listHead) {  
    //Handles a once per loop cleanup of any finished backgorund processes before re-prompting for the next user input
    struct backgroundProcess* focusProcess = listHead->next;
    while (focusProcess != NULL) {
        int status;
        int waitVal = waitpid(focusProcess->processID, &status, WNOHANG);
        if (waitVal == 0) {
            focusProcess = focusProcess->next;
        }
        else {
            if (WIFEXITED(status)) {
                printf("\nbackground pid %d is done: exit value %d\n", focusProcess->processID, status);
                fflush(stdout);
                focusProcess = focusProcess->prev;
                removeBackgroundProcess(focusProcess->next);
                focusProcess = focusProcess->next;
            }else if (WIFSIGNALED(status)) {
                printf("\nbackground pid %d is done: terminated by signal %d\n", focusProcess->processID, status);
                fflush(stdout);
                focusProcess = focusProcess->prev;
                removeBackgroundProcess(focusProcess->next);
                focusProcess = focusProcess->next;
            }
        }
    }
}

/* killRunningProcesses 
*   Inputs: listHead  - Head Node of the background process linkedList
*   Outputs: None
* 
*   Purpose: On exit, terminate/clean up all child processes in the process structure
*   This function is called upon recieving the 'exit' command from the user, and will terminate all running child processes managed by the shell 
*   as well as clear the linked list process structure before returning. The function first calls the cleanupBackgroundProcesses command to remove 
*   and already completed processes, then will iterate throguh the process list issueing SIGTERM commands to each child process running in the background.
*   After reaching the end of the list, the function will return
*/
void killRunningProcesses(struct backgroundProcess* listHead) {
    // Take the process ID out of the background monitoring list, kill it and continue down the list of open processes until all are terminated 
    cleanupBackgroundProcesses(listHead);
    struct backgroundProcess* target = listHead->next;
    while (target != NULL) {
        kill(target->processID, SIGTERM);
        target = target->next;
    }
    return;
}

/* checkBackgroundCommand 
*   Inputs:     arguments - User input character array
*   Outputs:    Integer representing a boolean value for whether it is a background process, 1 for True (Backgroud process and 0 for false (Not Background process)
* 
*   Purpose: Take a provided the command line input, verify whether the & was recieved and return a  value to report results
* 
*   Procedure:
*   This function accepts the user input character array, creates a copy to work with and will use strtok_r to iterate through the space-delimited command argments. 
*   At each argument, the variable for 'backgroundCommand' is toggled on (1) and off (0) depending on whether the last token seen was a solitary & character. 
*   In order to return true (1), the final token will need to set the variable to 1 just before the last strtok_r call returns null and the loop is exited. After
*   the loop is finished, the copy array memory will be freed.
*/
int checkBackgroundCommand(char* arguments) {
    //Create copy of the string to iterate over
    char* argCopy = calloc(strlen(arguments) + 1, sizeof(char));
    strcpy(argCopy, arguments);

    // Iterate over the full array, flipping the backgroundCommand variable with each token. If the last one is an &, the variable will end as a 1. Otherwise, 
    // an & earlier will be reset to 0 by the next token
    char* bgPtr;
    char* token = strtok_r(argCopy, " ", &bgPtr);
    int backgroundCommand = 0;
    while (token != NULL) {
        if (strcmp(token, "&") == 0) {
            backgroundCommand = 1;
        }
        else {
            backgroundCommand = 0;
        }
        token = strtok_r(NULL, " ", &bgPtr);
    }
    free(argCopy);
    return backgroundCommand;
}

/* redirectIO
*   Inputs:     expandedArge - Character array of arguments, after $$ variable expansion
*               foregrond    - integer value for whether the process is foreground (1) or background (0)
*   Outputs:    Integer value for the success of the file redirection, with 0 for success and -1 for failure. 
*
*   Purpose: Redirect the Input and output streams when requested, and set background process I/O to dev/null if no redirect is specified
* 
*   Procedure:
*   This function accepts the arguments with the $$ variables expanded, will make a copy to perform work on and then proceed to iterate over the space delimited 
*   arguments with the strtok_r method, searching for a '<' or '>' token. When one is found, the program will take the provided path argument immediately following the '<' or '>' token,
*   and will redirect the corresponding Input or output stream to the path argument provided. 
*/
int redirectIO(char* expandedArgs, int foreground) {

    //Create base path for inputs in the cwd
    char inPath[250];
    getcwd(inPath, 249);
    strcat(inPath, "/");

    //Create base path for outputs in the cwd
    char outPath[250];
    getcwd(outPath, 249);
    strcat(outPath, "/");

    // Declare token and location pointers for iterating over the arguments
    char* pathToken;
    char* IOptr;

    // Create boolean flag variables for whether input and output were each redirected
    int inRedirected = 0;
    int outRedirected = 0;

    // Copy Arguments to tokenize and iterate over
    char* IOCopy = malloc((strlen(expandedArgs) + 1) * sizeof(char));
    strcpy(IOCopy, expandedArgs);

    // Iterate over the arguments, searching for the < or > operators
    char* token = strtok_r(IOCopy, " ", &IOptr);
    while (token != NULL) {
        // One of the two operators found, save the path argument immediately following the operator
        if (strcmp(token, "<") == 0 || strcmp(token, ">") == 0) {
            pathToken = strtok_r(NULL, " ", &IOptr);

        }
        // Redirect Input if < was found
        if (strcmp(token, "<") == 0) {
            inRedirected = 1;
            strcat(inPath, pathToken);
            int newInput = open(inPath, O_RDONLY, 0777);
            if (newInput == -1) {
                printf("Unable to open %s for Input\n:", pathToken);
                fflush(stdout);
                return -1;
            }
            else {
                dup2(newInput, 0);
            }
        }
        // Redirect Output if > was found
        else if (strcmp(token, ">") == 0) {
            outRedirected = 1;
            strcat(outPath, pathToken);
            int newOutput = open(outPath, O_WRONLY | O_CREAT | O_TRUNC, 0777);
            if (newOutput == -1) {
                printf("Unable to open %s for Output\n:", pathToken);
                fflush(stdout);
                return -1;
            }
            else {
                dup2(newOutput, 1);
            }
        }
        token = strtok_r(NULL, " ", &IOptr);
    }
    // Redirect input and output to dev/null if this is a background process and the stream in question wasn't already redirected
    int bgdefault = open("/dev/null", O_RDWR);
    if (inRedirected == 0 && foreground == 0) {
        dup2(bgdefault, 0);
    }
    if (outRedirected == 0 && foreground == 0) {
        dup2(bgdefault, 1);
    }
    return 0;
}

/* expandVariables - Find all $$ occurances and expand them to the PID of the shell process
*   Inputs: arguments - Character array pointer leading to the user input
*   Outputs: Character array pointer for an expanded array created in this function
*
*   Purpose: 
*   This funciton accepts the user input array pointer, and will iterate through the array character by character to expand the $$ variables.
*   
*   Procedure
*   To perform this expansion, the function will advance one letter at a time, seeing if the next two letters are both $'s. If this is the case,
*   the function counts this as one double variable and advances the position 2 places instead of 1 to avoid double counting part of the pair. 
*   After recording the number of doubles to expand, a character array is allocated based on the number of necessary caracters for the expanded string 
*   (length of the original input with extra space added for PID's longer than 2 characters). The function will then copy all characters into the new array,
*   replacing the $$ pairs with the PID characters for the shell process
*/
char* expandVariables(char* arguments) {
    // Allocate a char array for the PID's string representation, size picked arbitrarily to be certain the PID wouldn't cause overflow
    char* pidChars = calloc(25, sizeof(char));
    
    // Print the PID characters into the new char array, measure the length in characters and initialize counter and index variables 
    sprintf(pidChars, "%d", getpid());
    int pidLen = strlen(pidChars);
    int doubleVarCount = 0;
    int index = 1;

    // Loop through characters, counting the number of occurrances of a the '$$' pairs
    while (index < strlen(arguments)) {
        char* letter1 = calloc(2, sizeof(char));
        char* letter2 = calloc(2, sizeof(char));
        sprintf(letter1, "%c", arguments[index-1]);
        sprintf(letter2, "%c", arguments[index]);
        if (strncmp(letter2, "$",1) == 0 && strncmp(letter1, "$", 1) == 0) {
            doubleVarCount++;
            index++;
        }
        index++;
        free(letter1);
        free(letter2);
    }

    // Allocate an expanded char array memory block, using the full PID length * the number of occurances added to the original length. 
    // This is more than strictly necessary to provide overflow security
    char* argCopy = calloc(strlen(arguments) + ((pidLen)*doubleVarCount) + 1, sizeof(char));
    index = 0;
    int doublePrintedLast = 0;

    // Iterate through the original array again, printing characters. When the double is encountered, concatenate the PID characters and 
    // advance the index twice to skip the 2nd $ character
    while (index < strlen(arguments)-1) {
        char* letter1 = calloc(2, sizeof(char));
        char* letter2 = calloc(2, sizeof(char));
        sprintf(letter1, "%c", arguments[index]);
        sprintf(letter2, "%c", arguments[index+1]);
        if (strncmp(letter2, "$", 1) == 0 && strncmp(letter1, "$", 1) == 0) {
            strcat(argCopy, pidChars);
            index++;
            doublePrintedLast = 1;
        }
        else {
            strcat(argCopy, letter1);
            doublePrintedLast = 0;
        }
        index++;
        free(letter1);
        free(letter2);
    }

    // Since it's possible to miss the last letter is an expansion occurs towards the end, 
    // copy the last letter if this has occurred based on ending index variable
    if (strlen(arguments)-index==1) {
        char* lastLetter = calloc(2, sizeof(char));
        sprintf(lastLetter, "%c", arguments[index]);
        strcat(argCopy, lastLetter);
        free(lastLetter);
    }
    return argCopy;
}

/* parseArgs - Parse the arguments from the user input character array into an array of pointers to individual arguments
*   Inputs: exandedArgs - The input Arguments character array
*   Outputs: One array of Character array pointers, each pointing to an argument string
*
*   Purpose: The purpose of this function is to create an array of pointers to the various argmuents that can be accepted
*   by the execvp function call.
* 
*   Procedure:
*   To parse the arguments necessary for an execvp function call, the function first copies the expanded inputs array to perform work on,
*   then will iterate over the space delimited array of arguments using strtok_r. The loop will allocate memory for the initial command name 
*   and each valid argument and record them into character arrays pointed to by each index of the pointer array. Tokens containing redrect operators, thier filepaths 
*   or the background & operator are skipped. After completing all tokens, a null terminator will be added (if a & was the final argument, it will be overwritten as it 
*   is not used for the execvp function
*/
char** parseArgs(char* expandedArgs) {

    //Create a copy of the inputs array to tokenize
    char* argCopy = malloc((strlen(expandedArgs)+1)*sizeof(char));
    strcpy(argCopy, expandedArgs);

    // Create the structure by allocating memory for an array of pointers. 514 chosen based on the 512 max argument requirement, 
    // +1 for the name of the command at index 0 and +1 for the null terminated final index required by execvp 
    char* saveArgPtr;
    char* token;
    char** args;
    args = malloc((514) * sizeof(char*));
    int argIndex = 0;

    //Parse Arguments into an array of strings passable to an exec function
    token = strtok_r(argCopy, " ", &saveArgPtr);
    while (token != NULL) {
        if (strcmp(token, "<") == 0 || strcmp(token, ">") == 0) {
            token = strtok_r(NULL, " ", &saveArgPtr);
        }
        else if (strcmp(token, "&") != 0) {
            args[argIndex] = (char*)malloc((strlen(token) + 1)*sizeof(char));
            strcpy(args[argIndex], token);
            argIndex++;
        }
        token = strtok_r(NULL, " ", &saveArgPtr);
    }

    // Add null terminato to the end of the argument pointers 
    args[argIndex+1] = NULL;
    free(expandedArgs);
    return args;
}

/* createBackgroundProcess
*   Inputs: args - A null terminated array of character array pointers, with each index corresponding to an argument. used in execvp
*           expandedArgs - The single characterArray of the arguments, for use with the input/output redirection in the child process
*           listHead  - Head Node of the background process linkedList
*   Outputs: None
* 
*   Purpose: This function creates a background process and adds it to the background process linked list data structure
* 
*   Procedure:
*   This funciton will first create a new background process struct to store the process, and create a child process fork
*   to execute the program contained withing the args variable array. Based upon the pid check performed after the fork to 
*   differentiate between parent and child behavior, the function will finish one of two ways. The Child process will 
*   redirect the input and output as necessary, then attempt to run the new program. The parent will add the child process
*   to the linkedList struct and will continue back to the user prompt to await the next input
*/
void createBackgroundProcess(char** args, char* expandedArgs, struct backgroundProcess* listHead) {
    //create Process struct and insert it into the list
    struct backgroundProcess* newProcess = malloc(sizeof(struct backgroundProcess));
    int newChildPid = fork();
    int redirectSuccess = 0;
    int executionSuccess;
    switch (newChildPid) {
    case -1:
        perror("Error Creating Fork");
        exit(1);
        break;
    case 0:
        //Fork off  and run the new process for the child
        redirectSuccess = redirectIO(expandedArgs, 0);
        if (redirectSuccess == -1) {
            exit(1);
            break;
        }
        else {
            args = parseArgs(expandedArgs);
            executionSuccess = execvp(args[0], args);
            if (executionSuccess != 0) {
                printf("%s: No such file or directory", args[0]);
                fflush(stdout);
            }
            exit(executionSuccess);
            break;
        }
    default:
        //For the parent, add the child process to the list of background processes and return to the main prompt
        newProcess->processID = newChildPid;
        printf("\nbackground pid is %d\n", newProcess->processID);
        fflush(stdout);
        struct backgroundProcess* endOfList = listHead;
        while (endOfList->next != NULL) {
            endOfList = endOfList->next;
        }
        newProcess->next = NULL;
        newProcess->prev = endOfList;
        endOfList->next = newProcess;
    }
    return;
}

/* main 
*   Inputs: None
*   Outputs: None
*
*   Purpose: To facilitate the interactive shell program, handle built in commands and run foreground processes. 
*
*   Procedure: 
*   To begin the program, first this function will set up the environment that the interactive shell loop will operate in, by 
*   changing the signal interrupts for SIGINT and SIGTSTP, as well as declaring/initializing some variables used for state tracking 
*   (running, lastStatus). The Linked List structure's head will be created as well, initialized with a -1 PID as a placeholder. Then the
*   program name is printed and the function enters the while loop until the exit command is recieved. The prompt will accept user input,
*   and handle the action based upon the command recieved. For cd, the working directory will be changed based on the relative or absolute
*   filepath provided, or to the base HOME directory if no argument for path is supplied. for status, the exit status of the last foreground 
*   process is printed. For exit, all existing processes are killed and the program ends. For all other provided commands, child processes are 
*   forked depending on the provided commands and run in either the foreground or backgrond. Once per cycle, the background processes that have 
*   exited are cleaned up
*/

int main(){
    // Set up custom handling of interrupts, assistance for this came from the signal handling API examples
    //Initialize sigaction struct's
    struct sigaction SIGTSTP_action = { 0 }, ignore = { 0 }, SIGINT_original_action = { 0 };
    
    // Set up interrupt handling to enable/disable foreground mode
    SIGTSTP_action.sa_handler = handle_CTLZ;
    sigfillset(&SIGTSTP_action.sa_mask);
    SIGTSTP_action.sa_flags = SA_RESTART;
    sigaction(SIGTSTP, &SIGTSTP_action, NULL);

    // Set up the ignore action for CTL-C 
    ignore.sa_handler = SIG_IGN;
    sigaction(SIGINT, &ignore, &SIGINT_original_action);

    //Set up loop, initialize status variable and open working directory
    int running = 1;
    int lastStatus = 0;
    
    // Create background process data structure
    struct backgroundProcess* listHead = malloc(sizeof(struct backgroundProcess));
    listHead->next = NULL;
    listHead->prev = NULL;
    listHead->processID = -1;

    //Print Program title 
    printf("smallsh\n");
    fflush(stdout);
    char userInput[2050];

    // Begin shell loop
    while (running == 1) {
        printf(": ");
        fflush(stdout);
        fgets(userInput, 2049, stdin);
        fflush(stdout);

        //If the user input contained any characters, remove the newline from the input string and proceed, otherwise reprompt
        if (strlen(userInput) > 1) {
            userInput[strlen(userInput) - 1] = '\0';

            // Expand the $$ variable to the shell's PID
            char* expandedArgs = expandVariables(userInput);

            // Create copy of arguments for tokenizing
            char* argCopy = calloc(strlen(expandedArgs) + 1, sizeof(char));
            strcpy(argCopy, expandedArgs);
            char* saveptr;

            // Pull the first argument, if it's not a comment (first character #) or proceed into the if/else tree to check built in commands
            char* inputToken = strtok_r(argCopy, " ", &saveptr);
            if (strncmp(inputToken, "#", 1) != 0) {
                if (strcmp(inputToken, "exit") == 0) {
                    killRunningProcesses(listHead);
                    cleanupBackgroundProcesses(listHead);
                    break;
                }
                else if (strcmp(inputToken, "cd") == 0) {
                    char directory[250];
                    getcwd(directory, 249);

                    // Change directory to the directory specified in the HOME directory to the supplied argument for new path
                    char* inputArg = strtok_r(NULL, " ", &saveptr);
                    char* path = getenv("HOME");
                    if (inputArg == NULL) {
                        chdir(path);
                    }
                    else {
                        // Absolute paths, checking to see if the base directory is in the passed argument. If so, it's an absolute path
                        if (strncmp(path, inputArg, sizeof(path) - 1) == 0) {
                            int success = chdir(inputArg);
                        }
                        //Relative paths, no base directory means it's not absolute so the CWD needs to be appended before the provided argument
                        else {
                            char directory[250];
                            getcwd(directory, 249);
                            strncat(directory, "/", 1);
                            strncat(directory, inputArg, strlen(inputArg) + 1);
                            chdir(directory);
                        }

                    }
                }
                else if (strcmp(inputToken, "status") == 0) {
                    // Prints out either the exit status or the terminating signal. 
                    // If it's run before any foreground command, return 0 (Status doesn't include the 3 foreground commands).
                    if (WIFEXITED(lastStatus)) {
                        printf("exit value %d", WEXITSTATUS(lastStatus));
                        fflush(stdout);
                    }
                    else {
                        printf("terminated by signal %d", WTERMSIG(lastStatus));
                        fflush(stdout);
                    }
                }
                else {   //Handle Non-Built in Commands and Executables

                    int backgroundCommand = checkBackgroundCommand(expandedArgs);
                    char** args;
                    
                    // If foreground only mode hasn't been toggled
                    if (foregroundOnly == 0 && backgroundCommand) {
                        createBackgroundProcess(args, expandedArgs, listHead);
                    }
                    else {
                        int newChildPid = fork();
                        int childStatus;
                        int redirectSuccess;
                        int executionSuccess;
                        switch (newChildPid) {
                        case -1:
                            perror("Error Creating Fork");
                            exit(1);
                            break;
                        case 0:
                            //Fork off and run the new process for the child
                            redirectSuccess = redirectIO(expandedArgs, 1);
                            if (redirectSuccess == -1) {
                                exit(1);
                                break;
                            }
                            else {

                                //Re-set SIGINT behaviour to interrupt FG child
                                sigaction(SIGINT, &SIGINT_original_action, NULL);

                                //Execute Command
                                args = parseArgs(expandedArgs);
                                executionSuccess = execvp(args[0], args);
                                if (executionSuccess != 0) {
                                    printf("%s: No such file or directory", args[0]);
                                    fflush(stdout);
                                    exit(1);
                                }
                                else {
                                    exit(executionSuccess);
                                }

                                break;
                            }
                        default:
                            // For parent, wait for the child process to finish and store the exit status in the lastStatus variable
                            waitpid(newChildPid, &childStatus, 0);
                            lastStatus = childStatus;
                        }
                    }
                }
            }
            free(argCopy);
        }
        cleanupBackgroundProcesses(listHead);
    }
    return EXIT_SUCCESS;
}
