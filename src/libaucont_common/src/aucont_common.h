#include <string>
#include <set>
#include <sys/types.h>

namespace aucont
{
    /**
     * writes given PID to pids_file if PID not exists there already 
     */
    bool add_container_pid(pid_t pid);

    /**
     * deletes given PID from pids_file if it exists
     */
    bool del_container_pid(pid_t pid);

    /**
     * reads pids set from pids_file
     */
    std::set<pid_t> get_containers_pids();    

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
}