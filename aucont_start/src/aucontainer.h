#pragma once

#include <unistd.h>
#include <ostream>
#include <netinet/in.h>


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
    };

    void daemonize();

    void pid_namespace_detach();
}