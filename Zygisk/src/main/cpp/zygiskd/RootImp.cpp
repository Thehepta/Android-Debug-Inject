//
// Created by chic on 2024/12/4.
//

#include <sys/prctl.h>
#include <cstdio>
#include <sys/stat.h>
#include <stdlib.h>
#include <linux/fs.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/mount.h>
#include <sys/sysmacros.h>
#include "RootImp.h"
#include "logging.h"
#include "socket_utils.h"

# define LOG_TAG "zygiskRootImp"


//bool exec_command(char *restrict buf, size_t len, const char *restrict file, char *const argv[]) {
//    int link[2];
//    pid_t pid;
//
//    if (pipe(link) == -1) {
//        LOGE("pipe: %s\n", strerror(errno));
//
//        return false;
//    }
//
//    if ((pid = fork()) == -1) {
//        LOGE("fork: %s\n", strerror(errno));
//
//        close(link[0]);
//        close(link[1]);
//
//        return false;
//    }
//
//    if (pid == 0) {
//        dup2(link[1], STDOUT_FILENO);
//        close(link[0]);
//        close(link[1]);
//
//        execv(file, argv);
//
//        LOGE("execv failed: %s\n", strerror(errno));
//        _exit(1);
//    } else {
//        close(link[1]);
//
//        ssize_t nbytes = read(link[0], buf, len);
//        if (nbytes > 0) buf[nbytes - 1] = '\0';
//            /* INFO: If something went wrong, at least we must ensure it is NULL-terminated */
//        else buf[0] = '\0';
//
//        wait(NULL);
//
//        close(link[0]);
//    }
//
//    return true;
//}

int RootImp::getProcessFlags(uid_t uid){
    int flag = 0;
//    if(uid_is_manager(uid)){
//        flag|=PROCESS_IS_MANAGER;
//    } else{
//        if(uid_granted_root(uid)) {
//            flag|=PROCESS_GRANTED_ROOT;
//        }
//        if (uid_should_umount(uid)) {
//            flag |= PROCESS_ON_DENYLIST;
//        }
//    }
    return flag;
}




int RootImp::is_command_available(const char *command) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "which %s", command);  // 使用 which 命令检查命令是否存在
    FILE *fp = popen(cmd, "r");
    if (fp == NULL) {
        return 0;  // 如果打开管道失败，返回 0
    }

    char result[256];
    if (fgets(result, sizeof(result), fp) != NULL) {
        // 如果读取到结果，表示命令存在
        fclose(fp);
        return 1;
    } else {
        // 如果没有读取到结果，表示命令不存在
        fclose(fp);
        return 0;
    }
}

bool RootImp::uid_is_manager(uid_t uid) const {
    return uid == this->manager_uid;
}



bool RootImp::is_magisk_root(){
    int version=0;

    if (is_command_available("magisk")) {
        FILE *fp = popen("magisk -V", "r");
        if (fp == NULL) {
            return false;  // 如果打开管道失败，返回 0
        }
        char result[256];
        if (fgets(result, sizeof(result), fp) != NULL) {
            // 如果读取到结果，表示命令存在
            fclose(fp);
            version = atoi(result);
        } else {
            // 如果没有读取到结果，表示命令不存在
            fclose(fp);
            return false;
        }
    } else {
        printf("Magisk command is not available.\n");
        return false;
    }
    if(version <= 0){
        return false;
    } else {
        return true;

    }

    return false;
}

bool switch_mount_namespace(pid_t pid) {
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "/proc/%d/ns/mnt", pid);
    int nsfd = open(path, O_RDONLY | O_CLOEXEC);
    if (nsfd == -1) {
        LOGE("Failed to open nsfd: %s\n", strerror(errno));
        return false;
    }

    if (setns(nsfd, CLONE_NEWNS) == -1) {
        LOGE("Failed to setns: %s\n", strerror(errno));
        close(nsfd);
        return false;
    }
    close(nsfd);
    return true;
}

