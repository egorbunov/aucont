#include "aucont_file.h"

#include <unistd.h>
#include <sys/stat.h>
#include <fstream>
#include <exception>
#include <iostream>
#include <sstream>
#include <cerrno>
#include <cstring>


namespace aucont
{
    namespace 
    {
        std::string aucont_dir = "/usr/share/aucont";
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
            out.close();
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
            pids.insert(pid);
        }
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
        aucont_dir = root_dir;
        pids_file = aucont_dir + "/containers";
        cgrouph_dir = aucont_dir + "/cgrouph";
    }
}

