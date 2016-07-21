#include <iostream>
#include <sstream>
#include <aucont_file.h>
#include <vector>
#include <string>
#include <cstring>
#include <cctype>
#include <ostream>
#include <cstdlib>
#include <arpa/inet.h>

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
        std::cout << "       --cpu CPU_PERC - percent of cpu resources allocated for container 0..100" << std::endl;
        std::cout << "       --net IP - create virtual network between host and container with container IP address" << std::endl;
    }

    bool is_number(const char* str) 
    {
        size_t len = std::strlen(str);
        for (size_t i = 0; i < len; ++i) {
            if (!std::isdigit(str[i])) {
                return false;
            }
        }
        return true;
    }

    aucont::options parse_options(int argc, const char** argv)
    {
        aucont::options opts;
        for (int i = 1; i < argc; ++i) {
            if ((!std::strcmp(argv[i], "--cpu") || !std::strcmp(argv[i], "--net")) && i + 1 >= argc) {
                throw std::runtime_error("No arguments specified for some options");
            }

            if (!std::strcmp(argv[i], "-d")) {
                opts.daemonize = true;
            } else if (!std::strcmp(argv[i], "--cpu")) {
                if (!is_number(argv[i + 1])) {
                    throw std::runtime_error("Percent of cpu resources must be a number in [0, 100]");
                } else {
                    opts.cpu_perc = std::max(0, std::min(100, std::atoi(argv[++i])));
                }
            } else if (!std::strcmp(argv[i], "--net")) {
                struct in_addr taddr;
                if (!inet_aton(argv[i + 1], &taddr)) {
                    throw std::runtime_error("Incorrect ip-address specified (see `man 3 inet_aton`)");
                }
                opts.ip = argv[++i];
            } else {
                opts.fsimg_path = argv[i++];
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
        if (opts.cmd == nullptr) {
            throw std::runtime_error("No command specified to run inside container");
        }

        return opts;
    }

    void print_options(const aucont::options& opts)
    {
            std::cout << "Daemonize     : " << (opts.daemonize ? "Yes" : "No") << std::endl
                      << "CPU perc      : " << opts.cpu_perc << std::endl
                      << "Containter IP : " << (opts.ip == nullptr ? "-" : opts.ip) << std::endl
                      << "FS Image Path : " << (opts.fsimg_path == nullptr ? "-" : opts.fsimg_path) << std::endl
                      << "CMD line      : " << (opts.cmd == nullptr ? "-" : opts.cmd);
            int i = 1; // args[0] is always == cmd
            while (opts.args[i] != nullptr) {
                std::cout << " " << opts.args[i++];
            }
            std::cout << std::endl;
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
    std::cout << "Starting container with options: " << std::endl;
    print_options(opts);

    char path_to_exe[1000];
    realpath(argv[0], path_to_exe);

    // determining exe path for correct scripts usage
    std::string exe_path = std::string(path_to_exe);
    exe_path = exe_path.substr(0, exe_path.find_last_of("/")) + "/";

    aucont::start_container(opts, exe_path);

    return 0;
}