//
// Created by chic on 2024/12/4.
//

#ifndef ZYGISKNEXT_ZYGISKD_H
#define ZYGISKNEXT_ZYGISKD_H

#include <dirent.h>
#include "daemon.h"
#include "RootImp.h"
#include "ksu.h"









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

    Zygiskd(const Zygiskd&)= delete;
    Zygiskd& operator=(const Zygiskd)=delete;
    int getProcessFlags(uid_t uid,std::string nice_name);
    static RootImp* getRootImp(){
        return getInstance().rootImp;
    }
    void rootImpInit();
private:
    Zygiskd(){
    }
    ~Zygiskd() {
    }
    std::vector<Module> module_list;
    std::string moduleRoot;
    bool running = false;
    RootImp* rootImp;
};






#endif //ZYGISKNEXT_ZYGISKD_H
