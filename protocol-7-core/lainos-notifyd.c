/*
 * lainos-notifyd.c – Protocol 7 Notify Socket Sink (sd_notify shim)
 *
 * Binds to /run/systemd/notify (DGRAM unix socket) and mirrors all received
 * notifications to syslog. Prevents startup failures in apps that try to
 * connect to NOTIFY_SOCKET but do not require real systemd.
 *
 * Hardened: early seccomp (unix socket only), privilege drop to nobody,
 *           idempotent socket setup, group 'notify' ownership (0660),
 *           fail-closed logging, no zombies.
 *
 * Compile:
 *   gcc -O2 -flto -s -Wall -pie -o /usr/libexec/lainos/lainos-notifyd lainos-notifyd.c -lseccomp
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <syslog.h>
#include <errno.h>
#include <pwd.h>
#include <grp.h>
#include <sys/prctl.h>
#include <seccomp.h>

#define SOCKET_PATH     "/run/systemd/notify"
#define SOCKET_DIR      "/run/systemd"
#define LOG_IDENT       "lainos-notifyd"
#define RECV_BUF_SIZE   2048
#define NOTIFY_GROUP    "notify"

static void die(const char *msg) {
    syslog(LOG_CRIT, "%s: %s", msg, strerror(errno));
    closelog();
    _exit(1);
}

static void apply_seccomp(void) {
    scmp_filter_ctx ctx = seccomp_init(SCMP_ACT_ERRNO(EPERM));
    if (!ctx) die("seccomp_init failed");

    const int allowed[] = {
        SCMP_SYS(read), SCMP_SYS(recvfrom), SCMP_SYS(recvmsg),
        SCMP_SYS(write), SCMP_SYS(sendto), SCMP_SYS(sendmsg),
        SCMP_SYS(socket), SCMP_SYS(bind), SCMP_SYS(listen),
        SCMP_SYS(setsockopt), SCMP_SYS(getsockopt),
        SCMP_SYS(close), SCMP_SYS(fstat), SCMP_SYS(fchmod),
        SCMP_SYS(mkdir), SCMP_SYS(unlink), SCMP_SYS(chown),
        SCMP_SYS(exit_group), SCMP_SYS(futex), SCMP_SYS(clock_gettime),
        SCMP_SYS(getpid), SCMP_SYS(getuid), SCMP_SYS(getgid)
    };

    for (size_t i = 0; i < sizeof(allowed)/sizeof(allowed[0]); i++)
        seccomp_rule_add(ctx, SCMP_ACT_ALLOW, allowed[i], 0);

    seccomp_rule_add(ctx, SCMP_ACT_KILL, SCMP_SYS(execve), 0);
    seccomp_rule_add(ctx, SCMP_ACT_KILL, SCMP_SYS(fork), 0);
    seccomp_rule_add(ctx, SCMP_ACT_KILL, SCMP_SYS(clone), 0);
    seccomp_rule_add(ctx, SCMP_ACT_KILL, SCMP_SYS(ptrace), 0);

    if (seccomp_load(ctx) < 0) die("seccomp_load failed");
    seccomp_release(ctx);
}

static void drop_privs(void) {
    struct passwd *pw = getpwnam("nobody");
    if (!pw) die("nobody user not found");

    setgroups(0, NULL);
    if (setgid(pw->pw_gid) == -1 || setuid(pw->pw_uid) == -1)
        die("privilege drop failed");

    prctl(PR_SET_DUMPABLE, 0);
}

static gid_t get_notify_gid(void) {
    struct group *grp = getgrnam(NOTIFY_GROUP);
    if (grp) return grp->gr_gid;

    syslog(LOG_WARNING, "Group '%s' not found – falling back to root gid", NOTIFY_GROUP);
    return 0;
}

int main(void) {
    openlog(LOG_IDENT, LOG_PID | LOG_CONS, LOG_DAEMON);

    if (getuid() != 0) die("must run as root");

    apply_seccomp();

    if (mkdir(SOCKET_DIR, 0755) == -1 && errno != EEXIST)
        die("cannot create socket directory");

    unlink(SOCKET_PATH);

    int fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (fd == -1) die("socket failed");

    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
        die("bind failed");

    mode_t mode = 0660;
    gid_t gid = get_notify_gid();

    if (chown(SOCKET_PATH, 0, gid) == -1 || chmod(SOCKET_PATH, mode) == -1)
        die("chown/chmod failed");

    drop_privs();

    syslog(LOG_INFO, "Notify sink active at %s (0660, gid %u)", SOCKET_PATH, (unsigned)gid);

    char buf[RECV_BUF_SIZE];
    while (1) {
        ssize_t n = recv(fd, buf, sizeof(buf) - 1, 0);
        if (n > 0) {
            buf[n] = '\0';
            syslog(LOG_INFO, "[NOTIFY] %s", buf);
        } else if (n == -1 && errno != EINTR) {
            syslog(LOG_WARNING, "recv error: %s", strerror(errno));
        }
    }

    close(fd);
    closelog();
    return 0;
}
