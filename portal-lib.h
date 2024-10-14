#pragma once
#include "portal-shared.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    buf_t *env_buf;
    buf_t *arg_buf;

    int num_fds;
    int fd_map[MAX_FDS];
    int fds[MAX_FDS];

    int client_fd;
} portal_spawn_t;

typedef struct {
    int listen_fd;
} portal_listen_t;

void portal_setup(portal_listen_t *portal, const char *portal_name);
portal_spawn_t *portal_handle(portal_listen_t *portal);

int portal_spawn_fd(portal_spawn_t *spawn, int fd);

int portal_spawn_argc(portal_spawn_t *spawn);
const char *portal_spawn_argv(portal_spawn_t *spawn, int n);
const char *portal_spawn_getenv(portal_spawn_t *spawn, const char *key);

void portal_spawn_exit(portal_spawn_t *spawn, int status);

#ifdef __cplusplus
}
#endif
