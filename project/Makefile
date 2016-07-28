default: department admission

department: Department.c Admission.h
	gcc Department.c -lnsl -lsocket -lresolv  -o department

admission: Admission.c Admission.h
	gcc Admission.c -lnsl -lsocket -lresolv -o admission

clean:
	-rm -f admission
	-rm -f department
