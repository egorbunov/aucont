#include <iostream>
#include <sstream>
#include <aucont_file.h>
#include <vector>
#include <netinet/in.h>
#include <string>
#include <cstring>
#include <cctype>
#include <ostream>

namespace 
{
    const size_t max_cmd_arg_size = 42;

    struct options 
    {
        bool daemonize;
        int cpu_perc;
        const char* ip;
        const char* fsimg_path;
        const char* cmd;
        const char* args[max_cmd_arg_size];

        options(): daemonize(false), cpu_perc(100), ip(nullptr), fsimg_path(nullptr), cmd(nullptr)
        {
            for (size_t i = 0; i < max_cmd_arg_size; ++i) {
                args[i] = nullptr;
            }
        }

        friend std::ostream& operator<<(std::ostream& out, const options& opts) {
            out << "Daemonize     : " << (opts.daemonize ? "Yes" : "No") << std::endl
                << "CPU perc      : " << opts.cpu_perc << std::endl
                << "Containter IP : " << (opts.ip == nullptr ? "-" : opts.ip) << std::endl
                << "FS Image Path : " << (opts.fsimg_path == nullptr ? "-" : opts.fsimg_path) << std::endl
                << "CMD line      : " << (opts.cmd == nullptr ? "-" : opts.cmd);
            int i = 0;
            while (opts.args[i] != nullptr) {
                out << " " << opts.args[i++];
            }
            out << std::endl;
            return out;
        }
    };

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

    options read_options(int argc, const char** argv)
    {
        options opts;
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
                    std::cout << argv[j + i] << std::endl;
                }
                opts.args[argc - i] = nullptr;
                break;
            }
        }

        return opts;
    }
}

int main(int argc, const char* argv[]) 
{
    auto opts = read_options(argc, argv);

    std::cout << opts << std::endl;

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
