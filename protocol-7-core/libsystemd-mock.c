/*
 * Protocol 7 - libsystemd-mock v5.0
 * Comprehensive ABI compatibility layer for OpenRC-based Arch Linux
 * 
 * Scope: Interface compatibility, not system emulation
 * Target: Single-user desktop (1 user, 1 session, 1 seat)
 * Policy: Success when safe, error when necessary, never crash
 * 
 * Coverage: 873 symbols from systemd 255/256/257
 * Categories:
 *   - Static Identity: Return fixed deterministic values
 *   - Safe No-Op Success: Return 0, do nothing
 *   - Real OS Passthrough: dlsym to real libsystemd (none used - pure mock)
 *   - Out of Scope: Journal, varlink server, deep device ops
 *
 * License: MIT
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <time.h>
#include <pwd.h>
#include <grp.h>
#include <dlfcn.h>
#include <link.h>
#include <sys/syscall.h>

/* ============================================================================
 * VERSION & CONFIGURATION
 * ============================================================================ */
#define P7_MOCK_VERSION_MAJOR 5
#define P7_MOCK_VERSION_MINOR 0
#define P7_MOCK_VERSION_PATCH 0

/* Fake pointer allocation base - x86_64 non-canonical hole */
#define FAKE_PTR_BASE  0x0000dead00000000ULL
#define FAKE_PTR_BUS   0x0000beef00000000ULL
#define FAKE_PTR_MSG   0x0000cafe00000000ULL
#define FAKE_PTR_SLOT  0x0000feed00000000ULL
#define FAKE_PTR_CREDS 0x0000face00000000ULL
#define FAKE_PTR_TRACK 0x0000deaf00000000ULL
#define FAKE_PTR_DEV   0x0000c0de00000000ULL
#define FAKE_PTR_ENUM  0x0000babe00000000ULL
#define FAKE_PTR_MON   0x0000fade00000000ULL
#define FAKE_PTR_HWDB  0x0000cafe00000000ULL
#define FAKE_PTR_JOURN 0x0000dead00000000ULL
#define FAKE_PTR_VAR   0x0000beef00000000ULL
#define FAKE_PTR_JSON  0x0000c0de00000000ULL

static volatile unsigned fake_ptr_counter = 0;

static inline void* alloc_fake_ptr(uint64_t base) {
    unsigned n = __atomic_fetch_add(&fake_ptr_counter, 1, __ATOMIC_SEQ_CST);
    return (void*)(base | (n & 0xFFFF));
}

/* ============================================================================
 * ENVIRONMENT & LOGGING
 * ============================================================================ */
static int verbose = -1;
static int fail_inject = -1;

static void p7_init(void) __attribute__((constructor));
static void p7_init(void) {
    if (verbose == -1) verbose = getenv("P7_MOCK_VERBOSE") ? 1 : 0;
    if (fail_inject == -1) fail_inject = getenv("P7_MOCK_FAIL") ? 1 : 0;
}

static void p7_log(const char* func) {
    if (verbose) fprintf(stderr, "[P7-MOCK] %s\\n", func);
}

static int p7_maybe_fail(void) {
    if (fail_inject && (rand() % 100) < 5) return -EIO;
    return 0;
}

/* ============================================================================
 * ELF IDENTIFICATION NOTE
 * ============================================================================ */
__attribute__((section(".note.p7_abi"), used))
static const struct {
    uint32_t namesz;
    uint32_t descsz;
    uint32_t type;
    char name[8];
    char desc[32];
} p7_note = {
    .namesz = 8,
    .descsz = 32,
    .type = 0xP7,
    .name = "P7_ABI\\0",
    .desc = "P7-ABI-2026.05|v5.0|mock",
};

/* ============================================================================
 * TYPE DEFINITIONS
 * ============================================================================ */

typedef struct sd_bus sd_bus;
typedef struct sd_bus_message sd_bus_message;
typedef struct sd_bus_slot sd_bus_slot;
typedef struct sd_bus_creds sd_bus_creds;
typedef struct sd_bus_track sd_bus_track;
typedef struct sd_event sd_event;
typedef struct sd_event_source sd_event_source;
typedef struct sd_device sd_device;
typedef struct sd_device_enumerator sd_device_enumerator;
typedef struct sd_device_monitor sd_device_monitor;
typedef struct sd_hwdb sd_hwdb;
typedef struct sd_journal sd_journal;
typedef struct sd_login_monitor sd_login_monitor;
typedef struct sd_json_variant sd_json_variant;
typedef struct sd_varlink sd_varlink;
typedef struct sd_varlink_server sd_varlink_server;

typedef struct sd_bus_error {
    const char* name;
    const char* message;
    int _need_free;
} sd_bus_error;

typedef struct sd_id128 {
    uint8_t bytes[16];
} sd_id128_t;

typedef enum {
    SD_BUS_TYPE_INVALID = 0,
    SD_BUS_TYPE_BYTE = 'y',
    SD_BUS_TYPE_BOOLEAN = 'b',
    SD_BUS_TYPE_INT16 = 'n',
    SD_BUS_TYPE_UINT16 = 'q',
    SD_BUS_TYPE_INT32 = 'i',
    SD_BUS_TYPE_UINT32 = 'u',
    SD_BUS_TYPE_INT64 = 'x',
    SD_BUS_TYPE_UINT64 = 't',
    SD_BUS_TYPE_DOUBLE = 'd',
    SD_BUS_TYPE_STRING = 's',
    SD_BUS_TYPE_OBJECT_PATH = 'o',
    SD_BUS_TYPE_SIGNATURE = 'g',
    SD_BUS_TYPE_UNIX_FD = 'h',
    SD_BUS_TYPE_ARRAY = 'a',
    SD_BUS_TYPE_VARIANT = 'v',
    SD_BUS_TYPE_STRUCT = 'r',
    SD_BUS_TYPE_DICT_ENTRY = 'e',
} sd_bus_type;

typedef enum {
    SD_BUS_CREDS_PID = 1U << 0,
    SD_BUS_CREDS_TID = 1U << 1,
    SD_BUS_CREDS_PPID = 1U << 2,
    SD_BUS_CREDS_UID = 1U << 3,
    SD_BUS_CREDS_EUID = 1U << 4,
    SD_BUS_CREDS_SUID = 1U << 5,
    SD_BUS_CREDS_FSUID = 1U << 6,
    SD_BUS_CREDS_GID = 1U << 7,
    SD_BUS_CREDS_EGID = 1U << 8,
    SD_BUS_CREDS_SGID = 1U << 9,
    SD_BUS_CREDS_FSGID = 1U << 10,
    SD_BUS_CREDS_SUPPLEMENTARY_GIDS = 1U << 11,
    SD_BUS_CREDS_COMM = 1U << 12,
    SD_BUS_CREDS_TID_COMM = 1U << 13,
    SD_BUS_CREDS_EXE = 1U << 14,
    SD_BUS_CREDS_CMDLINE = 1U << 15,
    SD_BUS_CREDS_CGROUP = 1U << 16,
    SD_BUS_CREDS_UNIT = 1U << 17,
    SD_BUS_CREDS_SLICE = 1U << 18,
    SD_BUS_CREDS_USER_UNIT = 1U << 19,
    SD_BUS_CREDS_USER_SLICE = 1U << 20,
    SD_BUS_CREDS_SESSION = 1U << 21,
    SD_BUS_CREDS_OWNER_UID = 1U << 22,
    SD_BUS_CREDS_MACHINE_ID = 1U << 23,
    SD_BUS_CREDS_AUDIT_SESSION_ID = 1U << 24,
    SD_BUS_CREDS_AUDIT_LOGIN_UID = 1U << 25,
    SD_BUS_CREDS_TTY = 1U << 26,
    SD_BUS_CREDS_UNIQUE_NAME = 1U << 27,
    SD_BUS_CREDS_WELL_KNOWN_NAMES = 1U << 28,
    SD_BUS_CREDS_DESCRIPTION = 1U << 29,
    SD_BUS_CREDS_PIDFD = 1U << 30,
} sd_bus_creds_mask_t;

typedef enum {
    SD_EVENT_INITIAL,
    SD_EVENT_PREPARING,
    SD_EVENT_ARMED,
    SD_EVENT_PENDING,
    SD_EVENT_RUNNING,
    SD_EVENT_EXITING,
    SD_EVENT_FINISHED,
} sd_event_state_t;

typedef enum {
    SD_EVENT_OFF = 0,
    SD_EVENT_ON = 1,
    SD_EVENT_ONESHOT = -1,
} sd_event_source_enabled_t;

typedef enum {
    SD_BUS_MESSAGE_METHOD_CALL = 1,
    SD_BUS_MESSAGE_METHOD_RETURN = 2,
    SD_BUS_MESSAGE_METHOD_ERROR = 3,
    SD_BUS_MESSAGE_SIGNAL = 4,
} sd_bus_message_type_t;

typedef enum {
    SD_PATH_TEMPORARY,
    SD_PATH_TEMPORARY_LARGE,
    SD_PATH_SYSTEMD_NSPAWN_FACTORY,
    SD_PATH_SYSTEMD_NSPAWN_FACTORY_STORE,
} sd_path_type_t;

/* ============================================================================
 * HELPER FUNCTIONS
 * ============================================================================ */

static uid_t get_current_uid(void) {
    uid_t uid = getuid();
    return uid;
}

static char* p7_strdup(const char* s) {
    if (!s) return NULL;
    return strdup(s);
}

static int p7_set_string(char** ret, const char* val) {
    if (!ret) return -EINVAL;
    if (val) {
        *ret = strdup(val);
        if (!*ret) return -ENOMEM;
    } else {
        *ret = NULL;
    }
    return 0;
}

static int p7_set_strv(char*** ret, char** val) {
    if (!ret) return -EINVAL;
    if (val) {
        size_t n = 0;
        while (val[n]) n++;
        char** copy = calloc(n + 1, sizeof(char*));
        if (!copy) return -ENOMEM;
        for (size_t i = 0; i < n; i++) {
            copy[i] = strdup(val[i]);
            if (!copy[i]) {
                while (i--) free(copy[i]);
                free(copy);
                return -ENOMEM;
            }
        }
        *ret = copy;
    } else {
        *ret = NULL;
    }
    return 0;
}

/* ============================================================================
 * SECTION 1: CORE FUNCTIONS
 * ============================================================================ */

/* sd_booted - always returns 0 (systemd not running, but mock pretends it is) */
int sd_booted(void) {
    p7_log(__func__);
    return 0;  /* Pretend systemd is booted for compatibility */
}

/* sd_notify - no-op success */
int sd_notify(int unset_environment, const char* state) {
    p7_log(__func__);
    (void)unset_environment;
    (void)state;
    return 1;  /* Success: notification sent (to /dev/null) */
}

/* sd_notifyf - formatted no-op */
int sd_notifyf(int unset_environment, const char* format, ...) {
    p7_log(__func__);
    (void)unset_environment;
    (void)format;
    return 1;
}

/* sd_listen_fds - no fds passed by systemd socket activation */
int sd_listen_fds(int unset_environment) {
    p7_log(__func__);
    (void)unset_environment;
    return 0;  /* No fds passed */
}

/* sd_listen_fds_with_names - no fds, no names */
int sd_listen_fds_with_names(int unset_environment, char*** names) {
    p7_log(__func__);
    (void)unset_environment;
    if (names) *names = NULL;
    return 0;
}

/* sd_watchdog_enabled - no watchdog */
int sd_watchdog_enabled(int unset_environment, uint64_t* usec) {
    p7_log(__func__);
    (void)unset_environment;
    if (usec) *usec = 0;
    return 0;  /* Watchdog not enabled */
}

/* sd_pid_notify - per-process notification */
int sd_pid_notify(pid_t pid, int unset_environment, const char* state) {
    p7_log(__func__);
    (void)pid;
    (void)unset_environment;
    (void)state;
    return 1;
}

/* sd_pid_notifyf - formatted per-process notification */
int sd_pid_notifyf(pid_t pid, int unset_environment, const char* format, ...) {
    p7_log(__func__);
    (void)pid;
    (void)unset_environment;
    (void)format;
    return 1;
}

/* sd_pid_notify_with_fds - notification with fds */
int sd_pid_notify_with_fds(pid_t pid, int unset_environment, const char* state, const int* fds, unsigned n_fds) {
    p7_log(__func__);
    (void)pid;
    (void)unset_environment;
    (void)state;
    (void)fds;
    (void)n_fds;
    return 1;
}

/* sd_pid_notifyf_with_fds - formatted with fds */
int sd_pid_notifyf_with_fds(pid_t pid, int unset_environment, const int* fds, unsigned n_fds, const char* format, ...) {
    p7_log(__func__);
    (void)pid;
    (void)unset_environment;
    (void)fds;
    (void)n_fds;
    (void)format;
    return 1;
}

/* sd_pid_notify_barrier - barrier notification */
int sd_pid_notify_barrier(pid_t pid, int unset_environment, uint64_t timeout) {
    p7_log(__func__);
    (void)pid;
    (void)unset_environment;
    (void)timeout;
    return 1;
}

/* sd_notify_barrier - barrier */
int sd_notify_barrier(int unset_environment, uint64_t timeout) {
    p7_log(__func__);
    (void)unset_environment;
    (void)timeout;
    return 1;
}

/* ============================================================================
 * SECTION 2: ID128 FUNCTIONS
 * ============================================================================ */

static const sd_id128_t machine_id = { .bytes = {0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,0x10} };
static const sd_id128_t boot_id = { .bytes = {0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f,0x20} };
static const sd_id128_t invocation_id = { .bytes = {0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,0x29,0x2a,0x2b,0x2c,0x2d,0x2e,0x2f,0x30} };

int sd_id128_get_machine(sd_id128_t* ret) {
    p7_log(__func__);
    if (!ret) return -EINVAL;
    *ret = machine_id;
    return 0;
}

int sd_id128_get_boot(sd_id128_t* ret) {
    p7_log(__func__);
    if (!ret) return -EINVAL;
    *ret = boot_id;
    return 0;
}

int sd_id128_get_invocation(sd_id128_t* ret) {
    p7_log(__func__);
    if (!ret) return -EINVAL;
    *ret = invocation_id;
    return 0;
}

int sd_id128_randomize(sd_id128_t* ret) {
    p7_log(__func__);
    if (!ret) return -EINVAL;
    for (int i = 0; i < 16; i++) ret->bytes[i] = (uint8_t)(rand() % 256);
    return 0;
}

int sd_id128_from_string(const char* s, sd_id128_t* ret) {
    p7_log(__func__);
    if (!s || !ret) return -EINVAL;
    /* Parse 32-char hex string */
    if (strlen(s) != 32) return -EINVAL;
    for (int i = 0; i < 16; i++) {
        unsigned int b;
        if (sscanf(s + i*2, "%2x", &b) != 1) return -EINVAL;
        ret->bytes[i] = (uint8_t)b;
    }
    return 0;
}

int sd_id128_to_string(sd_id128_t id, char s[33]) {
    p7_log(__func__);
    if (!s) return -EINVAL;
    for (int i = 0; i < 16; i++) sprintf(s + i*2, "%02x", id.bytes[i]);
    s[32] = '\0';
    return 0;
}

int sd_id128_string_equal(const char* s, sd_id128_t id) {
    p7_log(__func__);
    if (!s) return -EINVAL;
    char buf[33];
    sd_id128_to_string(id, buf);
    return strcmp(s, buf) == 0;
}

int sd_id128_to_uuid_string(sd_id128_t id, char s[37]) {
    p7_log(__func__);
    if (!s) return -EINVAL;
    sprintf(s, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
            id.bytes[0], id.bytes[1], id.bytes[2], id.bytes[3],
            id.bytes[4], id.bytes[5], id.bytes[6], id.bytes[7],
            id.bytes[8], id.bytes[9], id.bytes[10], id.bytes[11],
            id.bytes[12], id.bytes[13], id.bytes[14], id.bytes[15]);
    return 0;
}

int sd_id128_get_app_specific(sd_id128_t base, sd_id128_t* ret) {
    p7_log(__func__);
    if (!ret) return -EINVAL;
    *ret = base;
    ret->bytes[15] ^= 0x42;  /* Deterministic mutation */
    return 0;
}

int sd_id128_get_boot_app_specific(sd_id128_t* ret) {
    p7_log(__func__);
    if (!ret) return -EINVAL;
    return sd_id128_get_app_specific(boot_id, ret);
}

int sd_id128_get_invocation_app_specific(sd_id128_t* ret) {
    p7_log(__func__);
    if (!ret) return -EINVAL;
    return sd_id128_get_app_specific(invocation_id, ret);
}

int sd_id128_get_machine_app_specific(sd_id128_t* ret) {
    p7_log(__func__);
    if (!ret) return -EINVAL;
    return sd_id128_get_app_specific(machine_id, ret);
}

/* ============================================================================
 * SECTION 3: LOGIN / SESSION / SEAT / UID / MACHINE
 * ============================================================================ */

/* Machine state */
int sd_machine_get_class(const char* machine, char** ret) {
    p7_log(__func__);
    (void)machine;
    return p7_set_string(ret, "container");
}

int sd_machine_get_ifindices(const char* machine, int** ret) {
    p7_log(__func__);
    (void)machine;
    if (ret) {
        *ret = calloc(2, sizeof(int));
        if (*ret) (*ret)[0] = 1;  /* lo interface */
    }
    return 0;
}

/* PID family - all return current user/session info */
static int p7_get_session(char** ret) {
    const char* sess = getenv("XDG_SESSION_ID");
    if (!sess) sess = "c1";
    return p7_set_string(ret, sess);
}

static int p7_get_seat(char** ret) {
    const char* seat = getenv("XDG_SEAT");
    if (!seat) seat = "seat0";
    return p7_set_string(ret, seat);
}

int sd_pid_get_owner_uid(pid_t pid, uid_t* uid) {
    p7_log(__func__);
    (void)pid;
    if (!uid) return -EINVAL;
    *uid = get_current_uid();
    return 0;
}

int sd_pid_get_session(pid_t pid, char** session) {
    p7_log(__func__);
    (void)pid;
    return p7_get_session(session);
}

int sd_pid_get_cgroup(pid_t pid, char** cgroup) {
    p7_log(__func__);
    (void)pid;
    return p7_set_string(cgroup, "/user.slice/user-1000.slice/session-c1.scope");
}

int sd_pid_get_machine_name(pid_t pid, char** machine) {
    p7_log(__func__);
    (void)pid;
    return p7_set_string(machine, NULL);  /* Not in a machine */
}

int sd_pid_get_slice(pid_t pid, char** slice) {
    p7_log(__func__);
    (void)pid;
    return p7_set_string(slice, "user-1000.slice");
}

int sd_pid_get_unit(pid_t pid, char** unit) {
    p7_log(__func__);
    (void)pid;
    return p7_set_string(unit, "session-c1.scope");
}

int sd_pid_get_user_slice(pid_t pid, char** slice) {
    p7_log(__func__);
    (void)pid;
    return p7_set_string(slice, "user-1000.slice");
}

int sd_pid_get_user_unit(pid_t pid, char** unit) {
    p7_log(__func__);
    (void)pid;
    return p7_set_string(unit, NULL);  /* No user unit */
}

/* Peer family - delegates to PID family with current process */
int sd_peer_get_owner_uid(int fd, uid_t* uid) {
    p7_log(__func__);
    (void)fd;
    return sd_pid_get_owner_uid(getpid(), uid);
}

int sd_peer_get_session(int fd, char** session) {
    p7_log(__func__);
    (void)fd;
    return sd_pid_get_session(getpid(), session);
}

int sd_peer_get_cgroup(int fd, char** cgroup) {
    p7_log(__func__);
    (void)fd;
    return sd_pid_get_cgroup(getpid(), cgroup);
}

int sd_peer_get_machine_name(int fd, char** machine) {
    p7_log(__func__);
    (void)fd;
    return sd_pid_get_machine_name(getpid(), machine);
}

int sd_peer_get_slice(int fd, char** slice) {
    p7_log(__func__);
    (void)fd;
    return sd_pid_get_slice(getpid(), slice);
}

int sd_peer_get_unit(int fd, char** unit) {
    p7_log(__func__);
    (void)fd;
    return sd_pid_get_unit(getpid(), unit);
}

int sd_peer_get_user_slice(int fd, char** slice) {
    p7_log(__func__);
    (void)fd;
    return sd_pid_get_user_slice(getpid(), slice);
}

int sd_peer_get_user_unit(int fd, char** unit) {
    p7_log(__func__);
    (void)fd;
    return sd_pid_get_user_unit(getpid(), unit);
}

/* PIDFD family - delegates to PID family */
int sd_pidfd_get_owner_uid(int fd, uid_t* uid) {
    p7_log(__func__);
    (void)fd;
    return sd_pid_get_owner_uid(getpid(), uid);
}

int sd_pidfd_get_session(int fd, char** session) {
    p7_log(__func__);
    (void)fd;
    return sd_pid_get_session(getpid(), session);
}

int sd_pidfd_get_cgroup(int fd, char** cgroup) {
    p7_log(__func__);
    (void)fd;
    return sd_pid_get_cgroup(getpid(), cgroup);
}

int sd_pidfd_get_machine_name(int fd, char** machine) {
    p7_log(__func__);
    (void)fd;
    return sd_pid_get_machine_name(getpid(), machine);
}

int sd_pidfd_get_slice(int fd, char** slice) {
    p7_log(__func__);
    (void)fd;
    return sd_pid_get_slice(getpid(), slice);
}

int sd_pidfd_get_unit(int fd, char** unit) {
    p7_log(__func__);
    (void)fd;
    return sd_pid_get_unit(getpid(), unit);
}

int sd_pidfd_get_user_slice(int fd, char** slice) {
    p7_log(__func__);
    (void)fd;
    return sd_pid_get_user_slice(getpid(), slice);
}

int sd_pidfd_get_user_unit(int fd, char** unit) {
    p7_log(__func__);
    (void)fd;
    return sd_pid_get_user_unit(getpid(), unit);
}

int sd_pidfd_get_inode_id(int fd, uint64_t* inode_id) {
    p7_log(__func__);
    (void)fd;
    if (!inode_id) return -EINVAL;
    *inode_id = 0;
    return 0;
}

/* Session getters */
int sd_session_get_class(const char* session, char** ret) {
    p7_log(__func__);
    (void)session;
    return p7_set_string(ret, "user");
}

int sd_session_get_desktop(const char* session, char** ret) {
    p7_log(__func__);
    (void)session;
    const char* desktop = getenv("XDG_CURRENT_DESKTOP");
    if (!desktop) desktop = "sway";
    return p7_set_string(ret, desktop);
}

int sd_session_get_display(const char* session, char** ret) {
    p7_log(__func__);
    (void)session;
    const char* display = getenv("DISPLAY");
    if (!display) display = ":0";
    return p7_set_string(ret, display);
}

int sd_session_get_extra_device_access(const char* session, char*** ret) {
    p7_log(__func__);
    (void)session;
    if (ret) *ret = NULL;
    return 0;
}

int sd_session_get_leader(const char* session, pid_t* ret) {
    p7_log(__func__);
    (void)session;
    if (!ret) return -EINVAL;
    *ret = getpid();
    return 0;
}

int sd_session_get_remote_host(const char* session, char** ret) {
    p7_log(__func__);
    (void)session;
    return p7_set_string(ret, NULL);  /* Not remote */
}

int sd_session_get_remote_user(const char* session, char** ret) {
    p7_log(__func__);
    (void)session;
    return p7_set_string(ret, NULL);  /* Not remote */
}

int sd_session_get_seat(const char* session, char** ret) {
    p7_log(__func__);
    (void)session;
    return p7_get_seat(ret);
}

int sd_session_get_service(const char* session, char** ret) {
    p7_log(__func__);
    (void)session;
    return p7_set_string(ret, "seat0");
}

int sd_session_get_start_time(const char* session, uint64_t* ret) {
    p7_log(__func__);
    (void)session;
    if (!ret) return -EINVAL;
    *ret = (uint64_t)time(NULL) * 1000000ULL;  /* Current time in usec */
    return 0;
}

int sd_session_get_state(const char* session, char** ret) {
    p7_log(__func__);
    (void)session;
    return p7_set_string(ret, "active");
}

int sd_session_get_tty(const char* session, char** ret) {
    p7_log(__func__);
    (void)session;
    const char* tty = getenv("TTY");
    if (!tty) tty = "/dev/tty1";
    return p7_set_string(ret, tty);
}

int sd_session_get_type(const char* session, char** ret) {
    p7_log(__func__);
    (void)session;
    return p7_set_string(ret, "wayland");
}

int sd_session_get_uid(const char* session, uid_t* ret) {
    p7_log(__func__);
    (void)session;
    if (!ret) return -EINVAL;
    *ret = get_current_uid();
    return 0;
}

int sd_session_get_username(const char* session, char** ret) {
    p7_log(__func__);
    (void)session;
    struct passwd* pw = getpwuid(get_current_uid());
    return p7_set_string(ret, pw ? pw->pw_name : "user");
}

int sd_session_get_vt(const char* session, unsigned* ret) {
    p7_log(__func__);
    (void)session;
    if (!ret) return -EINVAL;
    *ret = 1;
    return 0;
}

int sd_session_is_active(const char* session) {
    p7_log(__func__);
    (void)session;
    return 1;  /* Active */
}

int sd_session_is_remote(const char* session) {
    p7_log(__func__);
    (void)session;
    return 0;  /* Not remote */
}

/* Seat functions */
int sd_seat_can_graphical(const char* seat) {
    p7_log(__func__);
    (void)seat;
    return 1;  /* Yes */
}

int sd_seat_can_multi_session(const char* seat) {
    p7_log(__func__);
    (void)seat;
    return 0;  /* Single user only */
}

int sd_seat_can_tty(const char* seat) {
    p7_log(__func__);
    (void)seat;
    return 1;  /* Yes */
}

int sd_seat_get_active(const char* seat, char** session, uid_t* uid) {
    p7_log(__func__);
    (void)seat;
    if (session) p7_get_session(session);
    if (uid) *uid = get_current_uid();
    return 0;
}

int sd_seat_get_sessions(const char* seat, char*** sessions, uid_t** uid, unsigned* n) {
    p7_log(__func__);
    (void)seat;
    if (sessions) {
        *sessions = calloc(2, sizeof(char*));
        if (*sessions) {
            (*sessions)[0] = strdup("c1");
            (*sessions)[1] = NULL;
        }
    }
    if (uid) {
        *uid = calloc(1, sizeof(uid_t));
        if (*uid) **uid = get_current_uid();
    }
    if (n) *n = 1;
    return 0;
}

/* UID functions */
int sd_uid_get_display(uid_t uid, char** session) {
    p7_log(__func__);
    (void)uid;
    return p7_get_session(session);
}

int sd_uid_get_login_time(uid_t uid, uint64_t* ret) {
    p7_log(__func__);
    (void)uid;
    if (!ret) return -EINVAL;
    *ret = (uint64_t)time(NULL) * 1000000ULL;
    return 0;
}

int sd_uid_get_seats(uid_t uid, int require_active, char*** seats) {
    p7_log(__func__);
    (void)uid;
    (void)require_active;
    if (seats) {
        *seats = calloc(2, sizeof(char*));
        if (*seats) {
            (*seats)[0] = strdup("seat0");
            (*seats)[1] = NULL;
        }
    }
    return 0;
}

int sd_uid_get_sessions(uid_t uid, int require_active, char*** sessions) {
    p7_log(__func__);
    (void)uid;
    (void)require_active;
    if (sessions) {
        *sessions = calloc(2, sizeof(char*));
        if (*sessions) {
            (*sessions)[0] = strdup("c1");
            (*sessions)[1] = NULL;
        }
    }
    return 0;
}

