// Copyright (C) 2026 The LineageOS Project
// SPDX-License-Identifier: Apache-2.0
// Explicit diagnostic only; never included in PRODUCT_PACKAGES.
#include <arpa/inet.h>
#include <dirent.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static bool write_adj(int value) {
    FILE* f = fopen("/proc/self/oom_score_adj", "we");
    if (!f) return false;
    bool ok = fprintf(f, "%d", value) > 0;
    return fclose(f) == 0 && ok;
}

static bool another_max_adj_task() {
    DIR* dir = opendir("/proc");
    if (!dir) return true;
    bool found = false;
    while (auto* entry = readdir(dir)) {
        if (entry->d_name[0] < '0' || entry->d_name[0] > '9') continue;
        char path[512];
        snprintf(path, sizeof(path), "/proc/%s/oom_score_adj", entry->d_name);
        FILE* f = fopen(path, "re");
        int adj = 0;
        if (f) {
            if (fscanf(f, "%d", &adj) == 1 && adj >= 1000) found = true;
            fclose(f);
        }
    }
    closedir(dir);
    return found;
}

static bool command(int fd, const int* values, size_t count) {
    uint32_t packet[5];
    if (count > 5) return false;
    for (size_t i = 0; i < count; ++i) packet[i] = htonl(values[i]);
    return send(fd, packet, count * sizeof(packet[0]), MSG_NOSIGNAL)
            == static_cast<ssize_t>(count * sizeof(packet[0]));
}

int main() {
    // Keep existing applications and LMK thresholds untouched. Only the child
    // is eligible at score 1000; abort if an existing process shares that score.
    if (getuid() != 0 || another_max_adj_task() || !write_adj(-1000)) {
        fprintf(stderr, "probe prerequisites failed\n");
        return 2;
    }
    int fd = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0);
    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    strcpy(address.sun_path, "/dev/socket/lmkd");
    if (fd < 0 || connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address))) {
        perror("connect lmkd");
        return 2;
    }
    const int kills[] = {5, 0}, stats[] = {5, 1};
    if (!command(fd, kills, 2) || !command(fd, stats, 2)) return 2;
    int gate[2];
    if (pipe2(gate, O_CLOEXEC)) return 2;
    pid_t child = fork();
    if (child < 0) return 2;
    if (!child) {
        close(fd);
        close(gate[1]);
        prctl(PR_SET_NAME, "aries-lmk-probe", 0, 0, 0);
        if (!write_adj(1000)) _exit(2);
        char start;
        if (read(gate[0], &start, 1) != 1) _exit(2);
        close(gate[0]);
        constexpr size_t step = 16 * 1024 * 1024;
        for (int i = 0; i < 80; ++i) { // A strict 1280 MiB allocation cap.
            void* block = mmap(nullptr, step, PROT_READ | PROT_WRITE,
                               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            if (block == MAP_FAILED) break;
            // Observable page writes prevent dead-store elimination. Merely
            // mapping virtual memory cannot exercise the real LMK policy.
            auto* pages = static_cast<volatile unsigned char*>(block);
            for (size_t offset = 0; offset < step; offset += 4096)
                pages[offset] = 0xa5;
            usleep(100000);
        }
        sleep(5);
        _exit(3); // No real LMK kill: inconclusive rather than changing policy.
    }
    close(gate[0]);
    const int priority[] = {1, child, 0, 1000, 0};
    bool registered = command(fd, priority, 5);
    usleep(100000);
    if (registered) registered = write(gate[1], "x", 1) == 1;
    close(gate[1]);
    bool kill_event = false, stat_event = false;
    for (int attempt = 0; registered && attempt < 60; ++attempt) {
        pollfd waiting{fd, POLLIN, 0};
        if (poll(&waiting, 1, 250) > 0 && (waiting.revents & POLLIN)) {
            uint32_t packet[512];
            ssize_t bytes = recv(fd, packet, sizeof(packet), 0);
            if (bytes >= 12 && ntohl(packet[0]) == 6
                    && ntohl(packet[1]) == static_cast<uint32_t>(child)
                    && ntohl(packet[2]) == 0) kill_event = true;
            if (bytes >= 4 && ntohl(packet[0]) == 8) stat_event = true;
        }
        if (kill_event && stat_event) break;
    }
    // Stop and reap the diagnostic child on every path, including timeout.
    kill(child, SIGKILL);
    int result = 0;
    waitpid(child, &result, 0);
    rusage usage{};
    getrusage(RUSAGE_CHILDREN, &usage);
    const int remove[] = {2, child};
    command(fd, remove, 2);
    close(fd);
    printf("{\"child_pid\":%d,\"kill_event\":%s,\"stats_event\":%s,\"child_sigkill\":%s,"
           "\"child_exit\":%d,\"maxrss_kb\":%ld,\"minor_faults\":%ld}\n",
           child, kill_event ? "true" : "false", stat_event ? "true" : "false",
           WIFSIGNALED(result) && WTERMSIG(result) == SIGKILL ? "true" : "false",
           WIFEXITED(result) ? WEXITSTATUS(result) : -1,
           usage.ru_maxrss, usage.ru_minflt);
    return kill_event && stat_event ? 0 : 3;
}
