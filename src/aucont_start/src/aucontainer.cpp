#include "aucontainer.h"

#include <sys/wait.h>
#include <sys/mount.h>
#include <linux/sched.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <type_traits>

#include <iostream>
#include <stdexcept>
#include <string>
#include <sstream>
#include <cstring>
#include <cerrno>
#include <cstdio>
#include <fstream>
#include <vector>

#include <aucont_common.h>

namespace aucont
{
    namespace
    {
        const size_t stack_size = 2 * 1024 * 1024; // 2 megabytes
        char container_stack[stack_size];

        struct cont_params
        {
            const options& opts;
            int in_pipe_fd;
            int out_pipe_fd;
            std::vector<int> fds_to_close;

            cont_params(const options& opts, int in_pipe_fd, int out_pipe_fd, std::initializer_list<int> const& fds_to_close)
            : opts(opts), in_pipe_fd(in_pipe_fd), out_pipe_fd(out_pipe_fd), fds_to_close(fds_to_close)
            {}
        };

        template<typename T>
        typename std::enable_if<std::is_pod<T>::value, T>::type read_from_pipe(int fd)
        {
            char buf[sizeof(T)];
            memset(buf, 0, sizeof(T));
            size_t offset = 0;
            while (offset != sizeof(T)) {
                ssize_t ret = read(fd, buf + offset, sizeof(T) - offset);
                if (ret < 0) {
                    stdlib_error("Can't read from pipe");
                }
                if (ret == 0) {
                    error("pipe read returned 0");
                }
                offset += ret;
            }
            return *reinterpret_cast<T*>(buf);
        }

        template<typename T>
        typename std::enable_if<std::is_pod<T>::value>::type write_to_pipe(int fd, T data)
        {
            void* buf = reinterpret_cast<void*>(&data);
            size_t count = sizeof(T);
            while (count > 0) {
                ssize_t ret = write(fd, buf, count);
                if (ret <= 0) {
                    stdlib_error("Can't write to pipe");
                }
                count -= ret;
                if (count > 0) {
                    buf = reinterpret_cast<void*>(reinterpret_cast<char*>(buf) + ret);
                }
            }
        }


        /**
         * Configure file system inside container
         * @param root path to new containers root folder (path in host fs)
         */
        void setup_fs(std::string root)
        {
            if (root[root.length() - 1] != '/') {
                root = root + "/";
            }
            const std::string p_root_dir_name = ".p_root";

            // recursively making all mount points private
            if (mount(NULL, "/", NULL, MS_PRIVATE | MS_REC, NULL) != 0) {
                stdlib_error("Can't make mount points private");
            }
            
            // mounting procfs
            std::string procfs_path = root + "proc";
            if (mount(NULL, procfs_path.c_str(), "proc", MS_NOSUID | MS_NOEXEC | MS_NODEV, NULL) != 0) {
                stdlib_error("Can't mount procfs to " + procfs_path);
            }
             
            std::string p_root = root + p_root_dir_name;
            struct stat st;
            if (stat(p_root.c_str(), &st) != 0) {
                if (mkdir(p_root.c_str(), 0777) != 0) {
                    stdlib_error("Can't create directory for old root");
                }
            }
            if (mount(root.c_str(), root.c_str(), "bind", MS_BIND | MS_REC, NULL) != 0) {
                stdlib_error("Mounting new root failed!");
            }
            if (syscall(SYS_pivot_root, root.c_str(), p_root.c_str()) != 0) {
                stdlib_error("Pivot root SYS call failed");
            }
            if (chdir("/") != 0) {
                stdlib_error("Can't change working directory");
            }
            std::string old_root_dir = "/" + p_root_dir_name;
            if (umount2(old_root_dir.c_str(), MNT_DETACH) != 0) {
                stdlib_error("Can't unmount old root");
            }
        }

        std::string get_cont_veth_name(pid_t container_pid)
        {
            std::stringstream ss;
            ss << "cont_" << container_pid << "_veth";
            return ss.str();
        }

        std::string get_host_veth_name(pid_t container_pid)
        {
            std::stringstream ss;
            ss << "host_" << container_pid << "_veth";
            return ss.str();   
        }

        /**
         * Configures container's side of networking interface
         * @param ip           container's process ip
         * @param host_pipe_fd read end of pipe to get veth device name from host
         */
        void setup_net_cont(std::string ip, pid_t cont_pid)
        {
            struct in_addr addr;
            inet_aton(ip.c_str(), &addr);
            std::string cont_ip(inet_ntoa(addr));
            cont_ip += "/24";

            std::string cont_veth_name = get_cont_veth_name(cont_pid);

            const size_t cmd_max_len = 300;
            char cmd[cmd_max_len];
            sprintf(cmd, "ip link set %s up", cont_veth_name.c_str());
            if (system(cmd) != 0) {
                error("Can't change cont veth state to UP");
            }

            sprintf(cmd, "ip addr add %s dev %s", cont_ip.c_str(), cont_veth_name.c_str());
            if (system(cmd) != 0) {
                error("Can't assign ip address to container");
            }
        }

