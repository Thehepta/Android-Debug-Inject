//
// Created by chic on 2025/5/17.
//

#include "parse_args.h"
#include <getopt.h>  // 必须包含此头文件
#include <cstring>
#include <cstdio>
#include <dirent.h>
#include <cstdlib>
#include "logging.h"

void daemon_parse_args(int argc, char **argv, ProgramArgs *args) {
    // 初始化默认值
    int opt;
    int option_index = 0;

    static struct option long_options[] = {

            {"help", no_argument,       0,            'h'},
            {"verbose", no_argument,       0,         'v'},
            {"config",    required_argument,       0, 'c'},
            {"db",   required_argument, 0,            OPT_SET_DB},
            {"unix_socket",   required_argument, 0,   OPT_SET_SOCKET},
            {"sqlite",   required_argument, 0,        's'},


            {0, 0, 0,                                 0}  // 结束标记
    };

    // 定义短选项（short options）
    const char *short_options = "himvc:p:n:";


    // 解析选项
    while ((opt = getopt_long(argc, argv, short_options, long_options, &option_index)) != -1) {
        switch (opt) {
            case 'h':
                args->help = true;
                break;
            case 's':
                args->sql = strdup(optarg);
                break;
            case 'v':
                args->verbose = true;
                break;
            case OPT_SET_DB:
                args->sqlite_db_path = strdup(optarg);
                break;
            case OPT_SET_SOCKET:
                args->unix_socket_path = strdup(optarg);
                break;
        }
    }
}

void module_parse_args(int argc, char **argv, ProgramArgs *args) {
    // 初始化默认值
    int opt;
    int option_index = 0;

    static struct option long_options[] = {
            {"help",    no_argument,       0, 'h'},
            {"list", no_argument,       0,            'l'},
            {"add-enable", no_argument,       0,         OPT_MOD_ADD_ENABLE},
            {"module", required_argument,       0,         OPT_MOD_ENABLE_MODULE},
            {"package", required_argument,       0,         OPT_MOD_ENABLE_PACKAGE},
            {"db",   required_argument, 0,            OPT_SET_DB},
            {"unix_socket",   required_argument, 0,   OPT_SET_SOCKET},
            {0, 0, 0,                                 0}  // 结束标记
    };

    // 定义短选项（short options）
    const char *short_options = "hl";


    // 解析选项
    while ((opt = getopt_long(argc, argv, short_options, long_options, &option_index)) != -1) {
        switch (opt) {
            case 'h':
                args->help = true;
                break;
            case 'l':
                args->mod_list = true;
                break;
            case OPT_MOD_ADD_ENABLE:
                args->add_enable = true;
                break;
            case OPT_MOD_ENABLE_MODULE:
                args->module = strdup(optarg);
                break;
            case OPT_MOD_ENABLE_PACKAGE:
                args->package = strdup(optarg);
                break;
            case OPT_SET_DB:
                args->sqlite_db_path = strdup(optarg);
                break;
            case OPT_SET_SOCKET:
                args->unix_socket_path = strdup(optarg);
                break;

        }
    }
}