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
#include <signal.h>
#include <pthread.h>
#include "Admission.h"
#include "Department.h"

#define BACKLOG 10

struct program
{
	int department_id;
	char* program_name;
	float required_gpa;
};

static struct program *prog_list;
static pthread_mutex_t lock;
static int counter;
static int running;

static int phase_1;


void sigchld_handler(int s)
{
	while(waitpid(-1, NULL, WNOHANG) > 0);
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) 
	{
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}


// Add recieced program information into admission database 
int input_program_info(char* program)
{
	pthread_mutex_lock(&lock);
	phase_1++;
	pthread_mutex_unlock(&lock);
}

// Handle new connected client
void connection_handler(int socket)
{
	int numbytes;
	int id;
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
	  id = atoi(&buf[0]);
	  printf("id recieved is %d\n", id);
	  if(send(socket, "ACK", 3, 0) == -1)
	  {
	    perror("send ACK");
	    close(socket);
	    exit(1);
	  }
	}

	while((numbytes = recv(socket, buf, MAXDATASIZE-1, 0)) != -1) 
	{
		buf[numbytes] = '\0';
		printf("server: received '%s'\n",buf);
		if(strcmp(buf, "END") == 0)
		{
			break;
		}
		else
		{

			input_program_info(buf);
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
	close(socket);
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

	counter = 0;
	running = 1;
	phase_1 = 0;

	// Initilize program list and the lock
	prog_list = malloc(sizeof(struct program)*3);
	if(prog_list == NULL)
	{
		printf("Allocate program list failed\n");
		return 1;
	}


	if(pthread_mutex_init(&lock, NULL) != 0)
    {
        printf("mutex init failed\n");
        return 1;
    }

	// Initilize addrinfo struct for loading
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	// Load server addressinfo struct, used for later
	if ((result = getaddrinfo(NULL, ADMISSION_PORT, &hints, &servinfo)) != 0) 
	{
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(result));
		free(prog_list);
		return 1;
	}

	// loop through all the results and bind to the first we can.
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
			free(prog_list);
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
		free(prog_list);
		return 2;
	}

	freeaddrinfo(servinfo);

	if(listen(server_socket, BACKLOG) == -1) 
	{
		perror("listen");
		free(prog_list);
		exit(1);
	}

	sa.sa_handler = sigchld_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) 
	{
		perror("sigaction");
		free(prog_list);
		exit(1);
	}

	printf("server: waiting for connections...\n");
	while(running && (phase_1 < PROGRAM_NUM*DEPARTMENT_NUM)) 
	{ 
		sin_size = sizeof(new_address);
		new_socket = accept(server_socket, (struct sockaddr *)&new_address, &sin_size);

		if (new_socket == -1) 
		{
			perror("accept");
			continue;
		}

		inet_ntop(new_address.ss_family, get_in_addr((struct sockaddr *)&new_address), s, sizeof(s));
		printf("server: got connection from %s port %d\n", s, ((struct sockaddr_in *)&new_address)->sin_port);

		if (!fork()) 
		{
			close(server_socket);
			connection_handler(new_socket);
			exit(0);
		}
		close(new_socket);
	}
	free(prog_list);
	pthread_mutex_destroy(&lock);
	return 0;
}