        /**
         * Configure host's side of networking interface
         * @param cont_ip      ip a.b.c.d, which will be assigned to container; host ip will be a.b.c.(d+1)
         * @param cont_pid      container's process id
         */
        void setup_net_host(std::string cont_ip, pid_t cont_pid)
        {
            struct in_addr addr;
            inet_aton(cont_ip.c_str(), &addr);
            std::string host_ip(inet_ntoa(inet_makeaddr(inet_netof(addr), inet_lnaof(addr) + 1)));
            host_ip += "/24";
            std::string host_veth_name = get_host_veth_name(cont_pid);
            std::string cont_veth_name = get_cont_veth_name(cont_pid);

            const size_t cmd_max_len = 300;
            char cmd[cmd_max_len];
            sprintf(cmd, "ip link add %s type veth peer name %s", host_veth_name.c_str(), cont_veth_name.c_str());
            if (system(cmd) != 0) {
                error("Can't create veth pair");
            }

            sprintf(cmd, "ip link set %s netns %d", cont_veth_name.c_str(), cont_pid);
            if (system(cmd) != 0) {
                error("Can't set container veth");
            }

            sprintf(cmd, "ip link set %s up", host_veth_name.c_str());
            if (system(cmd) != 0) {
                error("Can't change host veth state to UP");
            }

            sprintf(cmd, "ip addr add %s dev %s", host_ip.c_str(), host_veth_name.c_str());
            if (system(cmd) != 0) {
                error("Can't assign ip to host veth");
            }
        }

        void setup_cgroup(std::string scripts_path, int cpu_perc, pid_t cont_pid)
        {
            const std::string script = "setup_cpu_cgroup.bash";

            std::stringstream cmdss;
            cmdss << "bash " << scripts_path << script << " " 
                  << cpu_perc << " " << cont_pid 
                  << " \"" << aucont::get_cgrouph_path() << "\""; // path to cgroup hierarchy root
            std::string cmd = cmdss.str();

            if (system(cmd.c_str()) != 0) {
                error("Can't setup cpu restrictions");
            }
        }

        void map_id(const char *file, uint32_t from, uint32_t to)
        {
            std::string str = std::to_string(from) + " " + std::to_string(to) + " 1";
            std::ofstream out(file);
            if (!out) {
                error("Can't open file " + std::string(file));
            }
            out << str;
        }

        void setup_user(uid_t uid, gid_t gid)
        {
            // comment stolen from unshare sources
            /* since Linux 3.19 unprivileged writing of /proc/self/gid_map
             * has s been disabled unless /proc/self/setgroups is written
             * first to permanently disable the ability to call setgroups
             * in that user namespace. */
            std::ofstream out("/proc/self/setgroups");
            out << "deny";
            out.close();
            map_id("/proc/self/uid_map", 0, uid);
            map_id("/proc/self/gid_map", 0, gid);
        }

        void setup_uts()
        {
            const std::string hostname = "container";
            if (sethostname(hostname.c_str(), hostname.length()) != 0) {
                stdlib_error("Can't set container hostname");
            }
        }

        /**
         * Daemonizes current process.
         * Calling process (caller) will terminate during function execution
         * `getpid()` after call not equal to `getpid()` before, because 
         * daemonized child process returns from this function (not caller)
         */
        void daemonize()
        {
            auto pid = fork();
            if (pid < 0) {
                stdlib_error("Can't daemonize");
            } else if (pid == 0) {
                exit(0);
            }
            if (setsid() < 0) {
                stdlib_error("Can't daemonize (setsid failed)");
            }
            // double forking not to be session leader (see `man 3 daemon`)
            pid = fork();
            if (pid < 0) {
                stdlib_error("Can't daemonize (second fork)");
            } else if (pid == 0) {
                exit(0);
            }
            if (chdir("/") < 0) {
                stdlib_error("Can't daemonize (chdir failed)");
            }
            int fd = open("/dev/null", O_RDWR, 0);
            if (fd != -1) {
                if (dup2(fd, STDIN_FILENO) < 0) stdlib_error("dup stdin");
                if (dup2(fd, STDOUT_FILENO) < 0) stdlib_error("dup stdout");
                if (dup2(fd, STDERR_FILENO) < 0) stdlib_error("dup stderr");
                if (fd > 2 && close(fd) < 0) {
                    stdlib_error("close fd err");
                }
            }
            umask(027);
        }

