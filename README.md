# Exec portal

```

                .----.      |          .----.
               /      \     |         /  __  \
               |    \\\\....^.........| /  \  |
 .-------------|   '----- argv[] ---> | | =----------------.
 { portal-exec |   ------ envp[] ---->| |== portal-spawner }
 '-------------|   .----open fds ---> | | =----------------'
               |   /////''''v'''''''''|  ''   |
                \.__./      |          \.__../
                            |
```

Run a program in another environment on the same machine by forwarding
arguments, environment variables and open file descriptors through a single
unix domain socket.

## Example:

Start the spawner like this, providing the full path to an executable:

```
$ ./portal-spawner /usr/games/cowsay
```

This will create a unix domain socket by default named `portal.sock` in the
local directory. As long as this socket is reachable by `portal-exec`, try
the following in another terminal:

```
$ ./portal-exec moo
 _____
< moo >
 -----
        \   ^__^
         \  (oo)\_______
            (__)\       )\/\
                ||----w |
                ||     ||
```

`stdin`, `stdout` and `stderr` and the `moo` argument and all environment
variables of `portal-exec` are transferred through `portal.sock` to the
spawner process. It forks a worker process and then finally runs the
specified executable `cowsay` with all args, envs and file descriptors set
up. The exit code or exit signals are transferred back to `portal-exec`.
As `cowsay` has the file descriptors originating from `portal-exec`, all
output appears in its terminal.

## Additional features:

You can also omit the executable when running `portal-spawner` or already
provide arguments to a specified program. The arguments of `portal-exec`
are appended to the arguments of `portal-spawner` putting together the full
`argv[]` and executable in `argv[0]` passed to `exeve`.

You can change the `portal.sock` filename by setting a `PORTAL_NAME` prior
to launching both `portal-spawner` and `portal-exec`. `PORTAL_NAME` itself
is not transferred from exec to spawner.

## Use cases:

The `portal-spawner` can run in a restricted environment or directly use
something like bubblewrap to set one up on each invocation. Right now this
is used in an experimental info-beamer package where arguments and
crucially an dmabuf fd is forwarded from one process to another running in
a squashfs overlay. As the overlay can remain active between multiple runs,
the filesystem cache doesn't get invalidated as would be the case if
overlay would be set up each run.