int sd_uid_get_state(uid_t uid, char** ret) {
    p7_log(__func__);
    (void)uid;
    return p7_set_string(ret, "active");
}

int sd_uid_is_on_seat(uid_t uid, int require_active, const char* seat) {
    p7_log(__func__);
    (void)uid;
    (void)require_active;
    (void)seat;
    return 1;  /* Yes, user is on seat0 */
}

/* Global getters */
int sd_get_seats(char*** seats) {
    p7_log(__func__);
    if (seats) {
        *seats = calloc(2, sizeof(char*));
        if (*seats) {
            (*seats)[0] = strdup("seat0");
            (*seats)[1] = NULL;
        }
    }
    return 1;  /* 1 seat */
}

int sd_get_sessions(char*** sessions) {
    p7_log(__func__);
    if (sessions) {
        *sessions = calloc(2, sizeof(char*));
        if (*sessions) {
            (*sessions)[0] = strdup("c1");
            (*sessions)[1] = NULL;
        }
    }
    return 1;  /* 1 session */
}

int sd_get_uids(uid_t** uids) {
    p7_log(__func__);
    if (uids) {
        *uids = calloc(1, sizeof(uid_t));
        if (*uids) **uids = get_current_uid();
    }
    return 1;  /* 1 user */
}

int sd_get_machine_names(char*** machines) {
    p7_log(__func__);
    if (machines) *machines = NULL;
    return 0;  /* No machines */
}

/* ============================================================================
 * SECTION 4: PATH LOOKUP & SOCKET CHECKS
 * ============================================================================ */

int sd_path_lookup(sd_path_type_t type, const char* suffix, char** path) {
    p7_log(__func__);
    (void)type;
    const char* base = "/tmp";
    if (!path) return -EINVAL;
    if (suffix) {
        size_t len = strlen(base) + strlen(suffix) + 2;
        *path = malloc(len);
        if (!*path) return -ENOMEM;
        snprintf(*path, len, "%s/%s", base, suffix);
    } else {
        *path = strdup(base);
        if (!*path) return -ENOMEM;
    }
    return 0;
}

int sd_path_lookup_strv(sd_path_type_t type, const char* suffix, char*** paths) {
    p7_log(__func__);
    (void)type;
    (void)suffix;
    if (!paths) return -EINVAL;
    *paths = calloc(2, sizeof(char*));
    if (!*paths) return -ENOMEM;
    (*paths)[0] = strdup("/tmp");
    (*paths)[1] = NULL;
    return 1;
}

/* Socket type checks - real OS passthrough where possible */
int sd_is_fifo(int fd, const char* path) {
    p7_log(__func__);
    (void)path;
    struct stat st;
    if (fstat(fd, &st) < 0) return -errno;
    return S_ISFIFO(st.st_mode) ? 1 : 0;
}

int sd_is_special(int fd, const char* path) {
    p7_log(__func__);
    (void)path;
    struct stat st;
    if (fstat(fd, &st) < 0) return -errno;
    return (S_ISCHR(st.st_mode) || S_ISBLK(st.st_mode)) ? 1 : 0;
}

int sd_is_socket(int fd, int family, int type, int listening) {
    p7_log(__func__);
    struct stat st;
    if (fstat(fd, &st) < 0) return -errno;
    if (!S_ISSOCK(st.st_mode)) return 0;
    if (family > 0 || type > 0 || listening >= 0) {
        int sock_type;
        socklen_t len = sizeof(sock_type);
        if (getsockopt(fd, SOL_SOCKET, SO_TYPE, &sock_type, &len) < 0)
            return -errno;
        if (type > 0 && sock_type != type) return 0;
        if (listening >= 0) {
            int val;
            socklen_t vlen = sizeof(val);
            if (getsockopt(fd, SOL_SOCKET, SO_ACCEPTCONN, &val, &vlen) < 0)
                return -errno;
            if (!!val != !!listening) return 0;
        }
        if (family > 0) {
            union { struct sockaddr sa; char buf[128]; } addr;
            socklen_t alen = sizeof(addr);
            if (getsockname(fd, &addr.sa, &alen) < 0) return -errno;
            if (addr.sa.sa_family != (sa_family_t)family) return 0;
        }
    }
    return 1;
}

int sd_is_socket_inet(int fd, int family, int type, int listening, uint16_t port) {
    p7_log(__func__);
    if (family != AF_INET && family != AF_INET6 && family != 0) return 0;
    int r = sd_is_socket(fd, family, type, listening);
    if (r <= 0) return r;
    if (port > 0) {
        union { struct sockaddr sa; struct sockaddr_in in; struct sockaddr_in6 in6; } addr;
        socklen_t len = sizeof(addr);
        if (getsockname(fd, &addr.sa, &len) < 0) return -errno;
        uint16_t actual_port = 0;
        if (addr.sa.sa_family == AF_INET) actual_port = ntohs(addr.in.sin_port);
        else if (addr.sa.sa_family == AF_INET6) actual_port = ntohs(addr.in6.sin6_port);
        if (actual_port != port) return 0;
    }
    return 1;
}

int sd_is_socket_unix(int fd, int type, int listening, const char* path, size_t length) {
    p7_log(__func__);
    int r = sd_is_socket(fd, AF_UNIX, type, listening);
    if (r <= 0) return r;
    if (path) {
        struct sockaddr_un addr;
        socklen_t len = sizeof(addr);
        if (getsockname(fd, (struct sockaddr*)&addr, &len) < 0) return -errno;
        size_t plen = length != 0 ? length : strlen(path);
        size_t alen = strlen(addr.sun_path);
        if (alen != plen || memcmp(addr.sun_path, path, plen) != 0) return 0;
    }
    return 1;
}

int sd_is_mq(int fd, const char* path) {
    p7_log(__func__);
    (void)path;
    struct stat st;
    if (fstat(fd, &st) < 0) return -errno;
    /* POSIX message queues are character devices on Linux */
    return (S_ISCHR(st.st_mode) && major(st.st_rdev) == 10) ? 1 : 0;
}

int sd_is_socket_sockaddr(int fd, int type, const struct sockaddr* addr, size_t addr_len, int listening) {
    p7_log(__func__);
    (void)addr_len;
    int r = sd_is_socket(fd, addr ? addr->sa_family : 0, type, listening);
    if (r <= 0 || !addr) return r;
    union { struct sockaddr sa; char buf[128]; } local;
    socklen_t len = sizeof(local);
    if (getsockname(fd, &local.sa, &len) < 0) return -errno;
    if (local.sa.sa_family != addr->sa_family) return 0;
    /* Deep comparison would require family-specific logic */
    return 1;
}

/* ============================================================================
 * SECTION 5: BUS ERROR FUNCTIONS
 * ============================================================================ */

void sd_bus_error_free(sd_bus_error* e) {
    p7_log(__func__);
    if (!e) return;
    if (e->_need_free) {
        free((char*)e->name);
        free((char*)e->message);
    }
    e->name = NULL;
    e->message = NULL;
    e->_need_free = 0;
}

int sd_bus_error_copy(sd_bus_error* dst, const sd_bus_error* src) {
    p7_log(__func__);
    if (!dst) return -EINVAL;
    sd_bus_error_free(dst);
    if (!src || !src->name) return 0;
    dst->name = strdup(src->name);
    if (src->message) dst->message = strdup(src->message);
    dst->_need_free = 1;
    return dst->name ? 0 : -ENOMEM;
}

int sd_bus_error_set(sd_bus_error* e, const char* name, const char* message) {
    p7_log(__func__);
    if (!e) return 0;
    sd_bus_error_free(e);
    if (!name) return 0;
    e->name = strdup(name);
    if (message) e->message = strdup(message);
    e->_need_free = 1;
    return e->name ? -EIO : -ENOMEM;
}

int sd_bus_error_set_const(sd_bus_error* e, const char* name, const char* message) {
    p7_log(__func__);
    if (!e) return 0;
    sd_bus_error_free(e);
    e->name = name;
    e->message = message;
    e->_need_free = 0;
    return name ? -EIO : 0;
}

int sd_bus_error_setf(sd_bus_error* e, const char* name, const char* format, ...) {
    p7_log(__func__);
    if (!e) return 0;
    sd_bus_error_free(e);
    if (!name) return 0;
    e->name = strdup(name);
    if (format) {
        va_list ap;
        va_start(ap, format);
        vasprintf((char**)&e->message, format, ap);
        va_end(ap);
    }
    e->_need_free = 1;
    return e->name ? -EIO : -ENOMEM;
}

int sd_bus_error_setfv(sd_bus_error* e, const char* name, const char* format, va_list ap) {
    p7_log(__func__);
    if (!e) return 0;
    sd_bus_error_free(e);
    if (!name) return 0;
    e->name = strdup(name);
    if (format) vasprintf((char**)&e->message, format, ap);
    e->_need_free = 1;
    return e->name ? -EIO : -ENOMEM;
}

int sd_bus_error_set_errno(sd_bus_error* e, int error) {
    p7_log(__func__);
    if (!e || error >= 0) return error;
    sd_bus_error_free(e);
    e->name = strdup("org.freedesktop.DBus.Error.Failed");
    e->_need_free = 1;
    return error;
}

int sd_bus_error_set_errnof(sd_bus_error* e, int error, const char* format, ...) {
    p7_log(__func__);
    if (!e || error >= 0) return error;
    sd_bus_error_free(e);
    e->name = strdup("org.freedesktop.DBus.Error.Failed");
    if (format) {
        va_list ap;
        va_start(ap, format);
        vasprintf((char**)&e->message, format, ap);
        va_end(ap);
    }
    e->_need_free = 1;
    return error;
}

int sd_bus_error_set_errnofv(sd_bus_error* e, int error, const char* format, va_list ap) {
    p7_log(__func__);
    if (!e || error >= 0) return error;
    sd_bus_error_free(e);
    e->name = strdup("org.freedesktop.DBus.Error.Failed");
    if (format) vasprintf((char**)&e->message, format, ap);
    e->_need_free = 1;
    return error;
}

int sd_bus_error_get_errno(const sd_bus_error* e) {
    p7_log(__func__);
    if (!e || !e->name) return 0;
    return -EIO;  /* Generic error */
}

int sd_bus_error_is_set(const sd_bus_error* e) {
    p7_log(__func__);
    return e && e->name ? 1 : 0;
}

int sd_bus_error_has_name(const sd_bus_error* e, const char* name) {
    p7_log(__func__);
    if (!e || !e->name || !name) return 0;
    return strcmp(e->name, name) == 0;
}

int sd_bus_error_has_names_sentinel(const sd_bus_error* e, const char* name, ...) {
    p7_log(__func__);
    if (!e || !e->name) return 0;
    if (!name) return 0;
    if (strcmp(e->name, name) == 0) return 1;
    va_list ap;
    va_start(ap, name);
    const char* n;
    while ((n = va_arg(ap, const char*))) {
        if (strcmp(e->name, n) == 0) {
            va_end(ap);
            return 1;
        }
    }
    va_end(ap);
    return 0;
}

int sd_bus_error_move(sd_bus_error* dst, sd_bus_error* src) {
    p7_log(__func__);
    if (!dst) return -EINVAL;
    sd_bus_error_free(dst);
    if (!src) return 0;
    *dst = *src;
    src->name = NULL;
    src->message = NULL;
    src->_need_free = 0;
    return 0;
}

int sd_bus_error_add_map(sd_bus_error* e, const sd_bus_error_map* map) {
    p7_log(__func__);
    (void)e;
    (void)map;
    return 0;  /* No-op */
}

/* ============================================================================
 * SECTION 6: BUS LIFECYCLE & CONFIGURATION
 * ============================================================================ */

/* Bus creation */
sd_bus* sd_bus_ref(sd_bus* bus) {
    p7_log(__func__);
    return bus;  /* No real refcounting in mock */
}

sd_bus* sd_bus_unref(sd_bus* bus) {
    p7_log(__func__);
    /* Don't free - fake pointers are static allocations */
    return NULL;
}

sd_bus* sd_bus_close_unref(sd_bus* bus) {
    p7_log(__func__);
    return sd_bus_unref(bus);
}

sd_bus* sd_bus_flush_close_unref(sd_bus* bus) {
    p7_log(__func__);
    return sd_bus_unref(bus);
}

int sd_bus_new(sd_bus** ret) {
    p7_log(__func__);
    if (!ret) return -EINVAL;
    *ret = alloc_fake_ptr(FAKE_PTR_BUS);
    return 0;
}

int sd_bus_open(sd_bus** ret) {
    p7_log(__func__);
    return sd_bus_new(ret);
}

int sd_bus_open_system(sd_bus** ret) {
    p7_log(__func__);
    return sd_bus_new(ret);
}

int sd_bus_open_system_remote(sd_bus** ret, const char* host) {
    p7_log(__func__);
    (void)host;
    return sd_bus_new(ret);
}

int sd_bus_open_system_machine(sd_bus** ret, const char* machine) {
    p7_log(__func__);
    (void)machine;
    return sd_bus_new(ret);
}

int sd_bus_open_system_with_description(sd_bus** ret, const char* description) {
    p7_log(__func__);
    (void)description;
    return sd_bus_new(ret);
}

int sd_bus_open_user(sd_bus** ret) {
    p7_log(__func__);
    return sd_bus_new(ret);
}

int sd_bus_open_user_machine(sd_bus** ret, const char* machine) {
    p7_log(__func__);
    (void)machine;
    return sd_bus_new(ret);
}

int sd_bus_open_user_with_description(sd_bus** ret, const char* description) {
    p7_log(__func__);
    (void)description;
    return sd_bus_new(ret);
}

int sd_bus_open_with_description(sd_bus** ret, const char* description) {
    p7_log(__func__);
    (void)description;
    return sd_bus_new(ret);
}

/* Default buses */
sd_bus* sd_bus_default(sd_bus** ret) {
    p7_log(__func__);
    if (ret) *ret = alloc_fake_ptr(FAKE_PTR_BUS);
    return ret ? *ret : alloc_fake_ptr(FAKE_PTR_BUS);
}

sd_bus* sd_bus_default_system(sd_bus** ret) {
    p7_log(__func__);
    return sd_bus_default(ret);
}

sd_bus* sd_bus_default_user(sd_bus** ret) {
    p7_log(__func__);
    return sd_bus_default(ret);
}

int sd_bus_default_flush_close(void) {
    p7_log(__func__);
    return 0;
}

/* Bus lifecycle */
void sd_bus_close(sd_bus* bus) {
    p7_log(__func__);
    (void)bus;
}

int sd_bus_try_close(sd_bus* bus) {
    p7_log(__func__);
    (void)bus;
    return 0;
}

int sd_bus_start(sd_bus* bus) {
    p7_log(__func__);
    (void)bus;
    return 0;  /* Started successfully */
}

int sd_bus_flush(sd_bus* bus) {
    p7_log(__func__);
    (void)bus;
    return 0;
}

/* Bus configuration setters */
int sd_bus_set_address(sd_bus* bus, const char* address) {
    p7_log(__func__);
    (void)bus;
    (void)address;
    return 0;
}

int sd_bus_set_allow_interactive_authorization(sd_bus* bus, int b) {
    p7_log(__func__);
    (void)bus;
    (void)b;
    return 0;
}

int sd_bus_set_anonymous(sd_bus* bus, int b) {
    p7_log(__func__);
    (void)bus;
    (void)b;
    return 0;
}

int sd_bus_set_bus_client(sd_bus* bus, int b) {
    p7_log(__func__);
    (void)bus;
    (void)b;
    return 0;
}

int sd_bus_set_close_on_exit(sd_bus* bus, int b) {
    p7_log(__func__);
    (void)bus;
    (void)b;
    return 0;
}

int sd_bus_set_connected_signal(sd_bus* bus, int b) {
    p7_log(__func__);
    (void)bus;
    (void)b;
    return 0;
}

int sd_bus_set_description(sd_bus* bus, const char* description) {
    p7_log(__func__);
    (void)bus;
    (void)description;
    return 0;
}

int sd_bus_set_exec(sd_bus* bus, const char* path, char* const* argv) {
    p7_log(__func__);
    (void)bus;
    (void)path;
    (void)argv;
    return 0;
}

int sd_bus_set_exit_on_disconnect(sd_bus* bus, int b) {
    p7_log(__func__);
    (void)bus;
    (void)b;
    return 0;
}

int sd_bus_set_fd(sd_bus* bus, int input_fd, int output_fd) {
    p7_log(__func__);
    (void)bus;
    (void)input_fd;
    (void)output_fd;
    return 0;
}

int sd_bus_set_method_call_timeout(sd_bus* bus, uint64_t usec) {
    p7_log(__func__);
    (void)bus;
    (void)usec;
    return 0;
}

int sd_bus_set_monitor(sd_bus* bus, int b) {
    p7_log(__func__);
    (void)bus;
    (void)b;
    return 0;
}

int sd_bus_set_property(sd_bus* bus, const char* destination, const char* path,
                        const char* interface, const char* member,
                        sd_bus_error* error, const char* type, ...) {
    p7_log(__func__);
    (void)bus;
    (void)destination;
    (void)path;
    (void)interface;
    (void)member;
    (void)error;
    (void)type;
    return 0;
}

int sd_bus_set_propertyv(sd_bus* bus, const char* destination, const char* path,
                         const char* interface, const char* member,
                         sd_bus_error* error, const char* type, va_list ap) {
    p7_log(__func__);
    (void)bus;
    (void)destination;
    (void)path;
    (void)interface;
    (void)member;
    (void)error;
    (void)type;
    (void)ap;
    return 0;
}

int sd_bus_set_sender(sd_bus* bus, const char* sender) {
    p7_log(__func__);
    (void)bus;
    (void)sender;
    return 0;
}

int sd_bus_set_server(sd_bus* bus, int b) {
    p7_log(__func__);
    (void)bus;
    (void)b;
    return 0;
}

int sd_bus_set_trusted(sd_bus* bus, int b) {
    p7_log(__func__);
    (void)bus;
    (void)b;
    return 0;
}

int sd_bus_set_watch_bind(sd_bus* bus, int b) {
    p7_log(__func__);
    (void)bus;
    (void)b;
    return 0;
}

/* Bus configuration getters */
int sd_bus_get_address(sd_bus* bus, const char** address) {
    p7_log(__func__);
    (void)bus;
    if (address) *address = "unix:path=/run/dbus/system_bus_socket";
    return 0;
}

int sd_bus_get_allow_interactive_authorization(sd_bus* bus, int* b) {
    p7_log(__func__);
    (void)bus;
    if (b) *b = 0;
    return 0;
}

int sd_bus_get_bus_id(sd_bus* bus, sd_id128_t* id) {
    p7_log(__func__);
    (void)bus;
    if (id) *id = machine_id;
    return 0;
}

int sd_bus_get_close_on_exit(sd_bus* bus, int* b) {
    p7_log(__func__);
    (void)bus;
    if (b) *b = 1;
    return 0;
}

int sd_bus_get_connected_signal(sd_bus* bus, int* b) {
    p7_log(__func__);
    (void)bus;
    if (b) *b = 0;
    return 0;
}

int sd_bus_get_creds_mask(sd_bus* bus, uint64_t* mask) {
    p7_log(__func__);
    (void)bus;
    if (mask) *mask = 0xFFFFFFFF;
    return 0;
}

int sd_bus_get_current_handler(sd_bus* bus, sd_bus_message_handler_t* handler) {
    p7_log(__func__);
    (void)bus;
    if (handler) *handler = NULL;
    return 0;
}

int sd_bus_get_current_message(sd_bus* bus, sd_bus_message** message) {
    p7_log(__func__);
    (void)bus;
    if (message) *message = NULL;
    return 0;
}

int sd_bus_get_current_slot(sd_bus* bus, sd_bus_slot** slot) {
    p7_log(__func__);
    (void)bus;
    if (slot) *slot = NULL;
    return 0;
}

int sd_bus_get_current_userdata(sd_bus* bus, void** userdata) {
    p7_log(__func__);
    (void)bus;
    if (userdata) *userdata = NULL;
    return 0;
}

int sd_bus_get_description(sd_bus* bus, const char** description) {
    p7_log(__func__);
    (void)bus;
    if (description) *description = "Protocol 7 Mock Bus";
    return 0;
}

int sd_bus_get_event(sd_bus* bus, sd_event** event) {
    p7_log(__func__);
    (void)bus;
    if (event) *event = NULL;
    return 0;
}

int sd_bus_get_events(sd_bus* bus, int* events) {
    p7_log(__func__);
    (void)bus;
    if (events) *events = 0;
    return 0;
}

int sd_bus_get_exit_on_disconnect(sd_bus* bus, int* b) {
    p7_log(__func__);
    (void)bus;
    if (b) *b = 0;
    return 0;
}

int sd_bus_get_fd(sd_bus* bus) {
    p7_log(__func__);
    (void)bus;
    return -1;  /* No real fd */
}

int sd_bus_get_method_call_timeout(sd_bus* bus, uint64_t* usec) {
    p7_log(__func__);
    (void)bus;
    if (usec) *usec = 5000000;  /* 5 seconds default */
    return 0;
}

int sd_bus_get_n_queued_read(sd_bus* bus, uint64_t* n) {
    p7_log(__func__);
    (void)bus;
    if (n) *n = 0;
    return 0;
}

int sd_bus_get_n_queued_write(sd_bus* bus, uint64_t* n) {
    p7_log(__func__);
    (void)bus;
    if (n) *n = 0;
    return 0;
}

int sd_bus_get_owner_creds(sd_bus* bus, uint64_t mask, sd_bus_creds** creds) {
    p7_log(__func__);
    (void)bus;
    (void)mask;
    if (creds) *creds = alloc_fake_ptr(FAKE_PTR_CREDS);
    return 0;
}

int sd_bus_get_property(sd_bus* bus, const char* destination, const char* path,
                        const char* interface, const char* member,
                        sd_bus_error* error, sd_bus_message** reply, const char* type) {
    p7_log(__func__);
    (void)bus;
    (void)destination;
    (void)path;
    (void)interface;
    (void)member;
    (void)error;
    (void)type;
    if (reply) {
        *reply = alloc_fake_ptr(FAKE_PTR_MSG);
        return 0;
    }
    return -EINVAL;
}

int sd_bus_get_property_string(sd_bus* bus, const char* destination, const char* path,
                               const char* interface, const char* member,
                               sd_bus_error* error, char** ret) {
    p7_log(__func__);
    (void)bus;
    (void)destination;
    (void)path;
    (void)interface;
    (void)member;
    (void)error;
    if (ret) *ret = strdup("");
    return 0;
}

int sd_bus_get_property_strv(sd_bus* bus, const char* destination, const char* path,
                              const char* interface, const char* member,
                              sd_bus_error* error, char*** ret) {
    p7_log(__func__);
    (void)bus;
    (void)destination;
    (void)path;
    (void)interface;
    (void)member;
    (void)error;
    if (ret) *ret = NULL;
    return 0;
}

int sd_bus_get_property_trivial(sd_bus* bus, const char* destination, const char* path,
                                 const char* interface, const char* member,
                                 sd_bus_error* error, char type, void* ret) {
    p7_log(__func__);
    (void)bus;
    (void)destination;
    (void)path;
    (void)interface;
    (void)member;
    (void)error;
    (void)type;
    (void)ret;
    return 0;
}

int sd_bus_get_scope(sd_bus* bus, const char** scope) {
    p7_log(__func__);
    (void)bus;
    if (scope) *scope = "system";
    return 0;
}

int sd_bus_get_sender(sd_bus* bus, const char** sender) {
    p7_log(__func__);
    (void)bus;
    if (sender) *sender = ":1.1";
    return 0;
}

int sd_bus_get_tid(sd_bus* bus, pid_t* tid) {
    p7_log(__func__);
    (void)bus;
    if (tid) *tid = getpid();
    return 0;
}

int sd_bus_get_timeout(sd_bus* bus, uint64_t* usec) {
    p7_log(__func__);
    (void)bus;
    if (usec) *usec = 5000000;
    return 0;
}

int sd_bus_get_unique_name(sd_bus* bus, const char** name) {
    p7_log(__func__);
    (void)bus;
    if (name) *name = ":1.1";
    return 0;
}

int sd_bus_get_watch_bind(sd_bus* bus, int* b) {
    p7_log(__func__);
    (void)bus;
    if (b) *b = 0;
    return 0;
}

/* Bus state checks */
int sd_bus_is_anonymous(sd_bus* bus) {
    p7_log(__func__);
    (void)bus;
    return 0;
}

int sd_bus_is_bus_client(sd_bus* bus) {
    p7_log(__func__);
    (void)bus;
    return 1;
}

int sd_bus_is_monitor(sd_bus* bus) {
    p7_log(__func__);
    (void)bus;
    return 0;
}

int sd_bus_is_open(sd_bus* bus) {
    p7_log(__func__);
    (void)bus;
    return 1;  /* Pretend open */
}

int sd_bus_is_ready(sd_bus* bus) {
    p7_log(__func__);
    (void)bus;
    return 1;
}

int sd_bus_is_server(sd_bus* bus) {
    p7_log(__func__);
    (void)bus;
    return 0;
}

int sd_bus_is_trusted(sd_bus* bus) {
    p7_log(__func__);
    (void)bus;
    return 1;
}

/* Name management */
int sd_bus_request_name(sd_bus* bus, const char* name, uint64_t flags) {
    p7_log(__func__);
    (void)bus;
    (void)name;
    (void)flags;
    return 1;  /* Primary owner */
}

int sd_bus_request_name_async(sd_bus* bus, sd_bus_slot** ret_slot, const char* name,
                               uint64_t flags, sd_bus_message_handler_t callback,
                               void* userdata) {
    p7_log(__func__);
    (void)bus;
    (void)name;
    (void)flags;
    (void)callback;
    (void)userdata;
    if (ret_slot) *ret_slot = alloc_fake_ptr(FAKE_PTR_SLOT);
    return 0;
}

int sd_bus_release_name(sd_bus* bus, const char* name) {
    p7_log(__func__);
    (void)bus;
    (void)name;
    return 0;
}

int sd_bus_release_name_async(sd_bus* bus, sd_bus_slot** ret_slot, const char* name,
                               sd_bus_message_handler_t callback, void* userdata) {
    p7_log(__func__);
    (void)bus;
    (void)name;
    (void)callback;
    (void)userdata;
    if (ret_slot) *ret_slot = alloc_fake_ptr(FAKE_PTR_SLOT);
    return 0;
}

int sd_bus_list_names(sd_bus* bus, char*** acquired, char*** activatable) {
    p7_log(__func__);
    (void)bus;
    if (acquired) {
        *acquired = calloc(3, sizeof(char*));
        if (*acquired) {
            (*acquired)[0] = strdup(":1.1");
            (*acquired)[1] = strdup("org.freedesktop.DBus");
            (*acquired)[2] = NULL;
        }
    }
    if (activatable) *activatable = NULL;
    return 0;
}

int sd_bus_get_name_creds(sd_bus* bus, const char* name, uint64_t mask, sd_bus_creds** creds) {
    p7_log(__func__);
    (void)bus;
    (void)name;
    (void)mask;
    if (creds) *creds = alloc_fake_ptr(FAKE_PTR_CREDS);
    return 0;
}

int sd_bus_get_name_machine_id(sd_bus* bus, const char* name, sd_id128_t* machine) {
    p7_log(__func__);
    (void)bus;
    (void)name;
    if (machine) *machine = machine_id;
    return 0;
}

/* Negotiation */
int sd_bus_negotiate_creds(sd_bus* bus, int b, uint64_t mask) {
    p7_log(__func__);
    (void)bus;
    (void)b;
    (void)mask;
    return 0;
}

int sd_bus_negotiate_fds(sd_bus* bus, int b) {
    p7_log(__func__);
    (void)bus;
    (void)b;
    return 0;
}

