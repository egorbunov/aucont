#include <string>
#include <set>
#include <iostream>
#include <type_traits>
#include <vector>
#include <sstream>

#include <cstdint>

#include <unistd.h>
#include <sys/types.h>

namespace aucont
{
    struct container_t
    {
        pid_t pid;
        uint8_t cpu_perc;

        container_t(pid_t pid = -1, uint8_t cpu_perc = 100): pid(pid), cpu_perc(cpu_perc) 
        {}

        bool operator<(const container_t& other) const
        {
            return pid < other.pid;
        }

        bool operator==(const container_t& other) const
        {
            return pid == other.pid;
        }
    };

    /**
     * writes given container info to file  
     */
    bool add_container(const container_t& cont);

    /**
     * deletes given PID from file with container info, 
     * if it (container with such pid) exists
     */
    bool del_container(pid_t pid); 

    container_t get_container(pid_t pid);
    
    std::set<container_t> get_containers();

    /**
     * returns root path of cgroup hierarchy, which is mounted by aucont_start util
     * during start of any container with cpu restrictions
     */
    std::string get_cgrouph_path();

    /**
     * returns file with cont pids
     */
    std::string get_pids_path();

    /**
     * sets up root directory for creating files (cgrouph, file with pids)
     */
    void set_aucont_root(std::string root_dir);

    /**
     * returns name of cgroup (folder, only top level) for given cpu_perc
     * need for more convenient way of using cgroup names for containers 
     * in aucont utils
     */
    std::string get_cgroup_for_cpuperc(uint8_t cpu_perc);

    // utility stuff
    /**
     * returns full absolute path to directory, where given file points
     * @param  file_path path to file
     * @return path to directory with '/' symbol at the end
     */
    std::string get_file_real_dir(std::string file_path);

    /**
     * returns absolute path to file
     */
    std::string get_real_path(std::string file_path);

    /**
     * prints message to stderr and exit(1)
     */
    void error(std::string msg);

    /**
     * prints messege with added errno description and exit(1)
     */
    void stdlib_error(std::string msg);

    template<typename T>
    typename std::enable_if<std::is_pod<T>::value, T>::type read_from_pipe(int fd)
    {
        char buf[sizeof(T)];
        memset(buf, 0, sizeof(T));
        size_t offset = 0;
        while (offset != sizeof(T)) {
            ssize_t ret = read(fd, buf + offset, sizeof(T) - offset);
            if (ret < 0) {
                stdlib_error("Can't read from pipe");
            }
            if (ret == 0) {
                error("pipe read returned 0");
            }
            offset += ret;
        }
        return *reinterpret_cast<T*>(buf);
    }

    template<typename T>
    typename std::enable_if<std::is_pod<T>::value>::type write_to_pipe(int fd, T data)
    {
        void* buf = reinterpret_cast<void*>(&data);
        size_t count = sizeof(T);
        while (count > 0) {
            ssize_t ret = write(fd, buf, count);
            if (ret <= 0) {
                stdlib_error("Can't write to pipe");
            }
            count -= ret;
            if (count > 0) {
                buf = reinterpret_cast<void*>(reinterpret_cast<char*>(buf) + ret);
            }
        }
    }

    namespace
    {
        template<typename T>
        void print_arg(std::ostream& out, const T& t) {
            out << t;
        }

        template<typename T, typename... Args>
        void print_arg(std::ostream& out, const T& t, Args... args) {
            out << "\"" << t << "\"" << " ";
            print_arg(out, args...);
        }
    }

    /**
     * Executes given command via system() call.
     * Command is just a concatenation of given args with space delimiter.
     * Every argument framed with quotes
     */
    template<typename... Args>
    int sysrun(std::string cmd, Args... args)
    {
        std::stringstream ss;
        ss << cmd << " ";
        print_arg(ss, args...);
        std::string command = ss.str();
        return system(command.c_str());
    }
}