#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

#define MAX_LEN 2048
#define MAX_ARGS 512


// global variables: 

// status for current process
int currStatus;
// main loop flag
bool exitMainLoop = true; 
// flag to see if background is disabled
bool fgOnly = false;
// setting array of pids for easy manipulation (removal, adding)
int pidArray[100];
int pidPtr=0;


// structs for sigint and sigtstp
struct sigaction SIGINT_action = { 0 };
struct sigaction SIGTSTP_action = { 0 }; 


/*
**********************************************
		REDIRECTION
**********************************************
*/

/* gets lists of commands, count of commands, and a boolean to indicate whether we are in background mode or not
* checks if there is < or > with a specific redirection file, < or > with no specified redirection file (background mode only)
* if there is a redirection file, it creates a new file in the same directory else if in background mode, it uses /dev/null
* 
* returns: boolean value of whether command list has redirection. 
* 
*/


bool redirection(char** commandList, int numCommands, bool backgroundFlag) { 
	int fd;
	char* fileName;
	bool redirected = false;
	//foreground process
	for (int i = 0; i < numCommands; i++) {
		// input redirection
		if (strcmp(commandList[i], "<") == 0) { // file name not shown for wc when using < 
			redirected = true;
			// if background process and there is no redirection location specified we use pathname of dev/null
			if (!strcmp(commandList[i + 1], "&") && backgroundFlag) {
				fd = open("dev/null", O_RDONLY);
				if (fd == -1) {
					perror("error");
					fflush(stdout);
					exit(1);
				}
				if (dup2(fd, STDIN_FILENO) == -1) {
					perror("dup2");
				}
				close(fd);
			}
			// we get the filename and add the current directory pathway with ./
			else {
				fileName = calloc(strlen(commandList[i + 1]) + 4, sizeof(char));
				strcpy(fileName, "./");
				strcat(fileName, commandList[i + 1]);
				//printf("filename: %s\n", fileName);
				strcpy(fileName, commandList[i + 1]);
				fd = open(fileName, O_RDONLY);
				if (fd == -1) {
					printf("cannot open %s for input. \n", commandList[i + 1]);
					fflush(stdout);
					exit(1);
				}
				if (dup2(fd, STDIN_FILENO) == -1) {
					perror("dup2");
					}
				}
				close(fd);
			}
		// output redirection
		else if (strcmp(commandList[i], ">") == 0) {
			redirected = true;
			// if background process and there is no redirection location specified we use dev/null 
			if (!strcmp(commandList[i + 1], "&") && backgroundFlag) {
				fd = open("dev/null", O_RDWR | O_CREAT | O_TRUNC, 0766);
				if (fd == -1) {
					perror("error");
					fflush(stdout);
					exit(1);
				}
				if (dup2(fd, STDOUT_FILENO) == -1) {
					perror("dup2");
				}
				close(fd);
			}
			else {
				// last arg is the file name
				fileName = calloc(strlen(commandList[i + 1]) + 4, sizeof(char));
				strcpy(fileName, "./");
				strcat(fileName, commandList[i + 1]);
				//printf("filename: %s\n", fileName);
				strcpy(fileName, commandList[i + 1]);
				fd = open(fileName, O_RDWR | O_CREAT | O_TRUNC, 0766);
				if (fd == -1) {
					printf("cannot open %s for input. \n", commandList[i+1]);
					fflush(stdout);
					exit(1);
				}
				if (dup2(fd, STDOUT_FILENO) == -1) {
					perror("dup2");
				}
				close(fd);
			}
		}
	}
	if (redirected) {
		return true;
	}return false;
}

/*
**********************************************
		GET NUMBER OF COMMANDS
**********************************************
*/

/* gets string list of commands and iterates a pointer through the list until it reaches null value
*
* returns: count of how many commands exist in list
*/

int getNumCommands(char** commandList) {
	int num = 0;
	char* ptr = commandList[num];
	while (ptr != NULL) {
		num++;
		ptr = commandList[num];
	}
	return num;
}

/*
**********************************************
		REMOVE PID FROM PID ARRAY
**********************************************
*/

/* gets integer process id and iterates through array of process ids to remove it from the array. 
* does this by finding number in array then copying the next number into the current number position. 
* decrements process id pointer for array to indicate continuation point for array. 
*
* returns: n/a. 
*/
void removePidFromArray(int pid) {
	for (int i = 0; i < pidPtr; i++) {
		if (pidArray[i] == pid) {
			for (int j = i; j < pidPtr-1; j++) {
				pidArray[i] = pidArray[i + 1];
			}
			pidPtr--;
		}
	}
}

/*
**********************************************
		CHECKING ON BACKGROUND PROCESSES
**********************************************
*/

