#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <string>
#include <ostream>
#include <algorithm>

#include <cstring>
#include <cctype>
#include <cstdlib>

#include <arpa/inet.h>

#include <aucont_common.h>

#include "aucontainer.h"

namespace 
{
    void print_usage()
    {
        std::cout << "USAGE: ./aucont_start [-d --cpu CPU_PERC --net IP] IMAGE_PATH CMD [ARGS]" << std::endl;
        std::cout << "       IMAGE_PATH - path to image of container file system" << std::endl;
        std::cout << "       CMD - command to run inside container" << std::endl;
        std::cout << "       ARGS - arguments for CMD" << std::endl;
        std::cout << "       -d - daemonize" << std::endl;
        std::cout << "       --cpu CPU_PERC - percent of cpu resources allocated for container 1..100" << std::endl;
        std::cout << "       --net IP - create virtual network between host and container with container IP address" 
        << std::endl;
    }

    aucont::options parse_options(int argc, const char** argv)
    {
        aucont::options opts;
        for (int i = 1; i < argc; ++i) {
            if ((!std::strcmp(argv[i], "--cpu") || !std::strcmp(argv[i], "--net")) && i + 1 >= argc) {
                aucont::error("No arguments specified for some options");
            }

            if (!std::strcmp(argv[i], "-d")) {
                opts.daemonize = true;
            } else if (!std::strcmp(argv[i], "--cpu")) {
                if (std::any_of(argv[i + 1], argv[i + 1] + strlen(argv[i + 1]), 
                    [](char c){ return !std::isdigit(c); })) { // check if is number
                    throw std::runtime_error("Percent of cpu resources must be a number!");
                } else {
                    opts.cpu_perc = std::atoi(argv[++i]);
                    if (opts.cpu_perc < 1 || opts.cpu_perc > 100) {
                        throw std::runtime_error("Percent of cpu usage must be in [1, 100]");
                    }
                }
            } else if (!std::strcmp(argv[i], "--net")) {
                struct in_addr taddr;
                if (!inet_aton(argv[++i], &taddr)) {
                    throw std::runtime_error("Incorrect ip-address specified (see `man 3 inet_aton`)");
                }
                opts.ip = inet_ntoa(taddr);
            } else {
                opts.fsimg_path = aucont::get_real_path(argv[i++]);
                opts.cmd = argv[i++];
                opts.args[0] = opts.cmd;
                for (int j = 0; j < argc - i; ++j) {
                    opts.args[j + 1] = argv[j + i];
                }
                opts.args[argc - i + 1] = nullptr;
                break;
            }
        }

        // validating
        if (opts.fsimg_path.empty()) {
            throw std::runtime_error("No image path specified");
        }
        if (opts.cmd == nullptr) {
            throw std::runtime_error("No command specified to run inside container");
        }

        return opts;
    }
}

int main(int argc, const char* argv[]) 
{
    aucont::options opts;
    try {
        opts = parse_options(argc, argv);
    } catch (std::runtime_error err) {
        std::cout << "Bad arguments: " << err.what() << std::endl;
        print_usage();
        return 0;
    }

    auto exe_path = aucont::get_file_real_dir(argv[0]);
    aucont::set_aucont_root(exe_path);
    aucont::start_container(opts, exe_path);

    return 0;
}
