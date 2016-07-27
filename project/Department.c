#include "Department.h"

static struct program_info* program_list[3];
static int counter = 0;

struct program_info* read_proginfo(const char* file_name)
{
	char *line = NULL;
	size_t len = 0;
	FILE *fp = fopen(file_name, "r");
	struct program_info *pi;

	// Read in information about the program
	char* token;
	char delim = '#';
	while(getline(&line, &len, fp) != -1)
	{
		printf("%s", line);
		pi = malloc(sizeof(struct program_info));
		pi->department_id = DEPARTMENT_A;
		token = strtok(line, &delim);
		pi->program_name = token;
		token = strtok(NULL, &delim);
		pi->required_gpa = atof(token);
		program_list[counter] = pi;
		counter++;
	}

	return NULL;
}

int main(void)
{
	read_proginfo("departmentA.txt");
	for(int i = 0; i < counter; i++)
	{
		printf("department id is %i, program name is %s, required_gpa is %f \n", program_list[i]->department_id, program_list[i]->program_name, program_list[i]->required_gpa);
	}
}