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
    auto script = exe_path + "aucont_exec.bash";

    // reading arguments
    std::string pid_str = std::string(argv[1]);
    char* cmd = argv[2];
    char* args[100]; // 100 - magic cmd arg maximum
    for (int j = 2; j < argc; ++j) {
        args[j - 2] = argv[j];
    }
    args[argc - 2] = NULL;

    // loading container info
    auto cont = aucont::get_container(stoi(pid_str));
    if (cont.pid == -1) {
        aucont::error("No container running with pid (invalid pid) = " + pid_str);
    }


    // applying user and pid namespace and forking first
    // user first because it sets proper permissions for next unshare
    unshare_ns("user", pid_str); 
    unshare_ns("pid", pid_str);

    int pipefd[2];
    if (pipe2(pipefd, O_CLOEXEC) != 0) {
        aucont::stdlib_error("Unable to open pipe");
    }

    auto pid = fork();
    if (pid < 0) {
        aucont::stdlib_error("Fork failed");
    } else if (pid > 0) {
        if (close(pipefd[0]) != 0) aucont::stdlib_error("close pipe");

        // configuring cgroup for child
        
        // synch with child
        aucont::write_to_pipe(pipefd[1], true);
        if (close(pipefd[1]) != 0) aucont::stdlib_error("close pipe");
        if (waitpid(pid, NULL, 0) < 0) {
            aucont::stdlib_error("Waitpid failed");
        }
        exit(0);
    }
    if (close(pipefd[1]) != 0) aucont::stdlib_error("close pipe");

    // applying other namespaces
    std::array<std::string, 4> nss = {"net", "ipc", "uts", "mnt"}; // order is crucial
    for (auto ns : nss) {
        unshare_ns(ns, pid_str);
    }

    // synch...
    aucont::read_from_pipe<bool>(pipefd[0]);
    if (close(pipefd[0]) != 0) aucont::stdlib_error("close pipe");

    if (execvp(cmd, args) < 0) {
        aucont::stdlib_error("exec failed");
    }

    return 0;
}
