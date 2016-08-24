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
 * to the server. This is responsible for phase 2 first 
 * half job. */
int send_to_server(char** info_list, char id, int num)
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

	// Find port number and ip address of the client socket
	struct sockaddr_in localAddress;
	socklen_t addressLength;
	addressLength = sizeof localAddress;
	getsockname(sockfd, (struct sockaddr*)&localAddress, &addressLength);
	
	// Print out results
	printf("Student%c has TCP port %d and IP address %s\n", id, (int)ntohs(localAddress.sin_port), inet_ntoa( localAddress.sin_addr));
	
	freeaddrinfo(servinfo);

	// Send the first msg which contains the student id
	if(send(sockfd, &id, 1, 0) == -1)
	{
		perror("send id");
		close(sockfd);
		return 0;
	}

	// Wait to recieve ack msg from the serve
	if((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) <= 0)
	{
		perror("num not ack");
		close(sockfd);
		return 0;
	}
	
	// Verify the ack msg
	buf[numbytes] = '\0';
	if(strcmp(buf, "ACK") != 0)
	{
	  printf("num ACK not right");
	  return 0;
	}

	// Send the interested program number then
	char i_num = (num - 1) + '0';
	if (send(sockfd, &i_num, 1, 0) == -1)
	{
		perror("send id");
		close(sockfd);
		return 0;
	}

	// Wait to recieve ack msg from the serve
	if ((numbytes = recv(sockfd, buf, MAXDATASIZE - 1, 0)) <= 0)
	{
		perror("id not ack");
		close(sockfd);
		return 0;
	}

	// Verify the ack msg
	buf[numbytes] = '\0';
	if (strcmp(buf, "ACK") != 0)
	{
		printf("id ACK not right");
		return 0;
	}

	// Send GPA
	if(send(sockfd, info_list[0], strlen(info_list[0]), 0) == -1)
	{
		perror("send gpa");
		close(sockfd);
		return 0;
	}

	// Wait to recieve ack msg from the serve
	if((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) <= 0)
	{
		perror("gpa not ack");
		close(sockfd);
		return 0;
	}
	
	// Verify the ack msg
	buf[numbytes] = '\0';
	if(strcmp(buf, "ACK") != 0)
	{
	  printf("gpa ACK not right");
	  return 0;
	}

	//Send program interest
	int match;
	int index = 1;
	while((result = send(sockfd, info_list[index], strlen(info_list[index]), 0)) != -1)
	{

		//printf("student %c send %s to server\n", id, info_list[index]);

		// Wait for ack msg
		if((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) == -1)
		{
			perror("receive ACK");
			close(sockfd);
			return 0;
		}

		// Verify ack msg
		buf[numbytes] = '\0';
		if(strcmp(buf, "ACK") != 0)
		{
			printf("ACK not right");
			close(sockfd);
			return 0;
		}

		index++;
		// If all program information has been sent, then send the end msg and close the socket
		if(index == num)
		{
			// Send end msg
			while(send(sockfd, "END", 3, 0) == -1)
			{
				perror("send END");
			}

			printf("Completed sending application for Student%c\n", id);

			// Wait for match msg
			if ((numbytes = recv(sockfd, buf, MAXDATASIZE - 1, 0)) == -1)
			{
				perror("receive end ACK");
				close(sockfd);
				return 0;
			}

			// Verify match msg
			printf("Student%c has received the reply from the admission office\n", id);
			buf[numbytes] = '\0';
			match = atoi(&buf[0]);
			break;
		}
	}
	close(sockfd);  
	return match;
}

/* UDP handler for phase 2 second half job.
 * from Beej's guide. */
void udp_handler(int id)
{
	int sockfd;
	struct addrinfo hints, *servinfo, *p;
	int rv;
	int numbytes;
	struct sockaddr_storage their_addr;
	char buf[MAXDATASIZE];
	socklen_t addr_len;
	char s[INET6_ADDRSTRLEN];

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE;

	if ((rv = getaddrinfo(ADMISSION_HOSTNAME, student_ports[id], &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return;
	}
	// loop through all the results and bind to the first we can
	for (p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
			p->ai_protocol)) == -1) {
			perror("listener: socket");
			continue;
		}
		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("listener: bind");
			continue;
		}
		break;
	}

	if (p == NULL) {
		fprintf(stderr, "listener: failed to bind socket\n");
		return;
	}

	char cid = id + '1';
	struct sockaddr_in *localAddress = (struct sockaddr_in*)p->ai_addr;
	printf("Student%c has UDP port %d and IP address %s for Phase 2\n", cid, (int)ntohs(localAddress->sin_port), inet_ntoa(localAddress->sin_addr));
	freeaddrinfo(servinfo);

	// Receive results
	addr_len = sizeof their_addr;
	if ((numbytes = recvfrom(sockfd, buf, MAXDATASIZE - 1, 0, (struct sockaddr *)&their_addr, &addr_len)) == -1)
	{
		perror("recvfrom");
		exit(1);
	}

	buf[numbytes] = '\0';
	printf("Student%c has received the application result\n", cid);
	close(sockfd);
	printf("End of phase 2 for Student%c\n", cid);
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
	strtok(line, "\r");
	strtok(line, ":");
	char* gpa = strtok(NULL, ":");
	int gpalength = strlen(gpa);
	student_info[counter] = malloc(gpalength+1);
	strcpy(student_info[counter], gpa);
	counter++;

	// Read in information about the program
	while(fgets(line, len, fp) != NULL)
	{
		strtok(line, "\r");
		strtok(line, ":");
		char* interest = strtok(NULL, ":");
		int length = strlen(interest);

		// Make a deep copy of the parsed char*
		student_info[counter] = malloc(length+1);
		strcpy(student_info[counter], interest);
		counter++;
	}
	fclose(fp);
	return counter;
}

/* Flow control function for a forked department process */
void start_student(int student_id, const char *file_name)
{
	char **student_info = malloc(sizeof(char*)*MAXSTUDENT_INFO);		/* Array of all program information. */
	int num = read_studentinfo(file_name, student_info);
	if (send_to_server(student_info, (student_id + '1'), num) != 0)		/* Phase 2 first half handler. */
	{
		udp_handler(student_id);										/*Phase 2 second half handler if necessary. */
	}
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
