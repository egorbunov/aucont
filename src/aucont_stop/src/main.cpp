#include <iostream>
#include <sstream>
#include <string>
#include <stdexcept>
#include <aucont_file.h>
#include <sys/types.h>
#include <signal.h>
#include <cstring>
#include <cerrno>

void print_usage() {
    std::cout << "USAGE: ./aucont_stop PID [SIGNUM]" << std::endl;
}

int main(int argc, char* argv[]) {
    char path_to_exe[1000];
    realpath(argv[0], path_to_exe);
    auto exe_path = std::string(path_to_exe);
    exe_path = exe_path.substr(0, exe_path.find_last_of("/")) + "/";
    // preparing aucont common resources path
    aucont::set_aucont_root(exe_path);

    if (argc < 2 || argc > 3) {
        print_usage();
        return 0;
    }

    pid_t pid = atoi(argv[1]);
    int signum = SIGTERM;
    if (argc == 3) {
        signum = atoi(argv[2]);
    }

    auto pids = aucont::get_containers_pids();
    if (pids.find(pid) != pids.end() && kill(pid, 0) < 0 && errno == ESRCH) {
        aucont::del_container_pid(pid);
        return 0;
    } 

    std::cout << "SIGNAL = " << signum << std::endl;

    if (kill(pid, signum) < 0) {
        throw std::runtime_error("Can't send signal [ " + std::string(strerror(errno)) + " ] ");
    }

    std::cout << pid << std::endl;
    
    return 0;
}
