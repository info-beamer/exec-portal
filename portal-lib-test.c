#include <unistd.h>

#include "portal-lib.h"

int main(int argc, char *argv[]) {
    portal_listen_t listener;
    portal_setup(&listener, PORTAL_NAME);
    while (1) {
        portal_spawn_t *spawn = portal_handle(&listener);
        if (spawn) {
            printf("%d %s %s\n",
                portal_spawn_argc(spawn),
                portal_spawn_argv(spawn, 0),
                portal_spawn_getenv(spawn, "PATH")
            );
            write(portal_spawn_fd(spawn, 1), "hello\n", 6);
            portal_spawn_exit(spawn, 1);
        }
        sleep(1);
    }
    return 0;
}
