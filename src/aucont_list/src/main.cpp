#include <iostream>
#include <sstream>
#include <aucont_file.h>
#include <iomanip>
#include <csignal>
#include <stdexcept>
#include <cerrno>

// void pretty() {
//     auto pids = aucont::get_containers_pids();
//     std::cout << std::left << std::setw(20) << std::setfill(' ') << "CONTAINER_PID";
//     std::cout << std::left << std::setw(20) << std::setfill(' ') << "STATUS";
//     std::cout << std::endl;
//     std::cout << std::setw(40) << std::setfill('.') << ".";
//     std::cout << std::endl;
//     for (auto pid : pids) {
//         std::cout << std::left << std::setw(20) << std::setfill(' ') << pid;
//         std::string status = "running";
//         if (kill(pid, 0) < 0 && errno == ESRCH) {
//             status = "dead (daemon?)";
//         }
//         std::cout << std::left << std::setw(20) << std::setfill(' ') << status;
//         std::cout << std::endl;
//     }
// }

int main(int argc, char* argv[]) {
    (void) argc;
    char path_to_exe[1000];
    realpath(argv[0], path_to_exe);
    auto exe_path = std::string(path_to_exe);
    exe_path = exe_path.substr(0, exe_path.find_last_of("/")) + "/";
    // preparing aucont common resources path
    aucont::set_aucont_root(exe_path);

    auto pids = aucont::get_containers_pids();
    for (auto pid : pids) {
        if (kill(pid, 0) == 0) {
            std::cout << pid << std::endl;
	    } else if (errno == ESRCH) {
            aucont::del_container_pid(pid);
        }
    }
    return 0;
}
