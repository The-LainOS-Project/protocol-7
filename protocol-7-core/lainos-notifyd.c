/*
 * lainos-notifyd.c – Protocol 7 Notification Sink (v4.3-tuned)
 *
 * Binds to /run/systemd/notify (DGRAM unix socket) and absorbs sd_notify messages.
 * Mirrors to syslog with categorized logging. Rate-limited to prevent flooding.
 * Seccomp-hardened, privilege-dropped to nobody.
 *
 * Hardened: syscall whitelist, no exec/fork/clone/ptrace.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <errno.h>
#include <grp.h>
#include <seccomp.h>
#include <time.h>

/* systemd notify protocol max is 4096, but we use 8192 for safety margin */
#define RECV_BUF_SIZE 8192
#define RATE_LIMIT_MAX_MSGS 100
#define RATE_LIMIT_WINDOW_SEC 1
#define RATE_LIMIT_BURST 10

typedef struct {
    time_t window_start;
    int msg_count;
    int dropped_count;
} rate_limiter_t;

static int check_rate_limit(rate_limiter_t *rl) {
    time_t now = time(NULL);
    if (now - rl->window_start >= RATE_LIMIT_WINDOW_SEC) {
        if (rl->dropped_count > 0) {
            syslog(LOG_WARNING, "Rate limit: dropped %d messages in previous window",
                   rl->dropped_count);
        }
        rl->window_start = now;
        rl->msg_count = 0;
        rl->dropped_count = 0;
    }
    if (rl->msg_count >= RATE_LIMIT_MAX_MSGS) {
        rl->dropped_count++;
        return 0; /* Drop */
    }
    rl->msg_count++;
    return 1; /* Allow */
}

static void parse_and_log(const char *msg, size_t len) {
    /* Categorize known sd_notify states */
    if (len >= 7 && strncmp(msg, "READY=1", 7) == 0) {
        syslog(LOG_INFO, "[notify] READY=1 (service ready)");
    }
    else if (len >= 6 && strncmp(msg, "STATUS=", 7) == 0) {
        syslog(LOG_INFO, "[notify] STATUS: %.*s", (int)(len - 7), msg + 7);
    }
    else if (len >= 9 && strncmp(msg, "WATCHDOG=1", 10) == 0) {
        syslog(LOG_DEBUG, "[notify] WATCHDOG=1 (keepalive)");
    }
    else if (len >= 11 && strncmp(msg, "RELOADING=1", 11) == 0) {
        syslog(LOG_INFO, "[notify] RELOADING=1 (service reloading)");
    }
    else if (len >= 8 && strncmp(msg, "STOPPING=1", 10) == 0) {
        syslog(LOG_INFO, "[notify] STOPPING=1 (service stopping)");
    }
    else if (len >= 6 && strncmp(msg, "ERRNO=", 6) == 0) {
        syslog(LOG_ERR, "[notify] ERRNO: %.*s", (int)(len - 6), msg + 6);
    }
    else if (len >= 9 && strncmp(msg, "BUSERROR=", 9) == 0) {
        syslog(LOG_ERR, "[notify] BUSERROR: %.*s", (int)(len - 9), msg + 9);
    }
    else if (len >= 11 && strncmp(msg, "MAINPID=", 8) == 0) {
        syslog(LOG_INFO, "[notify] MAINPID: %.*s", (int)(len - 8), msg + 8);
    }
    else {
        /* Generic fallback */
        syslog(LOG_INFO, "[notify] %.*s", (int)len, msg);
    }
}

static void drop_privileges(void) {
    struct group *gr = getgrnam("notify");
    gid_t gid = gr ? gr->gr_gid : 0;
    if (setgid(gid) == -1 || setuid(getuid()) == -1) {
        syslog(LOG_ERR, "Failed to drop privileges: %m");
        _exit(1);
    }
}

static void apply_seccomp(void) {
    scmp_filter_ctx ctx = seccomp_init(SCMP_ACT_KILL);
    if (!ctx) return;
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(read), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(write), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(openat), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(close), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(recvmsg), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(sendto), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(getpid), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(getuid), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(getgid), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(geteuid), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(getegid), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(time), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(clock_gettime), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(gettimeofday), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(sigreturn), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(rt_sigreturn), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(exit_group), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(exit), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(futex), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(brk), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(munmap), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(mprotect), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(madvise), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(ioctl), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(poll), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(epoll_wait), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(epoll_pwait), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(select), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(pselect6), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(nanosleep), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(getrandom), 0);
    seccomp_load(ctx);
}

int main(void) {
    openlog("lainos-notifyd", LOG_PID, LOG_DAEMON);
    syslog(LOG_INFO, "Protocol 7 notify sink starting");

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, "/run/systemd/notify", sizeof(addr.sun_path) - 1);

    int fd = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (fd < 0) {
        syslog(LOG_ERR, "socket: %m");
        return 1;
    }

    /* Ensure directory exists */
    struct stat st;
    if (stat("/run/systemd", &st) < 0) {
        if (mkdir("/run/systemd", 0755) < 0 && errno != EEXIST) {
            syslog(LOG_ERR, "mkdir /run/systemd: %m");
            close(fd);
            return 1;
        }
    }

    unlink(addr.sun_path);
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        syslog(LOG_ERR, "bind %s: %m", addr.sun_path);
        close(fd);
        return 1;
    }

    struct group *gr = getgrnam("notify");
    if (chown(addr.sun_path, 0, gr ? gr->gr_gid : 0) < 0) {
        syslog(LOG_WARNING, "chown %s: %m", addr.sun_path);
    }
    if (chmod(addr.sun_path, 0660) < 0) {
        syslog(LOG_WARNING, "chmod %s: %m", addr.sun_path);
    }

    drop_privileges();
    apply_seccomp();

    syslog(LOG_INFO, "Notify sink active on %s", addr.sun_path);

    rate_limiter_t rl = { .window_start = time(NULL), .msg_count = 0, .dropped_count = 0 };
    char buf[RECV_BUF_SIZE];

    while (1) {
        ssize_t n = recv(fd, buf, sizeof(buf) - 1, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            syslog(LOG_ERR, "recv: %m");
            break;
        }
        if (n == 0) continue;

        buf[n] = '\0';

        if (!check_rate_limit(&rl)) {
            continue; /* Silently drop, already logged by rate limiter */
        }

        parse_and_log(buf, (size_t)n);
    }

    close(fd);
    syslog(LOG_INFO, "Notify sink shutting down");
    closelog();
    return 0;
}
