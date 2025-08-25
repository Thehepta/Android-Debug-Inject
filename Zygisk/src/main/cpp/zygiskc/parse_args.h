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

struct ProgramArgs {

    char *unix_socket_path;

    bool help;          // --help 或 -h

    bool daemonInfo;

    bool add_enable;
    char* module;
    char* package;

    bool mod_list;

    bool mod_install;
    char* mod_tmp_path;

};

void module_parse_args(int argc, char **argv, ProgramArgs *args);