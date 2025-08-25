#include <linux/un.h>
#include <sys/socket.h>
#include <unistd.h>

#include "daemon.h"
#include "socket_utils.h"


namespace zygiskComm {

    static std::string TMP_PATH;



    void InitRequestorSocket(const char *path) {
        TMP_PATH = path;
    }

    std::string GetRequestorSocket() {
        return TMP_PATH;
    }

    int Connect(uint8_t retry) {
        int fd = socket( AF_UNIX, SOCK_STREAM|SOCK_CLOEXEC, 0 );
        struct sockaddr_un remote_addr; //服务器端网络地址结构体
        set_sockaddr(remote_addr);

        while (retry--) {
            int r = connect(fd, reinterpret_cast<struct sockaddr*>(&remote_addr), sizeof(remote_addr));
            if (r == 0) return fd;
            if (retry) {
                PLOGE("Retrying to connect to zygiskd, sleep 1s\n");
                sleep(1);
            }
        }

        close(fd);
        return -1;
    }

    void set_sockaddr(struct sockaddr_un &addr ){
        memset(&addr,0,sizeof(addr));
        addr.sun_family=AF_UNIX;
        strcpy(addr.sun_path+1, GetRequestorSocket().c_str());
        addr.sun_path[0]='\0';
    }

    bool PingHeartbeat() {
        UniqueFd fd = Connect(5);
        if (fd == -1) {
            PLOGE("Connect to zygiskd");
            return false;
        }
        socket_utils::write_u8(fd, (uint8_t) SocketAction::PingHeartBeat);
        return true;
    }

    int RequestLogcatFd() {
        int fd = Connect(1);
        if (fd == -1) {
            PLOGE("RequestLogcatFd");
            return -1;
        }
        socket_utils::write_u8(fd, (uint8_t) SocketAction::RequestLogcatFd);
        return fd;
    }

    uint32_t GetProcessFlags(uid_t uid,std::string nice_name) {
        UniqueFd fd = Connect(1);
        if (fd == -1) {
            PLOGE("GetProcessFlags");
            return 0;
        }
        socket_utils::write_u8(fd, (uint8_t) SocketAction::GetProcessFlags);
        socket_utils::write_u32(fd, uid);
        socket_utils::write_string(fd, nice_name);
        return socket_utils::read_u32(fd);
    }
//这个函数会编译进注入库，同时被32为和64位使用，请求不同架构的文件描述符
    std::vector<Module> ReadModules(std::string process) {
        std::vector<Module> modules;
        UniqueFd fd = Connect(1);
        if (fd == -1) {
            PLOGE("ReadModules failed");
            return modules;
        }
        socket_utils::write_u8(fd, (uint8_t) SocketAction::ReadModules);
        socket_utils::write_string(fd, process);
        uint8_t arch=0;
#if defined(__LP64__)
        arch=1;
#endif
        socket_utils::write_u8(fd, arch);

        size_t len = socket_utils::read_usize(fd);
        for (size_t i = 0; i < len; i++) {
            Module md;
            std::string name = socket_utils::read_string(fd);
            int module_fd = socket_utils::recv_fd(fd);
            md.name = name;
            if(arch == 1){
                md.z64=module_fd;
            } else{
                md.z32=module_fd;
            }
            modules.emplace_back(md);
        }
        return modules;
    }


    int RequestRunInfo(std::string &version,std::string & model,std::string& RootDir) {
        UniqueFd fd = Connect(1);
        if (fd == -1) {
            PLOGE("CacheMountNamespace");
            return -1;
        }
        socket_utils::write_u8(fd, (uint8_t) SocketAction::RequestRunInfo);
        version = socket_utils::read_string(fd);
        model = socket_utils::read_string(fd);
        RootDir = socket_utils::read_string(fd);
        return 0;
    }

    std::string GetRootDir() {
        UniqueFd fd = Connect(1);
        if (fd == -1) {
            PLOGE("CacheMountNamespace");
        }
        socket_utils::write_u8(fd, (uint8_t) SocketAction::GetRootDir);
        std::string RootModule = socket_utils::read_string(fd);
        return RootModule;
    }



// 这个是实现的zygisk请求,向zygiskd服务发送请求,但是因为zygiskd 也是我们自己实现的
// 我们需要和zygiskd 服务中的这个请求的处理相匹配.
// 如果我们想要和别的zygiskd服务兼容需要去兼容别的zygisk服务
    int ConnectCompanion(size_t index) {
        int fd = Connect(1);
        if (fd == -1) {
            PLOGE("ConnectCompanion");
            return -1;
        }
        socket_utils::write_u8(fd, (uint8_t) SocketAction::RequestCompanionSocket);
#ifdef __LP64__
        socket_utils::write_u32(fd, 1);
#else
        socket_utils::write_usize(fd, 0);
#endif
        socket_utils::write_u32(fd, index);
        return fd;
        // magisk zygis 这里是没有等待的,所以这里也不等待
//        if (socket_utils::read_u8(fd) == 1) {
//            return fd;
//        } else {
//            close(fd);
//            return -1;
//        }
    }

    int GetModuleDir(size_t index) {
        UniqueFd fd = Connect(1);
        if (fd == -1) {
            PLOGE("GetModuleDir");
            return -1;
        }
        socket_utils::write_u8(fd, (uint8_t) SocketAction::GetModuleDir);
        socket_utils::write_usize(fd, index);
        return socket_utils::recv_fd(fd);
    }



    void ZygoteRestart() {
        UniqueFd fd = Connect(1);
        if (fd == -1) {
            if (errno == ENOENT) {
                LOGD("Could not notify ZygoteRestart (maybe it hasn't been created)");
            } else {
                LOGD("Could not notify ZygoteRestart");
            }
            return;
        }
        if (!socket_utils::write_u8(fd, (uint8_t) SocketAction::ZygoteRestart)) {
            LOGD("Failed to request ZygoteRestart");
        }
    }

    void SystemServerStarted() {
        UniqueFd fd = Connect(1);
        if (fd == -1) {
            PLOGE("Failed to report system server started");
        } else {
            if (!socket_utils::write_u8(fd, (uint8_t) SocketAction::SystemServerStarted)) {
                PLOGE("Failed to report system server started");
            }
        }
    }
}
