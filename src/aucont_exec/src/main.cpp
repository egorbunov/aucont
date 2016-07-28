#include <iostream>
#include <sstream>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <cstring>
#include <aucont_common.h>
#include <string>
#include <fcntl.h>
#include <sys/syscall.h>
#include <sched.h>
#include <array>
#include <sys/wait.h>
#include <fstream>

namespace 
{
    void print_usage() 
    {
        std::cout << "usage: ./aucont_exec PID CMD [ARGS]" << std::endl;
        std::cout << "    PID - container init process pid in its parent PID namespace" << std::endl;
        std::cout << "    CMD - command to run inside container" << std::endl;
        std::cout << "    ARGS - arguments for CMD" << std::endl;
    }

    void unshare_ns(std::string ns, std::string pid_str)
    {
        auto f = "/proc/" + pid_str + "/ns/" + ns;
        int fd = open(f.c_str(), O_RDONLY);
        if (fd < 0) {
            aucont::stdlib_error("Can't open ns fd [ " + f + " ]");
        }
        if (setns(fd, 0) < 0) {
            close(fd);
            aucont::stdlib_error("Can't set ns: " + ns);
        }
        close(fd);
    }
}


int main(int argc, char* argv[]) {
    if (argc < 3) {
        print_usage();
    }
    auto exe_path = aucont::get_file_real_dir(argv[0]);
    aucont::set_aucont_root(exe_path);

    std::string cont_pid_str = std::string(argv[1]);
    // loading container info
    auto cont = aucont::get_container(stoi(cont_pid_str));
    if (cont.pid == -1) {
        aucont::error("No container running with pid (invalid pid) = " + cont_pid_str);
    }

    int synch_pipe[2];
    if (pipe2(synch_pipe, O_CLOEXEC) < 0) {
        aucont::stdlib_error("pipe");
    }

    // applying user and pid namespace and forking first
    // unsharing user first because it sets proper permissions for next unshare
    unshare_ns("user", cont_pid_str); 
    unshare_ns("pid", cont_pid_str);

    auto cmd_pid = fork();
    if (cmd_pid < 0) {
        aucont::stdlib_error("Fork failed");
    } else if (cmd_pid > 0) {
        close(synch_pipe[0]);

        // setting up cgroup if needed
        if (cont.cpu_perc != 100) {
            auto cg_tasks_file = aucont::get_cgrouph_path() + "/" + aucont::get_cgroup_for_cpuperc(cont.cpu_perc) + "/tasks";
            std::ofstream out(cg_tasks_file, std::ios_base::out | std::ios_base::app);
            out << cmd_pid;
            out.close();
        }
        aucont::write_to_pipe(synch_pipe[1], true);
        close(synch_pipe[1]);
        
        if (waitpid(cmd_pid, NULL, 0) < 0) {
            aucont::stdlib_error("Waitpid failed");
        }
        exit(0);
    }
    close(synch_pipe[1]);

    unshare_ns("net", cont_pid_str);
    unshare_ns("ipc", cont_pid_str);
    unshare_ns("uts", cont_pid_str);
    unshare_ns("mnt", cont_pid_str);

    // waiting for cgroup to be configured 
    aucont::read_from_pipe<bool>(synch_pipe[0]);
    close(synch_pipe[0]);

    const int max_arg_num = 100;
    char* cmd = argv[2];
    char* args[max_arg_num];
    for (int j = 2; j < argc; ++j) {
        args[j - 2] = argv[j];
    }
    args[argc - 2] = NULL;
    if (execvp(cmd, args) < 0) {
        aucont::stdlib_error("exec failed");
    }
    return 0;
}
