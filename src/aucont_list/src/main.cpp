#include <iostream>
#include <sstream>
#include <aucont_file.h>

int main() {
    auto pids = aucont::get_containers_pids();
    std::cout << "CONTAINER PID" << std::endl;
    for (auto pid : pids) {
        std::cout << pid << std::endl;
    }
    return 0;
}
