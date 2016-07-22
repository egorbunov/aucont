#include <iostream>
#include <sstream>
#include <aucont_file.h>
#include <iomanip>
#include <csignal>
#include <cerrno>

int main() {
    auto pids = aucont::get_containers_pids();
    std::cout << std::left << std::setw(20) << std::setfill(' ') << "CONTAINER_PID";
    std::cout << std::left << std::setw(20) << std::setfill(' ') << "STATUS";
    std::cout << std::endl;
    std::cout << std::setw(40) << std::setfill('.') << ".";
    std::cout << std::endl;
    for (auto pid : pids) {
	    std::cout << std::left << std::setw(20) << std::setfill(' ') << pid;
	    std::string status = "running";
	    if (kill(pid, 0) < 0 && errno == ESRCH) {
	    	status = "dead (daemon?)";
	    }
	    std::cout << std::left << std::setw(20) << std::setfill(' ') << status;
	    std::cout << std::endl;
    }
    return 0;
}
