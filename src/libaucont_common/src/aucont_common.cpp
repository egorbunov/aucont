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
#include <cstdint>

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
        void write_containters(const std::set<container_t>& conts)
        {
            prepare();
            std::ofstream out(pids_file.c_str(), std::ios_base::trunc | std::ios_base::out | std::ios_base::binary);
            if (out.fail()) {
                std::stringstream ss;
                ss << "Can't open file with containers pids for write [ " << strerror(errno) << " ]";
                throw std::runtime_error(ss.str());
            }
            for (auto& cont : conts) {
                out.write(reinterpret_cast<const char*>(&cont), sizeof(container_t) / sizeof(char));
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

    std::set<container_t> get_containers()
    {
        if (not_exist(pids_file)) {
            return std::set<container_t>();
        }
        std::ifstream in(pids_file.c_str(), std::ios_base::in | std::ios_base::binary);
        if (in.fail()) {
            std::stringstream ss;
            ss << "Can't open file with containers info for read [ " << strerror(errno) << " ]";
            throw std::runtime_error(ss.str());
        }
        std::set<container_t> conts;
        container_t cont;

        while (in.read(reinterpret_cast<char*>(&cont), sizeof(container_t) / sizeof(char))) {
            if (!is_proc_dead(cont.pid)) {
                conts.insert(cont);
            }
        }
        write_containters(conts);
        return conts;
    }

    container_t get_container(pid_t pid)
    {
        auto conts = get_containers();
        auto it = conts.find(container_t(pid));
        if (it == conts.end()) {
            return container_t();
        } else {
            return *it;
        }
    }

    bool add_container(const container_t& cont)
    {
        auto conts = get_containers();
        auto sz = conts.size();
        conts.insert(cont);
        if (sz == conts.size()) {
            return false;
        }
        write_containters(conts);
        return true;
    }

    bool del_container(pid_t pid)
    {
        auto conts = get_containers();
        auto it = conts.find(container_t(pid));
        if (it == conts.end()) {
            return false;
        } 
        conts.erase(it);
        write_containters(conts);
        return true;
    }

    std::string get_cgroup_for_cpuperc(uint8_t cpu_perc)
    {
        return "cpu_restricted_" + std::to_string((int) cpu_perc);
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

