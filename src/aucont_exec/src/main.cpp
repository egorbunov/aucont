#include <iostream>
#include <sstream>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <cstring>
#include <aucont_file.h>
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
}


int main(int argc, char* argv[]) {
    if (argc < 3) {
        print_usage();
    }
    char path_to_exe[1000];
    realpath(argv[0], path_to_exe);
    auto exe_path = std::string(path_to_exe);
    exe_path = exe_path.substr(0, exe_path.find_last_of("/")) + "/";
    aucont::set_aucont_root(exe_path);

    auto script = exe_path + "aucont_exec.bash";

    // reading arguments
    std::string pids = std::string(argv[1]);
    std::string cmd = "";
    for (int j = 2; j < argc; ++j) {
        cmd += std::string(argv[j]);
        if (j != argc - 1) cmd += " ";
    }

    // applying namespaces
    // WARN: user ns must be first, mnt last
    std::array<std::string, 6> nss = {"user", "net", "ipc", "uts", "pid", "mnt"};
    for (auto ns : nss) {
        auto f = "/proc/" + pids + "/ns/" + ns;
        int fd = open(f.c_str(), O_RDONLY);
        if (fd < 0) {
            throw_err("Can't open ns fd [ " + f + " ]");
        }
        if (setns(fd, 0) < 0) {
            throw_err("Can't set ns: " + ns);
        }
        close(fd);
    }

    auto pid = fork();
    if (pid < 0) {
        throw_err("Fork =(");
    } else if (pid > 0) {
        if (waitpid(pid, NULL, 0) < 0) {
            throw_err("Waitpid =(");
        }
        return 0;
    }

    // running command
    system(cmd.c_str());

    return 0;
}
