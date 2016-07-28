#define _GNU_SOURCE
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
#include "Admission.h"

#define MAXDATASIZE 100 // max number of bytes we can get at once

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) 
	{
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}


void send_to_server(char** info_list)
{
	int sockfd, numbytes;
	char buf[MAXDATASIZE];
	struct addrinfo hints, *servinfo, *p;
	int rv;
	char s[INET_ADDRSTRLEN];

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_INET;
	hints.ai_socktype = SOCK_STREAM;

	if ((rv = getaddrinfo("127.0.0.1", ADMISSION_PORT, &hints, &servinfo)) != 0) 
	{
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		exit(1);
	}
	// loop through all the results and connect to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) 
	{
		if((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
		{
			perror("client: socket");
			continue;
		}

		if(connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) 
		{
			close(sockfd);
			perror("client: connect");
			continue;
		}

		break;
	}


	if (p == NULL) 
	{
		fprintf(stderr, "client: failed to connect\n");
		exit(2);
	}

	struct sockaddr_in localAddress;
	socklen_t addressLength;
	addressLength = sizeof localAddress;
	getsockname(sockfd, (struct sockaddr*)&localAddress, &addressLength);
	printf("local address: %s\n", inet_ntoa( localAddress.sin_addr));
	printf("local port: %d\n", (int) ntohs(localAddress.sin_port));

	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof(s));
	printf("client: connecting to %s\n", s);
	freeaddrinfo(servinfo); // all done with this structure

	if ((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) == -1) {
		perror("recv");
		exit(1);
	}

	buf[numbytes] = '\0';
	printf("client: received '%s'\n",buf);
	close(sockfd);  

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
	int i;
	for(i = 0; i < DEPARTMENT_NUM; i++)
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

	for(i = 0; i < DEPARTMENT_NUM; i++)
	{
		wait(NULL);
	}
	printf("main process terminated \n");
}