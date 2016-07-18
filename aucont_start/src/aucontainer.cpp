#include "aucontainer.h"

#include <sys/wait.h>
#include <sys/mount.h>
#include <sched.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/syscall.h>

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

            // Mounting new root
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

        }

        void setup_cgroup(int cpu_perc)
        {

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
            const options& opts = *reinterpret_cast<const options*>(arg);

            setup_fs(opts.fsimg_path);
            if (arg.ip != nullptr) {
                setup_networking(arg.ip);
            }
            if (arg.cpu_perc != 100) {
                setup_cgroup(cpu_perc);
            }
            if (arg.daemonize) {
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
        // spawning container process with it's own PID, MNT and NET namespaces
        auto cont_pid = clone(container_proc, container_stack + stack_size, 
                              CLONE_NEWPID | CLONE_NEWNS | CLONE_NEWNET | SIGCHLD, 
                              const_cast<void*>(reinterpret_cast<const void*>(&opts)));
        if (cont_pid < 0) {
            throw_err("Can't run container procss");
        }

        std::cout << "CONTAINER PID = " << cont_pid << std::endl;

        waitpid(cont_pid, NULL, 0);
    }
}