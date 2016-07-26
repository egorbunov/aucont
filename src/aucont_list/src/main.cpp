#include <iostream>
#include <sstream>
#include <aucont_common.h>
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
//     }0
// }

int main(int argc, char* argv[]) {
    // preparing aucont common resources path
    aucont::set_aucont_root(aucont::get_file_real_dir(argv[0]));

    auto pids = aucont::get_containers_pids();
    for (auto pid : pids) {
        std::cout << pid << std::endl;
    }
    (void) argc;
    return 0;
}
