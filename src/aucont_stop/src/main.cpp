#include <iostream>
#include <sstream>
#include <string>
#include <stdexcept>
#include <aucont_common.h>
#include <sys/types.h>
#include <csignal>
#include <cstring>
#include <cerrno>

namespace
{
    void print_usage() {
        std::cout << "USAGE: ./aucont_stop PID [SIGNUM]" << std::endl;
    }
}

int main(int argc, char* argv[]) {
    aucont::set_aucont_root(aucont::get_file_real_dir(argv[0]));

    if (argc < 2 || argc > 3) {
        print_usage();
        exit(1);
    }

    pid_t pid = atoi(argv[1]);
    int signum = SIGTERM;
    if (argc == 3) {
        signum = atoi(argv[2]);
    }

    auto pids = aucont::get_containers_pids();
    if (pids.find(pid) == pids.end()) {
        aucont::error("No container with pid [ " + std::to_string(pid) + " ]");
    }

    if (kill(pid, signum) < 0) {
        aucont::stdlib_error("Can't send signal");
    }

    return 0;
}
