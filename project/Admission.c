#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <pthread.h>
#include "Admission.h"

#define BACKLOG 10

struct program
{
	int department_id;
	char* program_name;
	float required_gpa;
};

static struct program *prog_list;				/* Store all recieved program information */
static pthread_mutex_t *lock;					/* Lock for avoid race condition */
static pthread_mutexattr_t attrmutex;
static int *counter;
const static char delim = '#';
const static char new_line = '\n';
static int *phase1;								/* Flag for phase 1. */
static int depart_sockets[DEPARTMENT_NUM];		/* Store all UDP sockets for departments. */
static int student_sockets[STUDENT_NUM];		/* Store all UDP sockets for students. */


/* Fill in prog_list from the local file. */
void fill_prog_list(void)
{
	if (prog_list != NULL) free(prog_list);
	prog_list = malloc(sizeof(struct program) * DEPARTMENT_NUM * PROGRAM_NUM);

	int counter = 0;
	char line[255];
	size_t len = 255;
	FILE *fp = fopen(PROGDATAFILE, "r");

	// Read in information about the program
	while(fgets(line, len, fp) != NULL)
	{
		strtok(line, "\n");
		char* prog_name = strtok(line, "#");
		char* gpa = strtok(NULL, "#");
		char* depart_id = strtok(NULL, "#");
		int length = strlen(prog_name);
		prog_list[counter].department_id = atoi(depart_id);
		prog_list[counter].program_name = malloc(length+1);
		strcpy(prog_list[counter].program_name, prog_name);
		prog_list[counter].required_gpa = atof(gpa);
		counter++;
	}
	fclose(fp);
}

// *from Beej's guide. For killing zombie processes
void sigchld_handler(int s)
{
	while(waitpid(-1, NULL, WNOHANG) > 0);
}