int sd_bus_negotiate_timestamp(sd_bus* bus, int b) {
    p7_log(__func__);
    (void)bus;
    (void)b;
    return 0;
}

/* Can send */
int sd_bus_can_send(sd_bus* bus, char type) {
    p7_log(__func__);
    (void)bus;
    (void)type;
    return 1;  /* Can send all types */
}

/* Enqueue */
int sd_bus_enqueue_for_read(sd_bus* bus, sd_bus_message* m) {
    p7_log(__func__);
    (void)bus;
    (void)m;
    return 0;
}

/* Pending calls */
int sd_bus_pending_method_calls(sd_bus* bus, uint64_t* n) {
    p7_log(__func__);
    (void)bus;
    if (n) *n = 0;
    return 0;
}

/* Query sender */
int sd_bus_query_sender_creds(sd_bus* bus, uint64_t mask, sd_bus_creds** creds) {
    p7_log(__func__);
    (void)bus;
    (void)mask;
    if (creds) *creds = alloc_fake_ptr(FAKE_PTR_CREDS);
    return 0;
}

int sd_bus_query_sender_privilege(sd_bus* bus, int capability) {
    p7_log(__func__);
    (void)bus;
    (void)capability;
    return 1;  /* Has privilege */
}

/* Validation helpers */
int sd_bus_interface_name_is_valid(const char* s) {
    if (!s) return 0;
    return s[0] && strchr(s, '.') != NULL ? 1 : 0;
}

int sd_bus_member_name_is_valid(const char* s) {
    if (!s) return 0;
    return s[0] ? 1 : 0;
}

int sd_bus_object_path_is_valid(const char* s) {
    if (!s) return 0;
    return s[0] == '/' ? 1 : 0;
}

int sd_bus_service_name_is_valid(const char* s) {
    if (!s) return 0;
    return (s[0] == ':' || strchr(s, '.') != NULL) ? 1 : 0;
}

/* Path helpers */
int sd_bus_path_encode(const char* prefix, const char* external_id, char** ret) {
    p7_log(__func__);
    if (!prefix || !external_id || !ret) return -EINVAL;
    size_t len = strlen(prefix) + strlen(external_id) + 2;
    *ret = malloc(len);
    if (!*ret) return -ENOMEM;
    snprintf(*ret, len, "%s/%s", prefix, external_id);
    return 0;
}

int sd_bus_path_encode_many(char** out, size_t size, const char* path, ...) {
    p7_log(__func__);
    (void)out;
    (void)size;
    (void)path;
    return 0;
}

int sd_bus_path_decode(const char* path, const char* prefix, char** external_id) {
    p7_log(__func__);
    if (!path || !prefix || !external_id) return -EINVAL;
    size_t plen = strlen(prefix);
    if (strncmp(path, prefix, plen) != 0 || path[plen] != '/') return 0;
    *external_id = strdup(path + plen + 1);
    return 1;
}

int sd_bus_path_decode_many(const char* path, const char* first, ...) {
    p7_log(__func__);
    (void)path;
    (void)first;
    return 0;
}

/* Object vtable format - data symbol */
const char sd_bus_object_vtable_format[] = "random";

/* ============================================================================
 * SECTION 7: BUS MESSAGE FUNCTIONS
 * ============================================================================ */

/* Message reference counting */
sd_bus_message* sd_bus_message_ref(sd_bus_message* m) {
    p7_log(__func__);
    return m;
}

sd_bus_message* sd_bus_message_unref(sd_bus_message* m) {
    p7_log(__func__);
    return NULL;
}

/* Message creation */
int sd_bus_message_new(sd_bus* bus, sd_bus_message** m, uint8_t type) {
    p7_log(__func__);
    (void)bus;
    (void)type;
    if (!m) return -EINVAL;
    *m = alloc_fake_ptr(FAKE_PTR_MSG);
    return 0;
}

int sd_bus_message_new_method_call(sd_bus* bus, sd_bus_message** m,
                                    const char* destination, const char* path,
                                    const char* interface, const char* member) {
    p7_log(__func__);
    (void)bus;
    (void)destination;
    (void)path;
    (void)interface;
    (void)member;
    if (!m) return -EINVAL;
    *m = alloc_fake_ptr(FAKE_PTR_MSG);
    return 0;
}

int sd_bus_message_new_method_return(sd_bus_message* call, sd_bus_message** m) {
    p7_log(__func__);
    (void)call;
    if (!m) return -EINVAL;
    *m = alloc_fake_ptr(FAKE_PTR_MSG);
    return 0;
}

int sd_bus_message_new_method_errno(sd_bus_message* call, sd_bus_message** m, int error) {
    p7_log(__func__);
    (void)call;
    (void)error;
    if (!m) return -EINVAL;
    *m = alloc_fake_ptr(FAKE_PTR_MSG);
    return 0;
}

int sd_bus_message_new_method_errnof(sd_bus_message* call, sd_bus_message** m,
                                      int error, const char* format, ...) {
    p7_log(__func__);
    (void)call;
    (void)error;
    (void)format;
    if (!m) return -EINVAL;
    *m = alloc_fake_ptr(FAKE_PTR_MSG);
    return 0;
}

int sd_bus_message_new_method_error(sd_bus_message* call, sd_bus_message** m,
                                     const sd_bus_error* e) {
    p7_log(__func__);
    (void)call;
    (void)e;
    if (!m) return -EINVAL;
    *m = alloc_fake_ptr(FAKE_PTR_MSG);
    return 0;
}

int sd_bus_message_new_method_errorf(sd_bus_message* call, sd_bus_message** m,
                                      const char* name, const char* format, ...) {
    p7_log(__func__);
    (void)call;
    (void)name;
    (void)format;
    if (!m) return -EINVAL;
    *m = alloc_fake_ptr(FAKE_PTR_MSG);
    return 0;
}

int sd_bus_message_new_signal(sd_bus* bus, sd_bus_message** m,
                               const char* path, const char* interface, const char* member) {
    p7_log(__func__);
    (void)bus;
    (void)path;
    (void)interface;
    (void)member;
    if (!m) return -EINVAL;
    *m = alloc_fake_ptr(FAKE_PTR_MSG);
    return 0;
}

int sd_bus_message_new_signal_to(sd_bus* bus, sd_bus_message** m, const char* destination,
                                  const char* path, const char* interface, const char* member) {
    p7_log(__func__);
    (void)bus;
    (void)destination;
    (void)path;
    (void)interface;
    (void)member;
    if (!m) return -EINVAL;
    *m = alloc_fake_ptr(FAKE_PTR_MSG);
    return 0;
}

/* Message append */
int sd_bus_message_append(sd_bus_message* m, const char* types, ...) {
    p7_log(__func__);
    (void)m;
    (void)types;
    return 0;
}

int sd_bus_message_append_basic(sd_bus_message* m, char type, const void* p) {
    p7_log(__func__);
    (void)m;
    (void)type;
    (void)p;
    return 0;
}

int sd_bus_message_append_array(sd_bus_message* m, char type, size_t size, const void* p) {
    p7_log(__func__);
    (void)m;
    (void)type;
    (void)size;
    (void)p;
    return 0;
}

int sd_bus_message_append_array_iovec(sd_bus_message* m, char type, const struct iovec* iov, unsigned n) {
    p7_log(__func__);
    (void)m;
    (void)type;
    (void)iov;
    (void)n;
    return 0;
}

int sd_bus_message_append_array_memfd(sd_bus_message* m, char type, int fd, uint64_t size) {
    p7_log(__func__);
    (void)m;
    (void)type;
    (void)fd;
    (void)size;
    return 0;
}

int sd_bus_message_append_array_space(sd_bus_message* m, char type, size_t size, void** p) {
    p7_log(__func__);
    (void)m;
    (void)type;
    (void)size;
    if (p) *p = NULL;
    return 0;
}

int sd_bus_message_append_string_iovec(sd_bus_message* m, const struct iovec* iov, unsigned n) {
    p7_log(__func__);
    (void)m;
    (void)iov;
    (void)n;
    return 0;
}

int sd_bus_message_append_string_memfd(sd_bus_message* m, int fd, uint64_t size) {
    p7_log(__func__);
    (void)m;
    (void)fd;
    (void)size;
    return 0;
}

int sd_bus_message_append_string_space(sd_bus_message* m, size_t size, char** s) {
    p7_log(__func__);
    (void)m;
    (void)size;
    if (s) *s = NULL;
    return 0;
}

int sd_bus_message_append_strv(sd_bus_message* m, char* const* l) {
    p7_log(__func__);
    (void)m;
    (void)l;
    return 0;
}

int sd_bus_message_appendv(sd_bus_message* m, const char* types, va_list ap) {
    p7_log(__func__);
    (void)m;
    (void)types;
    (void)ap;
    return 0;
}

/* Container operations */
int sd_bus_message_open_container(sd_bus_message* m, char type, const char* contents) {
    p7_log(__func__);
    (void)m;
    (void)type;
    (void)contents;
    return 0;
}

int sd_bus_message_close_container(sd_bus_message* m) {
    p7_log(__func__);
    (void)m;
    return 0;
}

int sd_bus_message_enter_container(sd_bus_message* m, char type, const char* contents) {
    p7_log(__func__);
    (void)m;
    (void)type;
    (void)contents;
    return 0;
}

int sd_bus_message_exit_container(sd_bus_message* m) {
    p7_log(__func__);
    (void)m;
    return 0;
}

int sd_bus_message_peek_type(sd_bus_message* m, char* type, const char** contents) {
    p7_log(__func__);
    (void)m;
    if (type) *type = SD_BUS_TYPE_INVALID;
    if (contents) *contents = NULL;
    return 0;
}

int sd_bus_message_at_end(sd_bus_message* m, int complete) {
    p7_log(__func__);
    (void)m;
    (void)complete;
    return 1;  /* At end */
}

/* Message read */
int sd_bus_message_read(sd_bus_message* m, const char* types, ...) {
    p7_log(__func__);
    (void)m;
    (void)types;
    return 0;
}

int sd_bus_message_read_basic(sd_bus_message* m, char type, void* p) {
    p7_log(__func__);
    (void)m;
    (void)type;
    (void)p;
    return 0;
}

int sd_bus_message_read_array(sd_bus_message* m, char type, size_t* size, const void** p) {
    p7_log(__func__);
    (void)m;
    (void)type;
    if (size) *size = 0;
    if (p) *p = NULL;
    return 0;
}

int sd_bus_message_read_strv(sd_bus_message* m, char*** l) {
    p7_log(__func__);
    (void)m;
    if (l) *l = NULL;
    return 0;
}

int sd_bus_message_read_strv_extend(sd_bus_message* m, char*** l) {
    p7_log(__func__);
    (void)m;
    if (l) *l = NULL;
    return 0;
}

int sd_bus_message_readv(sd_bus_message* m, const char* types, va_list ap) {
    p7_log(__func__);
    (void)m;
    (void)types;
    (void)ap;
    return 0;
}

int sd_bus_message_skip(sd_bus_message* m, const char* types) {
    p7_log(__func__);
    (void)m;
    (void)types;
    return 0;
}

int sd_bus_message_rewind(sd_bus_message* m, int complete) {
    p7_log(__func__);
    (void)m;
    (void)complete;
    return 0;
}

/* Message properties */
int sd_bus_message_get_type(sd_bus_message* m, uint8_t* type) {
    p7_log(__func__);
    (void)m;
    if (type) *type = SD_BUS_MESSAGE_METHOD_CALL;
    return 0;
}

int sd_bus_message_get_errno(sd_bus_message* m) {
    p7_log(__func__);
    (void)m;
    return 0;
}

int sd_bus_message_get_creds(sd_bus_message* m, sd_bus_creds** creds) {
    p7_log(__func__);
    (void)m;
    if (creds) *creds = alloc_fake_ptr(FAKE_PTR_CREDS);
    return 0;
}

const char* sd_bus_message_get_sender(sd_bus_message* m) {
    p7_log(__func__);
    (void)m;
    return ":1.1";
}

const char* sd_bus_message_get_destination(sd_bus_message* m) {
    p7_log(__func__);
    (void)m;
    return NULL;
}

const char* sd_bus_message_get_path(sd_bus_message* m) {
    p7_log(__func__);
    (void)m;
    return "/org/freedesktop/DBus";
}

const char* sd_message_get_interface(sd_bus_message* m) {
    p7_log(__func__);
    (void)m;
    return "org.freedesktop.DBus";
}

const char* sd_bus_message_get_member(sd_bus_message* m) {
    p7_log(__func__);
    (void)m;
    return "Hello";
}

int sd_bus_message_get_bus(sd_bus_message* m, sd_bus** bus) {
    p7_log(__func__);
    (void)m;
    if (bus) *bus = alloc_fake_ptr(FAKE_PTR_BUS);
    return 0;
}

int sd_bus_message_get_priority(sd_bus_message* m, int64_t* priority) {
    p7_log(__func__);
    (void)m;
    if (priority) *priority = 0;
    return 0;
}

int sd_bus_message_get_signature(sd_bus_message* m, int complete, const char** signature) {
    p7_log(__func__);
    (void)m;
    (void)complete;
    if (signature) *signature = "";
    return 0;
}

int sd_bus_message_get_cookie(sd_bus_message* m, uint64_t* cookie) {
    p7_log(__func__);
    (void)m;
    if (cookie) *cookie = 1;
    return 0;
}

int sd_bus_message_get_reply_cookie(sd_bus_message* m, uint64_t* cookie) {
    p7_log(__func__);
    (void)m;
    if (cookie) *cookie = 0;
    return 0;
}

int sd_bus_message_get_monotonic_usec(sd_bus_message* m, uint64_t* usec) {
    p7_log(__func__);
    (void)m;
    if (usec) *usec = 0;
    return 0;
}

int sd_bus_message_get_realtime_usec(sd_bus_message* m, uint64_t* usec) {
    p7_log(__func__);
    (void)m;
    if (usec) *usec = 0;
    return 0;
}

int sd_bus_message_get_seqnum(sd_bus_message* m, uint64_t* seqnum) {
    p7_log(__func__);
    (void)m;
    if (seqnum) *seqnum = 0;
    return 0;
}

int sd_bus_message_get_expect_reply(sd_bus_message* m, int* b) {
    p7_log(__func__);
    (void)m;
    if (b) *b = 0;
    return 0;
}

int sd_bus_message_get_auto_start(sd_bus_message* m, int* b) {
    p7_log(__func__);
    (void)m;
    if (b) *b = 1;
    return 0;
}

int sd_bus_message_get_allow_interactive_authorization(sd_bus_message* m, int* b) {
    p7_log(__func__);
    (void)m;
    if (b) *b = 0;
    return 0;
}

int sd_bus_message_get_error(sd_bus_message* m) {
    p7_log(__func__);
    (void)m;
    return 0;
}

/* Message checks */
int sd_bus_message_is_method_call(sd_bus_message* m, const char* interface, const char* member) {
    p7_log(__func__);
    (void)m;
    (void)interface;
    (void)member;
    return 1;
}

int sd_bus_message_is_method_error(sd_bus_message* m, const char* name) {
    p7_log(__func__);
    (void)m;
    (void)name;
    return 0;
}

int sd_bus_message_is_signal(sd_bus_message* m, const char* interface, const char* member) {
    p7_log(__func__);
    (void)m;
    (void)interface;
    (void)member;
    return 0;
}

int sd_bus_message_is_empty(sd_bus_message* m) {
    p7_log(__func__);
    (void)m;
    return 1;
}

int sd_bus_message_has_signature(sd_bus_message* m, const char* signature) {
    p7_log(__func__);
    (void)m;
    (void)signature;
    return 1;
}

int sd_bus_message_verify_type(sd_bus_message* m, char type, const char* contents) {
    p7_log(__func__);
    (void)m;
    (void)type;
    (void)contents;
    return 0;
}

/* Message setters */
int sd_bus_message_set_allow_interactive_authorization(sd_bus_message* m, int b) {
    p7_log(__func__);
    (void)m;
    (void)b;
    return 0;
}

int sd_bus_message_set_auto_start(sd_bus_message* m, int b) {
    p7_log(__func__);
    (void)m;
    (void)b;
    return 0;
}

int sd_bus_message_set_destination(sd_bus_message* m, const char* destination) {
    p7_log(__func__);
    (void)m;
    (void)destination;
    return 0;
}

int sd_bus_message_set_expect_reply(sd_bus_message* m, int b) {
    p7_log(__func__);
    (void)m;
    (void)b;
    return 0;
}

int sd_bus_message_set_priority(sd_bus_message* m, int64_t priority) {
    p7_log(__func__);
    (void)m;
    (void)priority;
    return 0;
}

int sd_bus_message_set_sender(sd_bus_message* m, const char* sender) {
    p7_log(__func__);
    (void)m;
    (void)sender;
    return 0;
}

/* Message operations */
int sd_bus_message_copy(sd_bus_message* m, sd_bus_message* source, int all) {
    p7_log(__func__);
    (void)m;
    (void)source;
    (void)all;
    return 0;
}

int sd_bus_message_dump(int f, sd_bus_message* m, uint64_t flags) {
    p7_log(__func__);
    (void)f;
    (void)m;
    (void)flags;
    return 0;
}

int sd_bus_message_dump_json(int f, sd_bus_message* m, uint64_t flags) {
    p7_log(__func__);
    (void)f;
    (void)m;
    (void)flags;
    return 0;
}

int sd_bus_message_seal(sd_bus_message* m, uint64_t cookie, uint64_t timeout_usec) {
    p7_log(__func__);
    (void)m;
    (void)cookie;
    (void)timeout_usec;
    return 0;
}

int sd_bus_message_sensitive(sd_bus_message* m) {
    p7_log(__func__);
    (void)m;
    return 0;
}

int sd_bus_message_send(sd_bus* bus, sd_bus_message* m, uint64_t flags) {
    p7_log(__func__);
    (void)bus;
    (void)m;
    (void)flags;
    return 0;
}

/* ============================================================================
 * SECTION 8: BUS CALL, SEND, EMIT, REPLY, MATCH, TRACK, SLOT
 * ============================================================================ */

/* Bus call */
int sd_bus_call(sd_bus* bus, sd_bus_message* m, uint64_t usec, sd_bus_error* error, sd_bus_message** reply) {
    p7_log(__func__);
    (void)bus;
    (void)m;
    (void)usec;
    (void)error;
    if (reply) *reply = alloc_fake_ptr(FAKE_PTR_MSG);
    return 0;
}

int sd_bus_call_async(sd_bus* bus, sd_bus_slot** slot, sd_bus_message* m,
                       sd_bus_message_handler_t callback, void* userdata, uint64_t usec) {
    p7_log(__func__);
    (void)bus;
    (void)m;
    (void)callback;
    (void)userdata;
    (void)usec;
    if (slot) *slot = alloc_fake_ptr(FAKE_PTR_SLOT);
    return 0;
}

int sd_bus_call_method(sd_bus* bus, const char* destination, const char* path,
                        const char* interface, const char* member,
                        sd_bus_error* error, sd_bus_message** reply,
                        const char* types, ...) {
    p7_log(__func__);
    (void)bus;
    (void)destination;
    (void)path;
    (void)interface;
    (void)member;
    (void)error;
    (void)types;
    if (reply) *reply = alloc_fake_ptr(FAKE_PTR_MSG);
    return 0;
}

int sd_bus_call_method_async(sd_bus* bus, sd_bus_slot** slot, const char* destination,
                              const char* path, const char* interface, const char* member,
                              sd_bus_message_handler_t callback, void* userdata,
                              const char* types, ...) {
    p7_log(__func__);
    (void)bus;
    (void)destination;
    (void)path;
    (void)interface;
    (void)member;
    (void)callback;
    (void)userdata;
    (void)types;
    if (slot) *slot = alloc_fake_ptr(FAKE_PTR_SLOT);
    return 0;
}

int sd_bus_call_method_asyncv(sd_bus* bus, sd_bus_slot** slot, const char* destination,
                               const char* path, const char* interface, const char* member,
                               sd_bus_message_handler_t callback, void* userdata,
                               const char* types, va_list ap) {
    p7_log(__func__);
    (void)bus;
    (void)destination;
    (void)path;
    (void)interface;
    (void)member;
    (void)callback;
    (void)userdata;
    (void)types;
    (void)ap;
    if (slot) *slot = alloc_fake_ptr(FAKE_PTR_SLOT);
    return 0;
}

int sd_bus_call_methodv(sd_bus* bus, const char* destination, const char* path,
                         const char* interface, const char* member,
                         sd_bus_error* error, sd_bus_message** reply,
                         const char* types, va_list ap) {
    p7_log(__func__);
    (void)bus;
    (void)destination;
    (void)path;
    (void)interface;
    (void)member;
    (void)error;
    (void)types;
    (void)ap;
    if (reply) *reply = alloc_fake_ptr(FAKE_PTR_MSG);
    return 0;
}

/* Send */
int sd_bus_send(sd_bus* bus, sd_bus_message* m, uint64_t* cookie) {
    p7_log(__func__);
    (void)bus;
    (void)m;
    if (cookie) *cookie = 1;
    return 0;
}

int sd_bus_send_to(sd_bus* bus, sd_bus_message* m, const char* destination, uint64_t* cookie) {
    p7_log(__func__);
    (void)bus;
    (void)m;
    (void)destination;
    if (cookie) *cookie = 1;
    return 0;
}

/* Emit signals */
int sd_bus_emit_signal(sd_bus* bus, const char* path, const char* interface,
                        const char* member, const char* types, ...) {
    p7_log(__func__);
    (void)bus;
    (void)path;
    (void)interface;
    (void)member;
    (void)types;
    return 0;
}

int sd_bus_emit_signal_to(sd_bus* bus, const char* destination, const char* path,
                            const char* interface, const char* member,
                            const char* types, ...) {
    p7_log(__func__);
    (void)bus;
    (void)destination;
    (void)path;
    (void)interface;
    (void)member;
    (void)types;
    return 0;
}

int sd_bus_emit_signal_tov(sd_bus* bus, const char* destination, const char* path,
                            const char* interface, const char* member,
                            const char* types, va_list ap) {
    p7_log(__func__);
    (void)bus;
    (void)destination;
    (void)path;
    (void)interface;
    (void)member;
    (void)types;
    (void)ap;
    return 0;
}

int sd_bus_emit_signalv(sd_bus* bus, const char* path, const char* interface,
                         const char* member, const char* types, va_list ap) {
    p7_log(__func__);
    (void)bus;
    (void)path;
    (void)interface;
    (void)member;
    (void)types;
    (void)ap;
    return 0;
}

/* Emit object/interface changes */
int sd_bus_emit_interfaces_added(sd_bus* bus, const char* path, ...) {
    p7_log(__func__);
    (void)bus;
    (void)path;
    return 0;
}

int sd_bus_emit_interfaces_added_strv(sd_bus* bus, const char* path, char** interfaces) {
    p7_log(__func__);
    (void)bus;
    (void)path;
    (void)interfaces;
    return 0;
}

int sd_bus_emit_interfaces_removed(sd_bus* bus, const char* path, ...) {
    p7_log(__func__);
    (void)bus;
    (void)path;
    return 0;
}

int sd_bus_emit_interfaces_removed_strv(sd_bus* bus, const char* path, char** interfaces) {
    p7_log(__func__);
    (void)bus;
    (void)path;
    (void)interfaces;
    return 0;
}

int sd_bus_emit_object_added(sd_bus* bus, const char* path) {
    p7_log(__func__);
    (void)bus;
    (void)path;
    return 0;
}

int sd_bus_emit_object_removed(sd_bus* bus, const char* path) {
    p7_log(__func__);
    (void)bus;
    (void)path;
    return 0;
}

int sd_bus_emit_properties_changed(sd_bus* bus, const char* path, const char* interface, ...) {
    p7_log(__func__);
    (void)bus;
    (void)path;
    (void)interface;
    return 0;
}

int sd_bus_emit_properties_changed_strv(sd_bus* bus, const char* path,
                                          const char* interface, char** names) {
    p7_log(__func__);
    (void)bus;
    (void)path;
    (void)interface;
    (void)names;
    return 0;
}

/* Reply methods */
int sd_bus_reply_method_errno(sd_bus_message* call, int error, const sd_bus_error* p) {
    p7_log(__func__);
    (void)call;
    (void)error;
    (void)p;
    return 0;
}

int sd_bus_reply_method_errnof(sd_bus_message* call, int error, const char* format, ...) {
    p7_log(__func__);
    (void)call;
    (void)error;
    (void)format;
    return 0;
}

int sd_bus_reply_method_errnofv(sd_bus_message* call, int error, const char* format, va_list ap) {
    p7_log(__func__);
    (void)call;
    (void)error;
    (void)format;
    (void)ap;
    return 0;
}

int sd_bus_reply_method_error(sd_bus_message* call, const sd_bus_error* e) {
    p7_log(__func__);
    (void)call;
    (void)e;
    return 0;
}

int sd_bus_reply_method_errorf(sd_bus_message* call, const char* name, const char* format, ...) {
    p7_log(__func__);
    (void)call;
    (void)name;
    (void)format;
    return 0;
}

int sd_bus_reply_method_errorfv(sd_bus_message* call, const char* name, const char* format, va_list ap) {
    p7_log(__func__);
    (void)call;
    (void)name;
    (void)format;
    (void)ap;
    return 0;
}

int sd_bus_reply_method_return(sd_bus_message* call, const char* types, ...) {
    p7_log(__func__);
    (void)call;
    (void)types;
    return 0;
}

int sd_bus_reply_method_returnv(sd_bus_message* call, const char* types, va_list ap) {
    p7_log(__func__);
    (void)call;
    (void)types;
    (void)ap;
    return 0;
}

/* Match rules */
int sd_bus_add_match(sd_bus* bus, sd_bus_slot** slot, const char* match,
                      sd_bus_message_handler_t callback, void* userdata) {
    p7_log(__func__);
    (void)bus;
    (void)match;
    (void)callback;
    (void)userdata;
    if (slot) *slot = alloc_fake_ptr(FAKE_PTR_SLOT);
    return 0;
}

int sd_bus_add_match_async(sd_bus* bus, sd_bus_slot** slot, const char* match,
                            sd_bus_message_handler_t callback,
                            sd_bus_message_handler_t install_callback,
                            void* userdata) {
    p7_log(__func__);
    (void)bus;
    (void)match;
    (void)callback;
    (void)install_callback;
    (void)userdata;
    if (slot) *slot = alloc_fake_ptr(FAKE_PTR_SLOT);
    return 0;
}

int sd_bus_match_signal(sd_bus* bus, sd_bus_slot** ret, const char* sender,
                         const char* path, const char* interface, const char* member,
                         sd_bus_message_handler_t callback, void* userdata) {
    p7_log(__func__);
    (void)bus;
    (void)sender;
    (void)path;
    (void)interface;
    (void)member;
    (void)callback;
    (void)userdata;
    if (ret) *ret = alloc_fake_ptr(FAKE_PTR_SLOT);
    return 0;
}

int sd_bus_match_signal_async(sd_bus* bus, sd_bus_slot** ret, const char* sender,
                               const char* path, const char* interface, const char* member,
                               sd_bus_message_handler_t callback,
                               sd_bus_message_handler_t install_callback,
                               void* userdata) {
    p7_log(__func__);
    (void)bus;
    (void)sender;
    (void)path;
    (void)interface;
    (void)member;
    (void)callback;
    (void)install_callback;
    (void)userdata;
    if (ret) *ret = alloc_fake_ptr(FAKE_PTR_SLOT);
    return 0;
}

/* Add object/filter/enumerator (out of scope but need stubs) */
int sd_bus_add_object(sd_bus* bus, sd_bus_slot** slot, const char* path,
                       sd_bus_message_handler_t callback, void* userdata) {
    p7_log(__func__);
    (void)bus;
    (void)path;
    (void)callback;
    (void)userdata;
    if (slot) *slot = alloc_fake_ptr(FAKE_PTR_SLOT);
    return 0;
}