bool parse_mountinfo(const char * pid, struct mountinfos * mounts) {
    char path[PATH_MAX];
    snprintf(path, PATH_MAX, "/proc/%s/mountinfo", pid);

    FILE *mountinfo = fopen(path, "r");
    if (mountinfo == NULL) {
        LOGE("fopen: %s\n", strerror(errno));

        return false;
    }

    char line[PATH_MAX];
    size_t i = 0;

    mounts->mounts = NULL;
    mounts->length = 0;

    while (fgets(line, sizeof(line), mountinfo) != NULL) {
        int root_start = 0, root_end = 0;
        int target_start = 0, target_end = 0;
        int vfs_option_start = 0, vfs_option_end = 0;
        int type_start = 0, type_end = 0;
        int source_start = 0, source_end = 0;
        int fs_option_start = 0, fs_option_end = 0;
        int optional_start = 0, optional_end = 0;
        unsigned int id, parent, maj, min;
        sscanf(line,
               "%u "           // (1) id
               "%u "           // (2) parent
               "%u:%u "        // (3) maj:min
               "%n%*s%n "      // (4) mountroot
               "%n%*s%n "      // (5) target
               "%n%*s%n"       // (6) vfs options (fs-independent)
               "%n%*[^-]%n - " // (7) optional fields
               "%n%*s%n "      // (8) FS type
               "%n%*s%n "      // (9) source
               "%n%*s%n",      // (10) fs options (fs specific)
               &id, &parent, &maj, &min, &root_start, &root_end, &target_start,
               &target_end, &vfs_option_start, &vfs_option_end,
               &optional_start, &optional_end, &type_start, &type_end,
               &source_start, &source_end, &fs_option_start, &fs_option_end);

        mounts->mounts = (struct mountinfo *)realloc(mounts->mounts, (i + 1) * sizeof( mountinfo));
        if (!mounts->mounts) {
            LOGE("Failed to allocate memory for mounts->mounts");

            fclose(mountinfo);
//            free_mounts(mounts);

            return false;
        }

        unsigned int shared = 0;
        unsigned int master = 0;
        unsigned int propagate_from = 0;
        if (strstr(line + optional_start, "shared:")) {
            shared = (unsigned int)atoi(strstr(line + optional_start, "shared:") + 7);
        }

        if (strstr(line + optional_start, "master:")) {
            master = (unsigned int)atoi(strstr(line + optional_start, "master:") + 7);
        }

        if (strstr(line + optional_start, "propagate_from:")) {
            propagate_from = (unsigned int)atoi(strstr(line + optional_start, "propagate_from:") + 15);
        }

        mounts->mounts[i].id = id;
        mounts->mounts[i].parent = parent;
        mounts->mounts[i].device = (dev_t)(makedev(maj, min));
        mounts->mounts[i].root = strndup(line + root_start, (size_t)(root_end - root_start));
        mounts->mounts[i].target = strndup(line + target_start, (size_t)(target_end - target_start));
        mounts->mounts[i].vfs_option = strndup(line + vfs_option_start, (size_t)(vfs_option_end - vfs_option_start));
        mounts->mounts[i].optional.shared = shared;
        mounts->mounts[i].optional.master = master;
        mounts->mounts[i].optional.propagate_from = propagate_from;
        mounts->mounts[i].type = strndup(line + type_start, (size_t)(type_end - type_start));
        mounts->mounts[i].source = strndup(line + source_start, (size_t)(source_end - source_start));
        mounts->mounts[i].fs_option = strndup(line + fs_option_start, (size_t)(fs_option_end - fs_option_start));

        i++;
    }

    fclose(mountinfo);

    mounts->length = i;

    return true;
}


void free_mounts(struct mountinfos * mounts) {
    for (size_t i = 0; i < mounts->length; i++) {
        free((void *)mounts->mounts[i].root);
        free((void *)mounts->mounts[i].target);
        free((void *)mounts->mounts[i].vfs_option);
        free((void *)mounts->mounts[i].type);
        free((void *)mounts->mounts[i].source);
        free((void *)mounts->mounts[i].fs_option);
    }

    free((void *)mounts->mounts);
}


