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

bool parse_args(int argc, char **argv, ProgramArgs *args) {
    // 初始化默认值
    int opt;
    int option_index = 0;
    char* niceName= nullptr;
    int is_niceName = -1;
    bool is_config = false;
    int is_inject_sopath = -1;
    int is_inject_funsym = -1;
    // 定义长选项（long options）
    static struct option long_options[] = {
            {"help",    no_argument,       0, 'h'},
            {"monitor",    no_argument,       0, 'm'},
            {"inject",    no_argument,       0, 'i'},
            {"config",    required_argument,       0, 'c'},
            {"verbose", no_argument,       0, 'v'},
            {"pid",  required_argument, 0, 'p'},
            {"niceName",  required_argument, 0, 'n'},
            {"exec",   required_argument, 0, OPT_TRACE_EXEC},
            {"waitSoPath",   required_argument, 0, OPT_TRACE_WAITSOPATH},
            {"waitFunSym",   required_argument, 0, OPT_TRACE_WAITFUNSYM},
            {"injectSoPath",  required_argument, 0, OPT_INJECT_SOPATH},
            {"injectFunSym",   required_argument, 0, OPT_INJECT_FUNSYM},
            {"injectFunArg",   required_argument, 0, OPT_INJECT_FUNARG},
            {"monitorCount",   required_argument, 0,OPT_MONITORCOUNT},
            {"hidemaps",   required_argument, 0,OPT_HIDEMAPS},
            {"unload",   required_argument, 0,OPT_UNLOAD},
            {0, 0, 0, 0}  // 结束标记
    };

    // 定义短选项（short options）
    const char *short_options = "himvc:p:n:";


    // 解析选项
    while ((opt = getopt_long(argc, argv, short_options, long_options, &option_index)) != -1) {
        switch (opt) {
            case 'c':
                args->config = strdup(optarg);
                is_config = true;
                break;
            case 'h':
                args->help = true;
                break;
            case 'i':
                args->inject = true;
                break;
            case 'p':
                args->pid = atoi(optarg);
                break;
            case 'm':
                args->monitor = true;
                break;
            case 'v':
                args->verbose = true;
                break;

            case OPT_TRACE_EXEC:
                args->exec = strdup(optarg);
                break;
            case OPT_TRACE_WAITSOPATH:
                args->waitSoPath = strdup(optarg);
                break;
            case OPT_TRACE_WAITFUNSYM:
                args->waitFunSym = strdup(optarg);
                break;
            case OPT_INJECT_SOPATH:
                args->injectSoPath = strdup(optarg);
                break;
            case OPT_INJECT_FUNSYM:
                args->injectFunSym = strdup(optarg);
                break;
            case OPT_INJECT_FUNARG:
                args->injectFunArg = strdup(optarg);
                break;
            case OPT_MONITORCOUNT:
                args->monitorCount = atoi(optarg);
                break;
            case OPT_HIDEMAPS:
                args->hidemaps = true;
                break;
            case OPT_UNLOAD:
                args->unload = true;
                break;

        }
    }


    return true;
}