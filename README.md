# aucont

Set of utils (for linux) that provide container creation (very^10 simplified docker).

## build

    ./build.sh

Everything will be under `./bin` directory in case of successful build.
All `aucont_*` tools (and additional scripts, that appear in `./bin`) must be under the same path while using aucont utilities.

## howto

    $ ./aucont_start --net 10.0.0.1 --cpu 50 -d /path/to/rootfs/ sleep 1000
    5224

That command should start container with it's own pid, mount, net,... namespaces; container ip will be `10.0.0.1` and any command running inside container may only use `50` percent of cpu time. Also, due to `-d` option container will start as a linux daemon (with no attached tty's and all that).
`5224` is container id (actually it's just pid) printed by `./aucont_start`

    $ ./aucont_list 
    4908
    5052
    5224

You can run `aucont_list` to see ids of all running containers. `5224` is here, huh!

    $ ./aucont_exec 5224 ps a
    PID   USER     COMMAND
        1 root     sleep 1000
        9 root     ps a

`aucont_exec` takes 2 arguments: containers id and command (with it's own args). So it executes given command *inside* container with given id. As u can see from `ps a` output where is only to processes running inside container and `sleep 1000` is the `init` process.

    $ ./aucont_stop 5224 9

Now we are done with our container, so lets kill it. Command above sends signal `9` to container with id `5224`. `9` here stands for `SIGKILL`. To see other signal values and their meaning look at `man 7 signal` page.

## test

```bash
cd test
./run_test.sh
```

1. You will be asked to provide your root password to build docker container, where all tests run
2. You will be asked to write docker container's root password to do some aucont privileged stuff. Password is `111` as specified in `container/Dockerfile`
3. As last test stage `sh` shell prompt will show up, that `sh` shell runs inside container, actually, it is just aucont started with command: `./aucont_start /path/to/rootfs /bin/sh`. So you can manually check isolated environment.
