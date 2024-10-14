#include "portal-shared.h"

void die(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
#ifdef DEBUG
    abort();
#else
    exit(1);
#endif
}

buf_t *buf_alloc(int size) {
    buf_t *buf = malloc(sizeof(buf_t) + size);
    buf->size = size;
    buf->fill = 0;
    return buf;
}

int buf_read(buf_t *buf, int fd) {
    int max_read = buf->size - buf->fill;
    int received = read(fd, buf->buf + buf->fill, max_read);
    if (received < 0)
        return -1;
    buf->fill += received;
    return received;
}

int buf_append(buf_t *buf, const char *str) {
    int str_len = strlen(str);
    if (buf->fill + str_len + 1 >= buf->size)
        return 0;
    memcpy(buf->buf + buf->fill, str, str_len);
    buf->fill += str_len;
    buf->buf[buf->fill++] = 0;
    return 1;
}

char *buf_mem(buf_t *buf) {
    return buf->buf;
}

int buf_fill(buf_t *buf) {
    return buf->fill;
}

const char *buf_iter_first(buf_t *buf) {
    if (buf->fill == 0)
        return NULL;
    return buf->buf;
}

const char *buf_iter_next(buf_t *buf, const char *last) {
    if (last && (last < buf->buf || last >= buf->buf + buf->fill))
        return NULL;
    int offset = last - buf->buf;
    while (offset < buf->fill && buf->buf[offset] != '\0')
        offset++;
    offset++;
    if (offset < buf->fill)
        return buf->buf + offset;
    return NULL;
}

void buf_free(buf_t *buf) {
    free(buf);
}

int send_fds(int unix_sock, int *fds, int nfds) {
    struct iovec iov = {
        .iov_base = ":)",
        .iov_len = 2
    };

    const size_t fds_size = sizeof(int) * nfds;
    union {
        char buf[CMSG_SPACE(fds_size)];
        struct cmsghdr align;
    } u;

    struct msghdr msg = {
        .msg_iov = &iov,
        .msg_iovlen = 1,
        .msg_control = u.buf,
        .msg_controllen = sizeof(u.buf)
    };

    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
    *cmsg = (struct cmsghdr){
        .cmsg_level = SOL_SOCKET,
        .cmsg_type = SCM_RIGHTS,
        .cmsg_len = CMSG_LEN(fds_size)
    };

    memcpy(CMSG_DATA(cmsg), fds, fds_size);
    return sendmsg(unix_sock, &msg, 0) >= 0;
}

int recv_fds(int unix_sock, int *fds, int nfds) {
    char buf[2];
    struct iovec iov = {.iov_base = buf, .iov_len = sizeof(buf)};
    
    const size_t fds_size = sizeof(int) * nfds;
    union {
        char buf[CMSG_SPACE(fds_size)];
        struct cmsghdr align;
    } u;
    
    struct msghdr msg = {
        .msg_iov = &iov,
        .msg_iovlen = 1,
        .msg_control = u.buf,
        .msg_controllen = sizeof(u.buf)
    };
    
    if (recvmsg(unix_sock, &msg, 0) <= 0)
        return 0; // Error or connection closed
    
    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
    if (cmsg == NULL || 
        cmsg->cmsg_level != SOL_SOCKET || 
        cmsg->cmsg_type != SCM_RIGHTS
    ) {
        return 0; // Invalid control message
    }
    
    memcpy(fds, CMSG_DATA(cmsg), fds_size);
    return fds_size;
}
