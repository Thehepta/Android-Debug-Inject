//
// Created by rzx on 2025/7/20.
//

#ifndef ANDROID_DEBUG_INJECT_MODULE_H
#define ANDROID_DEBUG_INJECT_MODULE_H

#include "string"

#define SECURE_DIR      "/data/adb"
#define MODULEROOT      SECURE_DIR "/modules"

void module_main(int argc, char **argv);
void list_modules(std::string );
void add_enable_module( std::string root_path,char* module_name ,char* packageName);
#endif //ANDROID_DEBUG_INJECT_MODULE_H
