//
// Created by chic on 2025/5/17.
//


#pragma once


enum {
    OPT = 1000,
    OPT_MOD_LIST,
    OPT_MOD_ADD_ENABLE,
    OPT_MOD_ENABLE_MODULE,
    OPT_MOD_ENABLE_PACKAGE,

    OPT_SET_ROOTDIR,
    OPT_SET_SOCKET,
};

#include <sys/types.h>

struct DaemonProgramArgs {
    bool help;          // --help 或 -h
    bool verbose;       // --verbose 或 -v
    int model;               //zygisk运行模式
    char *sql;               //数据库预留
    char *rootdir_path;      //zygisk 模块根目录
    char *unix_socket_path;  //zygisk socket请求套接字
    DaemonProgramArgs(){
        rootdir_path = nullptr;
        unix_socket_path = nullptr;
        model = 0;
    }
};


void daemon_parse_args(int argc, char **argv, DaemonProgramArgs *args);
