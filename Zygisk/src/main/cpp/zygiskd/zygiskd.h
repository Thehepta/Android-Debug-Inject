//
// Created by chic on 2024/12/4.
//

#ifndef ZYGISKNEXT_ZYGISKD_H
#define ZYGISKNEXT_ZYGISKD_H

#include <dirent.h>
#include "daemon.h"
#include "RootImp.h"
#include "ksu.h"

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

    std::string getModul_by_index(int index){
        return module_list.at(index).name;
    }
    void set_exec_path(std::string exec_path){
        this->exec_path = exec_path;

    }
    std::string get_exec_path(){
        return this->exec_path;
    }
    Zygiskd(const Zygiskd&)= delete;
    Zygiskd& operator=(const Zygiskd)=delete;
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
    std::string exec_path;
    bool running = false;
    RootImp* rootImp;
};






#endif //ZYGISKNEXT_ZYGISKD_H
