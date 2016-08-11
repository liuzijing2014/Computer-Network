default: department admission

ubuntu:	department1 admission1

department: Department.c Admission.h
	gcc Department.c -lnsl -lsocket -lresolv  -o department

department1: Department.c Admission.h
	gcc Department.c -o department

admission: Admission.c Admission.h
	gcc Admission.c -lnsl -lsocket -lresolv -o admission

admission1: Admission.c Admission.h
	gcc Admission.c -pthread -o admission

clean:
	-rm -f admission
	-rm -f department
