#include "shared.h"

extern char **environ;

int main(int argc, char *argv[]) {
    struct rlimit lim = { .rlim_cur = MAX_FDS, .rlim_max = MAX_FDS };
    if (setrlimit(RLIMIT_NOFILE, &lim) < 0)
        die("cannot set fd limit: %m");

    buf_t *env_buf = buf_alloc(MAX_BUF_SIZE);
    buf_t *arg_buf = buf_alloc(MAX_BUF_SIZE);

    for (int i = 0; environ[i]; i++) {
        if (!buf_append(env_buf, environ[i]))
            die("too many envs");
    }

    for (int i = 1; i < argc; i++) {
        if (!buf_append(arg_buf, argv[i]))
            die("too many args");
    }

    int fds[MAX_FDS];
    int num_fds = 0;
    for (int i = 0; i < MAX_FDS; i++) {
        if (fcntl(i, F_GETFD) == -1)
            continue;
        fds[num_fds] = i;
        num_fds++;
    }

    int server_fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (server_fd == -1)
        die("socket failed: %m");

    struct sockaddr_un name;
    memset(&name, 0, sizeof(name));
    name.sun_family = AF_UNIX;
    strncpy(name.sun_path, SOCKET_NAME, sizeof(name.sun_path) - 1);

    if (connect(server_fd, (const struct sockaddr *)&name, sizeof(name)) == -1)
        die("cannot connect to server '%s': %m", SOCKET_NAME);

    if (write(server_fd, buf_mem(env_buf), buf_fill(env_buf)) != buf_fill(env_buf))
        die("cannot send env: %m");
    if (write(server_fd, buf_mem(arg_buf), buf_fill(arg_buf)) != buf_fill(arg_buf))
        die("cannot send args: %m");
    if (write(server_fd, &fds, sizeof(int) * num_fds) != sizeof(int) * num_fds)
        die("cannot send fd mapping: %m");
    if (!send_fds(server_fd, fds, num_fds))
        die("cannot send fds: %m");

    struct pollfd p = {
        .fd = server_fd,
        .events = POLLIN,
    };
    int ready = poll(&p, 1, -1);
    if (ready == -1)
        die("poll failed: %m");

    int status;
    int ret = read(server_fd, &status, sizeof(int));
    if (ret < 0)
        die("reading response failed: %m");
    if (ret == 0)
        die("server closed connection");
    if (ret != sizeof(int))
        die("unexpected response size");
    if (WIFEXITED(status)){
        exit(WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
        kill(getpid(), WTERMSIG(status));
    }
    return EXIT_SUCCESS;
}
