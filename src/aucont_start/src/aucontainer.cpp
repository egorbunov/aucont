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
#include <exception>
#include <string>
#include <sstream>
#include <cstring>
#include <cerrno>
#include <cstdio>
#include <fstream>

#include <aucont_file.h>

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

        template<typename T>
        typename std::enable_if<std::is_pod<T>::value, T>::type read_from_pipe(int fd)
        {
            char buf[sizeof(T)];
            memset(buf, 0, sizeof(T));
            size_t offset = 0;
            while (offset != sizeof(T)) {
                ssize_t ret = read(fd, buf + offset, sizeof(T) - offset);
                if (ret <= 0) {
                    throw_err("Can't read from pipe");
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
                    throw_err("Can't write to pipe");
                }
                count -= ret;
                if (count > 0) {
                    buf = reinterpret_cast<void*>(reinterpret_cast<char*>(buf) + ret);
                }
            }
        }


        /**
         * Sets file system in container
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
                throw_err("Can't make mount points private");
            }
            
            // mounting procfs
            std::string procfs_path = root + "proc";
            if (mount(NULL, procfs_path.c_str(), "proc", MS_NOSUID | MS_NOEXEC | MS_NODEV, NULL) != 0) {
                throw_err("Can't mount procfs");
            }
             
            // mounting new root
            std::string p_root = root + p_root_dir_name;
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
        void setup_net_cont(const char* ip, pid_t cont_pid)
        {
            struct in_addr addr;
            inet_aton(ip, &addr);
            std::string cont_ip(inet_ntoa(addr));
            cont_ip += "/24";

            std::string cont_veth_name = get_cont_veth_name(cont_pid);

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
         */
        void setup_net_host(pid_t cont_id, const char* cont_ip)
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
                throw_err("Can't setup cpu restrictions");
            }
        }

        void map_id(const char *file, uint32_t from, uint32_t to)
        {
            std::string str = std::to_string(from) + " " + std::to_string(to) + " 1";
            std::ofstream out(file);
            if (!out) {
                throw_err("Can't open file " + std::string(file));
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
                throw_err("Can't set container hostname");
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

        }

        /**
         * Container init process main procedure
         */
        int container_proc(void* arg)
        {
            auto params = *reinterpret_cast<const cont_params*>(arg);
            auto opts = params.opts;
            
            // reading creators uid and gid from pipe
            uid_t uid = read_from_pipe<uid_t>(params.in_pipe_fd);
            gid_t gid = read_from_pipe<gid_t>(params.in_pipe_fd);

            setup_user(uid, gid);
            setup_uts();

            if (opts.ip != nullptr) {
                setup_net_cont(opts.ip, read_from_pipe<pid_t>(params.in_pipe_fd));
            }

            setup_fs(opts.fsimg_path);

            if (opts.daemonize) {
                daemonize();
            }

            // end configuring container
            write_to_pipe(params.out_pipe_fd, true);

            // waiting while host configures cgroups 
            // and other stuff, that must be configured before running 
            // command
            read_from_pipe<bool>(params.in_pipe_fd);
            close(params.in_pipe_fd);
            close(params.out_pipe_fd);

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
                if (waitpid(cmd_proc_pid, NULL, 0) < 0) {
                    throw_err("Waitpid faield");
                }
                return 0;
            }
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
            throw_err("Can't open pipes for IPC with container");
        }

        auto params = cont_params(opts, to_cont_pipe_fds[0], from_cont_pipe_fds[1]);
        auto cont_pid = clone(container_proc, container_stack + stack_size, 
                              CLONE_NEWPID | CLONE_NEWNET | CLONE_NEWNS | 
                              CLONE_NEWUTS | CLONE_NEWUSER | CLONE_NEWIPC | 
                              // CLONE_NEWCGROUP | // It seems to disable cpu restrictions =()
                              SIGCHLD, 
                              const_cast<void*>(reinterpret_cast<const void*>(&params)));
        if (cont_pid < 0) {
            throw_err("Can't run container process");
        }
        close(to_cont_pipe_fds[0]);
        close(from_cont_pipe_fds[1]);

        // sending uid and gid
        write_to_pipe(to_cont_pipe_fds[1], geteuid());
        write_to_pipe(to_cont_pipe_fds[1], getegid());

        // setting up networking if needed (host part)
        if (opts.ip != nullptr) {
            setup_net_host(cont_pid, opts.ip);
            // syncronizing with container; now container can setup it's network side
            write_to_pipe(to_cont_pipe_fds[1], cont_pid);
        }
        if (opts.cpu_perc != 100) {
            setup_cgroup(exe_path, opts.cpu_perc, cont_pid);
        }

        // waiting for container to be configured
        read_from_pipe<bool>(from_cont_pipe_fds[0]);
        close(from_cont_pipe_fds[0]);

        // printing containers pid
        if (!aucont::add_container_pid(cont_pid)) {
            throw std::runtime_error("Conatiner with such pid is already running");
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
            throw_err("Wait failed");
        }
        aucont::del_container_pid(cont_pid);
    }
}