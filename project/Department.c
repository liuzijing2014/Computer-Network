#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include "Department.h"



void send_to_server(char** info_list)
{
	for(int j = 0; j < PROGRAM_NUM; j++)
	{
		printf("%s\n", info_list[j]);
	}
}


/* Parse program inforamtion from an input filef or a given department. */
void read_proginfo(const char *file_name, char** program_list)
{
	int counter = 0;																/* Entry counter for the array */
	char *line = NULL;
	size_t len = 0;
	FILE *fp = fopen(file_name, "r");

	// Read in information about the program
	while(getline(&line, &len, fp) != -1)
	{
		int length = strlen(line);

		// Replace new line character with the terminating charcter
		if(line[length-1] == '\n') 
		{
			line[length-1] = '\0';
		}

		// Make a deep copy of the parsed char*
		program_list[counter] = malloc(length+1);
		strcpy(program_list[counter], line);

		counter++;
	}
}

/* Flow control function for a forked department process */
void start_department(int depart_id, const char *file_name)
{
	char *program_list[PROGRAM_NUM];												/* Array of all program information. */
	read_proginfo(file_name, program_list);
	send_to_server(program_list);
	printf("child process %d terminated \n", getpid());
	exit(0);
}

int main(void)
{
	pid_t department_list[DEPARTMENT_NUM];											/* Array of child process pid. */
	char *files[] = {"departmentA.txt","departmentB.txt","departmentC.txt" };		/* All input files's name. */
	
	// Start forking
	for(int i = 0; i < DEPARTMENT_NUM; i++)
	{
		department_list[i] = fork();

		if(department_list[i] < 0)
		{
			exit(-1);
		}

		if(department_list[i] != 0)
		{
			continue;
		}
		else
		{
			start_department(i, files[i]);
			break;
		}

	}

	for(int i = 0; i < DEPARTMENT_NUM; i++)
	{
		wait(NULL);
	}
	printf("main process terminated \n");
}