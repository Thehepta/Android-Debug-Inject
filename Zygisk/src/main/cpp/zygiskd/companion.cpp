//
// Created by chic on 2025/4/5.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <linux/limits.h>
#include <asm-generic/poll.h>
#include "logging.h"
#include "socket_utils.h"
#include <poll.h>
#include <android/dlext.h>
#include "dl.h"
#include "zygiskd.h"
#include "misc.hpp"
#include <vector>
#include <thread>

# define LOG_TAG "zygiskCommpanion"

using comp_entry = void(*)(int);

typedef void (*zygisk_companion_entry)(int);

struct companion_module_thread_args {
    int fd;
    zygisk_companion_entry entry;
};

zygisk_companion_entry load_module(int fd) {
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "/proc/self/fd/%d", fd);

    void *handle = DlopenExt(path, RTLD_NOW);

    if (!handle) return NULL;

    void *entry = dlsym(handle, "zygisk_companion_entry");
    if (!entry) {
        LOGE("Failed to dlsym zygisk_companion_entry: %s\n", dlerror());

        dlclose(handle);

        return NULL;
    }

    return (zygisk_companion_entry)entry;
}



void entry_thread(int client,comp_entry entry){
    struct stat s1;
    fstat(client, &s1);
    entry(client);
    // Only close client if it is the same file so we don't
    // accidentally close a re-used file descriptor.
    // This check is required because the module companion
    // handler could've closed the file descriptor already.
    if (struct stat s2; fstat(client, &s2) == 0) {
        if (s1.st_dev == s2.st_dev && s1.st_ino == s2.st_ino) {
            close(client);
        }
    }
}


/* WARNING: Dynamic memory based */
void companion_entry(int socket) {
    LOGW("start companion_entry:%d",getpid());

    if (getuid() != 0 || fcntl(socket, F_GETFD) < 0)
        exit(-1);



    // Load modules
    std::vector<comp_entry> modules;
    {
        std::vector<int> module_fds = socket_utils::recv_fds(socket);
        for (int fd : module_fds) {
            comp_entry entry = nullptr;
            struct stat s{};

            if (fstat(fd, &s) == 0 && S_ISREG(s.st_mode)) {
                android_dlextinfo info {
                        .flags = ANDROID_DLEXT_USE_LIBRARY_FD,
                        .library_fd = fd,
                };
                if (void *h = android_dlopen_ext("/jit-cache", RTLD_LAZY, &info)) {
                    *(void **) &entry = dlsym(h, "zygisk_companion_entry");
                } else {
                    LOGW("Failed to dlopen zygisk module: %s\n", dlerror());
                }
            }
            modules.push_back(entry);
            close(fd);
        }
    }

    // ack
    socket_utils::write_u32(socket, 0);

    // Start accepting requests
    pollfd pfd = { socket, POLLIN, 0 };
    for (;;) {
        poll(&pfd, 1, -1);
        if (pfd.revents && !(pfd.revents & POLLIN)) {
            // Something bad happened in magiskd, terminate zygiskd
            exit(0);
        }
        int client = socket_utils::recv_fd(socket);
        if (client < 0) {
            // Something bad happened in magiskd, terminate zygiskd
            exit(0);
        }
        int module_id = socket_utils::read_u32(client);

        if (module_id >= 0 && module_id < modules.size() && modules[module_id]) {
            std::thread new_thread(entry_thread,client,modules[module_id]);
            new_thread.detach(); // 线程分离，主线程不等待
        } else {
            close(client);
        }
    }

}

static std::vector<int> get_module_fds(bool is_64_bit) {
    std::vector<int> fds;
    // All fds passed to send_fds have to be valid file descriptors.
    // To workaround this issue, send over STDOUT_FILENO as an indicator of an
    // invalid fd as it will always be /dev/null in magiskd
    auto module_list = Zygiskd::getInstance().getModule_list();
#if defined(__LP64__)
    if (is_64_bit) {
        std::transform(module_list.begin(), module_list.end(), std::back_inserter(fds),
                       [](const Module &info) { return info.z64 < 0 ? STDOUT_FILENO : info.z64; });
    } else
#endif
    {
        std::transform(module_list.begin(), module_list.end(), std::back_inserter(fds),
                       [](const Module &info) { return info.z32 < 0 ? STDOUT_FILENO : info.z32; });
    }
    return fds;
}


static pthread_mutex_t zygiskd_lock = PTHREAD_MUTEX_INITIALIZER;
static int zygiskd_sockets[] = {-1, -1};


#define zygiskd_socket zygiskd_sockets[is_64_bit]

void connect_companion(int client, bool is_64_bit,std::string exe_path) {
    mutex_guard g(zygiskd_lock);

    if (zygiskd_socket >= 0) {
        // Make sure the socket is still valid
        pollfd pfd = {zygiskd_socket, 0, 0};
        poll(&pfd, 1, 0);
        if (pfd.revents) {
            // Any revent means error
            close(zygiskd_socket);
            zygiskd_socket = -1;
        }
    }
    if (zygiskd_socket < 0) {
        int fds[2];
        socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, fds);
        zygiskd_socket = fds[0];
        if (fork_dont_care() == 0) {
#if defined(__LP64__)
            std::string exe = exe_path;
#else
            std::string exe = exe_path+"32";
#endif
            // This fd has to survive exec
            fcntl(fds[1], F_SETFD, 0);
            char buf[16];
            ssprintf(buf, sizeof(buf), "%d", fds[1]);
            LOGD("zygiskd startup companion %s", exe.c_str());
            execl(exe.c_str(), "zygiskd", "companion", buf, (char *) nullptr);
            exit(-1);
        }
        close(fds[1]);
        std::vector<int> module_fds = get_module_fds(is_64_bit);
        socket_utils::send_fds(zygiskd_socket, module_fds.data(), module_fds.size());
        // Wait for ack
        if (socket_utils::read_u32(zygiskd_socket) != 0) {
            LOGE("zygiskd startup error\n");
            return;
        }
    }
    socket_utils::send_fd(zygiskd_socket, client);
}
