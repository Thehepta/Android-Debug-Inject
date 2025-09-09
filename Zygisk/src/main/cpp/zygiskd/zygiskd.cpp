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
#include "RootImp.h"
#include <thread>
#include <fcntl.h>
#include <sys/epoll.h>
#include <poll.h>
#include "companion.h"
#include "misc.hpp"
#include "vector"
#include "ksu.h"
#include "apatch.h"
#include "magisk.h"
#include "parse_args.h"
#include <sys/mman.h>
#include <sys/sendfile.h>
#include <asm-generic/unistd.h>

#define EPOLL_SIZE 10

# define LOG_TAG "zygiskd"

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
    if (faccessat(modfd, "zygisk/arm64-v8a.so", F_OK, 0) != 0) {
        return;
    }
    info.z32 = convert_to_memfd(info.z32,entry->d_name);
    if( info.z32 == -1){
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

void Zygiskd::collect_modules(){
    moduleRoot = MODULEROOT;
    auto dir = opendir(MODULEROOT);
    if (!dir)
        return;
    int dfd = dirfd(dir);
    for (dirent *entry; (entry = readdir(dir));) {
        if (entry->d_type == DT_DIR && (0 != strcmp(entry->d_name, "."))&& (0 != strcmp(entry->d_name, ".."))) {
            int modfd = openat(dfd, entry->d_name, O_RDONLY | O_CLOEXEC);
            foreach_module(dfd, entry, modfd);
            close(modfd);
        }
    }
}





void Zygiskd::rootImpInit() {
    const char *kernelsu = getenv("KSU");
    if (kernelsu != nullptr) {
        LOGD("Detected KernelSU (Version: %s)\n", kernelsu);
        rootImp =  new ksu();
        rootImp->init();
//        root_imp = PROCESS_ROOT_IS_KSU;
    }
    const char *apd = getenv("APATCH");
    if (apd != nullptr) {
        LOGD("Detected APATCH (Version: %s)\n", apd);
        rootImp =  new apatch();
        rootImp->init();

//        root_imp = PROCESS_ROOT_IS_APATCH;

    }
    if (RootImp::is_magisk_root()) {
        LOGD("Detected MAGISK\n");
        rootImp =  new magisk();
        rootImp->init();

//        root_imp = PROCESS_ROOT_IS_MAGISK;

    }
}



static void get_moddir(int client) {
    size_t index = socket_utils::read_usize(client);
    auto moduleDir = Zygiskd::getInstance().getModul_by_index(index);
    char buf[4096];
    ssprintf(buf, sizeof(buf), MODULEROOT "/%s", moduleDir.c_str());
    int dfd = open(buf, O_RDONLY | O_CLOEXEC);
    socket_utils::send_fd(client,dfd);
    close(dfd);
    LOGD("get_moddir:%s",buf);

}


void handle_daemon_action(int cmd,int fd) {
    LOGD("handle_daemon_action");

    switch (cmd) {
        case (uint8_t)zygiskComm::SocketAction::RequestLogcatFd:{
            LOGD("RequestLogcatFd");
            while (1){
                std::string msg =socket_utils::read_string(fd);
            }
            break;
        }

        case (uint8_t)zygiskComm::SocketAction::GetProcessFlags:{
            LOGD("GetProcessFlags");
            uid_t uid =socket_utils::read_u32(fd);
            int flags =  Zygiskd::getRootImp()->getProcessFlags(uid);
            socket_utils::write_u32(fd,flags);
            break;
        }
        case (uint8_t)zygiskComm::SocketAction::ReadModules:
            LOGD("ReadModules");
            zygiskComm::WriteModules(fd,Zygiskd::getInstance().getModule_list());
            break;
        case (uint8_t)zygiskComm::SocketAction::RequestCompanionSocket:{
            LOGD("RequestCompanionSocket");
            uint32_t arch = socket_utils::read_u32(fd);
            connect_companion(fd,arch);
            break;
        }
        case (uint8_t)zygiskComm::SocketAction::GetModuleDir:
            LOGD("GetModuleDir");
            get_moddir(fd);
            break;
    }

    close(fd);
}

void zygiskd_handle(int client_fd){
    uint8_t cmd =  socket_utils::read_u8(client_fd);
    switch (cmd) {
        case (uint8_t)zygiskComm::SocketAction::PingHeartBeat:
            LOGD("HandleEvent PingHeartBeat");
            break;
        case (uint8_t)zygiskComm::SocketAction::CacheMountNamespace: {
            pid_t pid =socket_utils::read_u32(client_fd);
            Zygiskd::getInstance().getRootImp()->cache_mount_namespace(pid);
        }
        case (uint8_t)zygiskComm::SocketAction::UpdateMountNamespace:{
            zygiskComm::MountNamespace type  = static_cast<zygiskComm::MountNamespace>(socket_utils::read_u8(client_fd));
            int fd = Zygiskd::getInstance().getRootImp()->update_mount_namespace(type);
            socket_utils::write_u32(client_fd,getpid());
            socket_utils::write_u32(client_fd,fd);
            break;
        }

        case (uint8_t)zygiskComm::SocketAction::ZygoteRestart:
            LOGD("HandleEvent ZygoteRestart");
            break;

        case (uint8_t)zygiskComm::SocketAction::SystemServerStarted:
            LOGD("HandleEvent SystemServerStarted");
            break;
        default:
            LOGD("HandleEvent default");
            int new_fd = dup(client_fd);
            std::thread ZygiskThread(handle_daemon_action,cmd, new_fd);
            ZygiskThread.detach();
    }
}

void zygiskd_main(char * exec_path ,const char* requestSocketPath){

    int client_fd;
    int epfd;

    zygiskComm::InitRequestorSocket(requestSocketPath);
    Zygiskd::getInstance().set_exec_path(exec_path);
    Zygiskd::getInstance().collect_modules();
    Zygiskd::getInstance().rootImpInit();
    int server_fd = socket(AF_UNIX,SOCK_STREAM,0);

    if (server_fd < 0){
        LOGE("Create Socket Errors, %d",server_fd);
        exit(server_fd);
    }
    struct sockaddr_un addr;
    zygiskComm::set_sockaddr(addr);
    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        LOGE("bind error");
        return ;
    }

    if (listen(server_fd, 10)<0)
    {
        LOGE("listen error");
        return ;
    }

    epfd = epoll_create(EPOLL_SIZE);
    if (epfd < 0)
    {
        LOGE("Epoll Create");
        exit(-1);
    }

    struct epoll_event ev;
    struct epoll_event events[EPOLL_SIZE];
    ev.data.fd = server_fd;
    ev.events = EPOLLIN ;
    epoll_ctl(epfd, EPOLL_CTL_ADD, server_fd, &ev);
    fcntl(server_fd, F_SETFL, fcntl(server_fd, F_GETFD, 0)| O_NONBLOCK);

    while (1)
    {
        int nfds = epoll_wait(epfd, events, EPOLL_SIZE, -1);
        if (nfds == -1)
        {
            if(errno != EINTR)
                LOGE("epoll failed");
            continue;
        }
        for (int i=0;i < nfds;i++)
        {
            if (events[i].data.fd==server_fd&&(events[i].events & EPOLLIN))
            {
                struct sockaddr_in remote_addr;

                socklen_t socklen = sizeof(struct sockaddr_in);
                client_fd = accept(server_fd, (struct sockaddr *)&remote_addr, &socklen);
                if (client_fd > 0)
                {
                    zygiskd_handle(client_fd);
                }
                close(client_fd);
            }else{
                LOGE("unknow  error else");
            }
        }
    }
    close(server_fd);
    close(epfd);
    return ;
}


int main(int argc, char *argv[]) {
    LOGD("zygiskd start run , arg1:%s",argv[1]);

    if (argc > 1) {
        if (strcmp(argv[1], "companion") == 0) {
            if (argc < 3) {
                LOGI("Usage: zygiskd companion <fd>\n");
                return 1;
            }
            int fd = atoi(argv[2]);
            companion_entry(fd);
            return 0;
        }else {
            zygiskd_main(argv[0],argv[2]);
            LOGI("Usage: zygiskd [companion|version|root|unix_socket]\n");
            return 0;
        }
    }
    return 0;
}