#include "aucontainer.h"

#include <sys/wait.h>
#include <sys/mount.h>
#include <sched.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <fcntl.h>

#include <iostream>
#include <exception>
#include <string>
#include <sstream>
#include <cstring>
#include <cerrno>

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

            cont_params(const options& opts, int in_pipe_fd, int out_pipe_fd)
            : opts(opts), in_pipe_fd(in_pipe_fd), out_pipe_fd(out_pipe_fd)
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

        void setup_networking(const char* ip)
        {
            (void) ip;
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
                setup_networking(opts.ip);
            }
            if (opts.cpu_perc != 100) {
                setup_cgroup(opts.cpu_perc);
            }
            if (opts.daemonize) {
                daemonize();
            }

            setup_fs(opts.fsimg_path);

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
        int from_cont_pipe_fds[2];
        if (pipe2(to_cont_pipe_fds, O_CLOEXEC) != 0 ||
            pipe2(from_cont_pipe_fds, O_CLOEXEC) != 0) {
            throw_err("Can't open pipes for IPC with container");
        }

        auto params = cont_params(opts, to_cont_pipe_fds[0], from_cont_pipe_fds[1]);
        auto cont_pid = clone(container_proc, container_stack + stack_size, 
                              CLONE_NEWPID | CLONE_NEWNET | CLONE_NEWNS | SIGCHLD, 
                              const_cast<void*>(reinterpret_cast<const void*>(&params)));
        if (cont_pid < 0) {
            throw_err("Can't run container procss");
        }

        std::cout << "CONTAINER PID = " << cont_pid << std::endl;

        waitpid(cont_pid, NULL, 0);
    }
}