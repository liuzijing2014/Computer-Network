default: department admission

department.o: Department.c Admission.h
	gcc Department.c -lnsl -lsocket -lresolv -o department.o

department: department.o
	gcc department.o -o department

admission.o: Admission.c Admission.h
	gcc Admission.c -lnsl -lsocket -lresolv -o admission.o

admission: admission.o
	gcc admission.o -o admission

clean:
	-rm -f admission.o
	-rm -f admission
	-rm -f department.o
	-rm -f department