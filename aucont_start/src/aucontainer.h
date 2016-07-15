#pragma once

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
        /**
         * array of char arrays terminated with NULL (end of array signal)
         */
        const char* args[max_cmd_arg_size];

        options(): daemonize(false), cpu_perc(100), ip(nullptr), fsimg_path(nullptr), cmd(nullptr)
        {
            for (size_t i = 0; i < max_cmd_arg_size; ++i) {
                args[i] = nullptr;
            }
        }
    };

    void start_container(const options&);

    /**
     * Daemonizes current process.
     * Calling process (caller) will terminate during function execution
     * `getpid()` after call not equal to `getpid()` before, because 
     * daemonized child process returns from this function (not caller)
     */
    void daemonize();
}