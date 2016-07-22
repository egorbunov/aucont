#include <iostream>
#include <sstream>
#include <string>
#include <aucont_file.h>
#include <sys/types.h>
#include <signal.h>
#include <cstring>
#include <cerrno>

void print_usage() {
    std::cout << "USAGE: ./aucont_stop PID [SIGNUM]" << std::endl;
}

void delete_if_absent(pid_t pid) {
    if (kill(pid, 0) < 0 && errno == ESRCH) {
        if (aucont::del_container_pid(pid)) {
            std::cout << "OK" << std::endl;
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2 || argc > 3) {
        print_usage();
        return 0;
    }

    pid_t pid = atoi(argv[1]);
    int signum = SIGTERM;
    if (argc == 3) {
        signum = atoi(argv[2]);
    }

    if (kill(pid, signum) < 0) {
        delete_if_absent(pid);
        throw std::runtime_error("Can't send signal [ " + std::string(strerror(errno)) + " ] ");
    }
    delete_if_absent(pid);
    
    return 0;
}
