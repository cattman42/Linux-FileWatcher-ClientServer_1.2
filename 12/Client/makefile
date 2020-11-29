fwd: fwd.c
	gcc fwd.c csapp.c -pthread -o fwd

install:
	cp fwd.conf /etc/fwd.conf
	rm -rf /var/fwd
	mkdir /var/fwd
	cp fwd.service /lib/systemd/system/fwd.service
	cp fwd /usr/sbin/fwd