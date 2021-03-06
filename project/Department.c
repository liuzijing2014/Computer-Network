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
void send_to_server(char** info_list, char id)
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
	printf("Department%c has TCP port %d and IP address %s for Phase 1\n", depart_name, (int)ntohs(localAddress.sin_port), inet_ntoa( localAddress.sin_addr));
	printf("Department%c is now connected to the admission office\n", depart_name);
	
	freeaddrinfo(servinfo);

	// Send the first msg which contains the department id
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

	// Send program information
	char delim = '#';
	int index = 0;
	while((result = send(sockfd, info_list[index], strlen(info_list[index]), 0)) != -1)
	{
		char *token = strtok(info_list[index++], &delim);
		printf("Department%c has sent %s to the admission office\n", depart_name, token);

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
		if(index == PROGRAM_NUM)
		{
			// Send end msg
			while(send(sockfd, "END", 3, 0) == -1)
			{
				perror("send END");
			}
			// Wait for ack msg
			if ((numbytes = recv(sockfd, buf, MAXDATASIZE - 1, 0)) == -1)
			{
				perror("receive ACK");
				close(sockfd);
				return;
			}

			// Verify ack msg
			buf[numbytes] = '\0';
			if (strcmp(buf, "ACK") != 0)
			{
				perror("ACK not right");
				close(sockfd);
				return;
			}
			printf("Updating the admission office is done for Department%c\n", depart_name);
			break;
		}
	}
	close(sockfd);  
	printf("End of Phase 1 for Department%c\n", depart_name);
}

/* Connection handler for phase 2 UDP connection.
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
	char depart_name = department_names[id];

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE;

	if ((rv = getaddrinfo(ADMISSION_HOSTNAME, department_ports[id], &hints, &servinfo)) != 0) {
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

	struct sockaddr_in *localAddress = (struct sockaddr_in*)p->ai_addr;
	printf("Department%c has UDP port %d and IP address %s for Phase 2\n", depart_name, (int)ntohs(localAddress->sin_port), inet_ntoa(localAddress->sin_addr));

	freeaddrinfo(servinfo);

	// Receive results
	addr_len = sizeof their_addr;
	while ((numbytes = recvfrom(sockfd, buf, MAXDATASIZE - 1, 0, (struct sockaddr *)&their_addr, &addr_len)) != -1)
	{
		buf[numbytes] = '\0';
		if (strcmp(buf, "END") == 0) break;
		char* student_id = strtok(buf, "#");
		char sid = student_id[7];
		printf("Student%c has been admitted to Department%c\n", sid, depart_name);
	}
		
	if(numbytes == -1)
	{
		perror("recvfrom");
		exit(1);
	}

	close(sockfd);
	printf("End of Phase 2 for Department%c\n", depart_name);
}

/* Parse program inforamtion from an input filef or a given department. */
void read_proginfo(const char *file_name, char** program_list)
{
	int counter = 0;						/* Entry counter for the array */
	char line[255];
	size_t len = 255;
	FILE *fp = fopen(file_name, "r");

	// Read in information about the program
	while(fgets(line, len, fp) != NULL)
	{
		strtok(line, "\r");
		int length = strlen(line);

		// Make a deep copy of the parsed char*
		program_list[counter] = malloc(length+1);
		strcpy(program_list[counter], line);
		counter++;
	}
	fclose(fp);
}

/* Flow control function for a forked department process */
void start_department(int depart_id, const char *file_name)
{
	char **program_list = malloc(sizeof(char*)*PROGRAM_NUM);			/* Array of all program information. */
	read_proginfo(file_name, program_list);
	send_to_server(program_list, (depart_id + '0'));					/* Phase 1 handler. */
	udp_handler(depart_id);												/* Phase 2 handler. */
	exit(0);
}

int main(void)
{
	pid_t department_list[DEPARTMENT_NUM];								/* Array of child process pid. */
	
	/* All input files's name. */
	char *files[] = { "departmentA.txt","departmentB.txt","departmentC.txt" };
	
	// Start forking
	int i;
	for(i = 0; i < DEPARTMENT_NUM; i++)
	{
		department_list[i] = fork();

		if(department_list[i] < 0)
		{
			printf("fork error\n");
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

	// Wait to avoid zombie processes
	for(i = 0; i < DEPARTMENT_NUM; i++)
	{
		wait(NULL);
	}
}
