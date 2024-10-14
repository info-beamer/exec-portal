#include "portal-lib.h"

void portal_setup(portal_listen_t *portal, const char *portal_name) {
    if (unlink(portal_name) < 0 && errno != ENOENT)
        die("cannot unlink existing socket %s: %m", portal_name);

    struct rlimit lim = { .rlim_cur = 1024, .rlim_max = 1024 };
    if (setrlimit(RLIMIT_NOFILE, &lim) < 0)
        die("cannot set fd limit: %m");

    mode_t old = umask(0);
    int listen_fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (listen_fd < 0)
        die("socket failed: %m");

    struct sockaddr_un name;
    memset(&name, 0, sizeof(name));

    name.sun_family = AF_UNIX;
    strncpy(name.sun_path, portal_name, sizeof(name.sun_path) - 1);

    if (bind(listen_fd, (const struct sockaddr *)&name, sizeof(name)) == -1)
        die("bind failed: %m");
    umask(old);

    if (fchmod(listen_fd, PORTAL_PERM) < 0)
        die("chmod of socket failed: %m");
    
    // if (fcntl(listen_fd, F_SETFL, fcntl(listen_fd, F_GETFL, 0) | O_NONBLOCK) != 0)
    //     die("cannot set non-blocking: %m");

    if (listen(listen_fd, 20) < 0)
        die("listen failed: %m");

    portal->listen_fd = listen_fd;
}

portal_spawn_t *portal_handle(portal_listen_t *portal) {
    int client_fd = accept(portal->listen_fd, NULL, NULL);
    if (client_fd < 0) {
        if (errno == EINTR || errno == EAGAIN)
            return NULL;
        die("accept failed: %m");
    }
    portal_spawn_t *spawn = malloc(sizeof(portal_spawn_t));
    memset(spawn, 0, sizeof(spawn));
    spawn->client_fd = client_fd;

    spawn->env_buf = buf_alloc(MAX_BUF_SIZE);
    if (buf_read(spawn->env_buf, spawn->client_fd) < 0)
        die("cannot read env buf");

    spawn->arg_buf = buf_alloc(MAX_BUF_SIZE);
    if (buf_read(spawn->arg_buf, spawn->client_fd) < 0)
        die("cannot read arg buf");

    int fd_map_size = sizeof(spawn->fd_map);
    int received = read(client_fd, &spawn->fd_map, fd_map_size);
    if (received < 0 || (received % sizeof(int)) != 0)
        die("unexpected fds size");
    spawn->num_fds = received / sizeof(int);
    recv_fds(spawn->client_fd, spawn->fds, spawn->num_fds);
    return spawn;
}

int portal_spawn_fd(portal_spawn_t *spawn, int fd) {
    for (int i = 0; i < spawn->num_fds; i++) {
        if (spawn->fd_map[i] == fd)
            return spawn->fds[i];
    }
    return -1;
}

int portal_spawn_argc(portal_spawn_t *spawn) {
    int count = 0;
    for (buf_iter(spawn->arg_buf, arg))
        count++;
    return count;
}

const char *portal_spawn_argv(portal_spawn_t *spawn, int n) {
    int idx = 0;
    for (buf_iter(spawn->arg_buf, arg)) {
        if (idx == n)
            return arg;
        idx++;
    }
    return NULL;
}

const char *portal_spawn_getenv(portal_spawn_t *spawn, const char *key) {
    const size_t key_len = strlen(key);
    int idx = 0;
    for (buf_iter(spawn->env_buf, arg)) {
        if (strncmp(arg, key, key_len) == 0 && arg[key_len] == '=')
            return arg + key_len + 1;
        idx++;
    }
    return NULL;
}

void portal_spawn_exit(portal_spawn_t *spawn, int status) {
    int exit_code = W_EXITCODE(status, 0);
    if (write(spawn->client_fd, &exit_code, sizeof(int)) != sizeof(int))
        /* nothing can be done here */ ;
    close(spawn->client_fd);
    for (int i = 0; i < spawn->num_fds; i++)
        close(spawn->fds[i]);
    buf_free(spawn->env_buf);
    buf_free(spawn->arg_buf);
    free(spawn);
}
