//
// Created by rzx on 2025/7/20.
//

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <dirent.h>
#include <fcntl.h>
#include <sstream>
#include <sys/stat.h>
#include "module.h"
#include "json.hpp"     // nlohmann/json 库
#include "parse_args.h"
#include "files.hpp"
#include "daemon.h"

// 使用 using 声明以方便使用
using json = nlohmann::json;
namespace fs = std::filesystem; // 为 filesystem 创建一个命名空间别名

// 之前的 trim 辅助函数，用于去除字符串首尾空白
std::string trim(const std::string& str) {
    const std::string whitespace = " \t\n\r\f\v";
    size_t first = str.find_first_not_of(whitespace);
    if (std::string::npos == first) return "";
    size_t last = str.find_last_not_of(whitespace);
    return str.substr(first, (last - first + 1));
}

std::string parse_enable_app_from_fd(int fd) {
    if (fd < 0) {
        return ""; // 无效的文件描述符
    }

    // 1. 将文件描述符包装成一个 FILE* 流
    FILE* file_stream = fdopen(fd, "r");
    if (file_stream == nullptr) {
        perror("fdopen failed");
        // 如果 fdopen 失败，fd 仍然是打开的，需要我们手动关闭
        close(fd);
        return "";
    }

    std::stringstream result_stream; // 用于高效拼接字符串
    bool first_line = true;

    // --- 使用 C 语言的 getline，因为它操作 FILE* ---
    char* line_buffer = nullptr;
    size_t buffer_len = 0;
    ssize_t chars_read;

    // 2. 在 FILE* 流上逐行读取
    while ((chars_read = getline(&line_buffer, &buffer_len, file_stream)) != -1) {
        if (!first_line) {
            result_stream << "|";
        }

        // getline 读取的行包含换行符，我们需要去掉它
        if (chars_read > 0 && line_buffer[chars_read - 1] == '\n') {
            // 直接写入 n-1 个字符，巧妙地去掉了换行符
            result_stream.write(line_buffer, chars_read - 1);
        } else {
            result_stream.write(line_buffer, chars_read);
        }
        first_line = false;
    }

    // 释放 getline 自动分配的内存
    free(line_buffer);

    // 3. 关闭 FILE* 流，这也会自动关闭底层的 fd
    fclose(file_stream);

    return result_stream.str();
}

// 将解析单个 module.prop 文件的逻辑封装成一个函数
// 返回一个 json 对象。如果解析失败，返回一个空的 json 对象。
// **修改后的函数：接受一个文件描述符 (fd)**
// 如果 fd 无效，或者读取失败，返回一个空的 json 对象。
json parse_module_prop_from_fd(int fd) {
    if (fd < 0) {
        return json::object(); // 无效的文件描述符
    }

    // **核心步骤**: 将文件描述符包装成一个 FILE* 流
    // "r" 表示我们以只读模式关联这个流
    FILE* file_stream = fdopen(fd, "r");
    if (file_stream == nullptr) {
        perror("fdopen failed");
        // 注意：fdopen 失败后，原始的 fd 仍然需要我们手动关闭
        close(fd);
        return json::object();
    }

    json module_info = json::object();

    // --- 从这里开始，代码和之前基于 C 的 getline 版本几乎一样 ---
    char *line_buffer = nullptr;
    size_t buffer_len = 0;

    while (getline(&line_buffer, &buffer_len, file_stream) != -1) {
        std::string line(line_buffer); // 将 C 风格字符串转换为 std::string
        line = trim(line);

        if (line.empty() || line[0] == '#') {
            continue;
        }

        size_t separator_pos = line.find('=');
        if (separator_pos == std::string::npos || separator_pos == 0) {
            continue;
        }

        std::string key = trim(line.substr(0, separator_pos));
        std::string value = trim(line.substr(separator_pos + 1));

        if (!key.empty()) {
            module_info[key] = value;
        }
    }

    // 释放 getline 分配的内存
    free(line_buffer);

    // **重要**: fclose 会关闭底层的 fd，所以我们不需要再手动 close(fd)
    fclose(file_stream);

    return module_info;
}


