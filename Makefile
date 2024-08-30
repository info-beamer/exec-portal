all: portal-spawner portal-exec

PORTAL_NAME ?= portal.sock
PORTAL_PERM ?= 777
CFLAGS      += -O2 -DPORTAL_NAME=\"${PORTAL_NAME}\" -DPORTAL_PERM=0${PORTAL_PERM}

portal-spawner.o: portal-spawner.c shared.h
portal-exec.o: portal-exec.c shared.h

portal-spawner: portal-spawner.o
	$(CC) $^ -o $@

portal-exec: portal-exec.o
	$(CC) $^ -o $@

clean:
	rm -rf portal-spawner portal-exec *.o portal.sock
