//
// Created by chic on 2025/4/5.
//

#ifndef ALINUX_DEBUG_INJECT_COMPANION_H
#define ALINUX_DEBUG_INJECT_COMPANION_H

void companion_entry(int fd);
void connect_companion(int client, bool is_64_bit);
#endif //ALINUX_DEBUG_INJECT_COMPANION_H
