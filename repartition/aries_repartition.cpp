/* SPDX-License-Identifier: Apache-2.0
 * Aries merged GPT layout: System (21), Cache (22), Userdata (23).
 * Only their shared boundaries change. A durable, CID-bound journal lives
 * on the existing Persist filesystem. Formatting requires a second Recovery
 * boot with matching kernel partition geometry and explicit confirmation.
 */
#define _LARGEFILE64_SOURCE
#include "capacity.h"
#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <linux/fs.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>
#ifndef ARIES_TEST
#include <cutils/properties.h>
#endif

using Bytes = std::vector<unsigned char>;
static const size_t ENTRIES = 16384, COPY = 512 + 512 + ENTRIES;
static const size_t PREFIX = 96, JOURNAL_SIZE = PREFIX + 2 * COPY + 4;
static std::string diskPath = "/dev/block/mmcblk0", storeRoot = "/tmp/aries-resize-persist";
static std::string cid, journalDir;
static bool testing = false;
static unsigned writeStep = 0;
static void need(bool b, const std::string &s) {
    if (!b)
        throw std::runtime_error(s);
}
static uint32_t u32(const Bytes &b, size_t p) {
    need(p + 4 <= b.size(), "truncated integer");
    return uint32_t(b[p]) | uint32_t(b[p + 1]) << 8 | uint32_t(b[p + 2]) << 16 |
           uint32_t(b[p + 3]) << 24;
}
static uint64_t u64(const Bytes &b, size_t p) {
    return u32(b, p) | (uint64_t(u32(b, p + 4)) << 32);
}
static void put32(Bytes &b, size_t p, uint32_t v) {
    need(p + 4 <= b.size(), "integer overflow");
    for (unsigned i = 0; i < 4; ++i)
        b[p + i] = (v >> (i * 8)) & 255;
}
static void put64(Bytes &b, size_t p, uint64_t v) {
    put32(b, p, uint32_t(v));
    put32(b, p + 4, uint32_t(v >> 32));
}
static uint32_t crc(const Bytes &b, size_t p, size_t n) {
    need(p + n <= b.size(), "CRC range overflow");
    uint32_t v = ~0U;
    for (size_t k = p; k < p + n; ++k) {
        v ^= b[k];
        for (unsigned j = 0; j < 8; ++j)
            v = (v >> 1) ^ ((v & 1) ? 0xedb88320U : 0);
    }
    return ~v;
}
struct FD {
    int n;
    explicit FD(int v) : n(v) { need(v >= 0, std::string("open failed: ") + strerror(errno)); }
    ~FD() { close(n); }
    FD(const FD &) = delete;
    FD &operator=(const FD &) = delete;
};
static Bytes readAt(int fd, uint64_t off, size_t size) {
    Bytes b(size);
    size_t done = 0;
    while (done < size) {
        ssize_t n = pread64(fd, b.data() + done, size - done, off + done);
        if (n < 0 && errno == EINTR)
            continue;
        need(n > 0, "short read");
        done += size_t(n);
    }
    return b;
}
static void flush(int fd) { need(fsync(fd) == 0, "fsync failed"); }
static void property(const std::string &key, const std::string &value) {
#ifdef ARIES_TEST
    printf("property aries.resize.%s=%s\n", key.c_str(), value.c_str());
#else
    property_set(("aries.resize." + key).c_str(), value.substr(0, 90).c_str());
#endif
}
#ifndef ARIES_TEST
static std::string textFile(const std::string &p) {
    FD f(open(p.c_str(), O_RDONLY | O_NOFOLLOW));
    std::string out;
    char b[4096];
    for (;;) {
        ssize_t n = read(f.n, b, sizeof(b));
        if (n < 0 && errno == EINTR)
            continue;
        need(n >= 0, "text read failed");
        if (!n)
            break;
        out.append(b, n);
        need(out.size() <= 1024 * 1024, "text input too large");
    }
    while (!out.empty() && (out.back() == '\n' || out.back() == '\r'))
        out.pop_back();
    return out;
}
#endif

