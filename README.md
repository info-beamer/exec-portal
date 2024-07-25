# Exec portal

```

             .----.      |          .----.
            /      \     |         /  __  \
            |    \\\\....^.........| /  \  |
   .--------|   '----- argv[] ---> | | =--------.
   | client |   ------ envp[] ---->| |== server |
   '--------|   .----open fds ---> | | =--------'
            |   /////''''v'''''''''|  ''   |
             \.__./      |          \.__../
                         |
```

Run a program in another environment on the same machine
by forwarding arguments, environment variables and open
file descriptors through a single unix domain socket.

## Example:

Start the server like this, providing the full path to an executable:

```
$ ./server /usr/games/cowsay 
```

This will create a unix domain socket by default named
`server.sock` in the local directory. As long as this socket
is reachable by `client`, try the following in another
terminal:

```
$ ./client moo
 _____
< moo >
 -----
        \   ^__^
         \  (oo)\_______
            (__)\       )\/\
                ||----w |
                ||     ||
```

`stdin`, `stdout` and `stderr` and the `moo` argument and all environment variables of
`client` are transferred through `server.sock` to the server process. It forks a worker process
and then finally runs the specified executable `cowsay` with all args, envs and
file descriptors set up. The exit code or exit signals are transferred back to `client`.
As `cowsay` has the file descriptors originating from `client`, all output appears
in its terminal.

## Use cases:

The `server` can run in a restricted environment or directly use something like bubblewrap
to set one up on each invocation. Right now this is used in an experimental info-beamer
package where arguments and crucially an dmabuf fd is forwarded from one process to another
running in a squashfs overlay. As the overlay can remain active between multiple runs, the
filesystem cache doesn't get invalidated as would be the case if overlay would be set up
each run.
