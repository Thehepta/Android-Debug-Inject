//
// Created by chic on 2025/5/17.
//

#include "daemon_parse_args.h"
#include <getopt.h>  // 必须包含此头文件
#include <cstring>
#include <cstdio>
#include <dirent.h>
#include <cstdlib>
#include "logging.h"

void daemon_parse_args(int argc, char **argv, DaemonProgramArgs *args) {
    // 初始化默认值
    int opt;
    int option_index = 0;

    static struct option long_options[] = {

            {"help", no_argument,       0,           'h'},
            {"verbose", no_argument,       0,        'v'},
            {"model",    required_argument,       0, 'm'},
            {"rootdir",   required_argument, 0,           OPT_SET_ROOTDIR},
            {"unix_socket",   required_argument, 0,  OPT_SET_SOCKET},
            {"sqlite",   required_argument, 0,       's'},


            {0, 0, 0,                                0}  // 结束标记
    };

    // 定义短选项（short options）
    const char *short_options = "hmvs";


    // 解析选项
    while ((opt = getopt_long(argc, argv, short_options, long_options, &option_index)) != -1) {
        switch (opt) {
            case 'h':
                args->help = true;
                break;
            case 'm':
                args->model = atoi(optarg);
                break;
            case 's':
                args->sql = strdup(optarg);
                break;
            case 'v':
                args->verbose = true;
                break;
            case OPT_SET_ROOTDIR:
                args->rootdir_path = strdup(optarg);
                break;
            case OPT_SET_SOCKET:
                args->unix_socket_path = strdup(optarg);
                break;
        }
    }
}