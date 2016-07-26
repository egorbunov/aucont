#include "aucont_common.h"

#include <unistd.h>
#include <sys/stat.h>
#include <fstream>
#include <stdexcept>
#include <iostream>
#include <sstream>
#include <cerrno>
#include <cstring>
#include <cstring>
#include <csignal>


namespace aucont
{
    namespace 
    {
        std::string aucont_dir = ".";
        std::string pids_file = aucont_dir + "/containers";
        std::string cgrouph_dir = aucont_dir + "/cgrouph";


        bool not_exist(std::string filename) {
            struct stat st;
            if (stat(filename.c_str(), &st) < 0 && errno == ENOENT) {
                return true;
            }
            return false;
        }

    
        void prepare()
        {
            if (not_exist(aucont_dir)) {
                int result = mkdir(aucont_dir.c_str(), 0777);
                if (result != 0) {
                    std::stringstream ss;
                    ss << "Can't create direcotry [ " << aucont_dir << " ], " << "error code = [ " << errno << " ]: " << strerror(errno); 
                    throw std::runtime_error(ss.str());
                }
            }
        }
        
        /**
         * rewrites pids_file completely with given set of pids
         */
        void write_containters_pids(std::set<pid_t> pids)
        {
            prepare();
            std::ofstream out(pids_file.c_str(), std::ios_base::trunc | std::ios_base::out | std::ios_base::binary);
            if (out.fail()) {
                std::stringstream ss;
                ss << "Can't open file with containers pids for write [ " << strerror(errno) << " ]";
                throw std::runtime_error(ss.str());
            }
            for (pid_t pid : pids) {
                out.write(reinterpret_cast<char*>(&pid), sizeof(pid_t) / sizeof(char));
            }
            out.flush();
            out.close();
        }

        bool is_proc_dead(pid_t pid)
        {
            return kill(pid, 0) == -1 && errno == ESRCH;
        }
    }

    std::string get_cgrouph_path() 
    {
        return cgrouph_dir;
    }

    std::string get_pids_path()
    {
        return pids_file;
    }

    std::set<pid_t> get_containers_pids()
    {
        if (not_exist(pids_file)) {
            return std::set<pid_t>();
        }

        std::ifstream in(pids_file.c_str(), std::ios_base::in | std::ios_base::binary);
        if (in.fail()) {
            std::stringstream ss;
            ss << "Can't open file with containers pids for read [ " << strerror(errno) << " ]";
            throw std::runtime_error(ss.str());
        }
        std::set<pid_t> pids;
        pid_t pid;

        while (in.read(reinterpret_cast<char*>(&pid), sizeof(pid_t) / sizeof(char))) {
            if (!is_proc_dead(pid)) {
                pids.insert(pid);
            }
        }
        write_containters_pids(pids);
        return pids;
    }

    bool add_container_pid(pid_t pid)
    {
        auto pids = get_containers_pids();
        if (pids.count(pid)) {
            return false;
        } 
        pids.insert(pid);
        write_containters_pids(pids);
        return true;
    }

    bool del_container_pid(pid_t pid)
    {
        auto pids = get_containers_pids();
        auto it = pids.find(pid);
        if (it == pids.end()) {
            return false;
        } 
        pids.erase(it);
        write_containters_pids(pids);
        return true;
    }

    void set_aucont_root(std::string root_dir)
    {
        if (root_dir[root_dir.length() - 1] == '/') {
            root_dir = root_dir.substr(0, root_dir.length() - 1);
        }
        aucont_dir = root_dir;
        pids_file = aucont_dir + "/containers";
        cgrouph_dir = aucont_dir + "/cgrouph";
    }

    std::string get_real_path(std::string file_path)
    {
        char path_to_file[1000];
        if (realpath(file_path.c_str(), path_to_file) == NULL) {
            throw std::runtime_error("Can't get real path");
        }
        return std::string(path_to_file);
    }

    std::string get_file_real_dir(std::string file_path)
    {
        std::string path = get_real_path(file_path);
        path = path.substr(0, path.find_last_of("/")) + "/";
        return path;
    }

    void error(std::string msg)
    {
        std::cerr << "ERROR: " << msg << std::endl;
        exit(1);
    }

    void stdlib_error(std::string msg)
    {
        std::stringstream ss;
        ss << "ERROR: " << msg << " [ " << strerror(errno) << " ]";
        std::cerr << ss.str() << std::endl;
        exit(1);
    }
}

