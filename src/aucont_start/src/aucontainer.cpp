#include "aucontainer.h"

#include <fstream>
#include <vector>
#include <tuple>
#include <utility>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sstream>

#include <cstring>
#include <cerrno>
#include <cstdio>

#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <sys/stat.h>
#include <linux/sched.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <aucont_common.h>

namespace aucont
{
    using std::string;
    using std::stringstream;
    using std::vector;

    namespace
    {
        const size_t stack_size = 2 * 1024 * 1024; // 2 megabytes
        char container_stack[stack_size];

        struct cont_params
        {
            const options& opts;
            int in_pipe_fd;
            int out_pipe_fd;
            vector<int> fds_to_close;
            string scripts_path;

            cont_params(const options& opts, int in_pipe_fd, int out_pipe_fd, vector<int> fds_to_close, 
                        string scripts_path)
            : opts(opts), in_pipe_fd(in_pipe_fd), out_pipe_fd(out_pipe_fd), 
              fds_to_close(std::move(fds_to_close)), scripts_path(scripts_path)
            {}
        };

        /**
         * Configures filesystem of container from host side
         * @param path to container root directory
         */
        void setup_fs_host(string cont_root)
        {
            (void) cont_root;
            // stringstream cmdss;
            // cmdss << "chown -R " << geteuid() << ":" << getegid() << " " << cont_root;
            // auto cmd = cmdss.str();
            // if (system(cmd.c_str()) != 0) {
            //     aucont::error("Can't setup container root dir permissions");
            // }
        }

        /**
         * Configure file system inside container
         * @param root path to new containers root folder (path in host fs)
         */
        void setup_fs(string root)
        {
            if (root[root.length() - 1] != '/') {
                root = root + "/";
            }
            const string p_root_dir_name = ".p_root";

            // recursively making all mount points private
            if (mount(NULL, "/", NULL, MS_PRIVATE | MS_REC, NULL) != 0) {
                stdlib_error("Can't make mount points private");
            }
            
            // mounting procfs
            string procfs_path = root + "proc";
            if (mount(NULL, procfs_path.c_str(), "proc", MS_NOSUID | MS_NOEXEC | MS_NODEV, NULL) != 0) {
                stdlib_error("Can't mount procfs to " + procfs_path);
            }

            // mounting sysfs
            string sysfs_path = root + "sys";
            if (mount(NULL, sysfs_path.c_str(), "sysfs", MS_NOSUID | MS_NOEXEC | MS_NODEV, NULL) != 0) {
                stdlib_error("Can't mount sysfs to " + sysfs_path);
            }

            // creating special device files
            vector<std::pair<string, bool>> devices = { std::make_pair("dev/zero", false), 
                                                                  std::make_pair("dev/null", false), 
                                                                  std::make_pair("dev/mqueue", true),
                                                                  std::make_pair("dev/shm", true) };
            for (auto dev : devices) {
                string from = "/" + dev.first;
                string to = root + dev.first;
                if (!dev.second) { // not a directory
                    auto dfd = open(to.c_str(), O_CREAT | O_RDWR, 0777);
                    if (dfd < 0 ||
                        close(dfd) < 0) {
                        stdlib_error("Can't create/open file " + to);
                    }
                } else { // directory
                    if (mkdir(to.c_str(), 0666) < 0 && errno != EEXIST) {
                        stdlib_error("Can't create dir " + to);
                    }
                }
                if (mount(from.c_str(), to.c_str(), "", MS_BIND, NULL) != 0) {
                    stdlib_error("Can't bind device " + dev.first);
                }
            }
             
            // changing root
            string p_root = root + p_root_dir_name;
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
            string old_root_dir = "/" + p_root_dir_name;
            if (umount2(old_root_dir.c_str(), MNT_DETACH) != 0) {
                stdlib_error("Can't unmount old root");
            }
        }

        string get_cont_veth_name(pid_t container_pid)
        {
            stringstream ss;
            ss << "cont_" << container_pid << "_veth";
            return ss.str();
        }

        string get_host_veth_name(pid_t container_pid)
        {
            stringstream ss;
            ss << "host_" << container_pid << "_veth";
            return ss.str();   
        }

        string get_host_ip(string cont_ip)
        {
            struct in_addr addr;
            inet_aton(cont_ip.c_str(), &addr);
            return inet_ntoa(inet_makeaddr(inet_netof(addr), inet_lnaof(addr) + 1));
        }

        /**
         * Configures container's side of networking interface
         * @param cont_ip           container's process ip
         * @param host_pipe_fd read end of pipe to get veth device name from host
         */
        void setup_net_cont(string scripts_path, string cont_ip, pid_t cont_pid)
        {
            const string script = scripts_path + "setup_net_cont.sh";

            if (sysrun(script, get_cont_veth_name(cont_pid), cont_ip, get_host_ip(cont_ip)) < 0) {
                error("Can't setup networking (from container)");
            }
        }

