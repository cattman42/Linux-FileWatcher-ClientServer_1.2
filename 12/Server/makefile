bu: bu.c
	gcc bu.c csapp.c -pthread -o bu

install:
	cp bu.conf /etc/bu.conf
	rm -rf /var/bu
	mkdir /var/bu
	cp bu.service /lib/systemd/system/bu.service
	cp bu /usr/sbin/bu