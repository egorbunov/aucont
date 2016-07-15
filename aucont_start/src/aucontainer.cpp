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

        void mount_fs(std::string root)
        {
            // recursively making all mount points private
            if (mount("ignored", "/", NULL, MS_PRIVATE | MS_REC, NULL) != 0) {
                throw std::runtime_error(form_error("Can't make mount points private"));
            }

            // Changing root
            std::string p_root = root + ((root[root.length() - 1] == '/') ? ".p_root" : "/.p_root/");
            std::cout << "pivot root = " << p_root << std::endl; 
            if (mkdir(p_root.c_str(), 0777) != 0 ||
                mount(root.c_str(), root.c_str(), "bind", MS_BIND | MS_REC, NULL) != 0 ||
                syscall(SYS_pivot_root, root.c_str(), p_root.c_str()) != 0 ||
                chdir("/") != 0 ||
                umount2(p_root.c_str(), MNT_DETACH) != 0) {
                throw std::runtime_error(form_error("Can't change root to " + root));
            }

            std::cout << "Mounting procfs" << std::endl;
            // 1) remount /proc because to disable mount/unmount events propagation to parent mount ns
            // 2) mount procfs!
            if (mount("ignored", "/proc", NULL, MS_PRIVATE | MS_REC, NULL) != 0 ||
                mount("ignored", "/proc", "proc", MS_NOSUID | MS_NOEXEC | MS_NODEV, NULL) != 0) {
                throw std::runtime_error(form_error("Can't mount procfs"));
            }

            (void) root;
        }

        /**
         * Container init process main procedure
         */
        int container_proc(void* arg)
        {
            const options& opts = *reinterpret_cast<const options*>(arg);

            mount_fs(opts.fsimg_path);

            // Running specified command
            auto cmd_proc_pid = fork();
            if (cmd_proc_pid < 0) {
                throw std::runtime_error(form_error("Can't create process for cmd execution inside container"));
            } else if (cmd_proc_pid == 0) { 
                // command execution process
                int result = execvp(opts.cmd, const_cast<char* const *>(opts.args));
                if (result < 0) {
                    throw std::runtime_error(form_error("Can't run command in container"));
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
            throw std::runtime_error(form_error("Can't run container procss"));
        }

        std::cout << "CONTAINER PID = " << cont_pid << std::endl;

        waitpid(cont_pid, NULL, 0);
    }
}