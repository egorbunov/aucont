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
    auto cgroup_conf_script = exe_path + "apply_cgroup.bash";

    // reading arguments
    std::string cont_pid_str = std::string(argv[1]);
    char* cmd = argv[2];
    char* args[100]; // 100 - magic cmd arg maximum
    for (int j = 2; j < argc; ++j) {
        args[j - 2] = argv[j];
    }
    args[argc - 2] = NULL;

    // loading container info
    auto cont = aucont::get_container(stoi(cont_pid_str));
    if (cont.pid == -1) {
        aucont::error("No container running with pid (invalid pid) = " + cont_pid_str);
    }

    int synch_pipe[2];
    int get_slave_pid_pipe[2];
    if (pipe2(get_slave_pid_pipe, O_CLOEXEC) < 0 ||
        pipe2(synch_pipe, O_CLOEXEC) < 0) {
        aucont::stdlib_error("pipe");
    }

    auto tmp_pid = fork();
    if (tmp_pid < 0) {
        aucont::stdlib_error("fork");
    } else if (tmp_pid > 0) {
        close(get_slave_pid_pipe[1]);
        close(synch_pipe[0]);
        auto slave_proc_pid = aucont::read_from_pipe<pid_t>(get_slave_pid_pipe[0]);
        std::string pid_str = std::to_string(slave_proc_pid);
        std::string cgrouph_path = aucont::get_cgrouph_path();
        if (system(("bash " + cgroup_conf_script + " " + cont_pid_str + " " + pid_str + " " + cgrouph_path).c_str()) != 0) {
            aucont::error("=(");
        }
        aucont::write_to_pipe<bool>(synch_pipe[1], true);
        if (waitpid(tmp_pid, NULL, 0) < 0) {
            aucont::stdlib_error("Waitpid failed");
        }
        exit(1);
    }

    // applying user and pid namespace and forking first
    // user first because it sets proper permissions for next unshare
    unshare_ns("user", cont_pid_str); 
    unshare_ns("pid", cont_pid_str);

    if (pipe2(synch_pipe, O_CLOEXEC) != 0) {
        aucont::stdlib_error("Unable to open pipe");
    }

    auto slave_pid = fork();
    if (slave_pid < 0) {
        aucont::stdlib_error("Fork failed");
    } else if (slave_pid > 0) {
        // TODO: close pipes
        aucont::write_to_pipe(get_slave_pid_pipe[1], slave_pid);
        if (waitpid(slave_pid, NULL, 0) < 0) {
            aucont::stdlib_error("Waitpid failed");
        }
        exit(0);
    }
    // TODO: close pipes

    // applying other namespaces
    std::array<std::string, 4> nss = {"net", "ipc", "uts", "mnt"}; // order is crucial
    for (auto ns : nss) {
        unshare_ns(ns, cont_pid_str);
    }

    // synch...
    aucont::read_from_pipe<bool>(synch_pipe[0]);

    if (execvp(cmd, args) < 0) {
        aucont::stdlib_error("exec failed");
    }
    return 0;
}
