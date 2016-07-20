#include "aucontainer.h"

#include <sys/wait.h>
#include <sys/mount.h>
#include <sched.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <iostream>
#include <exception>
#include <string>
#include <sstream>
#include <cstring>
#include <cerrno>
#include <cstdio>

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

            cont_params(const options& opts, int in_pipe_fd)
            : opts(opts), in_pipe_fd(in_pipe_fd)
            {}
        };

        std::string form_error(std::string prefix) 
        {
            std::stringstream ss;
            ss << prefix << " [ " << strerror(errno) << " ]";
            return ss.str();
        }

        void throw_err(std::string prefix)
        {
            throw std::runtime_error(form_error(prefix));         
        }

        /**
         * Sets file system in container
         * @param root path to new containers root folder (path in host fs)
         */
        void setup_fs(std::string root)
        {
            const std::string p_root_dir_name = ".p_root";

            // recursively making all mount points private
            if (mount(NULL, "/", NULL, MS_PRIVATE | MS_REC, NULL) != 0) {
                throw_err("Can't make mount points private");
            }

            // mounting new root
            std::string p_root = root + ((root[root.length() - 1] == '/') ? "" : "/") + p_root_dir_name;
            std::cout << "pivot root = " << p_root << std::endl;
            struct stat st;
            if (stat(p_root.c_str(), &st) != 0) {
                if (mkdir(p_root.c_str(), 0777) != 0) {
                    throw_err("Can't create directory for old root");
                }
            }
            if (mount(root.c_str(), root.c_str(), "bind", MS_BIND | MS_REC, NULL) != 0) {
                throw_err("Mounting new root failed!");
            }
            if (syscall(SYS_pivot_root, root.c_str(), p_root.c_str()) != 0) {
                throw_err("Pivot root SYS call failed");
            }
            if (chdir("/") != 0) {
                throw_err("Can't change working directory");
            }
            std::string old_root_dir = "/" + p_root_dir_name;
            if (umount2(old_root_dir.c_str(), MNT_DETACH) != 0) {
                throw_err("Can't unmount old root");
            }

            // mounting procfs
            if (mount("ignored", "/proc", "proc", MS_NOSUID | MS_NOEXEC | MS_NODEV, NULL) != 0) {
                throw_err("Can't mount procfs");
            }
        }

        std::string get_cont_veth_name(pid_t pid)
        {
            std::stringstream ss;
            ss << "cont_" << pid << "_veth";
            return ss.str();
        }

        std::string get_host_veth_name(pid_t pid)
        {
            std::stringstream ss;
            ss << "host_" << pid << "_veth";
            return ss.str();   
        }

        /**
         * Configures container side of networking interface
         * @param ip           container's process ip
         * @param host_pipe_fd read end of pipe to get veth device name from host
         */
        void setup_net_cont(const char* ip, int host_pipe_fd)
        {
            struct in_addr addr;
            inet_aton(ip, &addr);
            std::string cont_ip(inet_ntoa(addr));
            cont_ip += "/24";

            pid_t pid;
            if (read(host_pipe_fd, reinterpret_cast<void*>(&pid), sizeof(pid)) != sizeof(pid)) {
                throw_err("Error reading from pipe in cont");
            }
            std::string cont_veth_name = get_cont_veth_name(pid);

            const size_t cmd_max_len = 300;
            char cmd[cmd_max_len];
            sprintf(cmd, "ip link set %s up", cont_veth_name.c_str());
            if (system(cmd) != 0) {
                throw_err("Can't change cont veth state to UP");
            }

            sprintf(cmd, "ip addr add %s dev %s", cont_ip.c_str(), cont_veth_name.c_str());
            if (system(cmd) != 0) {
                throw_err("Can't assign ip address to container");
            }
        }

        /**
         * Configure host side of networking interface
         * @param cont_id      container's process id
         * @param cont_ip      ip a.b.c.d, which will be assigned to container; host ip will be a.b.c.(d+1)
         * @param cont_pipe_fd write end of pipe between host and container
         */
        void setup_net_host(pid_t cont_id, const char* cont_ip, int cont_pipe_fd)
        {
            struct in_addr addr;
            inet_aton(cont_ip, &addr);
            std::string host_ip(inet_ntoa(inet_makeaddr(inet_netof(addr), inet_lnaof(addr) + 1)));
            host_ip += "/24";
            std::string host_veth_name = get_host_veth_name(cont_id);
            std::string cont_veth_name = get_cont_veth_name(cont_id);

            const size_t cmd_max_len = 300;
            char cmd[cmd_max_len];
            sprintf(cmd, "ip link add %s type veth peer name %s", host_veth_name.c_str(), cont_veth_name.c_str());
            if (system(cmd) != 0) {
                throw_err("Can't create veth pair");
            }

            sprintf(cmd, "ip link set %s netns %d", cont_veth_name.c_str(), cont_id);
            if (system(cmd) != 0) {
                throw_err("Can't set container veth");
            }

            sprintf(cmd, "ip link set %s up", host_veth_name.c_str());
            if (system(cmd) != 0) {
                throw_err("Can't change host veth state to UP");
            }

            sprintf(cmd, "ip addr add %s dev %s", host_ip.c_str(), host_veth_name.c_str());
            if (system(cmd) != 0) {
                throw_err("Can't assign ip to host veth");
            }

            // syncronizing with container; now container can setup it's network side
            if (write(cont_pipe_fd, reinterpret_cast<void*>(&cont_id), sizeof(pid_t)) < 0) {
                throw_err("pipe write error");
            }
        }

        void setup_cgroup(int cpu_perc)
        {
            (void) cpu_perc;
        }

        /**
         * Daemonizes current process.
         * Calling process (caller) will terminate during function execution
         * `getpid()` after call not equal to `getpid()` before, because 
         * daemonized child process returns from this function (not caller)
         */
        void daemonize()
        {

        }

        /**
         * Container init process main procedure
         */
        int container_proc(void* arg)
        {
            auto params = *reinterpret_cast<const cont_params*>(arg);
            auto opts = params.opts;

            if (opts.ip != nullptr) {
                setup_net_cont(opts.ip, params.in_pipe_fd);
            }

            setup_fs(opts.fsimg_path);

            if (opts.cpu_perc != 100) {
                setup_cgroup(opts.cpu_perc);
            }
            if (opts.daemonize) {
                daemonize();
            }

            // Running specified command inside container
            auto cmd_proc_pid = fork();
            if (cmd_proc_pid < 0) {
                throw_err("Can't create process for cmd execution inside container");
            }
            if (cmd_proc_pid == 0) { 
                // command execution process
                int result = execvp(opts.cmd, const_cast<char* const *>(opts.args));
                if (result < 0) {
                    throw_err("Can't run command in container");
                }
                return result;
            } else { 
                // container process
                waitpid(cmd_proc_pid, NULL, 0);
                return 0;
            }
        }
    }

    void start_container(const options& opts)
    {
        /**
         * Setting up IPC for syncronization with container (child)
         * We need pipe to send stuff to container (name of virtual ethernet device, ...)
         */
        int to_cont_pipe_fds[2];
        if (pipe2(to_cont_pipe_fds, O_CLOEXEC) != 0) {
            throw_err("Can't open pipes for IPC with container");
        }

        auto params = cont_params(opts, to_cont_pipe_fds[0]);
        auto cont_pid = clone(container_proc, container_stack + stack_size, 
                              CLONE_NEWPID | CLONE_NEWNET | CLONE_NEWNS | SIGCHLD, 
                              const_cast<void*>(reinterpret_cast<const void*>(&params)));
        if (cont_pid < 0) {
            throw_err("Can't run container process");
        }

        // setting up networking if needed (host part)
        if (opts.ip != nullptr) {
            setup_net_host(cont_pid, opts.ip, to_cont_pipe_fds[1]);
        }
        close(to_cont_pipe_fds[1]);

        std::cout << "CONTAINER PID = " << cont_pid << std::endl;
        waitpid(cont_pid, NULL, 0);
    }
}