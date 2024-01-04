#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>

char* getpwd() {
	int size = 100;
	char* wd = malloc(size);
	while (getcwd(wd, size) == NULL) {
		size = size * 2;
		wd = realloc(wd, size);
	}
	return wd;
}
void printErr() {
	char str[30] = "An error has occurred\n";
	write(STDERR_FILENO, str, strlen(str));
}

int readArgs(char* line, char*** args, int *numArgs, char* delim) {
	//printf("reading...\n");
	*numArgs = 0;
	char *l = strdup(line);
	if (l == NULL) return -1;
	char *token = strtok(l, delim);
	while (token != NULL) {
		(*numArgs)++;
		token = strtok(NULL, delim);
	}
	free(l);

	*args = malloc(sizeof(char**) * (*numArgs + 1));
	*numArgs = 0;
	token = strtok(line, delim);
	while (token != NULL) {
		char* tCopy = strdup(token);
		if (tCopy == NULL) return -1;
		(*args)[(*numArgs)++] = tCopy;
		token = strtok(NULL, delim);
	}
	(*args)[*numArgs] = NULL;
	return 0;
}

void printArgs (char** arglist, int numArgs) {
	for (int i = 0; i < numArgs; i++) {
		printf("%s ", arglist[i]);
	}
	printf(":%d\n", numArgs);
}

int check (char** arglist, int numArgs, char* target) {
	for (int i = 0; i < numArgs; i++) {
		if (strcmp(arglist[i], target) == 0) {
			return i;
		}
	}
	return 0;
}

int main (int argc, char *argv[]) {
	long unsigned arbSz = 1000;
	char* input = (char*)malloc(arbSz);
	int numArgs = 0;
	char** arglist;
	int numCmds = 0;
	char** cmdlist;
	int index = 0;

	int loop = 0;
	int redir = 0;
	
	while (1) {
		if (loop > 0) {
			loop--;
			//printf("%d -- %s -- %d\n", loop, arglist[0], numArgs);
		} else {
			if (numCmds == 0) {		
				printf("smash> ");
				fflush(stdout);
				input = realloc(input, arbSz);
				getline(&input, &arbSz, stdin);
				if (readArgs(input, &cmdlist, &numCmds, ";") != 0) {
					printErr();
					continue;
				}
			}
			if (index >= numCmds) {
				numCmds = 0;
				index = 0;
				continue;
			}
			if (readArgs(cmdlist[index++], &arglist, &numArgs, " \t\n") != 0) {
				printErr();
				continue;
			}
			redir = check(arglist, numArgs, ">");
			//printArgs(arglist, numArgs);
		}
		if (numArgs == 0) continue;
		
		char* cmd = arglist[0];
		if (strcmp(cmd, "exit") == 0) {
			if (numArgs != 1) {
				printErr();
				continue;
			}
			exit(0);
		} else if (strcmp(cmd, "cd") == 0) {
			if (numArgs != 2 || chdir(arglist[1]) != 0) {
				printErr();
				continue;
			}
		} else if (strcmp(cmd, "pwd") == 0) {
			if (numArgs != 1) {
				printErr();
				continue;
			}
			printf("%s\n", getpwd());
			fflush(stdout);
		} else if (strcmp(cmd, "loop") == 0) {
			if (numArgs < 2 || atoi(arglist[1]) < 0) {
				printErr();
				continue;
			}
			loop = atoi(arglist[1]);
			char** duplist = malloc(sizeof(char**) * (numArgs - 1));
			for (int i = 2; i < numArgs; i++) {
				duplist[i - 2] = arglist[i];
			}
			free(arglist);
			arglist = duplist;
			numArgs -= 2;
			arglist[numArgs] = NULL;
			if (redir) redir -= 2;
		} else {
			//printArgs(arglist, numArgs);
			int pid = fork();
			if (pid < 0) {
				printErr();
				exit(0);
				continue;
			} else if (pid == 0) {
				int p = check(arglist, numArgs, "|");
				int sin = dup(STDIN_FILENO);
				int std = dup(STDOUT_FILENO);
				int err = dup(STDERR_FILENO);
				while (p > 0) {
					if (redir && redir < p) {
						p = -1;
						continue;
					}
					int pip[2];
					if (pipe(pip) == -1) {
						p = -1;
						continue;
					}
					char** duplist = malloc(sizeof(char**) * (p));
					char** changelist = malloc(sizeof(char**) * (numArgs - p));
					for (int i = 0; i < p; i++) {
						duplist[i] = arglist[i];
					}
					duplist[p] = NULL;
					for (int i = p + 1; i < numArgs; i++) {
						changelist[i - p - 1] = arglist[i];
					}
					free(arglist);
					arglist = changelist;
					numArgs -= p + 1;
					if (redir) redir -= p + 1;
					arglist[numArgs] = NULL;

					int tpid = fork();
					if (tpid < 0) {
						printErr();
						exit(0);
						continue;
					} else if (tpid == 0) {
						
						close(pip[0]);
						close(STDOUT_FILENO);
						dup(pip[1]);
						
						fflush(stdout);
						if (execv(duplist[0], duplist) == -1) {
							dup2(std, STDOUT_FILENO);
							printErr();
							free(duplist);
							exit(0);
							continue;
						}
						free(duplist);
					} else {
						waitpid(tpid, NULL, 0);
						
						close(pip[1]);
						close(STDIN_FILENO);
						dup(pip[0]);
						
					}
					p = check(arglist, numArgs, "|");
				}
				dup2(std, STDOUT_FILENO);
				if (p == -1) {
					printErr();
					dup2(sin, STDIN_FILENO);
					exit(0);
					continue;
				}

				//redir = check(arglist, numArgs, ">");
				if (redir) {
					//printf("%s %d\n", arglist[redir], redir);
					close(STDOUT_FILENO);
					close(STDERR_FILENO);
					char* file = arglist[redir + 1];
					if (numArgs - redir != 2 || open(file, O_CREAT|O_WRONLY|O_TRUNC, S_IRWXU) == -1 
							|| open(file, O_CREAT|O_WRONLY|O_TRUNC, S_IRWXU) == -1) {
						dup2(std, STDOUT_FILENO);
						//printf("nargs - redir = %d\n", numArgs - redir);
						dup2(sin, STDIN_FILENO);
						printErr();
						dup2(err, STDERR_FILENO);
						exit(0);
						continue;
					}
					char** duplist = malloc(sizeof(char**) * (numArgs - 1));
					for (int i = 0; i < redir; i++) {
						duplist[i] = arglist[i];
					}
					free(arglist);
					arglist = duplist;
					numArgs -= 2;
					arglist[numArgs] = NULL;
				}

				fflush(stdout);
				if (execv(arglist[0], arglist) == -1) {
					dup2(err, STDERR_FILENO);
					dup2(std, STDOUT_FILENO);
					dup2(sin, STDIN_FILENO);
					printErr();
					exit(0);
					continue;
				}
				if (redir) {
					dup2(std, STDOUT_FILENO);
					dup2(err, STDERR_FILENO);
				}
				dup2(sin, STDIN_FILENO);
			} else {
				waitpid(pid, NULL, 0);
			}
		}	

	}
	return 0;
}


