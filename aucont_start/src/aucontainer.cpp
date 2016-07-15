#include "aucontainer.h"

#include <sys/wait.h>
#include <sys/mount.h>
#include <sched.h>
#include <unistd.h>

#include <iostream>
#include <exception>
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

        void mount_fs(const char* target_root)
        {
            std::cout << "Bind mounting " << target_root << " into /" << std::endl;
            auto res = mount(target_root, "/", "none", MS_BIND, NULL);
            if (res < 0) {
                throw std::runtime_error(form_error("Can't mount given FS Path as root direcotry"));
            }
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