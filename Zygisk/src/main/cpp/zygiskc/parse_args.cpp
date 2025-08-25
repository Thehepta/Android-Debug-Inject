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

void module_parse_args(int argc, char **argv, ProgramArgs *args) {
    // 初始化默认值
    int opt;
    int option_index = 0;

    static struct option long_options[] = {
            {"help",    no_argument,       0,       'h'},

            {"list", no_argument,       0,          'l'},

            {"daemonInfo",    no_argument,       0,       'v'},

            {"add-enable", no_argument,       0,    OPT_MOD_ADD_ENABLE},
            {"module", required_argument,       0,  OPT_MOD_ENABLE_MODULE},
            {"package", required_argument,       0, OPT_MOD_ENABLE_PACKAGE},

            {"install", required_argument,       0, 'i'},

            {"unix_socket",   required_argument, 0, OPT_SET_SOCKET},
            {0, 0, 0,                               0}  // 结束标记
    };

    // 定义短选项（short options）
    const char *short_options = "hliv";


    // 解析选项
    while ((opt = getopt_long(argc, argv, short_options, long_options, &option_index)) != -1) {
        switch (opt) {
            case 'h':
                args->help = true;
                break;
            case 'i':
                args->mod_install = true;
                args->mod_tmp_path = strdup(optarg);
                break;
            case 'l':
                args->mod_list = true;
                break;
            case 'v':
                args->daemonInfo = true;
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
            case OPT_SET_SOCKET:
                args->unix_socket_path = strdup(optarg);
                break;
        }
    }
}