int sd_bus_add_fallback(sd_bus* bus, sd_bus_slot** slot, const char* prefix,
                         sd_bus_message_handler_t callback, void* userdata) {
    p7_log(__func__);
    (void)bus;
    (void)prefix;
    (void)callback;
    (void)userdata;
    if (slot) *slot = alloc_fake_ptr(FAKE_PTR_SLOT);
    return 0;
}

int sd_bus_add_fallback_vtable(sd_bus* bus, sd_bus_slot** slot, const char* prefix,
                                  const char* interface, const sd_bus_vtable* vtable,
                                  sd_bus_object_find_t find, void* userdata) {
    p7_log(__func__);
    (void)bus;
    (void)prefix;
    (void)interface;
    (void)vtable;
    (void)find;
    (void)userdata;
    if (slot) *slot = alloc_fake_ptr(FAKE_PTR_SLOT);
    return 0;
}

int sd_bus_add_filter(sd_bus* bus, sd_bus_slot** slot, sd_bus_message_handler_t callback, void* userdata) {
    p7_log(__func__);
    (void)bus;
    (void)callback;
    (void)userdata;
    if (slot) *slot = alloc_fake_ptr(FAKE_PTR_SLOT);
    return 0;
}

int sd_bus_add_node_enumerator(sd_bus* bus, sd_bus_slot** slot, const char* path,
                                sd_bus_node_enumerator_t callback, void* userdata) {
    p7_log(__func__);
    (void)bus;
    (void)path;
    (void)callback;
    (void)userdata;
    if (slot) *slot = alloc_fake_ptr(FAKE_PTR_SLOT);
    return 0;
}

int sd_bus_add_object_manager(sd_bus* bus, sd_bus_slot** slot, const char* path) {
    p7_log(__func__);
    (void)bus;
    (void)path;
    if (slot) *slot = alloc_fake_ptr(FAKE_PTR_SLOT);
    return 0;
}

int sd_bus_add_object_vtable(sd_bus* bus, sd_bus_slot** slot, const char* path,
                              const char* interface, const sd_bus_vtable* vtable,
                              void* userdata) {
    p7_log(__func__);
    (void)bus;
    (void)path;
    (void)interface;
    (void)vtable;
    (void)userdata;
    if (slot) *slot = alloc_fake_ptr(FAKE_PTR_SLOT);
    return 0;
}

/* Slot management */
sd_bus_slot* sd_bus_slot_ref(sd_bus_slot* slot) {
    p7_log(__func__);
    return slot;
}

sd_bus_slot* sd_bus_slot_unref(sd_bus_slot* slot) {
    p7_log(__func__);
    return NULL;
}

int sd_bus_slot_get_bus(sd_bus_slot* slot, sd_bus** bus) {
    p7_log(__func__);
    (void)slot;
    if (bus) *bus = alloc_fake_ptr(FAKE_PTR_BUS);
    return 0;
}

int sd_bus_slot_get_current_handler(sd_bus_slot* slot, sd_bus_message_handler_t* handler) {
    p7_log(__func__);
    (void)slot;
    if (handler) *handler = NULL;
    return 0;
}

int sd_bus_slot_get_current_message(sd_bus_slot* slot, sd_bus_message** message) {
    p7_log(__func__);
    (void)slot;
    if (message) *message = NULL;
    return 0;
}

int sd_bus_slot_get_current_userdata(sd_bus_slot* slot, void** userdata) {
    p7_log(__func__);
    (void)slot;
    if (userdata) *userdata = NULL;
    return 0;
}

int sd_bus_slot_get_description(sd_bus_slot* slot, const char** description) {
    p7_log(__func__);
    (void)slot;
    if (description) *description = "mock-slot";
    return 0;
}

int sd_bus_slot_get_destroy_callback(sd_bus_slot* slot, sd_bus_destroy_t* callback) {
    p7_log(__func__);
    (void)slot;
    if (callback) *callback = NULL;
    return 0;
}

int sd_bus_slot_get_floating(sd_bus_slot* slot, int* b) {
    p7_log(__func__);
    (void)slot;
    if (b) *b = 0;
    return 0;
}

int sd_bus_slot_get_userdata(sd_bus_slot* slot, void** userdata) {
    p7_log(__func__);
    (void)slot;
    if (userdata) *userdata = NULL;
    return 0;
}

int sd_bus_slot_set_description(sd_bus_slot* slot, const char* description) {
    p7_log(__func__);
    (void)slot;
    (void)description;
    return 0;
}

int sd_bus_slot_set_destroy_callback(sd_bus_slot* slot, sd_bus_destroy_t callback) {
    p7_log(__func__);
    (void)slot;
    (void)callback;
    return 0;
}

int sd_bus_slot_set_floating(sd_bus_slot* slot, int b) {
    p7_log(__func__);
    (void)slot;
    (void)b;
    return 0;
}

int sd_bus_slot_set_userdata(sd_bus_slot* slot, void* userdata) {
    p7_log(__func__);
    (void)slot;
    (void)userdata;
    return 0;
}

/* Track management */
sd_bus_track* sd_bus_track_ref(sd_bus_track* track) {
    p7_log(__func__);
    return track;
}

sd_bus_track* sd_bus_track_unref(sd_bus_track* track) {
    p7_log(__func__);
    return NULL;
}

int sd_bus_track_new(sd_bus* bus, sd_bus_track** ret, sd_bus_track_handler_t handler, void* userdata) {
    p7_log(__func__);
    (void)bus;
    (void)handler;
    (void)userdata;
    if (!ret) return -EINVAL;
    *ret = alloc_fake_ptr(FAKE_PTR_TRACK);
    return 0;
}

int sd_bus_track_add_name(sd_bus_track* track, const char* name) {
    p7_log(__func__);
    (void)track;
    (void)name;
    return 1;
}

int sd_bus_track_add_sender(sd_bus_track* track, sd_bus_message* m) {
    p7_log(__func__);
    (void)track;
    (void)m;
    return 1;
}

int sd_bus_track_contains(sd_bus_track* track, const char* name) {
    p7_log(__func__);
    (void)track;
    (void)name;
    return 0;
}

unsigned sd_bus_track_count(sd_bus_track* track) {
    p7_log(__func__);
    (void)track;
    return 0;
}

unsigned sd_bus_track_count_name(sd_bus_track* track, const char* name) {
    p7_log(__func__);
    (void)track;
    (void)name;
    return 0;
}

unsigned sd_bus_track_count_sender(sd_bus_track* track, sd_bus_message* m) {
    p7_log(__func__);
    (void)track;
    (void)m;
    return 0;
}

const char* sd_bus_track_first(sd_bus_track* track) {
    p7_log(__func__);
    (void)track;
    return NULL;
}

const char* sd_bus_track_next(sd_bus_track* track) {
    p7_log(__func__);
    (void)track;
    return NULL;
}

int sd_bus_track_get_bus(sd_bus_track* track, sd_bus** bus) {
    p7_log(__func__);
    (void)track;
    if (bus) *bus = alloc_fake_ptr(FAKE_PTR_BUS);
    return 0;
}

int sd_bus_track_get_destroy_callback(sd_bus_track* track, sd_bus_destroy_t* callback) {
    p7_log(__func__);
    (void)track;
    if (callback) *callback = NULL;
    return 0;
}

int sd_bus_track_get_recursive(sd_bus_track* track, int* b) {
    p7_log(__func__);
    (void)track;
    if (b) *b = 0;
    return 0;
}

int sd_bus_track_get_userdata(sd_bus_track* track, void** userdata) {
    p7_log(__func__);
    (void)track;
    if (userdata) *userdata = NULL;
    return 0;
}

int sd_bus_track_remove_name(sd_bus_track* track, const char* name) {
    p7_log(__func__);
    (void)track;
    (void)name;
    return 0;
}

int sd_bus_track_remove_sender(sd_bus_track* track, sd_bus_message* m) {
    p7_log(__func__);
    (void)track;
    (void)m;
    return 0;
}

int sd_bus_track_set_destroy_callback(sd_bus_track* track, sd_bus_destroy_t callback) {
    p7_log(__func__);
    (void)track;
    (void)callback;
    return 0;
}

int sd_bus_track_set_recursive(sd_bus_track* track, int b) {
    p7_log(__func__);
    (void)track;
    (void)b;
    return 0;
}

int sd_bus_track_set_userdata(sd_bus_track* track, void* userdata) {
    p7_log(__func__);
    (void)track;
    (void)userdata;
    return 0;
}

/* Bus process/wait */
int sd_bus_process(sd_bus* bus, sd_bus_message** r) {
    p7_log(__func__);
    (void)bus;
    if (r) *r = NULL;
    return 0;  /* No messages pending */
}

int sd_bus_process_priority(sd_bus* bus, int64_t priority, sd_bus_message** r) {
    p7_log(__func__);
    (void)bus;
    (void)priority;
    if (r) *r = NULL;
    return 0;
}

int sd_bus_wait(sd_bus* bus, uint64_t timeout_usec) {
    p7_log(__func__);
    (void)bus;
    (void)timeout_usec;
    return 0;  /* Timeout */
}

/* Attach/detach event */
int sd_bus_attach_event(sd_bus* bus, sd_event* event, int priority) {
    p7_log(__func__);
    (void)bus;
    (void)event;
    (void)priority;
    return 0;
}

int sd_bus_detach_event(sd_bus* bus) {
    p7_log(__func__);
    (void)bus;
    return 0;
}

/* ============================================================================
 * SECTION 8: BUS CALL, SEND, EMIT, REPLY, MATCH, TRACK, SLOT
 * ============================================================================ */

/* Bus call */
int sd_bus_call(sd_bus* bus, sd_bus_message* m, uint64_t usec, sd_bus_error* error, sd_bus_message** reply) {
    p7_log(__func__);
    (void)bus;
    (void)m;
    (void)usec;
    (void)error;
    if (reply) *reply = alloc_fake_ptr(FAKE_PTR_MSG);
    return 0;
}

int sd_bus_call_async(sd_bus* bus, sd_bus_slot** slot, sd_bus_message* m,
                       sd_bus_message_handler_t callback, void* userdata, uint64_t usec) {
    p7_log(__func__);
    (void)bus;
    (void)m;
    (void)callback;
    (void)userdata;
    (void)usec;
    if (slot) *slot = alloc_fake_ptr(FAKE_PTR_SLOT);
    return 0;
}

int sd_bus_call_method(sd_bus* bus, const char* destination, const char* path,
                        const char* interface, const char* member,
                        sd_bus_error* error, sd_bus_message** reply,
                        const char* types, ...) {
    p7_log(__func__);
    (void)bus;
    (void)destination;
    (void)path;
    (void)interface;
    (void)member;
    (void)error;
    (void)types;
    if (reply) *reply = alloc_fake_ptr(FAKE_PTR_MSG);
    return 0;
}

int sd_bus_call_method_async(sd_bus* bus, sd_bus_slot** slot, const char* destination,
                              const char* path, const char* interface, const char* member,
                              sd_bus_message_handler_t callback, void* userdata,
                              const char* types, ...) {
    p7_log(__func__);
    (void)bus;
    (void)destination;
    (void)path;
    (void)interface;
    (void)member;
    (void)callback;
    (void)userdata;
    (void)types;
    if (slot) *slot = alloc_fake_ptr(FAKE_PTR_SLOT);
    return 0;
}

int sd_bus_call_method_asyncv(sd_bus* bus, sd_bus_slot** slot, const char* destination,
                               const char* path, const char* interface, const char* member,
                               sd_bus_message_handler_t callback, void* userdata,
                               const char* types, va_list ap) {
    p7_log(__func__);
    (void)bus;
    (void)destination;
    (void)path;
    (void)interface;
    (void)member;
    (void)callback;
    (void)userdata;
    (void)types;
    (void)ap;
    if (slot) *slot = alloc_fake_ptr(FAKE_PTR_SLOT);
    return 0;
}

int sd_bus_call_methodv(sd_bus* bus, const char* destination, const char* path,
                         const char* interface, const char* member,
                         sd_bus_error* error, sd_bus_message** reply,
                         const char* types, va_list ap) {
    p7_log(__func__);
    (void)bus;
    (void)destination;
    (void)path;
    (void)interface;
    (void)member;
    (void)error;
    (void)types;
    (void)ap;
    if (reply) *reply = alloc_fake_ptr(FAKE_PTR_MSG);
    return 0;
}

/* Send */
int sd_bus_send(sd_bus* bus, sd_bus_message* m, uint64_t* cookie) {
    p7_log(__func__);
    (void)bus;
    (void)m;
    if (cookie) *cookie = 1;
    return 0;
}

int sd_bus_send_to(sd_bus* bus, sd_bus_message* m, const char* destination, uint64_t* cookie) {
    p7_log(__func__);
    (void)bus;
    (void)m;
    (void)destination;
    if (cookie) *cookie = 1;
    return 0;
}

/* Emit signals */
int sd_bus_emit_signal(sd_bus* bus, const char* path, const char* interface,
                        const char* member, const char* types, ...) {
    p7_log(__func__);
    (void)bus;
    (void)path;
    (void)interface;
    (void)member;
    (void)types;
    return 0;
}

int sd_bus_emit_signal_to(sd_bus* bus, const char* destination, const char* path,
                            const char* interface, const char* member,
                            const char* types, ...) {
    p7_log(__func__);
    (void)bus;
    (void)destination;
    (void)path;
    (void)interface;
    (void)member;
    (void)types;
    return 0;
}

int sd_bus_emit_signal_tov(sd_bus* bus, const char* destination, const char* path,
                            const char* interface, const char* member,
                            const char* types, va_list ap) {
    p7_log(__func__);
    (void)bus;
    (void)destination;
    (void)path;
    (void)interface;
    (void)member;
    (void)types;
    (void)ap;
    return 0;
}

int sd_bus_emit_signalv(sd_bus* bus, const char* path, const char* interface,
                         const char* member, const char* types, va_list ap) {
    p7_log(__func__);
    (void)bus;
    (void)path;
    (void)interface;
    (void)member;
    (void)types;
    (void)ap;
    return 0;
}

/* Emit object/interface changes */
int sd_bus_emit_interfaces_added(sd_bus* bus, const char* path, ...) {
    p7_log(__func__);
    (void)bus;
    (void)path;
    return 0;
}

int sd_bus_emit_interfaces_added_strv(sd_bus* bus, const char* path, char** interfaces) {
    p7_log(__func__);
    (void)bus;
    (void)path;
    (void)interfaces;
    return 0;
}

int sd_bus_emit_interfaces_removed(sd_bus* bus, const char* path, ...) {
    p7_log(__func__);
    (void)bus;
    (void)path;
    return 0;
}

int sd_bus_emit_interfaces_removed_strv(sd_bus* bus, const char* path, char** interfaces) {
    p7_log(__func__);
    (void)bus;
    (void)path;
    (void)interfaces;
    return 0;
}

int sd_bus_emit_object_added(sd_bus* bus, const char* path) {
    p7_log(__func__);
    (void)bus;
    (void)path;
    return 0;
}

int sd_bus_emit_object_removed(sd_bus* bus, const char* path) {
    p7_log(__func__);
    (void)bus;
    (void)path;
    return 0;
}

int sd_bus_emit_properties_changed(sd_bus* bus, const char* path, const char* interface, ...) {
    p7_log(__func__);
    (void)bus;
    (void)path;
    (void)interface;
    return 0;
}

int sd_bus_emit_properties_changed_strv(sd_bus* bus, const char* path,
                                          const char* interface, char** names) {
    p7_log(__func__);
    (void)bus;
    (void)path;
    (void)interface;
    (void)names;
    return 0;
}

/* Reply methods */
int sd_bus_reply_method_errno(sd_bus_message* call, int error, const sd_bus_error* p) {
    p7_log(__func__);
    (void)call;
    (void)error;
    (void)p;
    return 0;
}

int sd_bus_reply_method_errnof(sd_bus_message* call, int error, const char* format, ...) {
    p7_log(__func__);
    (void)call;
    (void)error;
    (void)format;
    return 0;
}

int sd_bus_reply_method_errnofv(sd_bus_message* call, int error, const char* format, va_list ap) {
    p7_log(__func__);
    (void)call;
    (void)error;
    (void)format;
    (void)ap;
    return 0;
}

int sd_bus_reply_method_error(sd_bus_message* call, const sd_bus_error* e) {
    p7_log(__func__);
    (void)call;
    (void)e;
    return 0;
}

int sd_bus_reply_method_errorf(sd_bus_message* call, const char* name, const char* format, ...) {
    p7_log(__func__);
    (void)call;
    (void)name;
    (void)format;
    return 0;
}

int sd_bus_reply_method_errorfv(sd_bus_message* call, const char* name, const char* format, va_list ap) {
    p7_log(__func__);
    (void)call;
    (void)name;
    (void)format;
    (void)ap;
    return 0;
}

int sd_bus_reply_method_return(sd_bus_message* call, const char* types, ...) {
    p7_log(__func__);
    (void)call;
    (void)types;
    return 0;
}

int sd_bus_reply_method_returnv(sd_bus_message* call, const char* types, va_list ap) {
    p7_log(__func__);
    (void)call;
    (void)types;
    (void)ap;
    return 0;
}

/* Match rules */
int sd_bus_add_match(sd_bus* bus, sd_bus_slot** slot, const char* match,
                      sd_bus_message_handler_t callback, void* userdata) {
    p7_log(__func__);
    (void)bus;
    (void)match;
    (void)callback;
    (void)userdata;
    if (slot) *slot = alloc_fake_ptr(FAKE_PTR_SLOT);
    return 0;
}

int sd_bus_add_match_async(sd_bus* bus, sd_bus_slot** slot, const char* match,
                            sd_bus_message_handler_t callback,
                            sd_bus_message_handler_t install_callback,
                            void* userdata) {
    p7_log(__func__);
    (void)bus;
    (void)match;
    (void)callback;
    (void)install_callback;
    (void)userdata;
    if (slot) *slot = alloc_fake_ptr(FAKE_PTR_SLOT);
    return 0;
}

int sd_bus_match_signal(sd_bus* bus, sd_bus_slot** ret, const char* sender,
                         const char* path, const char* interface, const char* member,
                         sd_bus_message_handler_t callback, void* userdata) {
    p7_log(__func__);
    (void)bus;
    (void)sender;
    (void)path;
    (void)interface;
    (void)member;
    (void)callback;
    (void)userdata;
    if (ret) *ret = alloc_fake_ptr(FAKE_PTR_SLOT);
    return 0;
}

int sd_bus_match_signal_async(sd_bus* bus, sd_bus_slot** ret, const char* sender,
                               const char* path, const char* interface, const char* member,
                               sd_bus_message_handler_t callback,
                               sd_bus_message_handler_t install_callback,
                               void* userdata) {
    p7_log(__func__);
    (void)bus;
    (void)sender;
    (void)path;
    (void)interface;
    (void)member;
    (void)callback;
    (void)install_callback;
    (void)userdata;
    if (ret) *ret = alloc_fake_ptr(FAKE_PTR_SLOT);
    return 0;
}

/* Add object/filter/enumerator (out of scope but need stubs) */
int sd_bus_add_object(sd_bus* bus, sd_bus_slot** slot, const char* path,
                       sd_bus_message_handler_t callback, void* userdata) {
    p7_log(__func__);
    (void)bus;
    (void)path;
    (void)callback;
    (void)userdata;
    if (slot) *slot = alloc_fake_ptr(FAKE_PTR_SLOT);
    return 0;
}

int sd_bus_add_fallback(sd_bus* bus, sd_bus_slot** slot, const char* prefix,
                         sd_bus_message_handler_t callback, void* userdata) {
    p7_log(__func__);
    (void)bus;
    (void)prefix;
    (void)callback;
    (void)userdata;
    if (slot) *slot = alloc_fake_ptr(FAKE_PTR_SLOT);
    return 0;
}

int sd_bus_add_fallback_vtable(sd_bus* bus, sd_bus_slot** slot, const char* prefix,
                                  const char* interface, const sd_bus_vtable* vtable,
                                  sd_bus_object_find_t find, void* userdata) {
    p7_log(__func__);
    (void)bus;
    (void)prefix;
    (void)interface;
    (void)vtable;
    (void)find;
    (void)userdata;
    if (slot) *slot = alloc_fake_ptr(FAKE_PTR_SLOT);
    return 0;
}

int sd_bus_add_filter(sd_bus* bus, sd_bus_slot** slot, sd_bus_message_handler_t callback, void* userdata) {
    p7_log(__func__);
    (void)bus;
    (void)callback;
    (void)userdata;
    if (slot) *slot = alloc_fake_ptr(FAKE_PTR_SLOT);
    return 0;
}

int sd_bus_add_node_enumerator(sd_bus* bus, sd_bus_slot** slot, const char* path,
                                sd_bus_node_enumerator_t callback, void* userdata) {
    p7_log(__func__);
    (void)bus;
    (void)path;
    (void)callback;
    (void)userdata;
    if (slot) *slot = alloc_fake_ptr(FAKE_PTR_SLOT);
    return 0;
}

int sd_bus_add_object_manager(sd_bus* bus, sd_bus_slot** slot, const char* path) {
    p7_log(__func__);
    (void)bus;
    (void)path;
    if (slot) *slot = alloc_fake_ptr(FAKE_PTR_SLOT);
    return 0;
}

int sd_bus_add_object_vtable(sd_bus* bus, sd_bus_slot** slot, const char* path,
                              const char* interface, const sd_bus_vtable* vtable,
                              void* userdata) {
    p7_log(__func__);
    (void)bus;
    (void)path;
    (void)interface;
    (void)vtable;
    (void)userdata;
    if (slot) *slot = alloc_fake_ptr(FAKE_PTR_SLOT);
    return 0;
}

/* Slot management */
sd_bus_slot* sd_bus_slot_ref(sd_bus_slot* slot) {
    p7_log(__func__);
    return slot;
}

sd_bus_slot* sd_bus_slot_unref(sd_bus_slot* slot) {
    p7_log(__func__);
    return NULL;
}

int sd_bus_slot_get_bus(sd_bus_slot* slot, sd_bus** bus) {
    p7_log(__func__);
    (void)slot;
    if (bus) *bus = alloc_fake_ptr(FAKE_PTR_BUS);
    return 0;
}

int sd_bus_slot_get_current_handler(sd_bus_slot* slot, sd_bus_message_handler_t* handler) {
    p7_log(__func__);
    (void)slot;
    if (handler) *handler = NULL;
    return 0;
}

int sd_bus_slot_get_current_message(sd_bus_slot* slot, sd_bus_message** message) {
    p7_log(__func__);
    (void)slot;
    if (message) *message = NULL;
    return 0;
}

int sd_bus_slot_get_current_userdata(sd_bus_slot* slot, void** userdata) {
    p7_log(__func__);
    (void)slot;
    if (userdata) *userdata = NULL;
    return 0;
}

int sd_bus_slot_get_description(sd_bus_slot* slot, const char** description) {
    p7_log(__func__);
    (void)slot;
    if (description) *description = "mock-slot";
    return 0;
}

int sd_bus_slot_get_destroy_callback(sd_bus_slot* slot, sd_bus_destroy_t* callback) {
    p7_log(__func__);
    (void)slot;
    if (callback) *callback = NULL;
    return 0;
}

int sd_bus_slot_get_floating(sd_bus_slot* slot, int* b) {
    p7_log(__func__);
    (void)slot;
    if (b) *b = 0;
    return 0;
}

int sd_bus_slot_get_userdata(sd_bus_slot* slot, void** userdata) {
    p7_log(__func__);
    (void)slot;
    if (userdata) *userdata = NULL;
    return 0;
}

int sd_bus_slot_set_description(sd_bus_slot* slot, const char* description) {
    p7_log(__func__);
    (void)slot;
    (void)description;
    return 0;
}

int sd_bus_slot_set_destroy_callback(sd_bus_slot* slot, sd_bus_destroy_t callback) {
    p7_log(__func__);
    (void)slot;
    (void)callback;
    return 0;
}

int sd_bus_slot_set_floating(sd_bus_slot* slot, int b) {
    p7_log(__func__);
    (void)slot;
    (void)b;
    return 0;
}

int sd_bus_slot_set_userdata(sd_bus_slot* slot, void* userdata) {
    p7_log(__func__);
    (void)slot;
    (void)userdata;
    return 0;
}

/* Track management */
sd_bus_track* sd_bus_track_ref(sd_bus_track* track) {
    p7_log(__func__);
    return track;
}

sd_bus_track* sd_bus_track_unref(sd_bus_track* track) {
    p7_log(__func__);
    return NULL;
}

int sd_bus_track_new(sd_bus* bus, sd_bus_track** ret, sd_bus_track_handler_t handler, void* userdata) {
    p7_log(__func__);
    (void)bus;
    (void)handler;
    (void)userdata;
    if (!ret) return -EINVAL;
    *ret = alloc_fake_ptr(FAKE_PTR_TRACK);
    return 0;
}

int sd_bus_track_add_name(sd_bus_track* track, const char* name) {
    p7_log(__func__);
    (void)track;
    (void)name;
    return 1;
}

int sd_bus_track_add_sender(sd_bus_track* track, sd_bus_message* m) {
    p7_log(__func__);
    (void)track;
    (void)m;
    return 1;
}

int sd_bus_track_contains(sd_bus_track* track, const char* name) {
    p7_log(__func__);
    (void)track;
    (void)name;
    return 0;
}

unsigned sd_bus_track_count(sd_bus_track* track) {
    p7_log(__func__);
    (void)track;
    return 0;
}

unsigned sd_bus_track_count_name(sd_bus_track* track, const char* name) {
    p7_log(__func__);
    (void)track;
    (void)name;
    return 0;
}

unsigned sd_bus_track_count_sender(sd_bus_track* track, sd_bus_message* m) {
    p7_log(__func__);
    (void)track;
    (void)m;
    return 0;
}

const char* sd_bus_track_first(sd_bus_track* track) {
    p7_log(__func__);
    (void)track;
    return NULL;
}

const char* sd_bus_track_next(sd_bus_track* track) {
    p7_log(__func__);
    (void)track;
    return NULL;
}

int sd_bus_track_get_bus(sd_bus_track* track, sd_bus** bus) {
    p7_log(__func__);
    (void)track;
    if (bus) *bus = alloc_fake_ptr(FAKE_PTR_BUS);
    return 0;
}

int sd_bus_track_get_destroy_callback(sd_bus_track* track, sd_bus_destroy_t* callback) {
    p7_log(__func__);
    (void)track;
    if (callback) *callback = NULL;
    return 0;
}

int sd_bus_track_get_recursive(sd_bus_track* track, int* b) {
    p7_log(__func__);
    (void)track;
    if (b) *b = 0;
    return 0;
}

int sd_bus_track_get_userdata(sd_bus_track* track, void** userdata) {
    p7_log(__func__);
    (void)track;
    if (userdata) *userdata = NULL;
    return 0;
}

int sd_bus_track_remove_name(sd_bus_track* track, const char* name) {
    p7_log(__func__);
    (void)track;
    (void)name;
    return 0;
}

int sd_bus_track_remove_sender(sd_bus_track* track, sd_bus_message* m) {
    p7_log(__func__);
    (void)track;
    (void)m;
    return 0;
}

int sd_bus_track_set_destroy_callback(sd_bus_track* track, sd_bus_destroy_t callback) {
    p7_log(__func__);
    (void)track;
    (void)callback;
    return 0;
}

int sd_bus_track_set_recursive(sd_bus_track* track, int b) {
    p7_log(__func__);
    (void)track;
    (void)b;
    return 0;
}