/* void function that retrieves a background process pid and checks if it has terminated
* removes pid if it has terminated and gets next pid
*
* returns: n/a
*
*/
void checkOnBgProcesses() {
	pid_t currPid;
	// use -1 for any pid
	// first occurrence
	currPid = waitpid(-1, &currStatus, WNOHANG);
	// using waitpid return value because its an easy way to loop through processes
	while (currPid > 0) {
		if (WIFSIGNALED(currStatus)) {
			printf("background pid %d is done: terminated by signal %d\n", currPid, WTERMSIG(currStatus));
			fflush(stdout);
		}
		else if (WIFEXITED(currStatus)) {
			printf("background pid %d is done: exit value is %d\n", currPid, WEXITSTATUS(currStatus));
			fflush(stdout);
		}
		// if pid is done then remove from non complete pid array
		removePidFromArray(currPid);
		// get next
		currPid = waitpid(-1, &currStatus, WNOHANG);
	}
}


/*
**********************************************
		ANY NON BUILT IN FUNCTION 
**********************************************
*/

/* void function that forks a child to execute a process that is not one of the built in functions
* by checking if we are in background mode, if we have redirection, and if we have any pending background processes to terminate
*
* returns: n/a
*
*/
void nonBuiltIns(char** commandList) {
	bool backgroundFlag = false;
	pid_t spawnPid = fork();
	int numCommands = getNumCommands(commandList);
	if (!strcmp(commandList[numCommands - 1], "&")) {
		// checks if we are in foreground mode only 
		if (fgOnly) {
			// gets rid of & so that we can just process it as a fg command
			commandList[numCommands - 1] = NULL;
			// decrements so that redirection function does not include & in its comparisons
			numCommands -= 1;
		}
		if (!fgOnly) {
			backgroundFlag = true;
		}
	}
	if (spawnPid == -1) {
		printf("Error creating child process\n");
		fflush(stdout);
		exit(1);
	}
	// child process
	else if (spawnPid == 0) {
		// check for redirection
		bool redirectionFlag = redirection(commandList, numCommands, backgroundFlag);
		if (backgroundFlag) {
			commandList[numCommands - 1] = NULL;
		}
		// if foreground process then change SIGINT_action back to default action
		if (!backgroundFlag) {
			SIGINT_action.sa_handler = SIG_DFL;
			// no flags set
			SIGINT_action.sa_flags = 0;
			sigaction(SIGINT, &SIGINT_action, NULL);
		}
		
		if (redirectionFlag) {
			// set redirection commands to null so that execvp can process (this works for multiple < > or ><) 
			for (int i = 0; i < numCommands; i++) {
				if (!strcmp(commandList[i], ">") || !strcmp(commandList[i], "<")) {
					commandList[i] = NULL;
				}
			}
			execvp(commandList[0], commandList);
			// unsuccessful execution -> printing our error
			perror("error ");
			fflush(stdout);
		}
		else if (!redirectionFlag) {
			execvp(commandList[0], commandList);
			// unsuccessful execution -> printing our error
			perror("error ");
			fflush(stdout);
		}
		exit(1);
	}
	// parent process
	else {
		// background 
		if (backgroundFlag) {
			printf("background pid is %d\n", spawnPid);
			fflush(stdout);
			// add pid to pidArray
			pidArray[pidPtr] = spawnPid;
			pidPtr++;
		}
		// foreground
		else {
			waitpid(spawnPid, &currStatus, 0);
			// if terminated abnormally
			if (WIFSIGNALED(currStatus)) {
				printf("child process terminated by signal %d\n", WTERMSIG(currStatus));
				fflush(stdout);
			}
			/*
			* allows for exit value to be shown after every command but not consistent with sample output
			* 			else if (WIFEXITED(currStatus)) {
				printf("The exit value is %d\n", WEXITSTATUS(currStatus));
				fflush(stdout);
			}
			*/
			// check background processes - periodic because only checks when non background process starts
			checkOnBgProcesses();
		}

	}
}

/*
**********************************************
		RUN COMMANDS
**********************************************
*/

/* void function that checks for the built in functions that our program is required: exit, cd, status. 
* Sends any non built in function to nonBuiltIns function. 
* Handles null or comment lines
*
* returns: n/a
*
*/

void runCommands(char** commands) {
	bool absPath = false;
	bool comment = false;
	bool firstWordNull = false;
	if (commands[0] == NULL) {
		firstWordNull = true;
	}
	if (!firstWordNull) {
		char* first = calloc(strlen(commands[0]) + 1, sizeof(char));
		strcpy(first, commands[0]);
		int i = 0;
		// checks for comment 
		while (first[i] != '\0') {
			if (first[i] == '#') {
				comment = true;
			}
			i++;
		}
		free(first);
	}
	// if command is blank or a comment
	if (firstWordNull || comment) {
		// do nothing
	}
	// if command is exit
	else if (strcmp(commands[0], "exit") == 0) {
		// send sigterm to background process
		exitMainLoop = false;
	}
	// if command is cd
	else if (strcmp(commands[0], "cd") == 0) { // this works
		// we change directory
		if (commands[1] != NULL) {
			char absDirPath[256];
			// if commands[1] not null then we know there is a rel or abs path
			// get current dir and then append commands[1]
			if (strstr(commands[1], "nfs")) {
				absPath = true;
				strcpy(absDirPath, commands[1]);
			}
			if (!absPath) {
				getcwd(absDirPath, sizeof(absDirPath));
				strcat(absDirPath, "/");
				strcat(absDirPath, commands[1]);
			}
			chdir(absDirPath);
		}
		else {
			// or we go to home directory
			chdir(getenv("HOME")); // might have to do an error statement in case chdir(getenv(string)) != 0
		}
	}
	// if command is status
	else if (strcmp(commands[0], "status") == 0) { 
		if (WIFSIGNALED(currStatus)) {
			printf("terminated by signal %d\n", WTERMSIG(currStatus));
			fflush(stdout);
		}
		else if (WIFEXITED(currStatus)) {
			printf("The exit value is %d\n", WEXITSTATUS(currStatus));
			fflush(stdout);
		}
	}
	else {
		nonBuiltIns(commands);
	}
}

