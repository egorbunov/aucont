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
     * returns root path of cgroup hierarchy
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
}