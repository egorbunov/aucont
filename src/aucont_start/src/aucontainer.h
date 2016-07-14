#pragma once

#include <ostream>
#include <netinet/in.h>
#include <string>


namespace aucont
{
    /**
     * Command line options for container start
     */
    struct options 
    {
        static const size_t max_cmd_arg_size = 42;
        
        bool daemonize;
        int cpu_perc;
        std::string ip;
        std::string fsimg_path;
        const char* cmd;
        /**
         * array of char arrays terminated with NULL (end of array signal)
         */
        const char* args[max_cmd_arg_size];

        options(): daemonize(false), cpu_perc(100), ip(""), fsimg_path(""), cmd(nullptr)
        {
            for (size_t i = 0; i < max_cmd_arg_size; ++i) {
                args[i] = nullptr;
            }
        }
    };

    void start_container(const options&, std::string exe_path);
}