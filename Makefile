all: raprec raprun
raprec: raprec.c
	gcc raprec.c -o raprec -Wall -lmysqlclient -lbluetooth
raprun: raprun.c
	gcc raprun.c -o raprun -Wall -lmysqlclient -lbluetooth

install:
	cp raprec /usr/bin
	cp raprun /usr/bin
	chmod u+s /usr/bin/raprun
clean:
	-rm raprec raprun

