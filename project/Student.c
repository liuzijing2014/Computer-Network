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

/* get sockaddr, IPv4 or IPv6. *from Beej's guide */
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) 
	{
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

/* Connection handler, which sends program information
 * to the server. This is responsible for phase 1 job. */
void send_to_server(char** info_list, char id, int num)
{
	// Connection Setup. *from Beej's guide
	int sockfd, numbytes, result;
	char buf[MAXDATASIZE];
	struct addrinfo hints, *servinfo, *p;
	int rv;
	char s[INET_ADDRSTRLEN];

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_INET;
	hints.ai_socktype = SOCK_STREAM;

	if ((rv = getaddrinfo(ADMISSION_HOSTNAME, ADMISSION_PORT, &hints, &servinfo)) != 0) 
	{
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		exit(1);
	}

	// loop through all the results and connect to the first we can. *from Beej's guide
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

	// Traslation of underlying department id to department name
	char depart_name;
	if(id == '0') depart_name = 'A';
	else if(id == '1') depart_name = 'B';
	else if(id == '2') depart_name = 'C';
	struct sockaddr_in localAddress;
	
	// Find port number and ip address of the client socket
	socklen_t addressLength;
	addressLength = sizeof localAddress;
	getsockname(sockfd, (struct sockaddr*)&localAddress, &addressLength);
	
	// Print out results
	//printf("Department%c has TCP port %d and IP address %s for Phase 1\n", depart_name, (int)ntohs(localAddress.sin_port), inet_ntoa( localAddress.sin_addr));
	//printf("Department%c is now connected to the admission office\n", depart_name);
	
	freeaddrinfo(servinfo);

	// Send the first msg which contains the student id
	if(send(sockfd, &id, 1, 0) == -1)
	{
		perror("send id");
		close(sockfd);
		return;
	}

	// Wait to recieve ack msg from the serve
	if((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) <= 0)
	{
		perror("id not ack");
		close(sockfd);
		return;
	}
	
	// Verify the ack msg
	buf[numbytes] = '\0';
	if(strcmp(buf, "ACK") != 0)
	{
	  printf("id ACK not right");
	  return;
	}

	// Send GPA
	if(send(sockfd, &info_list[0], strlen(info_list[0]), 0) == -1)
	{
		perror("send gpa");
		close(sockfd);
		return;
	}

	// Wait to recieve ack msg from the serve
	if((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) <= 0)
	{
		perror("gpa not ack");
		close(sockfd);
		return;
	}
	
	// Verify the ack msg
	buf[numbytes] = '\0';
	if(strcmp(buf, "ACK") != 0)
	{
	  printf("gpa ACK not right");
	  return;
	}

	// Send program interest
	char delim = '#';
	int index = 1;
	while((result = send(sockfd, info_list[index], strlen(info_list[index]), 0)) != -1)
	{
		//char *token = strtok(info_list[index++], &delim);
		//printf("Department%c has sent %s to the admission office\n", depart_name, token);

		// Wait for ack msg
		if((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) == -1)
		{
			perror("receive ACK");
			close(sockfd);
			return;
		}

		// Verify ack msg
		buf[numbytes] = '\0';
		if(strcmp(buf, "ACK") != 0)
		{
			perror("ACK not right");
			close(sockfd);
			return;
		}

		// If all program information has been sent, then send the end msg and close the socket
		if(index == num)
		{
			// Send end msg
			while(send(sockfd, "END", 3, 0) == -1)
			{
				perror("send END");
			}
			// Wait for ack msg
			if ((numbytes = recv(sockfd, buf, MAXDATASIZE - 1, 0)) == -1)
			{
				perror("receive end ACK");
				close(sockfd);
				return;
			}

			// Verify ack msg
			buf[numbytes] = '\0';
			if (strcmp(buf, "ACK") != 0)
			{
				perror("end ACK not right");
				close(sockfd);
				return;
			}
			//printf("Updating the admission office is done for Department%c\n", depart_name);
			break;
		}
	}
	close(sockfd);  
	//printf("End of Phase 1 for Department%c\n", depart_name);
}


/* Parse program inforamtion from an input filef or a given department. */
int read_studentinfo(const char *file_name, char** student_info)
{
	int counter = 0;						/* Entry counter for the array */
	char line[255];
	size_t len = 255;
	FILE *fp = fopen(file_name, "r");

	// Get GPA
	fgets(line, len, fp);
	strtok(line, "\n");
	strtok(line, ":");
	char* gpa = strtok(NULL, ":");
	int gpalength = strlen(gpa);
	student_info[counter] = malloc(gpalength+1);
	strcpy(student_info[counter], gpa);
	counter++;

	// Read in information about the program
	while(fgets(line, len, fp) != NULL)
	{
		strtok(line, "\n");
		strtok(line, ":");
		char* interest = strtok(NULL, ":");
		int length = strlen(interest);

		// Make a deep copy of the parsed char*
		student_info[counter] = malloc(length+1);
		strcpy(student_info[counter], interest);
		counter++;
	}
	fclose(fp);

	int h;
	for(h = 0; h < counter; h++)
	{
		printf("%s: %s\n", file_name, student_info[h]);
	}

	return counter;
}

/* Flow control function for a forked department process */
void start_student(int student_id, const char *file_name)
{
	char **student_info = malloc(sizeof(char*)*MAXSTUDENT_INFO);		/* Array of all program information. */
	int num = read_studentinfo(file_name, student_info);
	//send_to_server(student_info, (student_id + '1'), num);
	exit(0);
}

int main(void)
{
	pid_t student_list[STUDENT_NUM];					/* Array of child process pid. */
	
	/* All input files's name. */
	char *files[] = { "student1.txt","student2.txt","student3.txt","student4.txt","student5.txt" };
	
	// Start forking
	int i;
	for(i = 0; i < STUDENT_NUM; i++)
	{
		student_list[i] = fork();

		if(student_list[i] < 0)
		{
			printf("fork error\n");
			exit(-1);
		}

		if(student_list[i] != 0)
		{
			continue;
		}
		else
		{
			start_student(i, files[i]);
			break;
		}

	}

	// Wait to avoid zombie processes
	for(i = 0; i < STUDENT_NUM; i++)
	{
		wait(NULL);
	}
}
