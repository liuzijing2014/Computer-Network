#ifndef DEPARTMENT_H
#define DEPARTMENT_H

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

static const int DEPARTMENT_A = 0;
static const int DEPARTMENT_B = 1;
static const int DEPARTMENT_C = 2;

struct program_info
{
	int department_id;			/* The id of the department that the prgram belongs to. */
	char* program_name;			/* The name of the program, which is unique within the department. */
	float required_gpa;			/* The lowest accepted GPA for the program. */
};

#endif