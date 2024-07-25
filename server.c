#include "shared.h"

#if __has_include("linux/close_range.h") 

#include <linux/close_range.h>
static void close_fd_range(int lo, int hi) {
   close_range(lo, hi, 0);
}

#elif defined(FORCE_CLOSE_RANGE)

#ifndef SYS_close_range
#define SYS_close_range 436
#warning using hard coded close_range syscall number
#endif

static int close_fd_range(unsigned int first, unsigned int last) {
	return syscall(SYS_close_range, first, last, 0);
}

#else

static void close_fd_range(int lo, int hi) {
    for (int i = lo; i <= hi; i++)
        close(i);
}

#endif

#ifndef SYS_pidfd_open
#define SYS_pidfd_open 434
#warning using hard coded pidfd_open syscall number
#endif

static int pidfd_open(pid_t pid, unsigned int flags) {
   return syscall(SYS_pidfd_open, pid, flags);
}

#ifndef SYS_pidfd_send_signal
#define SYS_pidfd_send_signal 424
#warning using hard coded pidfd_send_signal syscall number
#endif

static int pidfd_send_signal(int pidfd, int sig) {
    return syscall(SYS_pidfd_send_signal, pidfd, sig, NULL, 0);
}

static int process_child(int our_argc, char *our_argv[], int client_fd) {
    buf_t *env_buf = buf_alloc(MAX_BUF_SIZE);
    if (buf_read(env_buf, client_fd) < 0)
        die("cannot read env buf");
    const char *envp[MAX_ENVS+1];
    int envc = 0;
    for (buf_iter(env_buf, env)) {
        if (envc >= MAX_ARGS)
            die("too many environment variables");
        envp[envc++] = env;
    }
    envp[envc] = NULL;

    buf_t *arg_buf = buf_alloc(MAX_BUF_SIZE);
    if (buf_read(arg_buf, client_fd) < 0)
        die("cannot read arg buf");
    const char *argv[MAX_ARGS+1];
    int argc = 0;
    for (int i = 0; i < our_argc; i++) {
        if (argc >= MAX_ARGS)
            die("too many arguments");
        argv[argc++] = our_argv[i];
    }
    for (buf_iter(arg_buf, arg)) {
        if (argc >= MAX_ARGS)
            die("too many arguments");
        argv[argc++] = arg;
    }
    argv[argc] = NULL;

    int fds[MAX_FDS];
    int received = read(client_fd, &fds, sizeof(fds));
    if (received < 0 || (received % sizeof(int)) != 0)
        die("unexpected fds size");
    int num_fds = received / sizeof(int);
    int imported_fds[MAX_FDS];
    fprintf(stderr, "env=%d, arg=%d, nfds=%d\n",
        buf_fill(env_buf), buf_fill(arg_buf), num_fds
    );

    close_fd_range(512, 1023);

    // save stdout/stderr
    dup2(1, 513);
    dup2(2, 514);

    // move client_fd
    dup2(client_fd, 512);
    close(client_fd);
    client_fd = 512;

    close_fd_range(0, 511);

    // Block lowest 512
    for (int i = 0; i < 512; i++)
        dup2(client_fd, i);

    recv_fds(client_fd, imported_fds, num_fds);

    // Release lowest 512
    close_fd_range(0, 511);

    // Copy into place
    for (int i = 0; i < num_fds; i++) {
        dup2(imported_fds[i], fds[i]);
        close(imported_fds[i]);
    }

    signal(SIGCHLD, SIG_DFL);

    const pid_t pid = fork();
    if (pid == 0) { // child
        setsid();
        close_fd_range(512, 1023);
        if (execve(argv[0], (char * const *)argv, (char * const *)envp) < 0)
            die("%s: %m", argv[0]);
    }

    // free lower ranges
    close_fd_range(0, 511);

    // move stdout/stderr back
    dup2(513, 1);
    dup2(514, 2);

    // close high ranges, but keep client_fd (512) alive
    close_fd_range(513, 1023);

    int pid_fd = pidfd_open(pid, 0);

    struct pollfd p[2] = {{
        .fd = pid_fd,
        .events = POLLIN,
    }, {
        .fd = client_fd,
        .events = POLLIN,
    }};

    int ready = poll((struct pollfd*)&p, 2, -1);
    if (ready == -1)
        die("poll failed: %m");
    if (p[0].revents & POLLIN) {
        // Child exited. Forward exit status
        int wstatus;
        waitpid(pid, &wstatus, 0);
        if (write(client_fd, &wstatus, sizeof(int)) != sizeof(int))
            die("cannot send result: %m");
    } else if (p[1].revents & POLLIN) {
        // Client has disconnected. TERM, then KILL child
        pidfd_send_signal(-pid_fd, SIGTERM);
        usleep(500);
        pidfd_send_signal(-pid_fd, SIGKILL);
        int wstatus;
        waitpid(pid, &wstatus, 0);
    } else {
        // ???
        exit(EXIT_FAILURE);
    }
    exit(EXIT_SUCCESS);
}

static void sig_chld(int signo) {
    int statol;
    if (wait(&statol) < 0)
        die("wait failed: %m");
    if (WIFSTOPPED(statol)){
        fprintf(stderr, "worker is stopped\n");
    } else if (WIFEXITED(statol)){
        fprintf(stderr, "worker exited\n");
    } else if (WIFCONTINUED(statol)){
        fprintf(stderr, "worker continued\n");
    } else if (WIFSIGNALED(statol)){
        int sig = WTERMSIG(statol);
        fprintf(stderr, "worker is signaled %d\n", sig);
    }
}

static int done = 0;
static void sig_done(int signo) {
    done = 1;
}

int main(int argc, char *argv[]) {
    if (unlink(SOCKET_NAME) < 0 && errno != ENOENT)
        die("cannot unlink existing socket %s: %m", SOCKET_NAME);

    struct rlimit lim = { .rlim_cur = 1024, .rlim_max = 1024 };
    if (setrlimit(RLIMIT_NOFILE, &lim) < 0)
        die("cannot set fd limit: %m");

    struct sigaction act, savechld;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    act.sa_handler = sig_chld;
    if (sigaction(SIGCHLD, &act, &savechld) < 0)
        die("sigaction failed: %m");

    act.sa_flags = 0;
    act.sa_handler = sig_done;
    if (sigaction(SIGTERM, &act, &savechld) < 0)
        die("sigaction failed: %m");
    if (sigaction(SIGINT, &act, &savechld) < 0)
        die("sigaction failed: %m");

    int listen_fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (listen_fd < 0)
        die("socket failed: %m");

    struct sockaddr_un name;
    memset(&name, 0, sizeof(name));

    name.sun_family = AF_UNIX;
    strncpy(name.sun_path, SOCKET_NAME, sizeof(name.sun_path) - 1);

    if (bind(listen_fd, (const struct sockaddr *)&name, sizeof(name)) == -1)
        die("bind failed: %m");

    if (fchmod(listen_fd, SOCKET_PERM) < 0)
        die("chmod of socket failed: %m");

    if (listen(listen_fd, 20) < 0)
        die("listen failed: %m");

    while (!done) {
        int client_fd = accept(listen_fd, NULL, NULL);
        if (client_fd < 0) {
            if (errno == EINTR)
                continue;
            die("accept failed: %m");
        }

        const pid_t pid = fork();
        if (pid == 0) { // child
            process_child(argc-1, argv+1, client_fd);
        } else {
            close(client_fd);
        }
    }

    close(listen_fd);
    unlink(SOCKET_NAME);
    return EXIT_SUCCESS;
}