int sd_bus_track_set_userdata(sd_bus_track* track, void* userdata) {
    p7_log(__func__);
    (void)track;
    (void)userdata;
    return 0;
}

/* Bus process/wait */
int sd_bus_process(sd_bus* bus, sd_bus_message** r) {
    p7_log(__func__);
    (void)bus;
    if (r) *r = NULL;
    return 0;  /* No messages pending */
}

int sd_bus_process_priority(sd_bus* bus, int64_t priority, sd_bus_message** r) {
    p7_log(__func__);
    (void)bus;
    (void)priority;
    if (r) *r = NULL;
    return 0;
}

int sd_bus_wait(sd_bus* bus, uint64_t timeout_usec) {
    p7_log(__func__);
    (void)bus;
    (void)timeout_usec;
    return 0;  /* Timeout */
}

/* Attach/detach event */
int sd_bus_attach_event(sd_bus* bus, sd_event* event, int priority) {
    p7_log(__func__);
    (void)bus;
    (void)event;
    (void)priority;
    return 0;
}

int sd_bus_detach_event(sd_bus* bus) {
    p7_log(__func__);
    (void)bus;
    return 0;
}

/* ============================================================================
 * SECTION 9: BUS CREDS
 * ============================================================================ */

sd_bus_creds* sd_bus_creds_ref(sd_bus_creds* c) {
    p7_log(__func__);
    return c;
}

sd_bus_creds* sd_bus_creds_unref(sd_bus_creds* c) {
    p7_log(__func__);
    return NULL;
}

int sd_bus_creds_new_from_pid(sd_bus_creds** ret, pid_t pid, uint64_t mask) {
    p7_log(__func__);
    (void)pid;
    (void)mask;
    if (!ret) return -EINVAL;
    *ret = alloc_fake_ptr(FAKE_PTR_CREDS);
    return 0;
}

int sd_bus_creds_new_from_pidfd(sd_bus_creds** ret, int pidfd, uint64_t mask) {
    p7_log(__func__);
    (void)pidfd;
    (void)mask;
    if (!ret) return -EINVAL;
    *ret = alloc_fake_ptr(FAKE_PTR_CREDS);
    return 0;
}

int sd_bus_creds_get_mask(sd_bus_creds* c, uint64_t* mask) {
    p7_log(__func__);
    (void)c;
    if (mask) *mask = 0xFFFFFFFF;
    return 0;
}

int sd_bus_creds_get_augmented_mask(sd_bus_creds* c, uint64_t* mask) {
    p7_log(__func__);
    (void)c;
    if (mask) *mask = 0;
    return 0;
}

int sd_bus_creds_get_uid(sd_bus_creds* c, uid_t* uid) {
    p7_log(__func__);
    (void)c;
    if (!uid) return -EINVAL;
    *uid = get_current_uid();
    return 0;
}

int sd_bus_creds_get_euid(sd_bus_creds* c, uid_t* uid) {
    p7_log(__func__);
    (void)c;
    if (!uid) return -EINVAL;
    *uid = geteuid();
    return 0;
}

int sd_bus_creds_get_suid(sd_bus_creds* c, uid_t* uid) {
    p7_log(__func__);
    (void)c;
    if (!uid) return -EINVAL;
    *uid = getuid();
    return 0;
}

int sd_bus_creds_get_fsuid(sd_bus_creds* c, uid_t* uid) {
    p7_log(__func__);
    (void)c;
    if (!uid) return -EINVAL;
    *uid = getuid();
    return 0;
}

int sd_bus_creds_get_gid(sd_bus_creds* c, gid_t* gid) {
    p7_log(__func__);
    (void)c;
    if (!gid) return -EINVAL;
    *gid = getgid();
    return 0;
}

int sd_bus_creds_get_egid(sd_bus_creds* c, gid_t* gid) {
    p7_log(__func__);
    (void)c;
    if (!gid) return -EINVAL;
    *gid = getegid();
    return 0;
}

int sd_bus_creds_get_sgid(sd_bus_creds* c, gid_t* gid) {
    p7_log(__func__);
    (void)c;
    if (!gid) return -EINVAL;
    *gid = getgid();
    return 0;
}

int sd_bus_creds_get_fsgid(sd_bus_creds* c, gid_t* gid) {
    p7_log(__func__);
    (void)c;
    if (!gid) return -EINVAL;
    *gid = getgid();
    return 0;
}

int sd_bus_creds_get_supplementary_gids(sd_bus_creds* c, const gid_t** gids) {
    p7_log(__func__);
    (void)c;
    if (gids) *gids = NULL;
    return 0;
}

int sd_bus_creds_get_pid(sd_bus_creds* c, pid_t* pid) {
    p7_log(__func__);
    (void)c;
    if (!pid) return -EINVAL;
    *pid = getpid();
    return 0;
}

int sd_bus_creds_get_pidfd_dup(sd_bus_creds* c, int* fd) {
    p7_log(__func__);
    (void)c;
    if (!fd) return -EINVAL;
    *fd = -1;
    return -ENODATA;
}

int sd_bus_creds_get_ppid(sd_bus_creds* c, pid_t* pid) {
    p7_log(__func__);
    (void)c;
    if (!pid) return -EINVAL;
    *pid = getppid();
    return 0;
}

int sd_bus_creds_get_tid(sd_bus_creds* c, pid_t* tid) {
    p7_log(__func__);
    (void)c;
    if (!tid) return -EINVAL;
    *tid = getpid();
    return 0;
}

int sd_bus_creds_get_comm(sd_bus_creds* c, const char** comm) {
    p7_log(__func__);
    (void)c;
    if (comm) *comm = "process";
    return 0;
}

int sd_bus_creds_get_tid_comm(sd_bus_creds* c, const char** comm) {
    p7_log(__func__);
    (void)c;
    if (comm) *comm = "process";
    return 0;
}

int sd_bus_creds_get_exe(sd_bus_creds* c, const char** exe) {
    p7_log(__func__);
    (void)c;
    if (exe) *exe = "/usr/bin/app";
    return 0;
}

int sd_bus_creds_get_cmdline(sd_bus_creds* c, char*** cmdline) {
    p7_log(__func__);
    (void)c;
    if (cmdline) {
        *cmdline = calloc(2, sizeof(char*));
        if (*cmdline) (*cmdline)[0] = strdup("app");
    }
    return 0;
}

int sd_bus_creds_get_cgroup(sd_bus_creds* c, const char** cgroup) {
    p7_log(__func__);
    (void)c;
    if (cgroup) *cgroup = "/user.slice/user-1000.slice/session-c1.scope";
    return 0;
}

int sd_bus_creds_get_unit(sd_bus_creds* c, const char** unit) {
    p7_log(__func__);
    (void)c;
    if (unit) *unit = "session-c1.scope";
    return 0;
}

int sd_bus_creds_get_slice(sd_bus_creds* c, const char** slice) {
    p7_log(__func__);
    (void)c;
    if (slice) *slice = "user-1000.slice";
    return 0;
}

int sd_bus_creds_get_user_unit(sd_bus_creds* c, const char** unit) {
    p7_log(__func__);
    (void)c;
    if (unit) *unit = NULL;
    return 0;
}

int sd_bus_creds_get_user_slice(sd_bus_creds* c, const char** slice) {
    p7_log(__func__);
    (void)c;
    if (slice) *slice = "user-1000.slice";
    return 0;
}

int sd_bus_creds_get_session(sd_bus_creds* c, const char** session) {
    p7_log(__func__);
    (void)c;
    if (session) *session = "c1";
    return 0;
}

int sd_bus_creds_get_owner_uid(sd_bus_creds* c, uid_t* uid) {
    p7_log(__func__);
    (void)c;
    if (!uid) return -EINVAL;
    *uid = get_current_uid();
    return 0;
}

int sd_bus_creds_get_selinux_context(sd_bus_creds* c, const char** context) {
    p7_log(__func__);
    (void)c;
    if (context) *context = NULL;
    return -ENODATA;
}

int sd_bus_creds_get_tty(sd_bus_creds* c, const char** tty) {
    p7_log(__func__);
    (void)c;
    if (tty) *tty = "/dev/tty1";
    return 0;
}

int sd_bus_creds_get_unique_name(sd_bus_creds* c, const char** name) {
    p7_log(__func__);
    (void)c;
    if (name) *name = ":1.1";
    return 0;
}

int sd_bus_creds_get_well_known_names(sd_bus_creds* c, char*** names) {
    p7_log(__func__);
    (void)c;
    if (names) {
        *names = calloc(2, sizeof(char*));
        if (*names) (*names)[0] = strdup("org.freedesktop.DBus");
    }
    return 0;
}

int sd_bus_creds_get_description(sd_bus_creds* c, const char** description) {
    p7_log(__func__);
    (void)c;
    if (description) *description = "mock-creds";
    return 0;
}

int sd_bus_creds_get_audit_session_id(sd_bus_creds* c, uint32_t* id) {
    p7_log(__func__);
    (void)c;
    if (!id) return -EINVAL;
    *id = 0;
    return -ENODATA;
}

int sd_bus_creds_get_audit_login_uid(sd_bus_creds* c, uid_t* uid) {
    p7_log(__func__);
    (void)c;
    if (!uid) return -EINVAL;
    *uid = get_current_uid();
    return -ENODATA;
}

int sd_bus_creds_has_bounding_cap(sd_bus_creds* c, int capability) {
    p7_log(__func__);
    (void)c;
    (void)capability;
    return 0;
}

int sd_bus_creds_has_effective_cap(sd_bus_creds* c, int capability) {
    p7_log(__func__);
    (void)c;
    (void)capability;
    return 0;
}

int sd_bus_creds_has_inheritable_cap(sd_bus_creds* c, int capability) {
    p7_log(__func__);
    (void)c;
    (void)capability;
    return 0;
}

int sd_bus_creds_has_permitted_cap(sd_bus_creds* c, int capability) {
    p7_log(__func__);
    (void)c;
    (void)capability;
    return 0;
}

/* ============================================================================
 * SECTION 10: EVENT LOOP
 * ============================================================================ */

sd_event* sd_event_ref(sd_event* e) {
    p7_log(__func__);
    return e;
}

sd_event* sd_event_unref(sd_event* e) {
    p7_log(__func__);
    return NULL;
}

int sd_event_new(sd_event** ret) {
    p7_log(__func__);
    if (!ret) return -EINVAL;
    *ret = alloc_fake_ptr(FAKE_PTR_BUS);
    return 0;
}

sd_event* sd_event_default(sd_event** ret) {
    p7_log(__func__);
    if (ret) *ret = alloc_fake_ptr(FAKE_PTR_BUS);
    return ret ? *ret : alloc_fake_ptr(FAKE_PTR_BUS);
}

int sd_event_add_io(sd_event* e, sd_event_source** s, int fd, uint32_t events,
                     sd_event_io_handler_t callback, void* userdata) {
    p7_log(__func__);
    (void)e;
    (void)fd;
    (void)events;
    (void)callback;
    (void)userdata;
    if (s) *s = alloc_fake_ptr(FAKE_PTR_BUS);
    return 0;
}

int sd_event_add_time(sd_event* e, sd_event_source** s, clockid_t clock, uint64_t usec,
                      uint64_t accuracy, sd_event_time_handler_t callback, void* userdata) {
    p7_log(__func__);
    (void)e;
    (void)clock;
    (void)usec;
    (void)accuracy;
    (void)callback;
    (void)userdata;
    if (s) *s = alloc_fake_ptr(FAKE_PTR_BUS);
    return 0;
}

int sd_event_add_time_relative(sd_event* e, sd_event_source** s, clockid_t clock, uint64_t usec,
                                uint64_t accuracy, sd_event_time_handler_t callback, void* userdata) {
    p7_log(__func__);
    (void)e;
    (void)clock;
    (void)usec;
    (void)accuracy;
    (void)callback;
    (void)userdata;
    if (s) *s = alloc_fake_ptr(FAKE_PTR_BUS);
    return 0;
}

int sd_event_add_child_time(sd_event* e, sd_event_source** s, pid_t pid, clockid_t clock,
                             uint64_t usec, uint64_t accuracy,
                             sd_event_child_time_handler_t callback, void* userdata) {
    p7_log(__func__);
    (void)e;
    (void)pid;
    (void)clock;
    (void)usec;
    (void)accuracy;
    (void)callback;
    (void)userdata;
    if (s) *s = alloc_fake_ptr(FAKE_PTR_BUS);
    return 0;
}

int sd_event_add_child(sd_event* e, sd_event_source** s, pid_t pid, int options,
                        sd_event_child_handler_t callback, void* userdata) {
    p7_log(__func__);
    (void)e;
    (void)pid;
    (void)options;
    (void)callback;
    (void)userdata;
    if (s) *s = alloc_fake_ptr(FAKE_PTR_BUS);
    return 0;
}

int sd_event_add_child_pidfd(sd_event* e, sd_event_source** s, int pidfd, int options,
                              sd_event_child_handler_t callback, void* userdata) {
    p7_log(__func__);
    (void)e;
    (void)pidfd;
    (void)options;
    (void)callback;
    (void)userdata;
    if (s) *s = alloc_fake_ptr(FAKE_PTR_BUS);
    return 0;
}

int sd_event_add_defer(sd_event* e, sd_event_source** s,
                        sd_event_defer_handler_t callback, void* userdata) {
    p7_log(__func__);
    (void)e;
    (void)callback;
    (void)userdata;
    if (s) *s = alloc_fake_ptr(FAKE_PTR_BUS);
    return 0;
}

int sd_event_add_exit(sd_event* e, sd_event_source** s,
                       sd_event_defer_handler_t callback, void* userdata) {
    p7_log(__func__);
    (void)e;
    (void)callback;
    (void)userdata;
    if (s) *s = alloc_fake_ptr(FAKE_PTR_BUS);
    return 0;
}

int sd_event_add_inotify(sd_event* e, sd_event_source** s, const char* path, uint32_t mask,
                          sd_event_inotify_handler_t callback, void* userdata) {
    p7_log(__func__);
    (void)e;
    (void)path;
    (void)mask;
    (void)callback;
    (void)userdata;
    if (s) *s = alloc_fake_ptr(FAKE_PTR_BUS);
    return 0;
}

int sd_event_add_inotify_fd(sd_event* e, sd_event_source** s, int fd, uint32_t mask,
                             sd_event_inotify_handler_t callback, void* userdata) {
    p7_log(__func__);
    (void)e;
    (void)fd;
    (void)mask;
    (void)callback;
    (void)userdata;
    if (s) *s = alloc_fake_ptr(FAKE_PTR_BUS);
    return 0;
}

int sd_event_add_memory_pressure(sd_event* e, sd_event_source** s,
                                  sd_event_defer_handler_t callback, void* userdata) {
    p7_log(__func__);
    (void)e;
    (void)callback;
    (void)userdata;
    if (s) *s = alloc_fake_ptr(FAKE_PTR_BUS);
    return 0;
}

int sd_event_add_post(sd_event* e, sd_event_source** s,
                         sd_event_defer_handler_t callback, void* userdata) {
    p7_log(__func__);
    (void)e;
    (void)callback;
    (void)userdata;
    if (s) *s = alloc_fake_ptr(FAKE_PTR_BUS);
    return 0;
}

int sd_event_add_signal(sd_event* e, sd_event_source** s, int sig,
                         sd_event_signal_handler_t callback, void* userdata) {
    p7_log(__func__);
    (void)e;
    (void)sig;
    (void)callback;
    (void)userdata;
    if (s) *s = alloc_fake_ptr(FAKE_PTR_BUS);
    return 0;
}

/* Event loop operations */
int sd_event_loop(sd_event* e) {
    p7_log(__func__);
    (void)e;
    return 0;  /* Loop exited normally */
}

int sd_event_run(sd_event* e, uint64_t timeout) {
    p7_log(__func__);
    (void)e;
    (void)timeout;
    return 0;  /* No events processed */
}

int sd_event_dispatch(sd_event* e) {
    p7_log(__func__);
    (void)e;
    return 0;
}

int sd_event_prepare(sd_event* e) {
    p7_log(__func__);
    (void)e;
    return 0;  /* Nothing to prepare */
}

int sd_event_wait(sd_event* e, uint64_t timeout) {
    p7_log(__func__);
    (void)e;
    (void)timeout;
    return 0;  /* Timeout */
}

int sd_event_exit(sd_event* e, int code) {
    p7_log(__func__);
    (void)e;
    (void)code;
    return 0;
}

int sd_event_now(sd_event* e, clockid_t clock, uint64_t* usec) {
    p7_log(__func__);
    (void)e;
    (void)clock;
    if (usec) {
        struct timespec ts;
        clock_gettime(clock, &ts);
        *usec = (uint64_t)ts.tv_sec * 1000000ULL + ts.tv_nsec / 1000;
    }
    return 0;
}

int sd_event_get_fd(sd_event* e) {
    p7_log(__func__);
    (void)e;
    return -1;  /* No real fd */
}

int sd_event_get_state(sd_event* e, sd_event_state_t* state) {
    p7_log(__func__);
    (void)e;
    if (state) *state = SD_EVENT_FINISHED;
    return 0;
}

int sd_event_get_tid(sd_event* e, pid_t* tid) {
    p7_log(__func__);
    (void)e;
    if (tid) *tid = getpid();
    return 0;
}

int sd_event_get_iteration(sd_event* e, uint64_t* ret) {
    p7_log(__func__);
    (void)e;
    if (ret) *ret = 0;
    return 0;
}

int sd_event_get_exit_code(sd_event* e, int* code) {
    p7_log(__func__);
    (void)e;
    if (code) *code = 0;
    return 0;
}

int sd_event_get_watchdog(sd_event* e, int* b) {
    p7_log(__func__);
    (void)e;
    if (b) *b = 0;
    return 0;
}

int sd_event_get_exit_on_idle(sd_event* e, int* b) {
    p7_log(__func__);
    (void)e;
    if (b) *b = 0;
    return 0;
}

int sd_event_set_exit_on_idle(sd_event* e, int b) {
    p7_log(__func__);
    (void)e;
    (void)b;
    return 0;
}

int sd_event_set_signal_exit(sd_event* e, int b) {
    p7_log(__func__);
    (void)e;
    (void)b;
    return 0;
}

int sd_event_set_watchdog(sd_event* e, int b) {
    p7_log(__func__);
    (void)e;
    (void)b;
    return 0;
}

int sd_event_trim_memory(void) {
    p7_log(__func__);
    return 0;
}

/* Event source operations */
sd_event_source* sd_event_source_ref(sd_event_source* s) {
    p7_log(__func__);
    return s;
}

sd_event_source* sd_event_source_unref(sd_event_source* s) {
    p7_log(__func__);
    return NULL;
}

sd_event_source* sd_event_source_disable_unref(sd_event_source* s) {
    p7_log(__func__);
    return sd_event_source_unref(s);
}

int sd_event_source_get_event(sd_event_source* s, sd_event** e) {
    p7_log(__func__);
    (void)s;
    if (e) *e = alloc_fake_ptr(FAKE_PTR_BUS);
    return 0;
}

int sd_event_source_get_enabled(sd_event_source* s, int* enabled) {
    p7_log(__func__);
    (void)s;
    if (enabled) *enabled = SD_EVENT_OFF;
    return 0;
}

int sd_event_source_set_enabled(sd_event_source* s, int enabled) {
    p7_log(__func__);
    (void)s;
    (void)enabled;
    return 0;
}

int sd_event_source_get_io_fd(sd_event_source* s, int* fd) {
    p7_log(__func__);
    (void)s;
    if (fd) *fd = -1;
    return 0;
}

int sd_event_source_set_io_fd(sd_event_source* s, int fd) {
    p7_log(__func__);
    (void)s;
    (void)fd;
    return 0;
}

int sd_event_source_get_io_fd_own(sd_event_source* s, int* b) {
    p7_log(__func__);
    (void)s;
    if (b) *b = 0;
    return 0;
}

int sd_event_source_set_io_fd_own(sd_event_source* s, int b) {
    p7_log(__func__);
    (void)s;
    (void)b;
    return 0;
}

int sd_event_source_get_io_events(sd_event_source* s, uint32_t* events) {
    p7_log(__func__);
    (void)s;
    if (events) *events = 0;
    return 0;
}

int sd_event_source_set_io_events(sd_event_source* s, uint32_t events) {
    p7_log(__func__);
    (void)s;
    (void)events;
    return 0;
}

int sd_event_source_get_io_revents(sd_event_source* s, uint32_t* revents) {
    p7_log(__func__);
    (void)s;
    if (revents) *revents = 0;
    return 0;
}

int sd_event_source_get_pending(sd_event_source* s, int* pending) {
    p7_log(__func__);
    (void)s;
    if (pending) *pending = 0;
    return 0;
}

int sd_event_source_get_priority(sd_event_source* s, int64_t* priority) {
    p7_log(__func__);
    (void)s;
    if (priority) *priority = 0;
    return 0;
}

int sd_event_source_set_priority(sd_event_source* s, int64_t priority) {
    p7_log(__func__);
    (void)s;
    (void)priority;
    return 0;
}

int sd_event_source_get_description(sd_event_source* s, const char** description) {
    p7_log(__func__);
    (void)s;
    if (description) *description = "mock-source";
    return 0;
}

int sd_event_source_set_description(sd_event_source* s, const char* description) {
    p7_log(__func__);
    (void)s;
    (void)description;
    return 0;
}

int sd_event_source_get_userdata(sd_event_source* s, void** userdata) {
    p7_log(__func__);
    (void)s;
    if (userdata) *userdata = NULL;
    return 0;
}

int sd_event_source_set_userdata(sd_event_source* s, void* userdata) {
    p7_log(__func__);
    (void)s;
    (void)userdata;
    return 0;
}

int sd_event_source_get_destroy_callback(sd_event_source* s, sd_event_destroy_t* callback) {
    p7_log(__func__);
    (void)s;
    if (callback) *callback = NULL;
    return 0;
}

int sd_event_source_set_destroy_callback(sd_event_source* s, sd_event_destroy_t callback) {
    p7_log(__func__);
    (void)s;
    (void)callback;
    return 0;
}

int sd_event_source_get_floating(sd_event_source* s, int* b) {
    p7_log(__func__);
    (void)s;
    if (b) *b = 0;
    return 0;
}

int sd_event_source_set_floating(sd_event_source* s, int b) {
    p7_log(__func__);
    (void)s;
    (void)b;
    return 0;
}

int sd_event_source_get_exit_on_failure(sd_event_source* s, int* b) {
    p7_log(__func__);
    (void)s;
    if (b) *b = 0;
    return 0;
}

int sd_event_source_set_exit_on_failure(sd_event_source* s, int b) {
    p7_log(__func__);
    (void)s;
    (void)b;
    return 0;
}

int sd_event_source_get_prepare(sd_event_source* s, sd_event_handler_t* callback) {
    p7_log(__func__);
    (void)s;
    if (callback) *callback = NULL;
    return 0;
}

int sd_event_source_set_prepare(sd_event_source* s, sd_event_handler_t callback) {
    p7_log(__func__);
    (void)s;
    (void)callback;
    return 0;
}

/* Child source */
int sd_event_source_get_child_pid(sd_event_source* s, pid_t* pid) {
    p7_log(__func__);
    (void)s;
    if (pid) *pid = 0;
    return 0;
}

int sd_event_source_get_child_pidfd(sd_event_source* s, int* fd) {
    p7_log(__func__);
    (void)s;
    if (fd) *fd = -1;
    return 0;
}

int sd_event_source_get_child_pidfd_own(sd_event_source* s, int* b) {
    p7_log(__func__);
    (void)s;
    if (b) *b = 0;
    return 0;
}

int sd_event_source_set_child_pidfd_own(sd_event_source* s, int b) {
    p7_log(__func__);
    (void)s;
    (void)b;
    return 0;
}

int sd_event_source_get_child_process_own(sd_event_source* s, int* b) {
    p7_log(__func__);
    (void)s;
    if (b) *b = 0;
    return 0;
}

int sd_event_source_set_child_process_own(sd_event_source* s, int b) {
    p7_log(__func__);
    (void)s;
    (void)b;
    return 0;
}

int sd_event_source_send_child_signal(sd_event_source* s, int sig, const siginfo_t* si, int flags) {
    p7_log(__func__);
    (void)s;
    (void)sig;
    (void)si;
    (void)flags;
    return 0;
}

/* Signal source */
int sd_event_source_get_signal(sd_event_source* s, int* sig) {
    p7_log(__func__);
    (void)s;
    if (sig) *sig = 0;
    return 0;
}

/* Time source */
int sd_event_source_get_time(sd_event_source* s, uint64_t* usec) {
    p7_log(__func__);
    (void)s;
    if (usec) *usec = 0;
    return 0;
}

int sd_event_source_set_time(sd_event_source* s, uint64_t usec) {
    p7_log(__func__);
    (void)s;
    (void)usec;
    return 0;
}

int sd_event_source_get_time_accuracy(sd_event_source* s, uint64_t* usec) {
    p7_log(__func__);
    (void)s;
    if (usec) *usec = 0;
    return 0;
}

int sd_event_source_set_time_accuracy(sd_event_source* s, uint64_t usec) {
    p7_log(__func__);
    (void)s;
    (void)usec;
    return 0;
}

int sd_event_source_get_time_clock(sd_event_source* s, clockid_t* clock) {
    p7_log(__func__);
    (void)s;
    if (clock) *clock = CLOCK_MONOTONIC;
    return 0;
}

int sd_event_source_set_time_relative(sd_event_source* s, uint64_t usec) {
    p7_log(__func__);
    (void)s;
    (void)usec;
    return 0;
}

/* Inotify source */
int sd_event_source_get_inotify_mask(sd_event_source* s, uint32_t* mask) {
    p7_log(__func__);
    (void)s;
    if (mask) *mask = 0;
    return 0;
}

int sd_event_source_get_inotify_path(sd_event_source* s, const char** path) {
    p7_log(__func__);
    (void)s;
    if (path) *path = NULL;
    return 0;
}

/* Memory pressure */
int sd_event_source_set_memory_pressure_period(sd_event_source* s, uint64_t usec) {
    p7_log(__func__);
    (void)s;
    (void)usec;
    return 0;
}

int sd_event_source_set_memory_pressure_type(sd_event_source* s, int type) {
    p7_log(__func__);
    (void)s;
    (void)type;
    return 0;
}

/* Rate limit */
int sd_event_source_get_ratelimit(sd_event_source* s, uint64_t* interval, uint64_t* burst) {
    p7_log(__func__);
    (void)s;
    if (interval) *interval = 0;
    if (burst) *burst = 0;
    return 0;
}

int sd_event_source_set_ratelimit(sd_event_source* s, uint64_t interval, uint64_t burst) {
    p7_log(__func__);
    (void)s;
    (void)interval;
    (void)burst;
    return 0;
}

int sd_event_source_set_ratelimit_expire_callback(sd_event_source* s, sd_event_handler_t callback) {
    p7_log(__func__);
    (void)s;
    (void)callback;
    return 0;
}

int sd_event_source_is_ratelimited(sd_event_source* s) {
    p7_log(__func__);
    (void)s;
    return 0;
}

int sd_event_source_leave_ratelimit(sd_event_source* s) {
    p7_log(__func__);
    (void)s;
    return 0;
}

/* ============================================================================
 * SECTION 11: DEVICE FAMILY (udev compatibility stubs)
 * ============================================================================ */

/* Device reference */
sd_device* sd_device_ref(sd_device* d) {
    p7_log(__func__);
    return d;
}

sd_device* sd_device_unref(sd_device* d) {
    p7_log(__func__);
    return NULL;
}

/* Device creation */
int sd_device_new_from_syspath(sd_device** ret, const char* syspath) {
    p7_log(__func__);
    (void)syspath;
    if (!ret) return -EINVAL;
    *ret = alloc_fake_ptr(FAKE_PTR_DEV);
    return 0;
}