        /**
         * Container init process main procedure
         */
        int container_start_proc(void* arg)
        {
            auto params = *reinterpret_cast<const cont_params*>(arg);
            auto opts = params.opts;

            for (auto fd : params.fds_to_close) {
                if (close(fd) < 0) {
                    stdlib_error("can't cleanup fds in child process");
                }
            }
            
            // reading creators uid and gid from pipe
            uid_t uid = read_from_pipe<uid_t>(params.in_pipe_fd);
            gid_t gid = read_from_pipe<gid_t>(params.in_pipe_fd);
            setup_user(uid, gid);
            setup_uts();

            if (opts.daemonize) {
                daemonize();
            }

            // finally forking container's init process (in new PID namespace) TODO: can we avoid it somehow?
            if (unshare(CLONE_NEWPID) < 0) {
                stdlib_error("can't unshare pid ns");
            }

            int pipefd[2];
            if (pipe2(pipefd, O_CLOEXEC) != 0) {
                stdlib_error("Can't open pipe to transfer container pid");
            }
            pid_t pid = fork();
            if (pid < 0) {
                stdlib_error("Can't fork final container init process");
            } else if (pid > 0) {
                if (close(pipefd[0]) < 0 ||
                    close(params.in_pipe_fd) < 0 ||
                    close(params.out_pipe_fd) < 0) {
                    stdlib_error("fail closing fds");
                }
                write_to_pipe(pipefd[1], pid);
                if (close(pipefd[1]) < 0) stdlib_error("fail closing pipe fd");
                if (waitpid(pid, NULL, 0) < 0) {
                    stdlib_error("waitpid failed for pid " + std::to_string(pid));
                }
                exit(0);
            }

            // final container process continues here
            pid_t cont_pid = read_from_pipe<pid_t>(pipefd[0]);
            write_to_pipe(params.out_pipe_fd, cont_pid);

            if (!opts.ip.empty()) {
                read_from_pipe<bool>(params.in_pipe_fd);
                setup_net_cont(opts.ip, cont_pid);
            }
            setup_fs(opts.fsimg_path);

            // end configuring container
            write_to_pipe(params.out_pipe_fd, true);
            // waiting while host does all the stuff needed before command execution
            read_from_pipe<bool>(params.in_pipe_fd);

            if (close(pipefd[1]) < 0 ||
                close(pipefd[0]) < 0 ||
                close(params.in_pipe_fd) < 0 ||
                close(params.out_pipe_fd) < 0) {
                stdlib_error("Error cleaning up file descriptors");
            }

            // Running specified command inside container
            if (execvp(opts.cmd, const_cast<char* const *>(opts.args)) < 0) {
                stdlib_error("Can't run command in container");
            }
            return 0;
        }
    }

    void start_container(const options& opts, std::string exe_path)
    {
        /**
         * Setting up IPC for syncronization with container (child)
         * We need pipe to send stuff to container (name of virtual ethernet device, ...)
         */
        int to_cont_pipe_fds[2];
        int from_cont_pipe_fds[2];
        if (pipe2(to_cont_pipe_fds, O_CLOEXEC) != 0 ||
            pipe2(from_cont_pipe_fds, O_CLOEXEC) != 0) {
            stdlib_error("Can't open pipes for IPC with container");
        }

        auto params = cont_params(opts, to_cont_pipe_fds[0], from_cont_pipe_fds[1],
                                  { to_cont_pipe_fds[1], from_cont_pipe_fds[0] });
        auto pid = clone(container_start_proc, container_stack + stack_size, 
                              CLONE_NEWNET | CLONE_NEWNS | CLONE_NEWUTS | CLONE_NEWUSER | CLONE_NEWIPC | 
                              SIGCHLD, 
                              const_cast<void*>(reinterpret_cast<const void*>(&params)));
        if (pid < 0) {
            stdlib_error("Can't run container process");
        }
        close(to_cont_pipe_fds[0]);
        close(from_cont_pipe_fds[1]);

        // sending uid and gid
        write_to_pipe(to_cont_pipe_fds[1], geteuid());
        write_to_pipe(to_cont_pipe_fds[1], getegid());

        // waiting for container starting proc to send us container PID
        auto cont_pid = read_from_pipe<pid_t>(from_cont_pipe_fds[0]);

        // setting up networking if needed (host part)
        if (!opts.ip.empty()) {
            setup_net_host(opts.ip, cont_pid);
            // syncronizing with container; now container can setup it's network side
            write_to_pipe(to_cont_pipe_fds[1], true);
        }
        if (opts.cpu_perc != 100) {
            setup_cgroup(exe_path, opts.cpu_perc, cont_pid);
        }

        // waiting for container to be configured
        read_from_pipe<bool>(from_cont_pipe_fds[0]);
        close(from_cont_pipe_fds[0]);

        if (!add_container(container_t(cont_pid, opts.cpu_perc))) {
            error("Container with pid: " + std::to_string(cont_pid) + " is already running");
        }
        std::cout << cont_pid << std::endl;

        // container can proceed to command execution
        write_to_pipe(to_cont_pipe_fds[1], true);
        close(to_cont_pipe_fds[1]);

        // do not need to wait for daemon...
        if (opts.daemonize) {
            return;
        }

        if (wait(NULL) < 0) {
            stdlib_error("wait failed");
        }
        aucont::del_container(cont_pid);
    }
}