        /**
         * Configure host's side of networking interface
         * @param cont_ip      ip a.b.c.d, which will be assigned to container; host ip will be a.b.c.(d+1)
         * @param cont_pid      container's process id
         */
        void setup_net_host(string scripts_path, string cont_ip, pid_t cont_pid)
        {
            const string script = scripts_path + "setup_net_host.sh";


            if (sysrun(script, cont_pid, get_host_veth_name(cont_pid), get_cont_veth_name(cont_pid), 
                        get_host_ip(cont_ip)) != 0) {
                error("Can't setup networking (from host)");
            }
        }

        void setup_cgroup(string scripts_path, int cpu_perc, pid_t cont_pid)
        {
            const string script = scripts_path + "setup_cpu_cgroup.sh";
            if (sysrun(script, cpu_perc, cont_pid, get_cgrouph_path(), get_cgroup_for_cpuperc(cpu_perc)) != 0) {
                error("Can't setup cpu restrictions");
            }
        }

        void map_id(string file, vector<std::tuple<uid_t, uid_t, uid_t>> mappings)
        {
            std::ofstream out(file);
            if (!out) {
                error("Can't open file " + file);
            }
            for (auto m : mappings) {
                out << std::get<0>(m) << " " << std::get<1>(m) << " " << std::get<2>(m) << std::endl;
            }
        }

        /**
         * called from host
         */
        void setup_user_in_container(pid_t cont_pid)
        {
            // comment stolen from unshare sources
            /* since Linux 3.19 unprivileged writing of /proc/self/gid_map
             * has s been disabled unless /proc/self/setgroups is written
             * first to permanently disable the ability to call setgroups
             * in that user namespace. */
            string cont_pid_str = std::to_string(cont_pid);
            std::ofstream out("/proc/" + cont_pid_str + "/setgroups");
            out << "deny";
            out.close();
            map_id("/proc/" + cont_pid_str + "/uid_map", { std::make_tuple(0, geteuid(), 1) });
            map_id("/proc/" + cont_pid_str + "/gid_map", { std::make_tuple(0, getegid(), 1) });
        }

        void setup_uts()
        {
            const string hostname = "container";
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
                write_to_pipe(pipefd[1], pid); // sending container pid to container (as seen from host)
                if (close(pipefd[1]) < 0) stdlib_error("fail closing pipe fd");
                if (waitpid(pid, NULL, 0) < 0) {
                    stdlib_error("waitpid failed for pid " + std::to_string(pid));
                }
                exit(0);
            }
            // final container process continues here
            pid_t cont_pid = read_from_pipe<pid_t>(pipefd[0]);
            // sending container pid to host
            write_to_pipe(params.out_pipe_fd, cont_pid);
            // wait for host configures user mappings for container
            read_from_pipe<bool>(params.in_pipe_fd);
            setup_uts();
            if (!opts.ip.empty()) {
                read_from_pipe<bool>(params.in_pipe_fd);
                setup_net_cont(params.scripts_path, opts.ip, cont_pid);
            }
            // filesystem configuration must be the very last
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

    void start_container(const options& opts, string exe_path)
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

        // configuring future container's root
        setup_fs_host(opts.fsimg_path);

        auto params = cont_params(opts, to_cont_pipe_fds[0], from_cont_pipe_fds[1],
                                  { to_cont_pipe_fds[1], from_cont_pipe_fds[0] }, exe_path);
        auto pid = clone(container_start_proc, container_stack + stack_size, 
                              CLONE_NEWNET | CLONE_NEWNS | CLONE_NEWUTS | CLONE_NEWUSER | CLONE_NEWIPC | 
                              SIGCHLD, 
                              const_cast<void*>(reinterpret_cast<const void*>(&params)));
        if (pid < 0) {
            stdlib_error("Can't run container process");
        }
        close(to_cont_pipe_fds[0]);
        close(from_cont_pipe_fds[1]);

        // waiting for container starting proc to send us container PID
        auto cont_pid = read_from_pipe<pid_t>(from_cont_pipe_fds[0]);

        // setting up user
        setup_user_in_container(cont_pid);
        write_to_pipe(to_cont_pipe_fds[1], true); // synch

        // setting up networking if needed (host part)
        if (!opts.ip.empty()) {
            setup_net_host(exe_path, opts.ip, cont_pid);
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

        if (opts.daemonize) {
            return;
        }

        if (wait(NULL) < 0) {
            stdlib_error("wait failed");
        }
        aucont::del_container(cont_pid);
    }
}