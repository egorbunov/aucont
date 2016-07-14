#include <iostream>
#include <sstream>
#include <aucont_file.h>
#include <vector>
#include <string>
#include <cstring>
#include <cctype>
#include <ostream>
#include "aucontainer.h"

namespace 
{
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
                throw std::runtime_error("Must specify argument for some options.");
            }

            if (!std::strcmp(argv[i], "-d")) {
                opts.daemonize = true;
            } else if (!std::strcmp(argv[i], "--cpu")) {
                if (!is_number(argv[i + 1])) {
                    throw std::runtime_error("Percent of cpu resources must be number in [0, 100]");
                } else {
                    opts.cpu_perc = std::max(0, std::min(100, std::atoi(argv[++i])));
                }
            } else if (!std::strcmp(argv[i], "--net")) {
                opts.ip = argv[++i];
            } else {
                opts.fsimg_path = argv[i++];
                opts.cmd = argv[i++];
                for (int j = 0; j < argc - i; ++j) {
                    opts.args[j] = argv[j + i];
                }
                opts.args[argc - i] = nullptr;
                break;
            }
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
            int i = 0;
            while (opts.args[i] != nullptr) {
                std::cout << " " << opts.args[i++];
            }
            std::cout << std::endl;
    }
}

int main(int argc, const char* argv[]) 
{
    auto opts = parse_options(argc, argv);
    std::cout << "RUNNING OPTIONS: " << std::endl;
    print_options(opts);



    return 0;
}
