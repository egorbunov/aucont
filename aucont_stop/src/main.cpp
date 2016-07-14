#include <iostream>
#include <sstream>
#include <string>
#include <aucont_file.h>
#include <sys/types.h>

void print_usage() {
    std::cout << "USAGE: ./aucont_stop PID [SIGNUM]" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 2 || argc > 3) {
        print_usage();
        return 0;
    }

    pid_t pid = atoi(argv[1]);
    // int signum = 0; // SIGTERM
    // if (argc == 3) {
    //     signum = atoi(argv[2]);
    // }

    // sending signal
    if (aucont::del_container_pid(pid)) {
        std::cout << "OK" << std::endl;
    } else {
        std::cout << "Failed: no container with PID [ " << pid << " ]" << std::endl;
    }
    

    return 0;
}
