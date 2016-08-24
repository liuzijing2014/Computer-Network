#ifndef ADMISSION_H
#define ADMISSION_H

/* Constants for departments */
#define DEPARTMENT_A (int)0
#define DEPARTMENT_B (int)1
#define DEPARTMENT_C (int)2
#define DEPARTMENT_NUM (int)3
#define PROGRAM_NUM (int)3
#define STUDENT_NUM (int)5
#define MAXSTUDENT_INFO (int)4

/* Constants for networking */
//#define ADMISSION_HOSTNAME "127.0.0.1"
#define ADMISSION_HOSTNAME "nunki.usc.edu"
#define ADMISSION_PORT "3426"
#define MAXDATASIZE 255 
#define MAXPROGNAME 128
#define PROGDATAFILE "program_data_file.txt"
#define ADMISSIONDATAFILE "admission_data_file.txt"
#define DEPARTMENTA_PORT "21226"
#define DEPARTMENTB_PORT "21326"
#define DEPARTMENTC_PORT "21426"
#define STUDENT1_PORT "21526"
#define STUDENT2_PORT "21626"
#define STUDENT3_PORT "21726"
#define STUDENT4_PORT "21826"
#define STUDENT5_PORT "21926"

static char department_names[] = { 'A', 'B', 'C' };
static char *department_ports[] = { DEPARTMENTA_PORT, DEPARTMENTB_PORT, DEPARTMENTC_PORT };
static char *student_ports[] = { STUDENT1_PORT, STUDENT2_PORT, STUDENT3_PORT, STUDENT4_PORT, STUDENT5_PORT };


#endif