bool revert_unmount(int rootImp) {
    /* INFO: We are already in the target pid mount namespace, so actually,
               when we use self here, we meant its pid.
    */
    struct mountinfos mounts;
    if (!parse_mountinfo("self", &mounts)) {
        LOGE("Failed to parse mountinfo\n");

        return false;
    }

    /* INFO: "Magisk" is the longest word that will ever be put in source_name */
    char source_name[sizeof("magisk")];
    if (rootImp == PROCESS_ROOT_IS_KSU) strcpy(source_name, "KSU");
    else if (rootImp == PROCESS_ROOT_IS_APATCH) strcpy(source_name, "APatch");
    else strcpy(source_name, "magisk");

    LOGI("[%s] Unmounting root", source_name);

    const char **targets_to_unmount = NULL;
    size_t num_targets = 0;

    for (size_t i = 0; i < mounts.length; i++) {
        struct mountinfo mount = mounts.mounts[i];

        bool should_unmount = false;
        /* INFO: The root implementations have their own /system mounts, so we
                    only skip the mount if they are from a module, not Magisk itself.
        */
        if (strncmp(mount.target, "/system/", strlen("/system/")) == 0 &&
            strncmp(mount.root, "/adb/modules/", strlen("/adb/modules/")) == 0 &&
            strncmp(mount.target, "/system/etc/", strlen("/system/etc/")) != 0) continue;

        if (strcmp(mount.source, source_name) == 0) should_unmount = true;
        if (strncmp(mount.target, "/data/adb/modules", strlen("/data/adb/modules")) == 0) should_unmount = true;
        if (strncmp(mount.root, "/adb/modules/", strlen("/adb/modules/")) == 0) should_unmount = true;

        if (!should_unmount) continue;

        num_targets++;
        targets_to_unmount = static_cast<const char **>(realloc(targets_to_unmount,num_targets * sizeof(char *)));
        if (targets_to_unmount == NULL) {
            LOGE("[%s] Failed to allocate memory for targets_to_unmount\n", source_name);

            free(targets_to_unmount);
            free_mounts(&mounts);

            return false;
        }

        targets_to_unmount[num_targets - 1] = mount.target;
    }

    for (size_t i = num_targets; i > 0; i--) {
        const char *target = targets_to_unmount[i - 1];
        if (umount2(target, MNT_DETACH) == -1) {
            LOGE("[%s] Failed to unmount %s: %s\n", source_name, target, strerror(errno));
        } else {
            LOGI("[%s] Unmounted %s\n", source_name, target);
        }
    }
    free(targets_to_unmount);

    free_mounts(&mounts);

    return true;
}

int new_mount_namespace(pid_t pid,int root_imp,zygiskComm::MountNamespace namespace_type){
    int pipes[2];
    if (pipe(pipes) == -1) {
        LOGD("new_mount_namespace pipe failed");
        return -1;
    }
    int reader = pipes[0];
    int writer = pipes[1];
    pid_t child_pid = fork();
    if (child_pid == 0) {
        // 子进程
        if (switch_mount_namespace(pid) != 0) {
            exit(EXIT_FAILURE);
        }

        if (namespace_type == zygiskComm::MountNamespace::Clean) {
            if (unshare(CLONE_NEWNS) == -1) {
                perror("unshare failed");
                exit(EXIT_FAILURE);
            }
            revert_unmount(root_imp);
        }

        int mypid = getpid();
        socket_utils::write_u32(writer, 0);
        sleep(1);             // 模拟等待
        socket_utils::write_u32(writer, mypid);
        exit(0);
    } else if (child_pid > 0) {
        // 父进程
        printf("Parent: waiting for child %d to cache mount namespace\n", child_pid);
        if (socket_utils::read_u32(reader) == 0) {
            printf("Child %d finished caching mount namespace\n", child_pid);
        }

        char ns_path[PATH_MAX];
        snprintf(ns_path, sizeof(ns_path), "/proc/%d/ns/mnt", child_pid);
        int ns_file = open(ns_path, O_RDONLY);
        if (ns_file == -1) {
            perror("Failed to open child namespace");
            close(reader);
            close(writer);
            waitpid(child_pid, NULL, 0);
            return -1;
        }

        child_pid = socket_utils::read_u32(writer);
        close(reader);
        close(writer);
        waitpid(child_pid, NULL, 0);
        return ns_file;
    } else {
        perror("fork failed");
        return -1;
    }

}

bool RootImp::cache_mount_namespace(pid_t pid) {

    if(module_mnt_ns_fd != -1){
        close(module_mnt_ns_fd);
    }

    if(clean_mnt_ns_fd != -1){
        close(clean_mnt_ns_fd);
    }

    module_mnt_ns_fd = new_mount_namespace( pid,root_imp,zygiskComm::MountNamespace::Module);
    clean_mnt_ns_fd = new_mount_namespace( pid,root_imp,zygiskComm::MountNamespace::Clean);


    return false;
}

int RootImp::update_mount_namespace( zygiskComm::MountNamespace type) {

    if(zygiskComm::MountNamespace::Module==type){
        return module_mnt_ns_fd;
    }
    if(zygiskComm::MountNamespace::Clean==type){
        return clean_mnt_ns_fd;
    }
}

