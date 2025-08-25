//
// Created by chic on 2024/12/4.
//

#include "zygiskd.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>          /* See NOTES */
#include <vector>
#include <linux/in.h>
#include "socket_utils.h"
#include <thread>
#include <fcntl.h>
#include <sys/epoll.h>
#include <poll.h>
#include <asm-generic/unistd.h>
#include <linux/memfd.h>
#include "companion.h"
#include "misc.hpp"
#include "vector"
#include "daemon_parse_args.h"
#include<sys/sendfile.h>
#define EPOLL_SIZE 10

# define LOG_TAG "zygiskd"

std::string exe_path ;

int convert_to_memfd(int fd,char * name){
    int memfd = syscall(__NR_memfd_create, "jit-cache", MFD_CLOEXEC);
    if (memfd >= 0) {
        sendfile(memfd, fd, nullptr, INT_MAX);
        close(fd);
    } else {
        LOGE("zygiskd memfd_create: %s  failed",name);
    }
    return memfd;
}

void Zygiskd::foreach_module(int fd, dirent *entry, int modfd) {
    if (faccessat(modfd, "disable", F_OK, 0) == 0) {
        return;
    }
    Module info;
    LOGD("enable_module:%s", entry->d_name);
    if (faccessat(modfd, "zygisk/armeabi-v7a.so", F_OK, 0) != 0) {
        return;
    }
    info.z32 = openat(modfd, "zygisk/armeabi-v7a.so", O_RDONLY | O_CLOEXEC);
    info.z32 = convert_to_memfd(info.z32,entry->d_name);
    if( info.z32 == -1){
        return;
    }


    if (faccessat(modfd, "zygisk/arm64-v8a.so", F_OK, 0) != 0) {
        return;
    }
    info.z64 = openat(modfd, "zygisk/arm64-v8a.so", O_RDONLY | O_CLOEXEC);
    info.z64 = convert_to_memfd(info.z64,entry->d_name);
    if( info.z64 == -1){
        return;
    }
    info.name = entry->d_name;
    module_list.push_back(info);

}



void Zygiskd::collect_modules() {

    if(module_list.size()>0){
        for(Module m:module_list){
            close(m.z64);
            close(m.z32);
        }
        module_list.clear();
    }
    auto dir = opendir(moduleRoot.c_str());
    if (!dir)
        return;
    int dfd = dirfd(dir);
    for (dirent *entry; (entry = readdir(dir));) {
        if (entry->d_type == DT_DIR && (0 != strcmp(entry->d_name, ".")) &&
            (0 != strcmp(entry->d_name, ".."))) {
            int modfd = openat(dfd, entry->d_name, O_RDONLY | O_CLOEXEC);
            foreach_module(dfd, entry, modfd);
            close(modfd);
        }
    }
}



int Zygiskd::getProcessFlags(uid_t uid,std::string nice_name) {
//    exec_sql("");
    return 0;
}

std::vector<Module> Zygiskd::getEnableModules(std::string processName){

    if(model){
        std::vector<Module> enbale_vec;
        size_t len = module_list.size();
        char file_path[4096];
        for (size_t i = 0; i < len; i++) {
            Module m = module_list[i];
            ssprintf(file_path, sizeof(file_path),  "%s/%s/zygisk/enable_app",getModuleRoot().c_str(), m.name.c_str());
            FILE *file = fopen(file_path, "r");
            if (file == NULL) {
                continue;
            }
            char *line = NULL; // **重要**：初始化为 NULL，让 getline 自动分配内存
            size_t len = 0;    // **重要**：初始化为 0
            ssize_t read;      // 用于接收 getline 的返回值
            int line_number = 0;
            while ((read = getline(&line, &len, file)) != -1) {
                line_number++;

                // 可选：移除行末的换行符
                if (read > 0 && line[read - 1] == '\n') {
                    line[read - 1] = '\0';
                }

                if (strstr(line, processName.c_str()) != NULL) {
                    enbale_vec.push_back(m);
                }
            }
            if(line){
                free(line);
            }
            fclose(file);
        }
        return enbale_vec;
    } else{
       return module_list;
    }

}


void handle_daemon_action(int cmd, int fd) {
    LOGD("handle_daemon_action");

    switch (cmd) {
        case (uint8_t) zygiskComm::SocketAction::RequestLogcatFd: {
            LOGD("RequestLogcatFd");
            while (1) {
                std::string msg = socket_utils::read_string(fd);
            }
            break;
        }

        case (uint8_t) zygiskComm::SocketAction::GetProcessFlags: {
            LOGD("GetProcessFlags");
            uid_t uid = socket_utils::read_u32(fd);
            std::string nice_name = socket_utils::read_string(fd);
            int flags = Zygiskd::getInstance().getProcessFlags(uid,nice_name);
            socket_utils::write_u32(fd, flags);
            break;
        }
        case (uint8_t) zygiskComm::SocketAction::ReadModules:{
            std::string nice_name = socket_utils::read_string(fd);
            std::vector<Module> modules =  Zygiskd::getInstance().getEnableModules(nice_name);
            uint8_t arch = socket_utils::read_u8(fd);
            size_t len = modules.size();
            socket_utils::write_usize(fd,len);

            for (size_t i = 0; i < len; i++) {
                LOGD("ReadModule enable %s ",modules[i].name.c_str());
                socket_utils::write_string(fd,modules[i].name);
                if(arch == 1){
                    socket_utils::send_fd(fd,modules[i].z64);
                } else{
                    socket_utils::send_fd(fd,modules[i].z32);
                }
            }
            break;
        }

        case (uint8_t) zygiskComm::SocketAction::RequestCompanionSocket: {
            LOGD("RequestCompanionSocket");
            uint32_t arch = socket_utils::read_u32(fd);
            connect_companion(fd, arch,exe_path);
            break;
        }
        case (uint8_t) zygiskComm::SocketAction::GetModuleDir:
            LOGD("GetModuleDir");
            size_t index = socket_utils::read_usize(fd);
            auto moduleDir = Zygiskd::getInstance().getModul_by_index(index);
            char buf[4096];
            ssprintf(buf, sizeof(buf),  "%s/%s",Zygiskd::getInstance().getModuleRoot().c_str(), moduleDir.c_str());
            int dfd = open(buf, O_RDONLY | O_CLOEXEC);
            socket_utils::send_fd(fd, dfd);
            close(dfd);
            break;
    }

    close(fd);
}