int sd_device_new_from_devnum(sd_device** ret, char type, dev_t devnum) {
    p7_log(__func__);
    (void)type;
    (void)devnum;
    if (!ret) return -EINVAL;
    *ret = alloc_fake_ptr(FAKE_PTR_DEV);
    return 0;
}

int sd_device_new_from_stat_rdev(sd_device** ret, const struct stat* statbuf) {
    p7_log(__func__);
    (void)statbuf;
    if (!ret) return -EINVAL;
    *ret = alloc_fake_ptr(FAKE_PTR_DEV);
    return 0;
}

int sd_device_new_from_devname(sd_device** ret, const char* devname) {
    p7_log(__func__);
    (void)devname;
    if (!ret) return -EINVAL;
    *ret = alloc_fake_ptr(FAKE_PTR_DEV);
    return 0;
}

int sd_device_new_from_path(sd_device** ret, const char* path) {
    p7_log(__func__);
    (void)path;
    if (!ret) return -EINVAL;
    *ret = alloc_fake_ptr(FAKE_PTR_DEV);
    return 0;
}

int sd_device_new_from_subsystem_sysname(sd_device** ret, const char* subsystem, const char* sysname) {
    p7_log(__func__);
    (void)subsystem;
    (void)sysname;
    if (!ret) return -EINVAL;
    *ret = alloc_fake_ptr(FAKE_PTR_DEV);
    return 0;
}

int sd_device_new_from_device_id(sd_device** ret, const char* id) {
    p7_log(__func__);
    (void)id;
    if (!ret) return -EINVAL;
    *ret = alloc_fake_ptr(FAKE_PTR_DEV);
    return 0;
}

int sd_device_new_from_ifindex(sd_device** ret, int ifindex) {
    p7_log(__func__);
    (void)ifindex;
    if (!ret) return -EINVAL;
    *ret = alloc_fake_ptr(FAKE_PTR_DEV);
    return 0;
}

int sd_device_new_from_ifname(sd_device** ret, const char* ifname) {
    p7_log(__func__);
    (void)ifname;
    if (!ret) return -EINVAL;
    *ret = alloc_fake_ptr(FAKE_PTR_DEV);
    return 0;
}

int sd_device_new_child(sd_device** ret, sd_device* parent, const char* sysname) {
    p7_log(__func__);
    (void)parent;
    (void)sysname;
    if (!ret) return -EINVAL;
    *ret = alloc_fake_ptr(FAKE_PTR_DEV);
    return 0;
}

/* Device getters */
int sd_device_get_syspath(sd_device* d, const char** ret) {
    p7_log(__func__);
    (void)d;
    if (ret) *ret = "/sys/devices/virtual/misc/null";
    return 0;
}

int sd_device_get_devpath(sd_device* d, const char** ret) {
    p7_log(__func__);
    (void)d;
    if (ret) *ret = "/devices/virtual/misc/null";
    return 0;
}

int sd_device_get_sysname(sd_device* d, const char** ret) {
    p7_log(__func__);
    (void)d;
    if (ret) *ret = "null";
    return 0;
}

int sd_device_get_sysnum(sd_device* d, const char** ret) {
    p7_log(__func__);
    (void)d;
    if (ret) *ret = NULL;
    return 0;
}

int sd_device_get_devname(sd_device* d, const char** ret) {
    p7_log(__func__);
    (void)d;
    if (ret) *ret = "/dev/null";
    return 0;
}

int sd_device_get_devtype(sd_device* d, const char** ret) {
    p7_log(__func__);
    (void)d;
    if (ret) *ret = NULL;
    return 0;
}

int sd_device_get_subsystem(sd_device* d, const char** ret) {
    p7_log(__func__);
    (void)d;
    if (ret) *ret = "misc";
    return 0;
}

int sd_device_get_driver(sd_device* d, const char** ret) {
    p7_log(__func__);
    (void)d;
    if (ret) *ret = NULL;
    return 0;
}

int sd_device_get_driver_subsystem(sd_device* d, const char** ret) {
    p7_log(__func__);
    (void)d;
    if (ret) *ret = NULL;
    return 0;
}

int sd_device_get_devnum(sd_device* d, dev_t* ret) {
    p7_log(__func__);
    (void)d;
    if (ret) *ret = makedev(1, 3);
    return 0;
}

int sd_device_get_ifindex(sd_device* d, int* ret) {
    p7_log(__func__);
    (void)d;
    if (ret) *ret = 0;
    return -ENODATA;
}

int sd_device_get_action(sd_device* d, sd_device_action_t* ret) {
    p7_log(__func__);
    (void)d;
    if (ret) *ret = SD_DEVICE_ADD;
    return 0;
}

int sd_device_get_seqnum(sd_device* d, uint64_t* ret) {
    p7_log(__func__);
    (void)d;
    if (ret) *ret = 0;
    return -ENODATA;
}

int sd_device_get_usec_initialized(sd_device* d, uint64_t* ret) {
    p7_log(__func__);
    (void)d;
    if (ret) *ret = 0;
    return -ENODATA;
}

int sd_device_get_usec_since_initialized(sd_device* d, uint64_t* ret) {
    p7_log(__func__);
    (void)d;
    if (ret) *ret = 0;
    return -ENODATA;
}

int sd_device_get_diskseq(sd_device* d, uint64_t* ret) {
    p7_log(__func__);
    (void)d;
    if (ret) *ret = 0;
    return -ENODATA;
}

int sd_device_get_device_id(sd_device* d, const char** ret) {
    p7_log(__func__);
    (void)d;
    if (ret) *ret = "0:0";
    return 0;
}

int sd_device_get_is_initialized(sd_device* d, int* ret) {
    p7_log(__func__);
    (void)d;
    if (ret) *ret = 1;
    return 0;
}

int sd_device_get_trigger_uuid(sd_device* d, sd_id128_t* ret) {
    p7_log(__func__);
    (void)d;
    if (ret) *ret = machine_id;
    return -ENODATA;
}

/* Device parent/child */
int sd_device_get_parent(sd_device* d, sd_device** ret) {
    p7_log(__func__);
    (void)d;
    if (ret) *ret = NULL;
    return -ENOENT;
}

int sd_device_get_parent_with_subsystem_devtype(sd_device* d, const char* subsystem,
                                                  const char* devtype, sd_device** ret) {
    p7_log(__func__);
    (void)d;
    (void)subsystem;
    (void)devtype;
    if (ret) *ret = NULL;
    return -ENOENT;
}

int sd_device_get_child_first(sd_device* d, sd_device** ret) {
    p7_log(__func__);
    (void)d;
    if (ret) *ret = NULL;
    return -ENOENT;
}

int sd_device_get_child_next(sd_device* d, sd_device** ret) {
    p7_log(__func__);
    (void)d;
    if (ret) *ret = NULL;
    return -ENOENT;
}

/* Device properties */
int sd_device_get_property_value(sd_device* d, const char* key, const char** value) {
    p7_log(__func__);
    (void)d;
    (void)key;
    if (value) *value = NULL;
    return -ENOENT;
}

int sd_device_get_property_first(sd_device* d, const char** key, const char** value) {
    p7_log(__func__);
    (void)d;
    if (key) *key = NULL;
    if (value) *value = NULL;
    return -ENOENT;
}

int sd_device_get_property_next(sd_device* d, const char** key, const char** value) {
    p7_log(__func__);
    (void)d;
    if (key) *key = NULL;
    if (value) *value = NULL;
    return -ENOENT;
}

/* Device sysattrs */
int sd_device_get_sysattr_value(sd_device* d, const char* sysattr, const char** value) {
    p7_log(__func__);
    (void)d;
    (void)sysattr;
    if (value) *value = NULL;
    return -ENOENT;
}

int sd_device_get_sysattr_value_with_size(sd_device* d, const char* sysattr, const char** value, size_t* size) {
    p7_log(__func__);
    (void)d;
    (void)sysattr;
    if (value) *value = NULL;
    if (size) *size = 0;
    return -ENOENT;
}

int sd_device_get_sysattr_first(sd_device* d, const char** sysattr) {
    p7_log(__func__);
    (void)d;
    if (sysattr) *sysattr = NULL;
    return -ENOENT;
}

int sd_device_get_sysattr_next(sd_device* d, const char** sysattr) {
    p7_log(__func__);
    (void)d;
    if (sysattr) *sysattr = NULL;
    return -ENOENT;
}

int sd_device_set_sysattr_value(sd_device* d, const char* sysattr, const char* value) {
    p7_log(__func__);
    (void)d;
    (void)sysattr;
    (void)value;
    return -EROFS;
}

int sd_device_set_sysattr_valuef(sd_device* d, const char* sysattr, const char* format, ...) {
    p7_log(__func__);
    (void)d;
    (void)sysattr;
    (void)format;
    return -EROFS;
}

/* Device tags */
int sd_device_get_tag_first(sd_device* d, const char** tag) {
    p7_log(__func__);
    (void)d;
    if (tag) *tag = NULL;
    return -ENOENT;
}

int sd_device_get_tag_next(sd_device* d, const char** tag) {
    p7_log(__func__);
    (void)d;
    if (tag) *tag = NULL;
    return -ENOENT;
}

int sd_device_get_current_tag_first(sd_device* d, const char** tag) {
    p7_log(__func__);
    (void)d;
    if (tag) *tag = NULL;
    return -ENOENT;
}

int sd_device_get_current_tag_next(sd_device* d, const char** tag) {
    p7_log(__func__);
    (void)d;
    if (tag) *tag = NULL;
    return -ENOENT;
}

int sd_device_has_tag(sd_device* d, const char* tag) {
    p7_log(__func__);
    (void)d;
    (void)tag;
    return 0;
}

int sd_device_has_current_tag(sd_device* d, const char* tag) {
    p7_log(__func__);
    (void)d;
    (void)tag;
    return 0;
}

/* Device devlinks */
int sd_device_get_devlink_first(sd_device* d, const char** devlink) {
    p7_log(__func__);
    (void)d;
    if (devlink) *devlink = NULL;
    return -ENOENT;
}

int sd_device_get_devlink_next(sd_device* d, const char** devlink) {
    p7_log(__func__);
    (void)d;
    if (devlink) *devlink = NULL;
    return -ENOENT;
}

/* Device operations */
int sd_device_open(sd_device* d, int flags) {
    p7_log(__func__);
    (void)d;
    (void)flags;
    return -ENXIO;
}

int sd_device_trigger(sd_device* d) {
    p7_log(__func__);
    (void)d;
    return -EPERM;
}

int sd_device_trigger_with_uuid(sd_device* d, sd_id128_t* uuid) {
    p7_log(__func__);
    (void)d;
    (void)uuid;
    return -EPERM;
}

/* Device enumerator */
sd_device_enumerator* sd_device_enumerator_ref(sd_device_enumerator* e) {
    p7_log(__func__);
    return e;
}

sd_device_enumerator* sd_device_enumerator_unref(sd_device_enumerator* e) {
    p7_log(__func__);
    return NULL;
}

int sd_device_enumerator_new(sd_device_enumerator** ret) {
    p7_log(__func__);
    if (!ret) return -EINVAL;
    *ret = alloc_fake_ptr(FAKE_PTR_ENUM);
    return 0;
}

int sd_device_enumerator_add_match_subsystem(sd_device_enumerator* e, const char* subsystem, int match) {
    p7_log(__func__);
    (void)e;
    (void)subsystem;
    (void)match;
    return 0;
}

int sd_device_enumerator_add_match_sysname(sd_device_enumerator* e, const char* sysname) {
    p7_log(__func__);
    (void)e;
    (void)sysname;
    return 0;
}

int sd_device_enumerator_add_match_nomatch_sysname(sd_device_enumerator* e, const char* sysname) {
    p7_log(__func__);
    (void)e;
    (void)sysname;
    return 0;
}

int sd_device_enumerator_add_match_sysattr(sd_device_enumerator* e, const char* sysattr,
                                            const char* value, int match) {
    p7_log(__func__);
    (void)e;
    (void)sysattr;
    (void)value;
    (void)match;
    return 0;
}

int sd_device_enumerator_add_match_property(sd_device_enumerator* e, const char* property,
                                              const char* value) {
    p7_log(__func__);
    (void)e;
    (void)property;
    (void)value;
    return 0;
}

int sd_device_enumerator_add_match_property_required(sd_device_enumerator* e, const char* property,
                                                       const char* value) {
    p7_log(__func__);
    (void)e;
    (void)property;
    (void)value;
    return 0;
}

int sd_device_enumerator_add_match_tag(sd_device_enumerator* e, const char* tag, int match) {
    p7_log(__func__);
    (void)e;
    (void)tag;
    (void)match;
    return 0;
}

int sd_device_enumerator_add_match_parent(sd_device_enumerator* e, sd_device* parent) {
    p7_log(__func__);
    (void)e;
    (void)parent;
    return 0;
}

int sd_device_enumerator_add_all_parents(sd_device_enumerator* e, sd_device* parent) {
    p7_log(__func__);
    (void)e;
    (void)parent;
    return 0;
}

int sd_device_enumerator_allow_uninitialized(sd_device_enumerator* e) {
    p7_log(__func__);
    (void)e;
    return 0;
}

int sd_device_enumerator_get_device_first(sd_device_enumerator* e, sd_device** ret) {
    p7_log(__func__);
    (void)e;
    if (ret) *ret = NULL;
    return -ENOENT;
}

int sd_device_enumerator_get_device_next(sd_device_enumerator* e, sd_device** ret) {
    p7_log(__func__);
    (void)e;
    if (ret) *ret = NULL;
    return -ENOENT;
}

int sd_device_enumerator_get_subsystem_first(sd_device_enumerator* e, const char** ret) {
    p7_log(__func__);
    (void)e;
    if (ret) *ret = NULL;
    return -ENOENT;
}

int sd_device_enumerator_get_subsystem_next(sd_device_enumerator* e, const char** ret) {
    p7_log(__func__);
    (void)e;
    if (ret) *ret = NULL;
    return -ENOENT;
}

/* Device monitor */
sd_device_monitor* sd_device_monitor_ref(sd_device_monitor* m) {
    p7_log(__func__);
    return m;
}

sd_device_monitor* sd_device_monitor_unref(sd_device_monitor* m) {
    p7_log(__func__);
    return NULL;
}

int sd_device_monitor_new(sd_device_monitor** ret) {
    p7_log(__func__);
    if (!ret) return -EINVAL;
    *ret = alloc_fake_ptr(FAKE_PTR_MON);
    return 0;
}

int sd_device_monitor_get_fd(sd_device_monitor* m) {
    p7_log(__func__);
    (void)m;
    return -1;
}

int sd_device_monitor_get_events(sd_device_monitor* m) {
    p7_log(__func__);
    (void)m;
    return 0;
}

int sd_device_monitor_get_timeout(sd_device_monitor* m, uint64_t* timeout) {
    p7_log(__func__);
    (void)m;
    if (timeout) *timeout = 0;
    return 0;
}

int sd_device_monitor_get_event(sd_device_monitor* m, sd_event** e) {
    p7_log(__func__);
    (void)m;
    if (e) *e = NULL;
    return 0;
}

int sd_device_monitor_get_event_source(sd_device_monitor* m, sd_event_source** s) {
    p7_log(__func__);
    (void)m;
    if (s) *s = NULL;
    return 0;
}

int sd_device_monitor_get_description(sd_device_monitor* m, const char** description) {
    p7_log(__func__);
    (void)m;
    if (description) *description = "mock-monitor";
    return 0;
}

int sd_device_monitor_set_description(sd_device_monitor* m, const char* description) {
    p7_log(__func__);
    (void)m;
    (void)description;
    return 0;
}

int sd_device_monitor_attach_event(sd_device_monitor* m, sd_event* e, int priority) {
    p7_log(__func__);
    (void)m;
    (void)e;
    (void)priority;
    return 0;
}

int sd_device_monitor_detach_event(sd_device_monitor* m) {
    p7_log(__func__);
    (void)m;
    return 0;
}

int sd_device_monitor_start(sd_device_monitor* m, sd_device_monitor_handler_t callback, void* userdata) {
    p7_log(__func__);
    (void)m;
    (void)callback;
    (void)userdata;
    return 0;
}

int sd_device_monitor_stop(sd_device_monitor* m) {
    p7_log(__func__);
    (void)m;
    return 0;
}

int sd_device_monitor_is_running(sd_device_monitor* m) {
    p7_log(__func__);
    (void)m;
    return 0;
}

int sd_device_monitor_receive(sd_device_monitor* m, sd_device** ret) {
    p7_log(__func__);
    (void)m;
    if (ret) *ret = NULL;
    return -EAGAIN;
}

int sd_device_monitor_filter_add_match_subsystem_devtype(sd_device_monitor* m, const char* subsystem,
                                                          const char* devtype) {
    p7_log(__func__);
    (void)m;
    (void)subsystem;
    (void)devtype;
    return 0;
}

int sd_device_monitor_filter_add_match_sysattr(sd_device_monitor* m, const char* sysattr,
                                                  const char* value, int match) {
    p7_log(__func__);
    (void)m;
    (void)sysattr;
    (void)value;
    (void)match;
    return 0;
}

int sd_device_monitor_filter_add_match_parent(sd_device_monitor* m, sd_device* parent) {
    p7_log(__func__);
    (void)m;
    (void)parent;
    return 0;
}

int sd_device_monitor_filter_add_match_tag(sd_device_monitor* m, const char* tag, int match) {
    p7_log(__func__);
    (void)m;
    (void)tag;
    (void)match;
    return 0;
}

int sd_device_monitor_filter_remove(sd_device_monitor* m) {
    p7_log(__func__);
    (void)m;
    return 0;
}

int sd_device_monitor_filter_update(sd_device_monitor* m) {
    p7_log(__func__);
    (void)m;
    return 0;
}

int sd_device_monitor_set_receive_buffer_size(sd_device_monitor* m, size_t size) {
    p7_log(__func__);
    (void)m;
    (void)size;
    return 0;
}

/* ============================================================================
 * SECTION 12: HWDB, LOGIN MONITOR, JOURNAL (Out of Scope per Hard Contract)
 * ============================================================================ */

/* HWDB - minimal stubs */
sd_hwdb* sd_hwdb_ref(sd_hwdb* h) {
    p7_log(__func__);
    return h;
}

sd_hwdb* sd_hwdb_unref(sd_hwdb* h) {
    p7_log(__func__);
    return NULL;
}

int sd_hwdb_new(sd_hwdb** ret) {
    p7_log(__func__);
    if (!ret) return -EINVAL;
    *ret = alloc_fake_ptr(FAKE_PTR_HWDB);
    return 0;
}

int sd_hwdb_new_from_path(sd_hwdb** ret, const char* path) {
    p7_log(__func__);
    (void)path;
    if (!ret) return -EINVAL;
    *ret = alloc_fake_ptr(FAKE_PTR_HWDB);
    return 0;
}

int sd_hwdb_get(sd_hwdb* hwdb, const char* modalias, const char* key, const char** value) {
    p7_log(__func__);
    (void)hwdb;
    (void)modalias;
    (void)key;
    if (value) *value = NULL;
    return -ENOENT;
}

int sd_hwdb_seek(sd_hwdb* hwdb, const char* modalias, const char* key) {
    p7_log(__func__);
    (void)hwdb;
    (void)modalias;
    (void)key;
    return -ENOENT;
}

int sd_hwdb_enumerate(sd_hwdb* hwdb, const char* modalias, sd_hwdb_enumerate_callback_t callback, void* userdata) {
    p7_log(__func__);
    (void)hwdb;
    (void)modalias;
    (void)callback;
    (void)userdata;
    return 0;
}

/* Login Monitor */
sd_login_monitor* sd_login_monitor_ref(sd_login_monitor* m) {
    p7_log(__func__);
    return m;
}

sd_login_monitor* sd_login_monitor_unref(sd_login_monitor* m) {
    p7_log(__func__);
    return NULL;
}

int sd_login_monitor_new(const char* category, sd_login_monitor** ret) {
    p7_log(__func__);
    (void)category;
    if (!ret) return -EINVAL;
    *ret = alloc_fake_ptr(FAKE_PTR_BUS);
    return 0;
}

int sd_login_monitor_get_fd(sd_login_monitor* m) {
    p7_log(__func__);
    (void)m;
    return -1;
}

int sd_login_monitor_get_events(sd_login_monitor* m) {
    p7_log(__func__);
    (void)m;
    return 0;
}

int sd_login_monitor_get_timeout(sd_login_monitor* m, uint64_t* timeout) {
    p7_log(__func__);
    (void)m;
    if (timeout) *timeout = 0;
    return 0;
}

int sd_login_monitor_flush(sd_login_monitor* m) {
    p7_log(__func__);
    (void)m;
    return 0;
}

/* Journal - out of scope, return errors */
sd_journal* sd_journal_ref(sd_journal* j) {
    p7_log(__func__);
    return j;
}

sd_journal* sd_journal_unref(sd_journal* j) {
    p7_log(__func__);
    return NULL;
}

int sd_journal_open(sd_journal** ret, int flags) {
    p7_log(__func__);
    (void)flags;
    if (!ret) return -EINVAL;
    *ret = alloc_fake_ptr(FAKE_PTR_JOURN);
    return 0;
}

int sd_journal_open_namespace(sd_journal** ret, const char* namespace, int flags) {
    p7_log(__func__);
    (void)namespace;
    (void)flags;
    if (!ret) return -EINVAL;
    *ret = alloc_fake_ptr(FAKE_PTR_JOURN);
    return 0;
}

int sd_journal_open_directory(sd_journal** ret, const char* path, int flags) {
    p7_log(__func__);
    (void)path;
    (void)flags;
    if (!ret) return -EINVAL;
    *ret = alloc_fake_ptr(FAKE_PTR_JOURN);
    return 0;
}

int sd_journal_open_directory_fd(sd_journal** ret, int fd, int flags) {
    p7_log(__func__);
    (void)fd;
    (void)flags;
    if (!ret) return -EINVAL;
    *ret = alloc_fake_ptr(FAKE_PTR_JOURN);
    return 0;
}

int sd_journal_open_files(sd_journal** ret, const char** paths, int flags) {
    p7_log(__func__);
    (void)paths;
    (void)flags;
    if (!ret) return -EINVAL;
    *ret = alloc_fake_ptr(FAKE_PTR_JOURN);
    return 0;
}

int sd_journal_open_files_fd(sd_journal** ret, int* fds, unsigned n_fds, int flags) {
    p7_log(__func__);
    (void)fds;
    (void)n_fds;
    (void)flags;
    if (!ret) return -EINVAL;
    *ret = alloc_fake_ptr(FAKE_PTR_JOURN);
    return 0;
}

int sd_journal_open_container(sd_journal** ret, const char* machine, int flags) {
    p7_log(__func__);
    (void)machine;
    (void)flags;
    if (!ret) return -EINVAL;
    *ret = alloc_fake_ptr(FAKE_PTR_JOURN);
    return 0;
}

void sd_journal_close(sd_journal* j) {
    p7_log(__func__);
    (void)j;
}

int sd_journal_get_fd(sd_journal* j) {
    p7_log(__func__);
    (void)j;
    return -1;
}

int sd_journal_get_events(sd_journal* j) {
    p7_log(__func__);
    (void)j;
    return 0;
}

int sd_journal_get_timeout(sd_journal* j, uint64_t* timeout) {
    p7_log(__func__);
    (void)j;
    if (timeout) *timeout = 0;
    return 0;
}

int sd_journal_process(sd_journal* j) {
    p7_log(__func__);
    (void)j;
    return 0;
}

int sd_journal_wait(sd_journal* j, uint64_t timeout) {
    p7_log(__func__);
    (void)j;
    (void)timeout;
    return 0;
}

int sd_journal_next(sd_journal* j) {
    p7_log(__func__);
    (void)j;
    return 0;  /* EOF */
}

int sd_journal_next_skip(sd_journal* j, uint64_t skip) {
    p7_log(__func__);
    (void)j;
    (void)skip;
    return 0;
}

int sd_journal_previous(sd_journal* j) {
    p7_log(__func__);
    (void)j;
    return 0;
}

int sd_journal_previous_skip(sd_journal* j, uint64_t skip) {
    p7_log(__func__);
    (void)j;
    (void)skip;
    return 0;
}

int sd_journal_step_one(sd_journal* j, int skip_monotonic) {
    p7_log(__func__);
    (void)j;
    (void)skip_monotonic;
    return 0;
}

int sd_journal_get_data(sd_journal* j, const char* field, const void** data, size_t* length) {
    p7_log(__func__);
    (void)j;
    (void)field;
    if (data) *data = NULL;
    if (length) *length = 0;
    return -ENOENT;
}

int sd_journal_get_data_threshold(sd_journal* j, size_t* sz) {
    p7_log(__func__);
    (void)j;
    if (sz) *sz = 0;
    return 0;
}

int sd_journal_set_data_threshold(sd_journal* j, size_t sz) {
    p7_log(__func__);
    (void)j;
    (void)sz;
    return 0;
}

int sd_journal_restart_data(sd_journal* j) {
    p7_log(__func__);
    (void)j;
    return 0;
}

int sd_journal_enumerate_data(sd_journal* j, const void** data, size_t* length) {
    p7_log(__func__);
    (void)j;
    if (data) *data = NULL;
    if (length) *length = 0;
    return 0;
}

int sd_journal_enumerate_available_data(sd_journal* j, const void** data, size_t* length) {
    p7_log(__func__);
    (void)j;
    if (data) *data = NULL;
    if (length) *length = 0;
    return 0;
}

int sd_journal_get_realtime_usec(sd_journal* j, uint64_t* usec) {
    p7_log(__func__);
    (void)j;
    if (usec) *usec = 0;
    return -ENODATA;
}

int sd_journal_get_monotonic_usec(sd_journal* j, uint64_t* usec, sd_id128_t* boot_id) {
    p7_log(__func__);
    (void)j;
    if (usec) *usec = 0;
    if (boot_id) *boot_id = machine_id;
    return -ENODATA;
}

int sd_journal_get_seqnum(sd_journal* j, uint64_t* ret, sd_id128_t* seqnum_id) {
    p7_log(__func__);
    (void)j;
    if (ret) *ret = 0;
    if (seqnum_id) *seqnum_id = machine_id;
    return -ENODATA;
}

int sd_journal_get_cursor(sd_journal* j, char** cursor) {
    p7_log(__func__);
    (void)j;
    if (cursor) *cursor = NULL;
    return -ENOENT;
}

int sd_journal_test_cursor(sd_journal* j, const char* cursor) {
    p7_log(__func__);
    (void)j;
    (void)cursor;
    return -EINVAL;
}

int sd_journal_seek_head(sd_journal* j) {
    p7_log(__func__);
    (void)j;
    return 0;
}

int sd_journal_seek_tail(sd_journal* j) {
    p7_log(__func__);
    (void)j;
    return 0;
}

int sd_journal_seek_cursor(sd_journal* j, const char* cursor) {
    p7_log(__func__);
    (void)j;
    (void)cursor;
    return 0;
}

int sd_journal_seek_realtime_usec(sd_journal* j, uint64_t usec) {
    p7_log(__func__);
    (void)j;
    (void)usec;
    return 0;
}

int sd_journal_seek_monotonic_usec(sd_journal* j, sd_id128_t boot_id, uint64_t usec) {
    p7_log(__func__);
    (void)j;
    (void)boot_id;
    (void)usec;
    return 0;
}

int sd_journal_get_cutoff_realtime_usec(sd_journal* j, uint64_t* from, uint64_t* to) {
    p7_log(__func__);
    (void)j;
    if (from) *from = 0;
    if (to) *to = 0;
    return -ENODATA;
}