/*
*********************************************
			PARSE INPUT
**********************************************
* /

/* gets the user input string and tokenizes it to create individual string arguments and puts them in a string array. 
*
* returns: n/a
*
*/


char** parseInput(char* line) {
	// tokenize 
	char** tokenArray = malloc(MAX_ARGS * sizeof(char*));
	char* token;
	int pos = 0;
	// 1st token
	token = strtok(line, " \a\r\n");
	while (token != NULL) {
		// add token to tokenArray (args)
		tokenArray[pos] = token;
		pos++;
		token = strtok(NULL, " \a\r\n");
	}
	tokenArray[pos] = NULL;
	return tokenArray;
}

/*
*********************************************
			EXPAND PROCESS PID
**********************************************
* /

/* gets the user input string and character process id to replace any instance of $$ with pid
*
* returns: the string with the pid replacing $$
*/

char* expand_pid(char* string, char* pid) {
	// there could be multiple instances of $$ so updated single case to any instance below
	char* newString = calloc(MAX_LEN, sizeof(char));
	strcpy(newString, string);
	char* copy = calloc(MAX_LEN, sizeof(char));
	int i = 0;
	// while loop through new string
	while (strstr(newString, "$$") != NULL) {
		if (i == 0) {
			strncpy(copy, newString, strstr(newString, "$$") - newString);
			i = 1;
		}
		else if (i == 1) {
			strncat(copy, newString, strstr(newString, "$$") - newString);
		}
		strcat(copy, pid);
		newString = strstr(newString, "$$");
		newString += 2;
	}


	return copy;
}

/*
*********************************************
			GET INPUT
**********************************************
* /

/* gives user prompt and then gets user input to process
*
* returns: user input string
*/



char* getInput() {

	// give user the starting sign
	printf(": ");
	fflush(stdout);

	// get the user input line here
	char input[MAX_LEN]; // input array character len
	fgets(input, sizeof(input), stdin);
	// might have to remove end newline char 
	strcspn(input, "/n"); 

	char* pidPointer = strstr(input, "$$");

	if (pidPointer) {
		char p[MAX_LEN];
		char* resultInput;
		sprintf(p, "%d", getpid());
		resultInput = expand_pid(input, p); // helper function to create
		char* res = calloc(strlen(resultInput) + 1, sizeof(char));
		strcpy(res, resultInput);
		free(resultInput);
		return res;
	}
	char* res = calloc(strlen(input) + 1, sizeof(char));
	strcpy(res, input);
	return res;
}

/*
**********************************************
		FOREGROUND HANDLER
**********************************************
*/

/* void function that writes to stdout regarding whether or not user is in foreground only mode or not.
*
* returns: n/a
*
*/
void fgHandler() {
	size_t count;
	if (fgOnly) {
		char* msg = "Exiting foreground-only mode\n";
		count = 30;
		write(STDOUT_FILENO, msg, count);
		fgOnly = false;
		//char* commandLine = ": ";
		//write(STDOUT_FILENO, commandLine, 2);

	}
	else {
		char* msg = "Entering foreground-only mode (& is now ignored)\n";
		count = 50;
		write(STDOUT_FILENO, msg, count);
		fgOnly = true;
		//char* commandLine = ": ";
		//write(STDOUT_FILENO, commandLine, 2);
	}

}

/*
*********************************************
			START LOOP
**********************************************
* /

/* void function that sets the SIGINT AND SIGTSTP handlers and gets user input to parse through. 
* After parsing, it executes the commands. Frees any allocated memory.
*
* returns: n/a.
* 
*/
void startLoop() {
	char* userInput;
	char** userArgs;
	// SIGINT handler: Register SIG_IGN as the signal handler and ignore default for now
	SIGINT_action.sa_handler = SIG_IGN;
	sigaction(SIGINT, &SIGINT_action, NULL );

	// SIGTSTP handler
	SIGTSTP_action.sa_handler = fgHandler; // foreground only mode
	SIGTSTP_action.sa_flags = SA_RESTART;
	sigfillset(&SIGTSTP_action.sa_mask);
	sigaction(SIGTSTP, &SIGTSTP_action, NULL);

	while (exitMainLoop) {

		// get user input here
		userInput = getInput();
		userArgs = parseInput(userInput);

		// run the commands entered here
		runCommands(userArgs);

		// free input line and args here
		free(userInput);
		free(userArgs);
	}
}


int main(int argc, char* argv[]) {
	// start the main loop here
	startLoop();

}