void list_modules(std::string root){
    json all_modules = json::array();
    auto dir = opendir(root.c_str());
    if (!dir)
        return;
    int dfd = dirfd(dir);
    for (dirent *entry; (entry = readdir(dir));) {
        if (entry->d_type == DT_DIR && (0 != strcmp(entry->d_name, ".")) &&(0 != strcmp(entry->d_name, ".."))) {
            int modfd = openat(dfd, entry->d_name, O_RDONLY | O_CLOEXEC);
            int prop_fd = openat(modfd, "module.prop", O_RDONLY | O_CLOEXEC);
            if(prop_fd < 0){
                continue;
            }
            json module_prop = parse_module_prop_from_fd(prop_fd);
            module_prop["dirName"] = entry->d_name;
            if (faccessat(modfd, "zygisk/enable_app", F_OK, 0) == 0) {
                int enable_app_fd = openat(modfd, "zygisk/enable_app", O_RDONLY | O_CLOEXEC);
                std::string enable_app = parse_enable_app_from_fd(enable_app_fd);
                module_prop["enable_app"]=enable_app;
            }
            all_modules.push_back(module_prop);
            close(prop_fd);
            close(modfd);
        }
    }
    std::cout << all_modules.dump(4) << std::endl;
}
void add_enable_module( std::string module_path,char * module_name,char* packageName){

    std::string path = module_path+"/"+module_name+"/zygisk/enable_app";
    std::fstream file;
    file.open(path, std::ios_base::in | std::ios_base::out | std::ios_base::app);
    if (!file.is_open()) {
        std::cerr << "module :" << module_name<<" add: "<< packageName <<"failed"<< std::endl;
    }
    file << packageName << std::endl;
    file.close();
}

void module_help() {
    fprintf(stderr,
            R"EOF(

Usage: zygisk <COMMAND>

Commands:
  --unix_socket    Install module <ZIP>
  -v --daemonInfo    Install module <ZIP>
  -i --install    Install module <ZIP>
  uninstall  Uninstall module <id>
  -l --list       list all modules
  --add-enable   example( zygisk module --add-enable --module sample --package com.hepat.manager)
  -h --help       Print this message or the help of the given subcommand(s)

)EOF");
}

void install_modules(std::string root,std::string mod_tmp_path) {

    std::string modules_id;
    std::string prop = mod_tmp_path+"/module.prop";
    if (access(prop.c_str(), F_OK) == 0) {
        int prop_fd = open(prop.c_str(), O_RDONLY | O_CLOEXEC);
        json module_prop = parse_module_prop_from_fd(prop_fd);
        modules_id = module_prop.value("id","");
    }
    if(modules_id != ""){
        std::string modules_path = root+"/"+modules_id;
        if (access(modules_path.c_str(), F_OK) != 0) {
            int result = mkdir(modules_path.c_str(), 0755);  // 权限 rwxr-xr-x
        }
        copy_directory(mod_tmp_path.c_str(),modules_path.c_str());

    }else{
        std::cerr << " mod_tmp_path is not zygisk module , not id in module.prop " ;
    }

}
int main(int argc, char *argv[]) {
    if(argc == 0){
        module_help();
        return -1;
    }
    ProgramArgs args;
    module_parse_args(argc,argv, &args);
    if(args.help){
        module_help();
    }

    if(args.unix_socket_path!= nullptr){
        zygiskComm::InitRequestorSocket(args.unix_socket_path);
    } else {
        zygiskComm::InitRequestorSocket("d63138f231");
    }
    std::string version, model, RootDir;
    if(zygiskComm::RequestRunInfo(version,model,RootDir)== -1){
        std::cerr << "zygisk client request failed" ;
        return 0;
    }
    if(args.daemonInfo){
        std::cout << "version:"<< version <<std::endl ;
        std::cout << "model:"  << model   <<std::endl ;
        std::cout << "RootDir:"<< RootDir <<std::endl ;

    }else if(args.add_enable){
        add_enable_module(RootDir,args.module,args.package);
    }else if(args.mod_list){
        list_modules(RootDir);
    }else if(args.mod_install){

        install_modules(RootDir,args.mod_tmp_path);
    }
    return 0;
}