void zygiskd_handle(int client_fd) {
    uint8_t cmd = socket_utils::read_u8(client_fd);
    switch (cmd) {
        case (uint8_t) zygiskComm::SocketAction::PingHeartBeat:
            LOGD("HandleEvent PingHeartBeat");
            break;
        case (uint8_t) zygiskComm::SocketAction::RequestRunInfo: {
            socket_utils::write_string(client_fd,"version:text");
            socket_utils::write_string(client_fd,"enfore");
            socket_utils::write_string(client_fd,Zygiskd::getInstance().getModuleRoot());
            break;
        }
        case (uint8_t) zygiskComm::SocketAction::GetRootDir: {
            socket_utils::write_string(client_fd,Zygiskd::getInstance().getModuleRoot());
            break;
        }

        case (uint8_t) zygiskComm::SocketAction::ZygoteRestart:
            LOGD("HandleEvent ZygoteRestart");
            break;

        case (uint8_t) zygiskComm::SocketAction::SystemServerStarted:
            LOGD("HandleEvent SystemServerStarted");
            break;
        default:
            LOGD("HandleEvent default");
            int new_fd = dup(client_fd);
            std::thread ZygiskThread(handle_daemon_action, cmd, new_fd);
            ZygiskThread.detach();
    }
}

void zygiskd_main() {

    int client_fd;
    int epfd;

    int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd < 0) {
        LOGE("Create Socket Errors, %d", server_fd);
        exit(server_fd);
    }
    struct sockaddr_un addr;
    zygiskComm::set_sockaddr(addr);
    if (bind(server_fd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        LOGE("bind error");
        return;
    }

    if (listen(server_fd, 10) < 0) {
        LOGE("listen error");
        return;
    }

    epfd = epoll_create(EPOLL_SIZE);
    if (epfd < 0) {
        LOGE("Epoll Create");
        exit(-1);
    }

    struct epoll_event ev;
    struct epoll_event events[EPOLL_SIZE];
    ev.data.fd = server_fd;
    ev.events = EPOLLIN;
    epoll_ctl(epfd, EPOLL_CTL_ADD, server_fd, &ev);
    fcntl(server_fd, F_SETFL, fcntl(server_fd, F_GETFD, 0) | O_NONBLOCK);

    while (1) {
        int nfds = epoll_wait(epfd, events, EPOLL_SIZE, -1);
        if (nfds == -1) {
            if (errno != EINTR)
                LOGE("epoll failed");
            continue;
        }
        for (int i = 0; i < nfds; i++) {
            if (events[i].data.fd == server_fd && (events[i].events & EPOLLIN)) {
                struct sockaddr_in remote_addr;

                socklen_t socklen = sizeof(struct sockaddr_in);
                client_fd = accept(server_fd, (struct sockaddr *) &remote_addr, &socklen);
                if (client_fd > 0) {
                    zygiskd_handle(client_fd);
                }
                close(client_fd);
            } else {
                LOGE("unknow  error else");
            }
        }
    }
    close(server_fd);
    close(epfd);
    return;
}

void usage() {
    fprintf(stderr,
            R"EOF(Magisk - Multi-purpose Utility

Usage: zygiskd [options]  [arguments]

Options:
   daemon                    print current binary version
   companion                 print running daemon version
   -V                        print running daemon version code


Advanced Options (Internal APIs):
   --help                   manually start magisk daemon
   --verbose                remove all magisk changes and stop daemon
   --model                  callback on init triggers. Valid triggers:
   --rootdir                set BLKROSET flag to OFF for all block devices
   --unix_socket            restore selinux context on Magisk files
)EOF");

    fprintf(stderr, "\n\n");
    exit(1);
}
int main(int argc, char *argv[]) {
    if (argc < 2)
        usage();
    exe_path = argv[0];
    if ((strcmp(argv[1], "companion") == 0) && (argc == 3)) {
        int fd = atoi(argv[2]);
        companion_entry(fd);
        return 0;
    }


    if ((strcmp(argv[1], "daemon") == 0)) {
        DaemonProgramArgs args;
        daemon_parse_args(argc - 1, ++argv, &args);
        if(args.unix_socket_path!= nullptr){
            zygiskComm::InitRequestorSocket(args.unix_socket_path);
        } else {
            zygiskComm::InitRequestorSocket("d63138f231");
        }
        if(args.rootdir_path != nullptr){
            Zygiskd::getInstance().setModuleRoot(args.rootdir_path);
        } else {
            Zygiskd::getInstance().setModuleRoot(MODULEROOT);
        }
        Zygiskd::getInstance().setModel(args.model);
        Zygiskd::getInstance().collect_modules();
        zygiskd_main();
    }


    return 0;
}


