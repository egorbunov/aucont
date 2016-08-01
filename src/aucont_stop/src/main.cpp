#include <iostream>
#include <sstream>
#include <string>
#include <stdexcept>

#include <csignal>
#include <cstring>
#include <cerrno>

#include <sys/types.h>

#include <aucont_common.h>

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

    if (aucont::get_container(pid).pid == -1) {
        std::cout << "No container with pid [ " + std::to_string(pid) + " ] ==> nothing to kill" << std::endl;
    }
    if (kill(pid, signum) < 0 && errno != ESRCH) {
        aucont::stdlib_error("Can't send signal");
    }

    return 0;
}