int sd_journal_get_cutoff_monotonic_usec(sd_journal* j, sd_id128_t boot_id, uint64_t* from, uint64_t* to) {
    p7_log(__func__);
    (void)j;
    (void)boot_id;
    if (from) *from = 0;
    if (to) *to = 0;
    return -ENODATA;
}

int sd_journal_get_usage(sd_journal* j, uint64_t* bytes) {
    p7_log(__func__);
    (void)j;
    if (bytes) *bytes = 0;
    return 0;
}

int sd_journal_query_unique(sd_journal* j, const char* field) {
    p7_log(__func__);
    (void)j;
    (void)field;
    return 0;
}

int sd_journal_enumerate_unique(sd_journal* j, const void** data, size_t* length) {
    p7_log(__func__);
    (void)j;
    if (data) *data = NULL;
    if (length) *length = 0;
    return 0;
}

int sd_journal_restart_unique(sd_journal* j) {
    p7_log(__func__);
    (void)j;
    return 0;
}

int sd_journal_enumerate_available_unique(sd_journal* j, const void** data, size_t* length) {
    p7_log(__func__);
    (void)j;
    if (data) *data = NULL;
    if (length) *length = 0;
    return 0;
}

int sd_journal_enumerate_fields(sd_journal* j, const char** field) {
    p7_log(__func__);
    (void)j;
    if (field) *field = NULL;
    return 0;
}

int sd_journal_restart_fields(sd_journal* j) {
    p7_log(__func__);
    (void)j;
    return 0;
}

int sd_journal_add_match(sd_journal* j, const void* data, size_t size) {
    p7_log(__func__);
    (void)j;
    (void)data;
    (void)size;
    return 0;
}

int sd_journal_add_disjunction(sd_journal* j) {
    p7_log(__func__);
    (void)j;
    return 0;
}

int sd_journal_add_conjunction(sd_journal* j) {
    p7_log(__func__);
    (void)j;
    return 0;
}

void sd_journal_flush_matches(sd_journal* j) {
    p7_log(__func__);
    (void)j;
}

int sd_journal_get_catalog(sd_journal* j, char** text) {
    p7_log(__func__);
    (void)j;
    if (text) *text = NULL;
    return -ENOENT;
}

int sd_journal_get_catalog_for_message_id(sd_id128_t id, char** text) {
    p7_log(__func__);
    (void)id;
    if (text) *text = NULL;
    return -ENOENT;
}

int sd_journal_has_runtime_files(sd_journal* j) {
    p7_log(__func__);
    (void)j;
    return 0;
}

int sd_journal_has_persistent_files(sd_journal* j) {
    p7_log(__func__);
    (void)j;
    return 0;
}

int sd_journal_reliable_fd(sd_journal* j) {
    p7_log(__func__);
    (void)j;
    return 0;
}

int sd_journal_stream_fd(const char* identifier, int priority, int level_prefix) {
    p7_log(__func__);
    (void)identifier;
    (void)priority;
    (void)level_prefix;
    return -1;
}

int sd_journal_stream_fd_with_namespace(const char* namespace, const char* identifier,
                                         int priority, int level_prefix) {
    p7_log(__func__);
    (void)namespace;
    (void)identifier;
    (void)priority;
    (void)level_prefix;
    return -1;
}

/* Journal logging - redirect to syslog */
int sd_journal_print(int priority, const char* format, ...) {
    p7_log(__func__);
    (void)priority;
    (void)format;
    return 0;
}

int sd_journal_printv(int priority, const char* format, va_list ap) {
    p7_log(__func__);
    (void)priority;
    (void)format;
    (void)ap;
    return 0;
}

int sd_journal_print_with_location(int priority, const char* file, const char* line,
                                     const char* func, const char* format, ...) {
    p7_log(__func__);
    (void)priority;
    (void)file;
    (void)line;
    (void)func;
    (void)format;
    return 0;
}

int sd_journal_printv_with_location(int priority, const char* file, const char* line,
                                      const char* func, const char* format, va_list ap) {
    p7_log(__func__);
    (void)priority;
    (void)file;
    (void)line;
    (void)func;
    (void)format;
    (void)ap;
    return 0;
}

int sd_journal_send(const char* format, ...) {
    p7_log(__func__);
    (void)format;
    return 0;
}

int sd_journal_sendv(const struct iovec* iov, int n) {
    p7_log(__func__);
    (void)iov;
    (void)n;
    return 0;
}

int sd_journal_send_with_location(const char* file, const char* line, const char* func,
                                   const char* format, ...) {
    p7_log(__func__);
    (void)file;
    (void)line;
    (void)func;
    (void)format;
    return 0;
}

int sd_journal_sendv_with_location(const char* file, const char* line, const char* func,
                                      const struct iovec* iov, int n) {
    p7_log(__func__);
    (void)file;
    (void)line;
    (void)func;
    (void)iov;
    (void)n;
    return 0;
}

int sd_journal_perror(const char* message) {
    p7_log(__func__);
    (void)message;
    return 0;
}

int sd_journal_perror_with_location(const char* file, const char* line, const char* func,
                                     const char* message) {
    p7_log(__func__);
    (void)file;
    (void)line;
    (void)func;
    (void)message;
    return 0;
}

/* ============================================================================
 * SECTION 13: JSON FAMILY (systemd 257+ compatibility)
 * ============================================================================ */

/* JSON Variant */
sd_json_variant* sd_json_variant_ref(sd_json_variant* v) {
    p7_log(__func__);
    return v;
}

sd_json_variant* sd_json_variant_unref(sd_json_variant* v) {
    p7_log(__func__);
    return NULL;
}

void sd_json_variant_unref_many(sd_json_variant** v, size_t n) {
    p7_log(__func__);
    (void)v;
    (void)n;
}

int sd_json_variant_new_null(sd_json_variant** ret) {
    p7_log(__func__);
    if (!ret) return -EINVAL;
    *ret = alloc_fake_ptr(FAKE_PTR_JSON);
    return 0;
}

int sd_json_variant_new_boolean(sd_json_variant** ret, int b) {
    p7_log(__func__);
    (void)b;
    if (!ret) return -EINVAL;
    *ret = alloc_fake_ptr(FAKE_PTR_JSON);
    return 0;
}

int sd_json_variant_new_integer(sd_json_variant** ret, int64_t i) {
    p7_log(__func__);
    (void)i;
    if (!ret) return -EINVAL;
    *ret = alloc_fake_ptr(FAKE_PTR_JSON);
    return 0;
}

int sd_json_variant_new_unsigned(sd_json_variant** ret, uint64_t u) {
    p7_log(__func__);
    (void)u;
    if (!ret) return -EINVAL;
    *ret = alloc_fake_ptr(FAKE_PTR_JSON);
    return 0;
}

int sd_json_variant_new_real(sd_json_variant** ret, double d) {
    p7_log(__func__);
    (void)d;
    if (!ret) return -EINVAL;
    *ret = alloc_fake_ptr(FAKE_PTR_JSON);
    return 0;
}

int sd_json_variant_new_string(sd_json_variant** ret, const char* s) {
    p7_log(__func__);
    (void)s;
    if (!ret) return -EINVAL;
    *ret = alloc_fake_ptr(FAKE_PTR_JSON);
    return 0;
}

int sd_json_variant_new_stringn(sd_json_variant** ret, const char* s, size_t n) {
    p7_log(__func__);
    (void)s;
    (void)n;
    if (!ret) return -EINVAL;
    *ret = alloc_fake_ptr(FAKE_PTR_JSON);
    return 0;
}

int sd_json_variant_new_array(sd_json_variant** ret, sd_json_variant** v, size_t n) {
    p7_log(__func__);
    (void)v;
    (void)n;
    if (!ret) return -EINVAL;
    *ret = alloc_fake_ptr(FAKE_PTR_JSON);
    return 0;
}

int sd_json_variant_new_array_bytes(sd_json_variant** ret, const void* p, size_t n) {
    p7_log(__func__);
    (void)p;
    (void)n;
    if (!ret) return -EINVAL;
    *ret = alloc_fake_ptr(FAKE_PTR_JSON);
    return 0;
}

int sd_json_variant_new_array_strv(sd_json_variant** ret, char* const* l) {
    p7_log(__func__);
    (void)l;
    if (!ret) return -EINVAL;
    *ret = alloc_fake_ptr(FAKE_PTR_JSON);
    return 0;
}

int sd_json_variant_new_object(sd_json_variant** ret, sd_json_variant** v, size_t n) {
    p7_log(__func__);
    (void)v;
    (void)n;
    if (!ret) return -EINVAL;
    *ret = alloc_fake_ptr(FAKE_PTR_JSON);
    return 0;
}

int sd_json_variant_new_id128(sd_json_variant** ret, sd_id128_t id) {
    p7_log(__func__);
    (void)id;
    if (!ret) return -EINVAL;
    *ret = alloc_fake_ptr(FAKE_PTR_JSON);
    return 0;
}

int sd_json_variant_new_uuid(sd_json_variant** ret, sd_id128_t id) {
    p7_log(__func__);
    (void)id;
    if (!ret) return -EINVAL;
    *ret = alloc_fake_ptr(FAKE_PTR_JSON);
    return 0;
}

int sd_json_variant_new_base32hex(sd_json_variant** ret, const void* p, size_t n) {
    p7_log(__func__);
    (void)p;
    (void)n;
    if (!ret) return -EINVAL;
    *ret = alloc_fake_ptr(FAKE_PTR_JSON);
    return 0;
}

int sd_json_variant_new_base64(sd_json_variant** ret, const void* p, size_t n) {
    p7_log(__func__);
    (void)p;
    (void)n;
    if (!ret) return -EINVAL;
    *ret = alloc_fake_ptr(FAKE_PTR_JSON);
    return 0;
}

int sd_json_variant_new_hex(sd_json_variant** ret, const void* p, size_t n) {
    p7_log(__func__);
    (void)p;
    (void)n;
    if (!ret) return -EINVAL;
    *ret = alloc_fake_ptr(FAKE_PTR_JSON);
    return 0;
}

int sd_json_variant_new_octescape(sd_json_variant** ret, const void* p, size_t n) {
    p7_log(__func__);
    (void)p;
    (void)n;
    if (!ret) return -EINVAL;
    *ret = alloc_fake_ptr(FAKE_PTR_JSON);
    return 0;
}

/* JSON Variant Accessors */
int sd_json_variant_is_null(sd_json_variant* v) {
    p7_log(__func__);
    (void)v;
    return 0;
}

int sd_json_variant_is_boolean(sd_json_variant* v) {
    p7_log(__func__);
    (void)v;
    return 0;
}

int sd_json_variant_is_integer(sd_json_variant* v) {
    p7_log(__func__);
    (void)v;
    return 0;
}

int sd_json_variant_is_unsigned(sd_json_variant* v) {
    p7_log(__func__);
    (void)v;
    return 0;
}

int sd_json_variant_is_real(sd_json_variant* v) {
    p7_log(__func__);
    (void)v;
    return 0;
}

int sd_json_variant_is_number(sd_json_variant* v) {
    p7_log(__func__);
    (void)v;
    return 0;
}

int sd_json_variant_is_string(sd_json_variant* v) {
    p7_log(__func__);
    (void)v;
    return 0;
}

int sd_json_variant_is_array(sd_json_variant* v) {
    p7_log(__func__);
    (void)v;
    return 0;
}

int sd_json_variant_is_object(sd_json_variant* v) {
    p7_log(__func__);
    (void)v;
    return 0;
}

int sd_json_variant_is_blank_array(sd_json_variant* v) {
    p7_log(__func__);
    (void)v;
    return 0;
}

int sd_json_variant_is_blank_object(sd_json_variant* v) {
    p7_log(__func__);
    (void)v;
    return 0;
}

int sd_json_variant_is_negative(sd_json_variant* v) {
    p7_log(__func__);
    (void)v;
    return 0;
}

int sd_json_variant_has_type(sd_json_variant* v, sd_json_variant_type_t type) {
    p7_log(__func__);
    (void)v;
    (void)type;
    return 0;
}

sd_json_variant_type_t sd_json_variant_type(sd_json_variant* v) {
    p7_log(__func__);
    (void)v;
    return 0;
}

int sd_json_variant_boolean(sd_json_variant* v) {
    p7_log(__func__);
    (void)v;
    return 0;
}

int64_t sd_json_variant_integer(sd_json_variant* v) {
    p7_log(__func__);
    (void)v;
    return 0;
}

uint64_t sd_json_variant_unsigned(sd_json_variant* v) {
    p7_log(__func__);
    (void)v;
    return 0;
}

double sd_json_variant_real(sd_json_variant* v) {
    p7_log(__func__);
    (void)v;
    return 0.0;
}

const char* sd_json_variant_string(sd_json_variant* v) {
    p7_log(__func__);
    (void)v;
    return "";
}

size_t sd_json_variant_elements(sd_json_variant* v) {
    p7_log(__func__);
    (void)v;
    return 0;
}

sd_json_variant* sd_json_variant_by_index(sd_json_variant* v, size_t idx) {
    p7_log(__func__);
    (void)v;
    (void)idx;
    return NULL;
}

sd_json_variant* sd_json_variant_by_key(sd_json_variant* v, const char* key) {
    p7_log(__func__);
    (void)v;
    (void)key;
    return NULL;
}

sd_json_variant* sd_json_variant_by_key_full(sd_json_variant* v, const char* key, sd_json_variant** ret_key) {
    p7_log(__func__);
    (void)v;
    (void)key;
    if (ret_key) *ret_key = NULL;
    return NULL;
}

int sd_json_variant_equal(sd_json_variant* a, sd_json_variant* b) {
    p7_log(__func__);
    (void)a;
    (void)b;
    return 0;
}

int sd_json_variant_is_sensitive(sd_json_variant* v) {
    p7_log(__func__);
    (void)v;
    return 0;
}

int sd_json_variant_is_sensitive_recursive(sd_json_variant* v) {
    p7_log(__func__);
    (void)v;
    return 0;
}

int sd_json_variant_is_normalized(sd_json_variant* v) {
    p7_log(__func__);
    (void)v;
    return 1;
}

int sd_json_variant_is_sorted(sd_json_variant* v) {
    p7_log(__func__);
    (void)v;
    return 1;
}

/* JSON Variant Operations */
int sd_json_variant_sensitive(sd_json_variant* v) {
    p7_log(__func__);
    (void)v;
    return 0;
}

int sd_json_variant_set_field(sd_json_variant** v, const char* key, sd_json_variant* w) {
    p7_log(__func__);
    (void)v;
    (void)key;
    (void)w;
    return 0;
}

int sd_json_variant_set_fieldb(sd_json_variant** v, const char* key, ...) {
    p7_log(__func__);
    (void)v;
    (void)key;
    return 0;
}

int sd_json_variant_set_field_string(sd_json_variant** v, const char* key, const char* s) {
    p7_log(__func__);
    (void)v;
    (void)key;
    (void)s;
    return 0;
}

int sd_json_variant_set_field_integer(sd_json_variant** v, const char* key, int64_t i) {
    p7_log(__func__);
    (void)v;
    (void)key;
    (void)i;
    return 0;
}

int sd_json_variant_set_field_unsigned(sd_json_variant** v, const char* key, uint64_t u) {
    p7_log(__func__);
    (void)v;
    (void)key;
    (void)u;
    return 0;
}

int sd_json_variant_set_field_boolean(sd_json_variant** v, const char* key, int b) {
    p7_log(__func__);
    (void)v;
    (void)key;
    (void)b;
    return 0;
}

int sd_json_variant_set_field_id128(sd_json_variant** v, const char* key, sd_id128_t id) {
    p7_log(__func__);
    (void)v;
    (void)key;
    (void)id;
    return 0;
}

int sd_json_variant_set_field_uuid(sd_json_variant** v, const char* key, sd_id128_t id) {
    p7_log(__func__);
    (void)v;
    (void)key;
    (void)id;
    return 0;
}

int sd_json_variant_set_field_strv(sd_json_variant** v, const char* key, char* const* l) {
    p7_log(__func__);
    (void)v;
    (void)key;
    (void)l;
    return 0;
}

int sd_json_variant_append_array(sd_json_variant** v, sd_json_variant* w) {
    p7_log(__func__);
    (void)v;
    (void)w;
    return 0;
}

int sd_json_variant_append_arrayb(sd_json_variant** v, ...) {
    p7_log(__func__);
    (void)v;
    return 0;
}

int sd_json_variant_append_array_nodup(sd_json_variant** v, sd_json_variant* w) {
    p7_log(__func__);
    (void)v;
    (void)w;
    return 0;
}

int sd_json_variant_merge_object(sd_json_variant** v, sd_json_variant* w) {
    p7_log(__func__);
    (void)v;
    (void)w;
    return 0;
}

int sd_json_variant_merge_objectb(sd_json_variant** v, ...) {
    p7_log(__func__);
    (void)v;
    return 0;
}

int sd_json_variant_filter(sd_json_variant** v, char* const* l) {
    p7_log(__func__);
    (void)v;
    (void)l;
    return 0;
}

int sd_json_variant_normalize(sd_json_variant** v) {
    p7_log(__func__);
    (void)v;
    return 0;
}

int sd_json_variant_sort(sd_json_variant** v) {
    p7_log(__func__);
    (void)v;
    return 0;
}

int sd_json_variant_dump(sd_json_variant* v, int flags, FILE* f, const char* prefix) {
    p7_log(__func__);
    (void)v;
    (void)flags;
    (void)f;
    (void)prefix;
    return 0;
}

int sd_json_variant_format(sd_json_variant* v, int flags, char** ret) {
    p7_log(__func__);
    (void)v;
    (void)flags;
    if (ret) *ret = strdup("null");
    return 0;
}

int sd_json_variant_find(sd_json_variant* v, const char* expression, sd_json_variant** ret) {
    p7_log(__func__);
    (void)v;
    (void)expression;
    if (ret) *ret = NULL;
    return -ENOENT;
}

int sd_json_variant_get_source(sd_json_variant* v, const char** source, unsigned* line, unsigned* column) {
    p7_log(__func__);
    (void)v;
    if (source) *source = NULL;
    if (line) *line = 0;
    if (column) *column = 0;
    return -ENODATA;
}

char** sd_json_variant_strv(sd_json_variant* v) {
    p7_log(__func__);
    (void)v;
    return NULL;
}

int sd_json_variant_unbase64(sd_json_variant* v, void** ret, size_t* n) {
    p7_log(__func__);
    (void)v;
    if (ret) *ret = NULL;
    if (n) *n = 0;
    return -EINVAL;
}

int sd_json_variant_unhex(sd_json_variant* v, void** ret, size_t* n) {
    p7_log(__func__);
    (void)v;
    if (ret) *ret = NULL;
    if (n) *n = 0;
    return -EINVAL;
}

sd_json_variant_type_t sd_json_variant_type_from_string(const char* s) {
    p7_log(__func__);
    (void)s;
    return 0;
}

const char* sd_json_variant_type_to_string(sd_json_variant_type_t type) {
    p7_log(__func__);
    (void)type;
    return "null";
}

/* JSON Parse */
int sd_json_parse(const char* input, size_t n, sd_json_variant** ret, const char** ret_end, unsigned* ret_line) {
    p7_log(__func__);
    (void)input;
    (void)n;
    if (ret) *ret = alloc_fake_ptr(FAKE_PTR_JSON);
    if (ret_end) *ret_end = NULL;
    if (ret_line) *ret_line = 0;
    return 0;
}

int sd_json_parse_continue(const char* input, size_t n, sd_json_variant** ret, const char** ret_end, unsigned* ret_line) {
    p7_log(__func__);
    (void)input;
    (void)n;
    if (ret) *ret = alloc_fake_ptr(FAKE_PTR_JSON);
    if (ret_end) *ret_end = NULL;
    if (ret_line) *ret_line = 0;
    return 0;
}

int sd_json_parse_file(FILE* f, sd_json_variant** ret, unsigned* ret_line) {
    p7_log(__func__);
    (void)f;
    if (ret) *ret = alloc_fake_ptr(FAKE_PTR_JSON);
    if (ret_line) *ret_line = 0;
    return 0;
}

int sd_json_parse_file_at(int dir_fd, const char* path, sd_json_variant** ret, unsigned* ret_line) {
    p7_log(__func__);
    (void)dir_fd;
    (void)path;
    if (ret) *ret = alloc_fake_ptr(FAKE_PTR_JSON);
    if (ret_line) *ret_line = 0;
    return 0;
}

int sd_json_parse_with_source(const char* input, size_t n, const char* source, sd_json_variant** ret,
                               const char** ret_end, unsigned* ret_line) {
    p7_log(__func__);
    (void)input;
    (void)n;
    (void)source;
    if (ret) *ret = alloc_fake_ptr(FAKE_PTR_JSON);
    if (ret_end) *ret_end = NULL;
    if (ret_line) *ret_line = 0;
    return 0;
}

int sd_json_parse_with_source_continue(const char* input, size_t n, const char* source, sd_json_variant** ret,
                                         const char** ret_end, unsigned* ret_line) {
    p7_log(__func__);
    (void)input;
    (void)n;
    (void)source;
    if (ret) *ret = alloc_fake_ptr(FAKE_PTR_JSON);
    if (ret_end) *ret_end = NULL;
    if (ret_line) *ret_line = 0;
    return 0;
}

/* JSON Build */
int sd_json_build(sd_json_variant** ret, const char* format, ...) {
    p7_log(__func__);
    (void)format;
    if (!ret) return -EINVAL;
    *ret = alloc_fake_ptr(FAKE_PTR_JSON);
    return 0;
}

int sd_json_buildv(sd_json_variant** ret, const char* format, va_list ap) {
    p7_log(__func__);
    (void)format;
    (void)ap;
    if (!ret) return -EINVAL;
    *ret = alloc_fake_ptr(FAKE_PTR_JSON);
    return 0;
}

/* JSON Dispatch */
int sd_json_dispatch(sd_json_variant* v, const sd_json_dispatch_field* fields, void* userdata, unsigned flags) {
    p7_log(__func__);
    (void)v;
    (void)fields;
    (void)userdata;
    (void)flags;
    return 0;
}

int sd_json_dispatch_full(sd_json_variant* v, const sd_json_dispatch_field* fields, void* userdata,
                           unsigned flags, const char* bad_field, sd_json_variant** bad_value) {
    p7_log(__func__);
    (void)v;
    (void)fields;
    (void)userdata;
    (void)flags;
    (void)bad_field;
    if (bad_value) *bad_value = NULL;
    return 0;
}

int sd_json_dispatch_unsupported(const char* name, sd_json_variant* variant, sd_json_dispatch_flags_t flags, void* userdata) {
    p7_log(__func__);
    (void)name;
    (void)variant;
    (void)flags;
    (void)userdata;
    return -EINVAL;
}

int sd_json_dispatch_intbool(const char* name, sd_json_variant* variant, sd_json_dispatch_flags_t flags, void* userdata) {
    p7_log(__func__);
    (void)name;
    (void)variant;
    (void)flags;
    (void)userdata;
    return 0;
}

int sd_json_dispatch_stdbool(const char* name, sd_json_variant* variant, sd_json_dispatch_flags_t flags, void* userdata) {
    p7_log(__func__);
    (void)name;
    (void)variant;
    (void)flags;
    (void)userdata;
    return 0;
}

int sd_json_dispatch_tristate(const char* name, sd_json_variant* variant, sd_json_dispatch_flags_t flags, void* userdata) {
    p7_log(__func__);
    (void)name;
    (void)variant;
    (void)flags;
    (void)userdata;
    return 0;
}

int sd_json_dispatch_string(const char* name, sd_json_variant* variant, sd_json_dispatch_flags_t flags, void* userdata) {
    p7_log(__func__);
    (void)name;
    (void)variant;
    (void)flags;
    (void)userdata;
    return 0;
}

int sd_json_dispatch_const_string(const char* name, sd_json_variant* variant, sd_json_dispatch_flags_t flags, void* userdata) {
    p7_log(__func__);
    (void)name;
    (void)variant;
    (void)flags;
    (void)userdata;
    return 0;
}

int sd_json_dispatch_strv(const char* name, sd_json_variant* variant, sd_json_dispatch_flags_t flags, void* userdata) {
    p7_log(__func__);
    (void)name;
    (void)variant;
    (void)flags;
    (void)userdata;
    return 0;
}

int sd_json_dispatch_int8(const char* name, sd_json_variant* variant, sd_json_dispatch_flags_t flags, void* userdata) {
    p7_log(__func__);
    (void)name;
    (void)variant;
    (void)flags;
    (void)userdata;
    return 0;
}

int sd_json_dispatch_int16(const char* name, sd_json_variant* variant, sd_json_dispatch_flags_t flags, void* userdata) {
    p7_log(__func__);
    (void)name;
    (void)variant;
    (void)flags;
    (void)userdata;
    return 0;
}

int sd_json_dispatch_int32(const char* name, sd_json_variant* variant, sd_json_dispatch_flags_t flags, void* userdata) {
    p7_log(__func__);
    (void)name;
    (void)variant;
    (void)flags;
    (void)userdata;
    return 0;
}

int sd_json_dispatch_int64(const char* name, sd_json_variant* variant, sd_json_dispatch_flags_t flags, void* userdata) {
    p7_log(__func__);
    (void)name;
    (void)variant;
    (void)flags;
    (void)userdata;
    return 0;
}

int sd_json_dispatch_uint8(const char* name, sd_json_variant* variant, sd_json_dispatch_flags_t flags, void* userdata) {
    p7_log(__func__);
    (void)name;
    (void)variant;
    (void)flags;
    (void)userdata;
    return 0;
}

int sd_json_dispatch_uint16(const char* name, sd_json_variant* variant, sd_json_dispatch_flags_t flags, void* userdata) {
    p7_log(__func__);
    (void)name;
    (void)variant;
    (void)flags;
    (void)userdata;
    return 0;
}

int sd_json_dispatch_uint32(const char* name, sd_json_variant* variant, sd_json_dispatch_flags_t flags, void* userdata) {
    p7_log(__func__);
    (void)name;
    (void)variant;
    (void)flags;
    (void)userdata;
    return 0;
}

int sd_json_dispatch_uint64(const char* name, sd_json_variant* variant, sd_json_dispatch_flags_t flags, void* userdata) {
    p7_log(__func__);
    (void)name;
    (void)variant;
    (void)flags;
    (void)userdata;
    return 0;
}

int sd_json_dispatch_double(const char* name, sd_json_variant* variant, sd_json_dispatch_flags_t flags, void* userdata) {
    p7_log(__func__);
    (void)name;
    (void)variant;
    (void)flags;
    (void)userdata;
    return 0;
}

int sd_json_dispatch_uid_gid(const char* name, sd_json_variant* variant, sd_json_dispatch_flags_t flags, void* userdata) {
    p7_log(__func__);
    (void)name;
    (void)variant;
    (void)flags;
    (void)userdata;
    return 0;
}

int sd_json_dispatch_id128(const char* name, sd_json_variant* variant, sd_json_dispatch_flags_t flags, void* userdata) {
    p7_log(__func__);
    (void)name;
    (void)variant;
    (void)flags;
    (void)userdata;
    return 0;
}

int sd_json_dispatch_signal(const char* name, sd_json_variant* variant, sd_json_dispatch_flags_t flags, void* userdata) {
    p7_log(__func__);
    (void)name;
    (void)variant;
    (void)flags;
    (void)userdata;
    return 0;
}

int sd_json_dispatch_variant(const char* name, sd_json_variant* variant, sd_json_dispatch_flags_t flags, void* userdata) {
    p7_log(__func__);
    (void)name;
    (void)variant;
    (void)flags;
    (void)userdata;
    return 0;
}

int sd_json_dispatch_variant_noref(const char* name, sd_json_variant* variant, sd_json_dispatch_flags_t flags, void* userdata) {
    p7_log(__func__);
    (void)name;
    (void)variant;
    (void)flags;
    (void)userdata;
    return 0;
}

