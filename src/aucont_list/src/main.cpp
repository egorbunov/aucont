#include <iostream>
#include <sstream>
#include <iomanip>
#include <stdexcept>

#include <csignal>
#include <cerrno>

#include <aucont_common.h>


int main(int argc, char* argv[]) {
    // preparing aucont common resources path
    aucont::set_aucont_root(aucont::get_file_real_dir(argv[0]));

    auto conts = aucont::get_containers();
    for (auto cont : conts) {
        std::cout << cont.pid << std::endl;
    }
    (void) argc;
    return 0;
}
