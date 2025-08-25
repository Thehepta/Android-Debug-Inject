//
// Created by chic on 2024/12/4.
//

#ifndef ZYGISKNEXT_ZYGISKD_H
#define ZYGISKNEXT_ZYGISKD_H

#include <dirent.h>
#include "daemon.h"




#define SECURE_DIR      "/data/adb"
#define MODULEROOT      SECURE_DIR "/modules"

class Zygiskd  {

public:
    void collect_modules();
    void foreach_module(int fd,dirent *entry,int modfd);
    static Zygiskd& getInstance(){
        static Zygiskd instance;
        return instance;
    }
    std::vector<Module> getModule_list(){
        return module_list;
    }
    std::vector<Module> getEnableModules(std::string processName);

    std::string getModul_by_index(int index){
        return module_list.at(index).name;
    }
    std::string getModuleRoot() {
        return moduleRoot;
    }
    void setModuleRoot(std::string str){
        moduleRoot = str;
    }

    void setModel(int md){
        model = md;
    }
    int getModel(){
        return model;
    }

    Zygiskd(const Zygiskd&)= delete;
    Zygiskd& operator=(const Zygiskd)=delete;
    int getProcessFlags(uid_t uid,std::string nice_name);
private:
    Zygiskd(){
    }
    ~Zygiskd() {
    }
    std::vector<Module> module_list;
    std::string moduleRoot;
    bool model = false;
};






#endif //ZYGISKNEXT_ZYGISKD_H