static bool exists(const std::string &p) {
    struct stat st;
    return lstat(p.c_str(), &st) == 0;
}
static uint64_t diskSize(int fd) {
    struct stat64 st;
    need(fstat64(fd, &st) == 0, "stat failed");
    uint64_t bytes = 0;
#ifdef ARIES_TEST
    need(S_ISREG(st.st_mode), "host tests accept regular images only");
    bytes = st.st_size;
#else
    need(S_ISBLK(st.st_mode), "expected eMMC block device");
    need(ioctl(fd, BLKGETSIZE64, &bytes) == 0, "disk size unavailable");
    int sector = 0;
    need(ioctl(fd, BLKSSZGET, &sector) == 0 && sector == 512, "unsupported sector size");
#endif
    need(bytes >= 8ULL * 1024 * 1024 * 1024 && bytes <= 64ULL * 1024 * 1024 * 1024 &&
             bytes % 512 == 0,
         "unsupported eMMC capacity");
    return bytes / 512;
}
#ifndef ARIES_TEST
static void run(const std::vector<std::string> &args) {
    pid_t pid = fork();
    need(pid >= 0, "fork failed");
    if (!pid) {
        std::vector<char *> av;
        for (const auto &s : args)
            av.push_back(const_cast<char *>(s.c_str()));
        av.push_back(nullptr);
        for (int f = 3; f < 1024; ++f)
            close(f);
        execv(av[0], av.data());
        _exit(127);
    }
    int status = 0;
    while (waitpid(pid, &status, 0) < 0)
        need(errno == EINTR, "wait failed");
    need(WIFEXITED(status) && WEXITSTATUS(status) == 0, "command failed: " + args[0]);
}
#endif
static void environment() {
#ifndef ARIES_TEST
    char product[PROP_VALUE_MAX] = {};
    property_get("ro.product.device", product, "");
    need(std::string(product) == "aries", "not an Aries recovery");
    need(exists("/etc/aries-repartition-ready"),
         "repartitioning is available only in this Recovery");
    cid = textFile("/sys/class/block/mmcblk0/device/cid");
#endif
    need(cid.size() == 32 && cid.find_first_not_of("0123456789abcdefABCDEF") == std::string::npos,
         "invalid eMMC CID");
}
struct Table {
    uint64_t sectors;
    Bytes primary, backup, entries;
};
static std::string name(const Table &t, unsigned index) {
    std::string s;
    size_t base = index * 128 + 56;
    for (unsigned i = 0; i < 36; ++i) {
        unsigned lo = t.entries[base + i * 2], hi = t.entries[base + i * 2 + 1];
        if (!lo && !hi)
            break;
        need(!hi && lo >= 32 && lo < 127, "non-ASCII partition name");
        s += char(lo);
    }
    return s;
}
static uint64_t first(const Table &t, unsigned i) { return u64(t.entries, i * 128 + 32); }
static uint64_t last(const Table &t, unsigned i) { return u64(t.entries, i * 128 + 40); }
static uint64_t length(const Table &t, unsigned i) { return last(t, i) - first(t, i) + 1; }
static void header(const Bytes &h, uint64_t here, uint64_t other, uint64_t tableLba,
                   uint64_t sectors) {
    need(h.size() == 512 && !memcmp(h.data(), "EFI PART", 8), "invalid GPT signature");
    need(u32(h, 8) == 0x10000 && u32(h, 12) == 92 && u32(h, 20) == 0, "unsupported GPT header");
    Bytes copy = h;
    put32(copy, 16, 0);
    need(crc(copy, 0, 92) == u32(h, 16), "GPT header CRC mismatch");
    need(u64(h, 24) == here && u64(h, 32) == other && u64(h, 72) == tableLba,
         "GPT header offsets mismatch");
    need(u64(h, 40) == 34 && u64(h, 48) == sectors - 34 && u32(h, 80) >= 24 && u32(h, 80) <= 128 &&
             u32(h, 80) % 4 == 0 && u32(h, 84) == 128,
         "unsupported GPT geometry");
}
static void validate(const Table &t) {
    header(t.primary, 1, t.sectors - 1, 2, t.sectors);
    header(t.backup, t.sectors - 1, 1, t.sectors - 1 - t.entries.size() / 512, t.sectors);
    need(t.entries.size() == u32(t.primary, 80) * 128 && u32(t.primary, 80) == u32(t.backup, 80) &&
             crc(t.entries, 0, t.entries.size()) == u32(t.primary, 88) &&
             u32(t.backup, 88) == u32(t.primary, 88),
         "GPT entries CRC mismatch");
    need(std::equal(t.primary.begin() + 40, t.primary.begin() + 72, t.backup.begin() + 40),
         "GPT copies disagree");
    const char *names[] = {"modem",   "sbl1",    "sbl2",    "sbl3",  "rpm",     "tz",
                           "DDR",     "aboot",   "misc",    "logo",  "m9kefs1", "m9kefs2",
                           "m9kefsc", "bk1",     "m9kefs3", "bk2",   "boot",    "recovery",
                           "bk3",     "persist", "system",  "cache", "userdata"};
    uint64_t previous = 33;
    for (unsigned i = 0; i < t.entries.size() / 128; ++i) {
        bool empty = std::all_of(t.entries.begin() + i * 128, t.entries.begin() + i * 128 + 16,
                                 [](unsigned char c) { return c == 0; });
        if (i >= 23) {
            need(empty &&
                     std::all_of(t.entries.begin() + i * 128, t.entries.begin() + (i + 1) * 128,
                                 [](unsigned char c) { return c == 0; }),
                 "unmerged or extra GPT partition");
            continue;
        }
        need(!empty && name(t, i) == names[i], "unexpected Aries partition identity");
        need(first(t, i) > previous && last(t, i) >= first(t, i) && last(t, i) <= t.sectors - 34,
             "partition overlap / range error");
        previous = last(t, i);
    }
    need(first(t, 16) == 524288 && length(t, 16) == 61440 && first(t, 17) == 585728 &&
             length(t, 17) == 30720,
         "boot/recovery geometry differs");
    need(first(t, 19) == 638976 && length(t, 19) == 16384 && first(t, 20) == 655360,
         "persist/system geometry differs");
    need(last(t, 20) + 1 == first(t, 21) && last(t, 21) + 1 == first(t, 22) &&
             last(t, 22) == t.sectors - 34,
         "System/Cache/Data are not contiguous at disk tail");
    need(length(t, 20) % 2048 == 0 && length(t, 20) >= ARIES_SYSTEM_MIN_MIB * 2048ULL &&
             length(t, 20) <= ARIES_SYSTEM_MAX_MIB * 2048ULL,
         "System outside supported range");
    need(length(t, 21) % 2048 == 0 && length(t, 21) >= 64 * 2048ULL &&
             length(t, 21) <= 512 * 2048ULL && length(t, 22) >= 1024 * 2048ULL,
         "unexpected Cache/Data capacity");
}
static Table raw(int fd, uint64_t sectors) {
    Bytes h = readAt(fd, 512, 512);
    unsigned n = u32(h, 80);
    need(n >= 24 && n <= 128 && n % 4 == 0 && u32(h, 84) == 128, "unsupported GPT entry count");
    Table t{sectors, h, readAt(fd, (sectors - 1) * 512, 512), readAt(fd, 1024, n * 128)};
    return t;
}
static uint64_t backupLba(const Table &t) { return t.sectors - 1 - t.entries.size() / 512; }
static Table current(int fd, uint64_t sectors) {
    Table t = raw(fd, sectors);
    validate(t);
    need(readAt(fd, backupLba(t) * 512, t.entries.size()) == t.entries,
         "primary/backup entries differ");
    return t;
}
static void checksum(Table &t) {
    uint32_t c = crc(t.entries, 0, t.entries.size());
    for (Bytes *h : {&t.primary, &t.backup}) {
        put32(*h, 88, c);
        put32(*h, 16, 0);
        put32(*h, 16, crc(*h, 0, 92));
    }
}
static unsigned target(const std::string &s) {
    need(!s.empty() && s.size() <= 4 && s.find_first_not_of("0123456789") == std::string::npos,
         "invalid target");
    unsigned n = unsigned(strtoul(s.c_str(), nullptr, 10));
    need(n >= ARIES_SYSTEM_MIN_MIB && n <= ARIES_SYSTEM_MAX_MIB, "System must be 512..3072 MiB");
    return n;
}
static Table plan(const Table &old, unsigned mib) {
    validate(old);
    Table t = old;
    uint64_t end = first(old, 20) + mib * 2048ULL - 1;
    put64(t.entries, 20 * 128 + 40, end);
    put64(t.entries, 21 * 128 + 32, end + 1);
    put64(t.entries, 21 * 128 + 40, end + length(old, 21));
    put64(t.entries, 22 * 128 + 32, end + length(old, 21) + 1);
    checksum(t);
    validate(t);
    return t;
}
static bool equal(const Table &a, const Table &b) {
    return a.sectors == b.sectors && a.primary == b.primary && a.backup == b.backup &&
           a.entries == b.entries;
}
static void show(const Table &t) {
    property("current", std::to_string(length(t, 20) / 2048));
    property("cache", std::to_string(length(t, 21) / 2048));
    property("data", std::to_string(length(t, 22) / 2048));
    property("budget", std::to_string((length(t, 20) + length(t, 22)) / 2048));
}
static void printPlan(const Table &t) {
    printf("{\"system_mib\":%llu,\"cache_mib\":%llu,\"data_mib\":%llu,\"system_start\":%llu,"
           "\"system_end\":%llu,\"cache_start\":%llu,\"cache_end\":%llu,\"data_start\":%llu,\"data_"
           "end\":%llu}\n",
           (unsigned long long)(length(t, 20) / 2048), (unsigned long long)(length(t, 21) / 2048),
           (unsigned long long)(length(t, 22) / 2048), (unsigned long long)first(t, 20),
           (unsigned long long)last(t, 20), (unsigned long long)first(t, 21),
           (unsigned long long)last(t, 21), (unsigned long long)first(t, 22),
           (unsigned long long)last(t, 22));
}
static void mountedStore(bool writable) {
#ifndef ARIES_TEST
    struct stat st;
    if (lstat(storeRoot.c_str(), &st))
        need(mkdir(storeRoot.c_str(), 0700) == 0, "cannot create journal mountpoint");
    else
        need(S_ISDIR(st.st_mode) && !S_ISLNK(st.st_mode), "invalid journal mountpoint");
    std::string mounts = textFile("/proc/mounts");
    std::istringstream lines(mounts);
    std::string source, at, type, opts, line;
    bool found = false;
    struct stat persist;
    need(std::stoull(textFile("/sys/class/block/mmcblk0p20/start")) == 638976 &&
             std::stoull(textFile("/sys/class/block/mmcblk0p20/size")) == 16384,
         "Persist kernel geometry differs");
    need(stat("/dev/block/mmcblk0p20", &persist) == 0 && S_ISBLK(persist.st_mode),
         "Persist missing");
    while (std::getline(lines, line)) {
        std::istringstream in(line);
        in >> source >> at >> type >> opts;
        if (at != storeRoot)
            continue;
        struct stat dev;
        need(!stat(source.c_str(), &dev) && dev.st_rdev == persist.st_rdev && type == "ext4",
             "journal storage is not Persist");
        found = true;
    }
    if (!found)
        run({"/sbin/mount", "-t", "ext4", "-o",
             writable ? "rw,nosuid,nodev,noexec" : "ro,nosuid,nodev,noexec",
             "/dev/block/mmcblk0p20", storeRoot});
    else if (writable)
        run({"/sbin/mount", "-o", "remount,rw,nosuid,nodev,noexec", storeRoot});
#else
    (void)writable;
    struct stat st;
    need(!lstat(storeRoot.c_str(), &st) && S_ISDIR(st.st_mode) && !S_ISLNK(st.st_mode),
         "invalid test store");
#endif
    journalDir = storeRoot + "/.aries-repartition";
    if (exists(journalDir)) {
        struct stat st;
        need(!lstat(journalDir.c_str(), &st) && S_ISDIR(st.st_mode) && !S_ISLNK(st.st_mode) &&
                 st.st_uid == getuid(),
             "invalid journal directory");
    } else if (writable) {
        need(mkdir(journalDir.c_str(), 0700) == 0, "cannot create durable journal directory");
        FD parent(open(storeRoot.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW));
        flush(parent.n);
    }
}
static void unmounted() {
#ifndef ARIES_TEST
    struct stat dev[3];
    for (unsigned i = 0; i < 3; ++i)
        need(!stat(("/dev/block/mmcblk0p" + std::to_string(21 + i)).c_str(), &dev[i]),
             "missing resize partition");
    std::istringstream mounts(textFile("/proc/mounts"));
    std::string line;
    while (std::getline(mounts, line)) {
        std::istringstream in(line);
        std::string source;
        in >> source;
        struct stat s;
        if (stat(source.c_str(), &s) || !S_ISBLK(s.st_mode))
            continue;
        for (const auto &d : dev)
            need(s.st_rdev != d.st_rdev, "System/Cache/Data is still mounted");
    }
    for (unsigned i = 21; i <= 23; ++i) {
        std::string dir = "/sys/class/block/mmcblk0p" + std::to_string(i) + "/holders";
        need(exists(dir), "partition holder state unavailable");
        run({"/sbin/sh", "-c", "holders=$(ls " + dir + ") || exit 1; test -z \"$holders\""});
    }
#else
    need(!getenv("ARIES_TEST_MOUNTED"), "System/Cache/Data is still mounted");
#endif
}
static void writeBytes(int fd, uint64_t off, const Bytes &b, uint64_t sectors) {
    need((off >= 512 && off + b.size() <= 34 * 512ULL) ||
             (off >= (sectors - 33) * 512 && off + b.size() <= sectors * 512),
         "write outside GPT metadata refused");
    size_t done = 0;
    while (done < b.size()) {
        ssize_t n = pwrite64(fd, b.data() + done, b.size() - done, off + done);
        if (n < 0 && errno == EINTR)
            continue;
        need(n > 0, "GPT write failed");
        done += size_t(n);
    }
    flush(fd);
    ++writeStep;
#ifdef ARIES_TEST
    const char *fail = getenv("ARIES_TEST_FAIL_WRITE");
    if (fail && unsigned(atoi(fail)) == writeStep)
        _exit(75);
#endif
}
static void writeTable(int fd, const Table &t) {
    writeStep = 0;
    writeBytes(fd, backupLba(t) * 512, t.entries, t.sectors);
    writeBytes(fd, (t.sectors - 1) * 512, t.backup, t.sectors);
    writeBytes(fd, 1024, t.entries, t.sectors);
    writeBytes(fd, 512, t.primary, t.sectors);
    need(equal(current(fd, t.sectors), t), "GPT readback differs");
}
static Bytes serialize(const Table &t) {
    Bytes b;
    b.insert(b.end(), t.primary.begin(), t.primary.end());
    b.insert(b.end(), t.backup.begin(), t.backup.end());
    b.insert(b.end(), t.entries.begin(), t.entries.end());
    b.resize(COPY, 0);
    return b;
}
static Table deserialize(const Bytes &b, size_t off, uint64_t sectors) {
    need(off + COPY <= b.size(), "truncated table snapshot");
    unsigned n = u32(b, off + 80);
    need(n >= 24 && n <= 128 && n % 4 == 0, "unsupported journal entry count");
    Table t{sectors, Bytes(b.begin() + off, b.begin() + off + 512),
            Bytes(b.begin() + off + 512, b.begin() + off + 1024),
            Bytes(b.begin() + off + 1024, b.begin() + off + 1024 + n * 128)};
    validate(t);
    return t;
}
struct Journal {
    unsigned phase, mib;
    Table old, next;
    Bytes mbr;
};
static Bytes journalBytes(const Journal &j) {
    Bytes b(PREFIX, 0);
    memcpy(b.data(), "ARIES-GPT-v1", 12);
    put32(b, 16, 1);
    put32(b, 20, j.phase);
    put32(b, 24, j.mib);
    put64(b, 32, j.old.sectors);
    memcpy(b.data() + 40, cid.c_str(), 32);
    memcpy(b.data() + 72, "yes", 4);
    put32(b, 76, crc(j.mbr, 0, 512));
    for (const auto &t : {j.old, j.next}) {
        Bytes c = serialize(t);
        b.insert(b.end(), c.begin(), c.end());
    }
    b.resize(JOURNAL_SIZE);
    put32(b, JOURNAL_SIZE - 4, crc(b, 0, JOURNAL_SIZE - 4));
    return b;
}
static void atomicFile(const std::string &path, const Bytes &b) {
    std::string tmp = path + ".tmp";
    struct stat st;
    if (!lstat(tmp.c_str(), &st)) {
        need(S_ISREG(st.st_mode) && st.st_uid == getuid() && st.st_nlink == 1,
             "unsafe journal temp file");
        need(unlink(tmp.c_str()) == 0, "cannot remove old temp file");
    }
    FD f(open(tmp.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600));
    size_t done = 0;
    while (done < b.size()) {
        ssize_t n = write(f.n, b.data() + done, b.size() - done);
        if (n < 0 && errno == EINTR)
            continue;
        need(n > 0, "journal write failed");
        done += size_t(n);
    }
    flush(f.n);
    need(rename(tmp.c_str(), path.c_str()) == 0, "journal rename failed");
    FD d(open(journalDir.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW));
    flush(d.n);
}
static Journal loadJournal(int fd, uint64_t sectors) {
    FD f(open((journalDir + "/journal.bin").c_str(), O_RDONLY | O_NOFOLLOW));
    struct stat st;
    need(!fstat(f.n, &st) && S_ISREG(st.st_mode) && st.st_nlink == 1 && st.st_size == JOURNAL_SIZE,
         "invalid journal length/type");
    Bytes b = readAt(f.n, 0, JOURNAL_SIZE);
    need(!memcmp(b.data(), "ARIES-GPT-v1", 12) && u32(b, 16) == 1 && u32(b, 20) >= 1 &&
             u32(b, 20) <= 4,
         "unknown journal format");
    need(crc(b, 0, b.size() - 4) == u32(b, b.size() - 4), "journal CRC mismatch");
    need(u64(b, 32) == sectors && !memcmp(b.data() + 40, cid.c_str(), 32) &&
             !memcmp(b.data() + 72, "yes", 4),
         "journal belongs to another device / missing authorization");
    Bytes mbr = readAt(fd, 0, 512);
    need(crc(mbr, 0, 512) == u32(b, 76), "protective MBR changed");
    Journal j{u32(b, 20), target(std::to_string(u32(b, 24))), deserialize(b, PREFIX, sectors),
              deserialize(b, PREFIX + COPY, sectors), mbr};
    need(equal(plan(j.old, j.mib), j.next), "journal plan mismatch");
    return j;
}
static bool allowedByte(size_t i) {
    unsigned p = i / 128;
    unsigned o = i % 128;
    return (p == 20 && o >= 40 && o < 48) || (p == 21 && o >= 32 && o < 48) ||
           (p == 22 && o >= 32 && o < 40);
}
static void recoverable(int fd, const Journal &j) {
    Table t = raw(fd, j.old.sectors);
    for (const auto &pair :
         {std::make_pair(t.primary, j.old.primary), std::make_pair(t.backup, j.old.backup)})
        for (size_t i = 0; i < 512; ++i)
            if (!((i >= 16 && i < 20) || (i >= 88 && i < 92)))
                need(pair.first[i] == pair.second[i],
                     "GPT header identity changed; recovery refused");
    for (const auto &entries :
         {t.entries, readAt(fd, backupLba(j.old) * 512, j.old.entries.size())})
        for (size_t i = 0; i < j.old.entries.size(); ++i)
            if (!allowedByte(i))
                need(entries[i] == j.old.entries[i],
                     "protected GPT entry changed; recovery refused");
}
static bool diskMatches(int fd, const Table &t) {
    Table r = raw(fd, t.sectors);
    return equal(r, t) && readAt(fd, backupLba(t) * 512, t.entries.size()) == t.entries;
}
// A temporary fastboot boot must not start a two-boot migration that
// would reboot into an older installed Recovery without this tool.
static bool recoveryInstalled() {
#ifdef ARIES_TEST
    return !getenv("ARIES_TEST_RECOVERY_NOT_INSTALLED");
#else
    FD fd(open("/dev/block/mmcblk0p18", O_RDONLY | O_NOFOLLOW));
    Bytes h = readAt(fd.n, 0, 2048);
    if (memcmp(h.data(), "ANDROID!", 8))
        return false;
    std::string command(reinterpret_cast<const char *>(h.data() + 64), 512);
    command = command.substr(0, command.find('\0'));
    std::istringstream words(command);
    std::string word;
    while (words >> word)
        if (word == "androidboot.ariesresize=1")
            return true;
    return false;
#endif
}
static void affirmative(const std::string &s) {
    need(s == "yes", "type exactly yes to authorize loss of System, Cache and all Data");
}
static void geometry(const Table &t) {
#ifndef ARIES_TEST
    for (unsigned i = 20; i < 23; ++i) {
        std::string path = "/sys/class/block/mmcblk0p" + std::to_string(i + 1);
        need(std::stoull(textFile(path + "/start")) == first(t, i) &&
                 std::stoull(textFile(path + "/size")) == length(t, i),
             "reboot Recovery before formatting: kernel partition geometry is stale");
    }
#else
    (void)t;
    need(!getenv("ARIES_TEST_STALE_KERNEL"),
         "reboot Recovery before formatting: kernel partition geometry is stale");
#endif
}
// Android's userdata fstab uses encryptable=footer. Erase the old footer
// and reserve its final 16 KiB before creating the new unencrypted ext4.
static void clearDataFooter(const Table &t) {
#ifdef ARIES_TEST
    FD fd(open(diskPath.c_str(), O_RDWR | O_NOFOLLOW));
    uint64_t offset = (last(t, 22) + 1) * 512 - 16384;
#else
    FD fd(open("/dev/block/mmcblk0p23", O_RDWR | O_NOFOLLOW));
    uint64_t size = 0;
    need(ioctl(fd.n, BLKGETSIZE64, &size) == 0 && size == length(t, 22) * 512,
         "Data geometry changed before footer erase");
    uint64_t offset = size - 16384;
#endif
    Bytes zero(16384, 0);
    size_t done = 0;
    while (done < zero.size()) {
        ssize_t n = pwrite64(fd.n, zero.data() + done, zero.size() - done, offset + done);
        if (n < 0 && errno == EINTR)
            continue;
        need(n > 0, "cannot erase old Data encryption footer");
        done += size_t(n);
    }
    flush(fd.n);
}
static void format(const Table &t) {
    for (unsigned i = 20; i < 23; ++i) {
        const char *label = i == 20 ? "system" : i == 21 ? "cache" : "userdata";
        if (i == 22)
            clearDataFooter(t);
#ifdef ARIES_TEST
        FD f(open((journalDir + "/formatted-" + label).c_str(),
                  O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW, 0600));
        std::string value = std::to_string(first(t, i)) + " " + std::to_string(length(t, i)) + "\n";
        need(write(f.n, value.data(), value.size()) == ssize_t(value.size()),
             "test formatter failed");
        flush(f.n);
        const char *fail = getenv("ARIES_TEST_FAIL_FORMAT");
        if (fail && unsigned(atoi(fail)) == i - 19)
            _exit(76);
#else
        std::string path = "/dev/block/mmcblk0p" + std::to_string(i + 1);
        std::vector<std::string> args = {"/sbin/make_ext4fs", "-L", label};
        if (i == 22) {
            args.push_back("-l");
            args.push_back(std::to_string(length(t, i) * 512 - 16384));
        }
        args.push_back(path);
        run(args);
        FD f(open(path.c_str(), O_RDONLY));
        Bytes super = readAt(f.n, 1024, 1024);
        need(super[56] == 0x53 && super[57] == 0xef, "formatter did not produce ext4");
        need(!memcmp(super.data() + 120, label, strlen(label)), "filesystem label differs");
        uint32_t log = u32(super, 24);
        need(log <= 6 && (uint64_t(u32(super, 4)) << (10 + log)) <= length(t, i) * 512,
             "filesystem exceeds partition");
#endif
        printf("Formatted %s\n", label);
    }
}
static void probe(int fd, uint64_t sectors) {
    property("ready", "0");
    property("finish", "0");
    property("repair", "0");
    property("restore", "0");
    property("phase", "idle");
    property("error", "");
    property("installed", recoveryInstalled() ? "1" : "0");
    if (exists(journalDir + "/journal.bin")) {
        Journal j = loadJournal(fd, sectors);
        if (j.phase != 4) {
            recoverable(fd, j);
            property("restore", "1");
            show(j.next);
            if (diskMatches(fd, j.next)) {
                property("finish", "1");
                property("phase", "finish");
            } else {
                property("repair", "1");
                property("phase", "repair");
            }
            return;
        }
    }
    Table t = current(fd, sectors);
    show(t);
    struct statvfs v;
    need(!statvfs(storeRoot.c_str(), &v) && uint64_t(v.f_bavail) * v.f_frsize >= 512 * 1024,
         "not enough Persist space for GPT journal");
    property("ready", "1");
    property("phase", "ready");
    if (!recoveryInstalled())
        property("error", "Flash this Recovery before resizing; temporary boot is preview only.");
}
int main(int argc, char **argv) {
    try {
        int at = 1;
#ifdef ARIES_TEST
        need(argc >= 5 && std::string(argv[1]) == "--image" && std::string(argv[3]) == "--store",
             "host test requires --image PATH --store DIR");
        diskPath = argv[2];
        storeRoot = argv[4];
        at = 5;
        testing = true;
        cid = "0123456789abcdef0123456789abcdef";
        const char *testCid = getenv("ARIES_TEST_CID");
        if (testCid)
            cid = testCid;
#endif
        (void)testing;
        need(argc > at, "usage: probe | plan MiB | apply MiB yes | finish/repair/restore yes");
        std::string command = argv[at];
        bool write = command == "apply" || command == "finish" || command == "repair" ||
                     command == "restore";
        need(command == "probe" || command == "plan" || write, "unknown command");
        if (write) {
            need(argc == at + (command == "apply" ? 3 : 2), "invalid command arguments");
            affirmative(argv[argc - 1]);
        } else
            need(argc == at + (command == "plan" ? 2 : 1), "invalid command arguments");
        environment();
        FD lock(open((testing ? storeRoot + "/tool.lock" : "/tmp/aries-repartition.lock").c_str(),
                     O_RDWR | O_CREAT | O_NOFOLLOW, 0600));
        need(!flock(lock.n, LOCK_EX | LOCK_NB), "another partition operation is running");
        FD disk(open(diskPath.c_str(), (write ? O_RDWR : O_RDONLY) | O_NOFOLLOW));
        uint64_t sectors = diskSize(disk.n);
        Bytes mbr = readAt(disk.n, 0, 512);
        need(mbr[510] == 0x55 && mbr[511] == 0xaa && mbr[450] == 0xee, "not a protective GPT disk");
        if (command == "plan") {
            printPlan(plan(current(disk.n, sectors), target(argv[at + 1])));
            return 0;
        }
        if (command == "apply")
            need(recoveryInstalled(),
                 "Flash this Recovery before resizing; temporary boot is preview only.");
        mountedStore(write);
        if (command == "probe") {
            probe(disk.n, sectors);
            return 0;
        }
        unmounted();
#ifndef ARIES_TEST
        need(access("/sbin/make_ext4fs", X_OK) == 0, "required formatter missing");
#endif
        if (command == "apply") {
            if (exists(journalDir + "/journal.bin"))
                need(loadJournal(disk.n, sectors).phase == 4,
                     "unfinished operation: finish, repair or restore it first");
            Table old = current(disk.n, sectors), next = plan(old, target(argv[at + 1]));
            need(!equal(old, next), "selected size is unchanged; nothing was erased");
            struct statvfs v;
            need(!statvfs(storeRoot.c_str(), &v) && uint64_t(v.f_bavail) * v.f_frsize >= 512 * 1024,
                 "not enough journal space");
            Journal j{1, target(argv[at + 1]), old, next, mbr};
            atomicFile(journalDir + "/journal.bin", journalBytes(j));
            (void)loadJournal(disk.n, sectors);
            recoverable(disk.n, j);
            writeTable(disk.n, next);
            j.phase = 2;
            atomicFile(journalDir + "/journal.bin", journalBytes(j));
            property("phase", "reboot");
            puts("GPT updated. Reboot Recovery, then finish formatting System/Cache/Data. Do not "
                 "boot Android yet.");
        } else {
            Journal j = loadJournal(disk.n, sectors);
            need(j.phase != 4, "no unfinished repartition operation");
            recoverable(disk.n, j);
            if (command == "finish") {
                need(diskMatches(disk.n, j.next), "partition table incomplete; repair it first");
                geometry(j.next);
                j.phase = 3;
                atomicFile(journalDir + "/journal.bin", journalBytes(j));
                format(j.next);
                j.phase = 4;
                atomicFile(journalDir + "/journal.bin", journalBytes(j));
                property("phase", "complete");
                puts("New physical partitions formatted. Install a ROM before booting Android.");
            } else {
                const Table &desired = command == "restore" ? j.old : j.next;
                writeTable(disk.n, desired);
                j.phase = command == "restore" ? 4 : 2;
                atomicFile(journalDir + "/journal.bin", journalBytes(j));
                property("phase", "reboot");
                puts(command == "restore"
                         ? "Previous GPT restored; filesystem contents are not restored. Reboot "
                           "Recovery."
                         : "Pending GPT repaired. Reboot Recovery before formatting.");
            }
        }
        property("error", "");
        return 0;
    } catch (const std::exception &e) {
        fprintf(stderr, "Aries repartition: %s\n", e.what());
        property("ready", "0");
        property("finish", "0");
        property("repair", "0");
        property("restore", "0");
        property("error", e.what());
        return 1;
    }
}
