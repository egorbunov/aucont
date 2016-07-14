#include <iostream>
#include <sstream>
#include <aucont_file.h>
#include <vector>

int main() {
    std::vector<pid_t> pids = {123, 21321, 1231, 9999, 0, 1, 2, 3};
    for (pid_t p : pids) {
        if (aucont::add_container_pid(p)) {
            std::cout << "Added!" << std::endl;
        } else {
            std::cout << "Not added!" << std::endl;
        }
    }

    return 0;
}
