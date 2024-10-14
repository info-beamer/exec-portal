all: portal-spawner portal-exec portal-lib-test

PORTAL_NAME ?= portal.sock
PORTAL_PERM ?= 777
CFLAGS      += -O2 -DPORTAL_NAME=\"${PORTAL_NAME}\" -DPORTAL_PERM=0${PORTAL_PERM}

portal-spawner.o: portal-spawner.c portal-shared.c
portal-exec.o: portal-exec.c portal-shared.c

portal-spawner: portal-spawner.o portal-shared.o
	$(CC) $^ -o $@

portal-exec: portal-exec.o portal-shared.o
	$(CC) $^ -o $@

portal-lib-test: portal-lib-test.o portal-lib.o portal-shared.o
	$(CC) $^ -o $@

clean:
	rm -rf portal-spawner portal-exec portal-lib-test *.o portal.sock
