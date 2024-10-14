#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>


#ifndef PORTAL_NAME
#define PORTAL_NAME "portal.sock"
#endif

#ifndef SOCKET_PERM
#define SOCKET_PERM 0777
#endif

#define MAX_BUF_SIZE 65536
#define MAX_FDS 256
#define MAX_ARGS 1024
#define MAX_ENVS 1024

void die(const char *fmt, ...);

typedef struct {
    int size;
    int fill;
    char buf[];
} buf_t;

#define buf_iter(buf, it) \
    const char *it = buf_iter_first(buf); it; it = buf_iter_next(buf, it)

buf_t *buf_alloc(int size);
int buf_read(buf_t *buf, int fd);
int buf_append(buf_t *buf, const char *str);
char *buf_mem(buf_t *buf);
int buf_fill(buf_t *buf);
const char *buf_iter_first(buf_t *buf);
const char *buf_iter_next(buf_t *buf, const char *last);
void buf_free(buf_t *buf);

int send_fds(int unix_sock, int *fds, int nfds);
int recv_fds(int unix_sock, int *fds, int nfds);

#ifdef __cplusplus
}
#endif
