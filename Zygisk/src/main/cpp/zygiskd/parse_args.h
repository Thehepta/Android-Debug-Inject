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

    OPT_SET_DB,
    OPT_SET_SOCKET,
};

#include <sys/types.h>

struct ProgramArgs {
    bool help;          // --help 或 -h
    bool verbose;       // --verbose 或 -v
    bool start_daemon;

    char *sql;
    char *sqlite_db_path;
    char *unix_socket_path;
    bool add_enable;
    char* module;
    char* package;
    bool mod_list;
    ProgramArgs() {
        mod_list = false;
        help = false;
        verbose = false;
        start_daemon = false;
        add_enable = false;
        sqlite_db_path = nullptr;
        module = nullptr;
        package = nullptr;
        sql = nullptr;
        unix_socket_path = nullptr;

    }
};


void daemon_parse_args(int argc, char **argv, ProgramArgs *args);
void module_parse_args(int argc, char **argv, ProgramArgs *args);