// Get sockaddr, IPv4 or IPv6. *from Beej's guide
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) 
	{
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

/* Add recieced program information into admission database. */
int input_program_info(char* program, int id)
{
	char id_char = '0' + id;
	pthread_mutex_lock(lock);
	FILE *fp = fopen(PROGDATAFILE, "a");
	fputs(program, fp);
	fputc(delim, fp);
	fputc(id_char, fp);
	fputc(new_line, fp);
	fclose(fp);
	(*counter)++;
	pthread_mutex_unlock(lock);
}

/* Make admission decions based on student's gpa. Write admission details
 * to the local ADMISSIONDATAFILE file if student has a matched program. */
int general_selection(char** inter_list, int num, char name, float gpa, char* cgpa)
{
	// Fill in prog_list
	pthread_mutex_lock(lock);
	if (prog_list == NULL)
	{
		fill_prog_list();
	}
	pthread_mutex_unlock(lock);

	int result = 0;
	int i;
	for (i = 0; i < num; i++)
	{
		int j;
		for (j = 0; j < (PROGRAM_NUM * DEPARTMENT_NUM); j++)
		{
			// Find an interested program
			if (strcmp(inter_list[i], prog_list[j].program_name) == 0)
			{
				// Check if gpa is greater than required gpa.
				result = 1;
				if (gpa >= prog_list[j].required_gpa)
				{
					// Write admission details for acception.
					char depart_id = prog_list[j].department_id + '0';
					pthread_mutex_lock(lock);
					FILE *fp = fopen(ADMISSIONDATAFILE, "a");
					fputs("Accept", fp);
					fputc(delim, fp);
					fputc(name, fp);
					fputc(delim, fp);
					fputc(depart_id, fp);
					fputc(delim, fp);
					fputs(cgpa, fp);
					fputc(delim, fp);
					fputs(prog_list[j].program_name, fp);
					fputc(new_line, fp);
					fclose(fp);
					pthread_mutex_unlock(lock);
					return result;
				}
			}
		}
	}
	if (result == 1)
	{
		// Write admission detail for rejection.
		pthread_mutex_lock(lock);
		FILE *fp = fopen(ADMISSIONDATAFILE, "a");
		fputs("Reject", fp);
		fputc(delim, fp);
		fputc(name, fp);
		fputc(new_line, fp);
		fclose(fp);
		pthread_mutex_unlock(lock);
	}
	return result;
}

/* Handle new connected client for phase1. from Beej's guide */
void connection_handler(int socket)
{
	// Prepare to communicate. *from Beej's guide
	int numbytes;
	int id;
	char name;
	char buf[MAXDATASIZE];
	
	// First msg should be the department id
	if((numbytes = recv(socket, buf, MAXDATASIZE-1, 0)) == -1)
	{
		perror("recieve id");
		close(socket);
		exit(1);
	}
	else
	{
		// Decode name and id
		if(buf[0] == '0') name = 'A';
		else if(buf[0] == '1') name = 'B';
		else if(buf[0] == '2') name = 'C';
		id = atoi(&buf[0]);

		if(send(socket, "ACK", 3, 0) == -1)
		{
			perror("send ACK");
			close(socket);
			exit(1);
		}
	}

	// Receive all following information
	while((numbytes = recv(socket, buf, MAXDATASIZE-1, 0)) != -1) 
	{
		buf[numbytes] = '\0';
		//printf("server: received:%s length is %d\n",buf,strlen(buf));
		if(strcmp(buf, "END") == 0)
		{
			printf("Received the program list from Department%c\n", name);
			if (send(socket, "ACK", 3, 0) == -1)
			{
				perror("send");
				close(socket);
				exit(1);
			}
			break;
		}
		else
		{
			// Input program info and send ack msg
			input_program_info(buf, id);
			if (send(socket, "ACK", 3, 0) == -1) 
			{
				perror("send");
				close(socket);
				exit(1);
			}
		}
	}

	if(numbytes == -1)
	{
		perror("recv");
	}

	pthread_mutex_lock(lock);
	if (*counter == (PROGRAM_NUM * DEPARTMENT_NUM) && (*phase1 == 0))
	{
		*phase1 = 1;
		printf("End of Phase 1 for the admission office\n");
		fill_prog_list();
	}
	pthread_mutex_unlock(lock);
	close(socket);
}

/* Handle new connected student client for phase 2. from Beej's guide. */
void student_handler(int socket)
{
	// Prepare to communicate. *from Beej's guide
	int numbytes;
	int id;
	char name;
	float gpa;
	char* cgpa;
	int interest_num;
	char buf[MAXDATASIZE];

	// First msg should be the student id
	if ((numbytes = recv(socket, buf, MAXDATASIZE - 1, 0)) == -1)
	{
		perror("recieve id");
		close(socket);
		exit(1);
	}
	else
	{
		// Decode name and id
		id = atoi(&buf[0]);
		name = buf[0];
		if (send(socket, "ACK", 3, 0) == -1)
		{
			perror("send ACK");
			close(socket);
			exit(1);
		}
	}

	// Receive interested program number
	if ((numbytes = recv(socket, buf, MAXDATASIZE - 1, 0)) == -1)
	{
		perror("recieve interest_num");
		close(socket);
		exit(1);
	}
	else
	{
		// Decode num
		interest_num = atoi(&buf[0]);
		if (send(socket, "ACK", 3, 0) == -1)
		{
			perror("send ACK");
			close(socket);
			exit(1);
		}
	}

	// Receive GPA then
	if ((numbytes = recv(socket, buf, MAXDATASIZE - 1, 0)) == -1)
	{
		perror("recieve gpa");
		close(socket);
		exit(1);
	}
	else
	{
		// Decode gpa
		buf[numbytes] = '\0';
		gpa = atof(buf);
		cgpa = malloc(numbytes + 1);
		strcpy(cgpa, buf);

		if (send(socket, "ACK", 3, 0) == -1)
		{
			perror("send ACK");
			close(socket);
			exit(1);
		}
	}

	char **interest_list = malloc(sizeof(char*)*(interest_num));
	int index = 0;
	// Receive all following information
	while ((numbytes = recv(socket, buf, MAXDATASIZE - 1, 0)) != -1)
	{
		buf[numbytes] = '\0';
		//printf("received %s\n",buf);
		if (strcmp(buf, "END") == 0)
		{
			int result = general_selection(interest_list, interest_num, name, gpa, cgpa);
			char cresult = result + '0';
			//printf("Received the program list from Department%c\n", name);
			if (send(socket, &cresult, 1, 0) == -1)
			{
				perror("send");
				close(socket);
				exit(1);
			}
			break;
		}
		else
		{
			// Input program interest and send ack msg
			interest_list[index] = malloc(numbytes + 1);
			strcpy(interest_list[index], buf);
			index++;
			if (send(socket, "ACK", 3, 0) == -1)
			{
				perror("send");
				close(socket);
				exit(1);
			}
		}
	}

	if (numbytes == -1)
	{
		perror("recv");
	}

	printf("Admission office receive the application from Student%c\n", name);

	close(socket);
}

/* UDP Connection handler for phase 2. from Beej's guide.*/
int udp_handler(char* port, char* msg, int socketid)
{
	int sockfd;
	struct addrinfo hints, *servinfo, *p;
	int rv;
	int numbytes;
	servinfo = NULL;

	// If UDP connection is not yet setup for the given department/student
	if (socketid == -10)
	{
		memset(&hints, 0, sizeof hints);
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_DGRAM;

		if ((rv = getaddrinfo(ADMISSION_HOSTNAME, port, &hints, &servinfo)) != 0) {
			fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
			return -10;
		}

		// loop through all the results and make a socket
		for (p = servinfo; p != NULL; p = p->ai_next)
		{
			if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
			{
				perror("talker: socket");
				continue;
			}
			break;
		}

		if (p == NULL)
		{
			fprintf(stderr, "talker: failed to bind socket\n");
			return -10;
		}

		struct sockaddr_in *localAddress = (struct sockaddr_in*)p->ai_addr;
		printf("The admission office has UDP port %d and IP address %s for Phase 2\n", (int)ntohs(localAddress->sin_port), inet_ntoa(localAddress->sin_addr));
		
		if ((numbytes = sendto(sockfd, msg, strlen(msg), 0, p->ai_addr, p->ai_addrlen)) == -1)
		{
			perror("talker: sendto");
			exit(1);
		}

		freeaddrinfo(servinfo);
	}
	else   /*If UDP connection has already been setup for the given department/student. */
	{
		sockfd = socketid;

		memset(&hints, 0, sizeof hints);
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_DGRAM;

		if ((rv = getaddrinfo(ADMISSION_HOSTNAME, port, &hints, &servinfo)) != 0) {
			fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
			return -10;
		}
		p = servinfo;
		if ((numbytes = sendto(sockfd, msg, strlen(msg), 0, p->ai_addr, p->ai_addrlen)) == -1)
		{
			perror("talker: sendto");
			exit(1);
		}

		freeaddrinfo(servinfo);
	}

	return sockfd;
}

int main(void)
{
	int server_socket, new_socket; 						/* Listen on server_soket, new connection on new_socket */
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_storage new_address; 				/* Connector's address information */
	socklen_t sin_size;
	int result;											/* Track function return value */
	struct sigaction sa;
	int yes = 1;
	char s[INET_ADDRSTRLEN];

	// Allocate globally shared memeory and initilize the value
	lock = (pthread_mutex_t*)mmap(NULL, sizeof(pthread_mutex_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	counter =  (int*)mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	phase1 = (int*)mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	*counter = 0;
	*phase1 = 0;
	prog_list = NULL;

	pthread_mutexattr_init(&attrmutex);
	pthread_mutexattr_setpshared(&attrmutex, PTHREAD_PROCESS_SHARED);
	if(pthread_mutex_init(lock, &attrmutex) != 0)
	{
	  printf("mutex init failed\n");
	  return 1;
	}

	// Remove exited database file if already exits
	FILE *f = fopen(PROGDATAFILE, "r");
	if (f != NULL)
	{
		fclose(f);
		remove(PROGDATAFILE);
	}
	f = fopen(ADMISSIONDATAFILE, "r");
	if (f != NULL)
	{
		fclose(f);
		remove(ADMISSIONDATAFILE);
	}

	// Initilize addrinfo struct for loading. *from Beej's guide
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_INET;
	hints.ai_socktype = SOCK_STREAM;

	// Load server addressinfo struct, used for later. *from Beej's guide
	if ((result = getaddrinfo(ADMISSION_HOSTNAME, ADMISSION_PORT, &hints, &servinfo)) != 0) 
	{
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(result));
		return 1;
	}

	// loop through all the results and bind to the first we can. *from Beej's guide
	for(p = servinfo; p != NULL; p = p->ai_next) 
	{
		// Create the socket
		if((server_socket = socket(p->ai_family, p->ai_socktype,p->ai_protocol)) == -1) 
		{
			perror("server: socket");
			continue;
		}

		// Configure the server_socket
		if(setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &yes,sizeof(int)) == -1) 
		{
			perror("setsockopt");
			exit(1);
		}

		// Bind the server_socket
		if(bind(server_socket, p->ai_addr, p->ai_addrlen) == -1) 
		{
			close(server_socket);
			perror("server: bind");
			continue;
		}

		break;
	}

	if(p == NULL) 
	{
		fprintf(stderr, "server: failed to bind\n");
		//free(prog_list);
		return 2;
	}
	

	freeaddrinfo(servinfo);

	if(listen(server_socket, BACKLOG) == -1) 
	{
		perror("listen");
		//free(prog_list);
		exit(1);
	}
	int port_number = (int)ntohs(((struct sockaddr_in*)p->ai_addr)->sin_port);
	char* IP_addr = inet_ntoa(((struct sockaddr_in*)p->ai_addr)->sin_addr);
	printf("The admission office has TCP port %d and IP address %s\n", port_number, IP_addr);//(int) ntohs(((struct sockaddr_in*)p->ai_addr)->sin_port), inet_ntoa(((struct sockaddr_in*)p->ai_addr)->sin_addr));
	
	//Reap all memory *from Beej's guide
	sa.sa_handler = sigchld_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) 
	{
		perror("sigaction");
		//free(prog_list);
		exit(1);
	}

	// Start handling connection
	int phase2 = 0;
	int student_counter = 0;
	while(1) 
	{ 
		sin_size = sizeof(new_address);
		new_socket = accept(server_socket, (struct sockaddr *)&new_address, &sin_size);

		if (new_socket == -1) 
		{
			//perror("accept");
			continue;
		}

		if ( ( *counter == (PROGRAM_NUM * DEPARTMENT_NUM) ) && (phase2 == 0))
		{
			printf("The admission office has TCP port %d and IP address %s\n", port_number, IP_addr);
			phase2 = 1;
		}

		inet_ntop(new_address.ss_family, get_in_addr((struct sockaddr *)&new_address), s, sizeof(s));
		//printf("server: got connection from %s port %d\n", s, ((struct sockaddr_in *)&new_address)->sin_port);

		if (!fork()) 
		{
			if(phase2 == 0)
			{
				close(server_socket);
				connection_handler(new_socket);
				exit(0);
			}
			else
			{
				close(server_socket);
				student_handler(new_socket);
				exit(0);
			}
		}
		close(new_socket);
		if (phase2 != 0) student_counter++;
		if (student_counter == STUDENT_NUM) break;
	}
	
	while (wait(0) != -1) {}

	// UDP phase 2
	// Initilize depart_sockets and student_sockets with -10's because all UDP connection is not yet setup.
	int l;
	for (l = 0; l < DEPARTMENT_NUM; l++)
	{
		depart_sockets[l] = -10;
	}
	for (l = 0; l < STUDENT_NUM; l++)
	{
		student_sockets[l] = -10;
	}

	char line[255];
	size_t len = 255;
	FILE *fp = fopen(ADMISSIONDATAFILE, "r");

	// Read in information about the admission
	while (fgets(line, len, fp) != NULL)
	{
		strtok(line, "\r");
		strtok(line, "\n");
		char* offer = strtok(line, "#");
		char* sid = strtok(NULL, "#");
		char* s_port = student_ports[atoi(sid) - 1];
		
		// Send rejection msg
		if (strcmp(offer, "Reject") == 0)
		{
			student_sockets[atoi(sid) - 1] = udp_handler(s_port, "Reject", student_sockets[atoi(sid) - 1]);
			printf("The admission office has send the application result to Student%s\n", sid);
		}
		else
		{
			char* did = strtok(NULL, "#");
			char* gpa = strtok(NULL, "#");
			char* prog_name = strtok(NULL, "#");
			char *cdid = malloc(2);
			cdid[0] = department_names[atoi(did)];
			cdid[1] = '\0';
			char* d_port = department_ports[atoi(did)];

			char* s_msg = malloc(strlen(prog_name) + 20);
			strcpy(s_msg, "Accept");
			strcat(s_msg, "#");
			strcat(s_msg, prog_name);
			strcat(s_msg, "#");
			strcat(s_msg, "department");
			strcat(s_msg, cdid);
			student_sockets[atoi(sid)-1] = udp_handler(s_port, s_msg, student_sockets[atoi(sid)-1]);

			// Send accpetion msg
			printf("The admission office has send the application result to Student%s\n", sid);


			char* d_msg = malloc(strlen(prog_name) + strlen(gpa) + 11);
			strcpy(d_msg, "Student");
			strcat(d_msg, sid);
			strcat(d_msg, "#");
			strcat(d_msg, gpa);
			strcat(d_msg, "#");
			strcat(d_msg, prog_name);
			depart_sockets[atoi(did)] = udp_handler(d_port, d_msg, depart_sockets[atoi(did)]);

			//Send admission msg
			printf("The admission office has send one admitted student to Department%s\n", cdid);
		}
	}
	fclose(fp);

	// Close all UDP connections
	for (l = 0; l < DEPARTMENT_NUM; l++)
	{
		udp_handler(department_ports[l], "END", depart_sockets[l]);
		close(depart_sockets[l]);
	}
	for (l = 0; l < STUDENT_NUM; l++)
	{
		close(student_sockets[l]);
	}

	printf("End of Phase 2 for the admission office\n");

	// Free resources
	munmap((void*)counter, sizeof(int));
	munmap((void*)phase1, sizeof(int));
	free(prog_list);
	pthread_mutex_destroy(lock);
	pthread_mutexattr_destroy(&attrmutex);
	return 0;
}