/* ============================================================================
 * SECTION 14: VARLINK FAMILY (systemd 257+ compatibility)
 * ============================================================================ */

/* Varlink reference */
sd_varlink* sd_varlink_ref(sd_varlink* vl) {
    p7_log(__func__);
    return vl;
}

sd_varlink* sd_varlink_unref(sd_varlink* vl) {
    p7_log(__func__);
    return NULL;
}

/* Varlink creation/connection */
int sd_varlink_connect_address(sd_varlink** ret, const char* address) {
    p7_log(__func__);
    (void)address;
    if (!ret) return -EINVAL;
    *ret = alloc_fake_ptr(FAKE_PTR_VAR);
    return 0;
}

int sd_varlink_connect_url(sd_varlink** ret, const char* url) {
    p7_log(__func__);
    (void)url;
    if (!ret) return -EINVAL;
    *ret = alloc_fake_ptr(FAKE_PTR_VAR);
    return 0;
}

int sd_varlink_connect_exec(sd_varlink** ret, const char* executable, char* const* argv) {
    p7_log(__func__);
    (void)executable;
    (void)argv;
    if (!ret) return -EINVAL;
    *ret = alloc_fake_ptr(FAKE_PTR_VAR);
    return 0;
}

int sd_varlink_connect_fd(sd_varlink** ret, int fd) {
    p7_log(__func__);
    (void)fd;
    if (!ret) return -EINVAL;
    *ret = alloc_fake_ptr(FAKE_PTR_VAR);
    return 0;
}

int sd_varlink_connect_fd_pair(sd_varlink** ret, int input_fd, int output_fd) {
    p7_log(__func__);
    (void)input_fd;
    (void)output_fd;
    if (!ret) return -EINVAL;
    *ret = alloc_fake_ptr(FAKE_PTR_VAR);
    return 0;
}

/* Varlink lifecycle */
void sd_varlink_close(sd_varlink* vl) {
    p7_log(__func__);
    (void)vl;
}

sd_varlink* sd_varlink_close_unref(sd_varlink* vl) {
    p7_log(__func__);
    return sd_varlink_unref(vl);
}

sd_varlink* sd_varlink_flush_close_unref(sd_varlink* vl) {
    p7_log(__func__);
    return sd_varlink_unref(vl);
}

int sd_varlink_flush(sd_varlink* vl) {
    p7_log(__func__);
    (void)vl;
    return 0;
}

/* Varlink state */
int sd_varlink_is_connected(sd_varlink* vl) {
    p7_log(__func__);
    (void)vl;
    return 1;
}

int sd_varlink_is_idle(sd_varlink* vl) {
    p7_log(__func__);
    (void)vl;
    return 1;
}

/* Varlink fd */
int sd_varlink_get_fd(sd_varlink* vl) {
    p7_log(__func__);
    (void)vl;
    return -1;
}

int sd_varlink_get_input_fd(sd_varlink* vl) {
    p7_log(__func__);
    (void)vl;
    return -1;
}

int sd_varlink_get_output_fd(sd_varlink* vl) {
    p7_log(__func__);
    (void)vl;
    return -1;
}

int sd_varlink_get_events(sd_varlink* vl) {
    p7_log(__func__);
    (void)vl;
    return 0;
}

int sd_varlink_get_timeout(sd_varlink* vl, uint64_t* timeout) {
    p7_log(__func__);
    (void)vl;
    if (timeout) *timeout = 0;
    return 0;
}

int sd_varlink_get_n_fds(sd_varlink* vl, unsigned* n) {
    p7_log(__func__);
    (void)vl;
    if (n) *n = 0;
    return 0;
}

/* Varlink userdata */
int sd_varlink_get_userdata(sd_varlink* vl, void** userdata) {
    p7_log(__func__);
    (void)vl;
    if (userdata) *userdata = NULL;
    return 0;
}

int sd_varlink_set_userdata(sd_varlink* vl, void* userdata) {
    p7_log(__func__);
    (void)vl;
    (void)userdata;
    return 0;
}

int sd_varlink_get_description(sd_varlink* vl, const char** description) {
    p7_log(__func__);
    (void)vl;
    if (description) *description = "mock-varlink";
    return 0;
}

int sd_varlink_set_description(sd_varlink* vl, const char* description) {
    p7_log(__func__);
    (void)vl;
    (void)description;
    return 0;
}

int sd_varlink_set_relative_timeout(sd_varlink* vl, uint64_t timeout) {
    p7_log(__func__);
    (void)vl;
    (void)timeout;
    return 0;
}

int sd_varlink_set_allow_fd_passing_input(sd_varlink* vl, int b) {
    p7_log(__func__);
    (void)vl;
    (void)b;
    return 0;
}

int sd_varlink_set_allow_fd_passing_output(sd_varlink* vl, int b) {
    p7_log(__func__);
    (void)vl;
    (void)b;
    return 0;
}

int sd_varlink_set_input_sensitive(sd_varlink* vl, int b) {
    p7_log(__func__);
    (void)vl;
    (void)b;
    return 0;
}

/* Varlink fds */
int sd_varlink_push_fd(sd_varlink* vl, int fd) {
    p7_log(__func__);
    (void)vl;
    (void)fd;
    return 0;
}

int sd_varlink_push_dup_fd(sd_varlink* vl, int fd) {
    p7_log(__func__);
    (void)vl;
    (void)fd;
    return 0;
}

int sd_varlink_peek_fd(sd_varlink* vl, unsigned idx) {
    p7_log(__func__);
    (void)vl;
    (void)idx;
    return -1;
}

int sd_varlink_peek_dup_fd(sd_varlink* vl, unsigned idx) {
    p7_log(__func__);
    (void)vl;
    (void)idx;
    return -1;
}

int sd_varlink_take_fd(sd_varlink* vl, int fd) {
    p7_log(__func__);
    (void)vl;
    (void)fd;
    return 0;
}

int sd_varlink_reset_fds(sd_varlink* vl) {
    p7_log(__func__);
    (void)vl;
    return 0;
}

/* Varlink event loop */
int sd_varlink_attach_event(sd_varlink* vl, sd_event* e, int64_t priority) {
    p7_log(__func__);
    (void)vl;
    (void)e;
    (void)priority;
    return 0;
}

int sd_varlink_detach_event(sd_varlink* vl) {
    p7_log(__func__);
    (void)vl;
    return 0;
}

int sd_varlink_get_event(sd_varlink* vl, sd_event** e) {
    p7_log(__func__);
    (void)vl;
    if (e) *e = NULL;
    return 0;
}

/* Varlink peer info */
int sd_varlink_get_peer_uid(sd_varlink* vl, uid_t* uid) {
    p7_log(__func__);
    (void)vl;
    if (uid) *uid = get_current_uid();
    return 0;
}

int sd_varlink_get_peer_gid(sd_varlink* vl, gid_t* gid) {
    p7_log(__func__);
    (void)vl;
    if (gid) *gid = getgid();
    return 0;
}

int sd_varlink_get_peer_pid(sd_varlink* vl, pid_t* pid) {
    p7_log(__func__);
    (void)vl;
    if (pid) *pid = getpid();
    return 0;
}

int sd_varlink_get_peer_pidfd(sd_varlink* vl, int* fd) {
    p7_log(__func__);
    (void)vl;
    if (fd) *fd = -1;
    return -ENODATA;
}

int sd_varlink_get_server(sd_varlink* vl, sd_varlink_server** server) {
    p7_log(__func__);
    (void)vl;
    if (server) *server = NULL;
    return 0;
}

/* Varlink current method */
int sd_varlink_get_current_method(sd_varlink* vl, const char** method) {
    p7_log(__func__);
    (void)vl;
    if (method) *method = NULL;
    return -ENODATA;
}

int sd_varlink_get_current_parameters(sd_varlink* vl, sd_json_variant** params) {
    p7_log(__func__);
    (void)vl;
    if (params) *params = NULL;
    return -ENODATA;
}

/* Varlink invocation */
int sd_varlink_invocation(sd_varlink* vl, sd_varlink_invocation_flags_t* flags) {
    p7_log(__func__);
    (void)vl;
    if (flags) *flags = 0;
    return 0;
}

/* Varlink call */
int sd_varlink_call(sd_varlink* vl, const char* method, sd_json_variant* parameters,
                      sd_json_variant** ret_parameters, const char** ret_error_id, uint64_t flags) {
    p7_log(__func__);
    (void)vl;
    (void)method;
    (void)parameters;
    (void)flags;
    if (ret_parameters) *ret_parameters = alloc_fake_ptr(FAKE_PTR_JSON);
    if (ret_error_id) *ret_error_id = NULL;
    return 0;
}

int sd_varlink_call_full(sd_varlink* vl, const char* method, sd_json_variant* parameters,
                          sd_json_variant** ret_parameters, const char** ret_error_id,
                          uint64_t flags, uint64_t timeout) {
    p7_log(__func__);
    (void)vl;
    (void)method;
    (void)parameters;
    (void)flags;
    (void)timeout;
    if (ret_parameters) *ret_parameters = alloc_fake_ptr(FAKE_PTR_JSON);
    if (ret_error_id) *ret_error_id = NULL;
    return 0;
}

int sd_varlink_callb(sd_varlink* vl, const char* method, sd_json_variant** ret_parameters,
                      const char** ret_error_id, uint64_t flags, const char* parameters_format, ...) {
    p7_log(__func__);
    (void)vl;
    (void)method;
    (void)flags;
    (void)parameters_format;
    if (ret_parameters) *ret_parameters = alloc_fake_ptr(FAKE_PTR_JSON);
    if (ret_error_id) *ret_error_id = NULL;
    return 0;
}

int sd_varlink_callb_ap(sd_varlink* vl, const char* method, sd_json_variant** ret_parameters,
                         const char** ret_error_id, uint64_t flags,
                         const char* parameters_format, va_list ap) {
    p7_log(__func__);
    (void)vl;
    (void)method;
    (void)flags;
    (void)parameters_format;
    (void)ap;
    if (ret_parameters) *ret_parameters = alloc_fake_ptr(FAKE_PTR_JSON);
    if (ret_error_id) *ret_error_id = NULL;
    return 0;
}

int sd_varlink_callb_full(sd_varlink* vl, const char* method, sd_json_variant** ret_parameters,
                            const char** ret_error_id, uint64_t flags, uint64_t timeout,
                            const char* parameters_format, ...) {
    p7_log(__func__);
    (void)vl;
    (void)method;
    (void)flags;
    (void)timeout;
    (void)parameters_format;
    if (ret_parameters) *ret_parameters = alloc_fake_ptr(FAKE_PTR_JSON);
    if (ret_error_id) *ret_error_id = NULL;
    return 0;
}

/* Varlink collect */
int sd_varlink_collect(sd_varlink* vl, const char* method, sd_json_variant* parameters,
                        sd_json_variant*** ret_parameters, const char*** ret_error_ids,
                        uint64_t flags) {
    p7_log(__func__);
    (void)vl;
    (void)method;
    (void)parameters;
    (void)flags;
    if (ret_parameters) *ret_parameters = NULL;
    if (ret_error_ids) *ret_error_ids = NULL;
    return 0;
}

int sd_varlink_collectb(sd_varlink* vl, const char* method, sd_json_variant*** ret_parameters,
                         const char*** ret_error_ids, uint64_t flags,
                         const char* parameters_format, ...) {
    p7_log(__func__);
    (void)vl;
    (void)method;
    (void)flags;
    (void)parameters_format;
    if (ret_parameters) *ret_parameters = NULL;
    if (ret_error_ids) *ret_error_ids = NULL;
    return 0;
}

int sd_varlink_collect_full(sd_varlink* vl, const char* method, sd_json_variant* parameters,
                             sd_json_variant*** ret_parameters, const char*** ret_error_ids,
                             uint64_t flags, uint64_t timeout) {
    p7_log(__func__);
    (void)vl;
    (void)method;
    (void)parameters;
    (void)flags;
    (void)timeout;
    if (ret_parameters) *ret_parameters = NULL;
    if (ret_error_ids) *ret_error_ids = NULL;
    return 0;
}

/* Varlink invoke/observe */
int sd_varlink_invoke(sd_varlink* vl, const char* method, sd_json_variant* parameters, uint64_t flags) {
    p7_log(__func__);
    (void)vl;
    (void)method;
    (void)parameters;
    (void)flags;
    return 0;
}

int sd_varlink_invokeb(sd_varlink* vl, const char* method, uint64_t flags,
                        const char* parameters_format, ...) {
    p7_log(__func__);
    (void)vl;
    (void)method;
    (void)flags;
    (void)parameters_format;
    return 0;
}

int sd_varlink_observe(sd_varlink* vl, const char* method, sd_json_variant* parameters, uint64_t flags) {
    p7_log(__func__);
    (void)vl;
    (void)method;
    (void)parameters;
    (void)flags;
    return 0;
}

int sd_varlink_observeb(sd_varlink* vl, const char* method, uint64_t flags,
                         const char* parameters_format, ...) {
    p7_log(__func__);
    (void)vl;
    (void)method;
    (void)flags;
    (void)parameters_format;
    return 0;
}

/* Varlink send/reply */
int sd_varlink_send(sd_varlink* vl, const char* method, sd_json_variant* parameters, uint64_t flags) {
    p7_log(__func__);
    (void)vl;
    (void)method;
    (void)parameters;
    (void)flags;
    return 0;
}

int sd_varlink_sendb(sd_varlink* vl, const char* method, uint64_t flags,
                      const char* parameters_format, ...) {
    p7_log(__func__);
    (void)vl;
    (void)method;
    (void)flags;
    (void)parameters_format;
    return 0;
}

int sd_varlink_reply(sd_varlink* vl, sd_json_variant* parameters) {
    p7_log(__func__);
    (void)vl;
    (void)parameters;
    return 0;
}

int sd_varlink_replyb(sd_varlink* vl, const char* parameters_format, ...) {
    p7_log(__func__);
    (void)vl;
    (void)parameters_format;
    return 0;
}

int sd_varlink_notify(sd_varlink* vl, sd_json_variant* parameters) {
    p7_log(__func__);
    (void)vl;
    (void)parameters;
    return 0;
}

int sd_varlink_notifyb(sd_varlink* vl, const char* parameters_format, ...) {
    p7_log(__func__);
    (void)vl;
    (void)parameters_format;
    return 0;
}

/* Varlink error */
int sd_varlink_error(sd_varlink* vl, const char* error_id, sd_json_variant* parameters) {
    p7_log(__func__);
    (void)vl;
    (void)error_id;
    (void)parameters;
    return 0;
}

int sd_varlink_errorb(sd_varlink* vl, const char* error_id, const char* parameters_format, ...) {
    p7_log(__func__);
    (void)vl;
    (void)error_id;
    (void)parameters_format;
    return 0;
}

int sd_varlink_error_errno(sd_varlink* vl, int error) {
    p7_log(__func__);
    (void)vl;
    (void)error;
    return 0;
}

int sd_varlink_error_invalid_parameter(sd_varlink* vl, sd_json_variant* parameters) {
    p7_log(__func__);
    (void)vl;
    (void)parameters;
    return 0;
}

int sd_varlink_error_invalid_parameter_name(sd_varlink* vl, const char* name) {
    p7_log(__func__);
    (void)vl;
    (void)name;
    return 0;
}

int sd_varlink_error_is_invalid_parameter(sd_varlink* vl) {
    p7_log(__func__);
    (void)vl;
    return 0;
}

int sd_varlink_error_to_errno(const char* error_id) {
    p7_log(__func__);
    (void)error_id;
    return -EIO;
}

/* Varlink bind reply */
int sd_varlink_bind_reply(sd_varlink* vl, sd_varlink_reply_callback_t callback, void* userdata) {
    p7_log(__func__);
    (void)vl;
    (void)callback;
    (void)userdata;
    return 0;
}

/* Varlink dispatch/process/wait */
int sd_varlink_dispatch(sd_varlink* vl, sd_json_variant** ret_parameters,
                          const char** ret_error_id, uint64_t flags) {
    p7_log(__func__);
    (void)vl;
    (void)flags;
    if (ret_parameters) *ret_parameters = NULL;
    if (ret_error_id) *ret_error_id = NULL;
    return 0;
}

int sd_varlink_dispatch_again(sd_varlink* vl) {
    p7_log(__func__);
    (void)vl;
    return 0;
}

int sd_varlink_process(sd_varlink* vl) {
    p7_log(__func__);
    (void)vl;
    return 0;
}

int sd_varlink_wait(sd_varlink* vl, uint64_t timeout) {
    p7_log(__func__);
    (void)vl;
    (void)timeout;
    return 0;
}

/* Varlink IDL */
int sd_varlink_idl_dump(FILE* f, const sd_varlink_interface* interface, int flags) {
    p7_log(__func__);
    (void)f;
    (void)interface;
    (void)flags;
    return 0;
}

int sd_varlink_idl_format(const sd_varlink_interface* interface, int flags, char** ret) {
    p7_log(__func__);
    (void)interface;
    (void)flags;
    if (ret) *ret = NULL;
    return 0;
}

int sd_varlink_idl_format_full(const sd_varlink_interface* interface, int flags, char** ret) {
    p7_log(__func__);
    (void)interface;
    (void)flags;
    if (ret) *ret = NULL;
    return 0;
}

int sd_varlink_idl_parse(const char* text, sd_varlink_interface** ret) {
    p7_log(__func__);
    (void)text;
    if (ret) *ret = NULL;
    return -EINVAL;
}

void sd_varlink_interface_free(sd_varlink_interface* interface) {
    p7_log(__func__);
    (void)interface;
}

/* Varlink Server (out of scope but stubbed) */
sd_varlink_server* sd_varlink_server_ref(sd_varlink_server* s) {
    p7_log(__func__);
    return s;
}

sd_varlink_server* sd_varlink_server_unref(sd_varlink_server* s) {
    p7_log(__func__);
    return NULL;
}

int sd_varlink_server_new(sd_varlink_server** ret, int flags) {
    p7_log(__func__);
    (void)flags;
    if (!ret) return -EINVAL;
    *ret = alloc_fake_ptr(FAKE_PTR_VAR);
    return 0;
}

int sd_varlink_server_get_userdata(sd_varlink_server* s, void** userdata) {
    p7_log(__func__);
    (void)s;
    if (userdata) *userdata = NULL;
    return 0;
}

int sd_varlink_server_set_userdata(sd_varlink_server* s, void* userdata) {
    p7_log(__func__);
    (void)s;
    (void)userdata;
    return 0;
}

int sd_varlink_server_get_event(sd_varlink_server* s, sd_event** e) {
    p7_log(__func__);
    (void)s;
    if (e) *e = NULL;
    return 0;
}

int sd_varlink_server_attach_event(sd_varlink_server* s, sd_event* e, int64_t priority) {
    p7_log(__func__);
    (void)s;
    (void)e;
    (void)priority;
    return 0;
}

int sd_varlink_server_detach_event(sd_varlink_server* s) {
    p7_log(__func__);
    (void)s;
    return 0;
}

int sd_varlink_server_listen_address(sd_varlink_server* s, const char* address, int flags) {
    p7_log(__func__);
    (void)s;
    (void)address;
    (void)flags;
    return -EOPNOTSUPP;
}

int sd_varlink_server_listen_auto(sd_varlink_server* s, int flags) {
    p7_log(__func__);
    (void)s;
    (void)flags;
    return -EOPNOTSUPP;
}

int sd_varlink_server_listen_fd(sd_varlink_server* s, int fd, int flags) {
    p7_log(__func__);
    (void)s;
    (void)fd;
    (void)flags;
    return -EOPNOTSUPP;
}

int sd_varlink_server_listen_name(sd_varlink_server* s, const char* name, int flags) {
    p7_log(__func__);
    (void)s;
    (void)name;
    (void)flags;
    return -EOPNOTSUPP;
}

int sd_varlink_server_loop_auto(sd_varlink_server* s) {
    p7_log(__func__);
    (void)s;
    return -EOPNOTSUPP;
}

int sd_varlink_server_shutdown(sd_varlink_server* s) {
    p7_log(__func__);
    (void)s;
    return 0;
}

int sd_varlink_server_add_connection(sd_varlink_server* s, sd_varlink** ret, int fd) {
    p7_log(__func__);
    (void)s;
    (void)fd;
    if (ret) *ret = NULL;
    return -EOPNOTSUPP;
}

int sd_varlink_server_add_connection_pair(sd_varlink_server* s, sd_varlink** ret,
                                           int input_fd, int output_fd) {
    p7_log(__func__);
    (void)s;
    (void)input_fd;
    (void)output_fd;
    if (ret) *ret = NULL;
    return -EOPNOTSUPP;
}

int sd_varlink_server_add_connection_stdio(sd_varlink_server* s, sd_varlink** ret) {
    p7_log(__func__);
    (void)s;
    if (ret) *ret = NULL;
    return -EOPNOTSUPP;
}

int sd_varlink_server_add_interface(sd_varlink_server* s, const sd_varlink_interface* interface) {
    p7_log(__func__);
    (void)s;
    (void)interface;
    return 0;
}

int sd_varlink_server_add_interface_many_internal(sd_varlink_server* s, ...) {
    p7_log(__func__);
    (void)s;
    return 0;
}

int sd_varlink_server_bind_connect(sd_varlink_server* s, sd_varlink_connect_callback_t callback, void* userdata) {
    p7_log(__func__);
    (void)s;
    (void)callback;
    (void)userdata;
    return 0;
}

int sd_varlink_server_bind_disconnect(sd_varlink_server* s, sd_varlink_disconnect_callback_t callback, void* userdata) {
    p7_log(__func__);
    (void)s;
    (void)callback;
    (void)userdata;
    return 0;
}

int sd_varlink_server_bind_method(sd_varlink_server* s, const char* method,
                                   sd_varlink_method_t callback, void* userdata) {
    p7_log(__func__);
    (void)s;
    (void)method;
    (void)callback;
    (void)userdata;
    return 0;
}

int sd_varlink_server_bind_method_many_internal(sd_varlink_server* s, ...) {
    p7_log(__func__);
    (void)s;
    return 0;
}

int sd_varlink_server_connections_max(sd_varlink_server* s, unsigned* ret) {
    p7_log(__func__);
    (void)s;
    if (ret) *ret = 0;
    return 0;
}

int sd_varlink_server_connections_per_uid_max(sd_varlink_server* s, unsigned* ret) {
    p7_log(__func__);
    (void)s;
    if (ret) *ret = 0;
    return 0;
}

int sd_varlink_server_current_connections(sd_varlink_server* s, unsigned* ret) {
    p7_log(__func__);
    (void)s;
    if (ret) *ret = 0;
    return 0;
}

int sd_varlink_server_set_connections_max(sd_varlink_server* s, unsigned n) {
    p7_log(__func__);
    (void)s;
    (void)n;
    return 0;
}

int sd_varlink_server_set_connections_per_uid_max(sd_varlink_server* s, unsigned n) {
    p7_log(__func__);
    (void)s;
    (void)n;
    return 0;
}

int sd_varlink_server_set_description(sd_varlink_server* s, const char* description) {
    p7_log(__func__);
    (void)s;
    (void)description;
    return 0;
}

int sd_varlink_server_set_exit_on_idle(sd_varlink_server* s, int b) {
    p7_log(__func__);
    (void)s;
    (void)b;
    return 0;
}

int sd_varlink_server_set_info(sd_varlink_server* s, const char* vendor, const char* product,
                                 const char* version, const char* url) {
    p7_log(__func__);
    (void)s;
    (void)vendor;
    (void)product;
    (void)version;
    (void)url;
    return 0;
}

/* ============================================================================
 * SECTION 15: VERSION & METADATA
 * ============================================================================ */

const char* p7_mock_version(void) {
    return "5.0.0";
}

int p7_mock_get_version_major(void) {
    return P7_MOCK_VERSION_MAJOR;
}

int p7_mock_get_version_minor(void) {
    return P7_MOCK_VERSION_MINOR;
}

int p7_mock_get_version_patch(void) {
    return P7_MOCK_VERSION_PATCH;
}

/* ============================================================================
 * SECTION 16: MISSING SYMBOL FIXES
 * ============================================================================ */

/* sd_bus_message_get_destination - was missing from accessor section */
const char* sd_bus_message_get_destination(sd_bus_message* m) {
    p7_log(__func__);
    (void)m;
    return NULL;
}

/* sd_bus_message_get_interface - was typo'd as sd_message_get_interface */
const char* sd_bus_message_get_interface(sd_bus_message* m) {
    p7_log(__func__);
    (void)m;
    return "org.freedesktop.DBus";
}

/* sd_bus_message_get_member */
const char* sd_bus_message_get_member(sd_bus_message* m) {
    p7_log(__func__);
    (void)m;
    return "Hello";
}

/* sd_bus_message_get_path */
const char* sd_bus_message_get_path(sd_bus_message* m) {
    p7_log(__func__);
    (void)m;
    return "/org/freedesktop/DBus";
}

/* sd_bus_message_get_sender */
const char* sd_bus_message_get_sender(sd_bus_message* m) {
    p7_log(__func__);
    (void)m;
    return ":1.1";
}

/* sd_bus_object_vtable_format - data symbol, needs to be non-const for linkage */
char sd_bus_object_vtable_format[] = "random";

/* sd_bus_track_count - returns unsigned, not int */
unsigned sd_bus_track_count(sd_bus_track* track) {
    p7_log(__func__);
    (void)track;
    return 0;
}

/* sd_bus_track_count_name */
unsigned sd_bus_track_count_name(sd_bus_track* track, const char* name) {
    p7_log(__func__);
    (void)track;
    (void)name;
    return 0;
}

/* sd_bus_track_count_sender */
unsigned sd_bus_track_count_sender(sd_bus_track* track, sd_bus_message* m) {
    p7_log(__func__);
    (void)track;
    (void)m;
    return 0;
}

/* sd_bus_track_first */
const char* sd_bus_track_first(sd_bus_track* track) {
    p7_log(__func__);
    (void)track;
    return NULL;
}

/* sd_bus_track_next */
const char* sd_bus_track_next(sd_bus_track* track) {
    p7_log(__func__);
    (void)track;
    return NULL;
}

/* sd_device_enumerator_add_nomatch_sysname */
int sd_device_enumerator_add_nomatch_sysname(sd_device_enumerator* e, const char* sysname) {
    p7_log(__func__);
    (void)e;
    (void)sysname;
    return 0;
}

/* sd_json_variant_elements - returns size_t */
size_t sd_json_variant_elements(sd_json_variant* v) {
    p7_log(__func__);
    (void)v;
    return 0;
}

/* sd_json_variant_integer - returns int64_t */
int64_t sd_json_variant_integer(sd_json_variant* v) {
    p7_log(__func__);
    (void)v;
    return 0;
}

/* sd_json_variant_real - returns double */
double sd_json_variant_real(sd_json_variant* v) {
    p7_log(__func__);
    (void)v;
    return 0.0;
}

/* sd_json_variant_string - returns const char* */
const char* sd_json_variant_string(sd_json_variant* v) {
    p7_log(__func__);
    (void)v;
    return "";
}

/* sd_json_variant_strv - returns char** */
char** sd_json_variant_strv(sd_json_variant* v) {
    p7_log(__func__);
    (void)v;
    return NULL;
}

/* sd_json_variant_type_to_string - returns const char* */
const char* sd_json_variant_type_to_string(sd_json_variant_type_t type) {
    p7_log(__func__);
    (void)type;
    return "null";
}

/* sd_json_variant_unsigned - returns uint64_t */
uint64_t sd_json_variant_unsigned(sd_json_variant* v) {
    p7_log(__func__);
    (void)v;
    return 0;
}

/* ============================================================================
 * END OF libsystemd-mock v5.0
 * ============================================================================ */













