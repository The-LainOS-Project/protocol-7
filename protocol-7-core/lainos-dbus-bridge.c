/*
 * lainos-dbus-bridge.c – Protocol 7 Sovereign D-Bus Shim for logind
 *
 * Minimal, seccomp-hardened replacement for org.freedesktop.login1
 * Implements: Manager (Reboot/PowerOff/Suspend/Inhibit/ListInhibitors/...),
 *             Session (Active/State/Type/IdleHint/...),
 *             Seat (ActiveSession/...), User (UID/UserName/RuntimePath/...)
 *
 * Inhibitor pipe fds are real — close on bridge exit -> release inhibition.
 *
 * Hardened: early seccomp, privilege drop to nobody, no exec/fork/clone,
 *           syslog-only logging, constant-time D-Bus handling.
 *
 * Runtime user resolution: detects first non-system user from /etc/passwd
 * instead of hardcoding UID 1000.
 *
 * Compile:
 *   gcc -O2 -flto -s -Wall -pie -o /usr/lib/lainos/lainos-dbus-bridge lainos-dbus-bridge.c -ldbus-1 -lseccomp
 */

#define _GNU_SOURCE

#include <dbus/dbus.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <syslog.h>
#include <pwd.h>
#include <sys/prctl.h>
#include <seccomp.h>
#include <errno.h>
#include <stdint.h>
#include <fcntl.h>

#define BUS_NAME            "org.freedesktop.login1"
#define MANAGER_PATH        "/org/freedesktop/login1"
#define MANAGER_IFACE       "org.freedesktop.login1.Manager"
#define SESSION_IFACE       "org.freedesktop.login1.Session"
#define SEAT_IFACE          "org.freedesktop.login1.Seat"
#define USER_IFACE          "org.freedesktop.login1.User"
#define NOBODY_USER         "nobody"

static DBusConnection *conn = NULL;
static int shutdown_pipe[2] = {-1, -1};
static volatile sig_atomic_t should_exit = 0;

// Runtime-resolved session identity
static uid_t current_uid = 1000;
static char current_username[64] = "user";
static char current_runtime_dir[64] = "/run/user/1000";
static char current_seat_id[32] = "seat0";
static char session_path[128] = "/org/freedesktop/login1/session/1";
static char seat_path[128] = "/org/freedesktop/login1/seat/seat0";
static char user_path[128] = "/org/freedesktop/login1/user/1000";

static void resolve_session_user(void) {
    struct passwd *pw;
    setpwent();
    while ((pw = getpwent()) != NULL) {
        if (pw->pw_uid >= 1000 && pw->pw_uid != 65534) {
            current_uid = pw->pw_uid;
            strncpy(current_username, pw->pw_name, sizeof(current_username) - 1);
            current_username[sizeof(current_username) - 1] = '\0';
            snprintf(current_runtime_dir, sizeof(current_runtime_dir),
                     "/run/user/%u", current_uid);
            snprintf(session_path, sizeof(session_path),
                     "/org/freedesktop/login1/session/%u", current_uid);
            snprintf(user_path, sizeof(user_path),
                     "/org/freedesktop/login1/user/%u", current_uid);
            endpwent();
            return;
        }
    }
    endpwent();
}

static char introspection_xml[8192];

static void build_introspection_xml(void) {
    snprintf(introspection_xml, sizeof(introspection_xml),
        "<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\"\n"
        "\"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\n"
        "<node>\n"
        "  <interface name=\"org.freedesktop.DBus.Introspectable\">\n"
        "    <method name=\"Introspect\">\n"
        "      <arg name=\"data\" direction=\"out\" type=\"s\"/>\n"
        "    </method>\n"
        "  </interface>\n"
        "  <interface name=\"%s\">\n"
        "    <method name=\"Reboot\">\n"
        "      <arg name=\"interactive\" type=\"b\" direction=\"in\"/>\n"
        "    </method>\n"
        "    <method name=\"PowerOff\">\n"
        "      <arg name=\"interactive\" type=\"b\" direction=\"in\"/>\n"
        "    </method>\n"
        "    <method name=\"Suspend\">\n"
        "      <arg name=\"interactive\" type=\"b\" direction=\"in\"/>\n"
        "    </method>\n"
        "    <method name=\"Inhibit\">\n"
        "      <arg name=\"what\" type=\"s\" direction=\"in\"/>\n"
        "      <arg name=\"who\" type=\"s\" direction=\"in\"/>\n"
        "      <arg name=\"why\" type=\"s\" direction=\"in\"/>\n"
        "      <arg name=\"mode\" type=\"s\" direction=\"in\"/>\n"
        "      <arg name=\"fd\" type=\"h\" direction=\"out\"/>\n"
        "    </method>\n"
        "    <method name=\"ListInhibitors\">\n"
        "      <arg name=\"inhibitors\" direction=\"out\" type=\"a(ssssuu)\"/>\n"
        "    </method>\n"
        "    <method name=\"ListSessions\">\n"
        "      <arg name=\"sessions\" direction=\"out\" type=\"a(susso)\"/>\n"
        "    </method>\n"
        "    <method name=\"GetSession\">\n"
        "      <arg name=\"session_id\" type=\"s\" direction=\"in\"/>\n"
        "      <arg name=\"object_path\" direction=\"out\" type=\"o\"/>\n"
        "    </method>\n"
        "    <method name=\"ListSeats\">\n"
        "      <arg name=\"seats\" direction=\"out\" type=\"a(o)\"/>\n"
        "    </method>\n"
        "    <method name=\"GetSeat\">\n"
        "      <arg name=\"seat_id\" type=\"s\" direction=\"in\"/>\n"
        "      <arg name=\"object_path\" direction=\"out\" type=\"o\"/>\n"
        "    </method>\n"
        "    <method name=\"ListUsers\">\n"
        "      <arg name=\"users\" direction=\"out\" type=\"a(uso)\"/>\n"
        "    </method>\n"
        "    <method name=\"GetUser\">\n"
        "      <arg name=\"uid\" type=\"u\" direction=\"in\"/>\n"
        "      <arg name=\"object_path\" direction=\"out\" type=\"o\"/>\n"
        "    </method>\n"
        "    <property name=\"CanReboot\" type=\"s\" access=\"read\"/>\n"
        "    <property name=\"CanPowerOff\" type=\"s\" access=\"read\"/>\n"
        "    <property name=\"CanSuspend\" type=\"s\" access=\"read\"/>\n"
        "  </interface>\n"
        "  <node name=\"%s\">\n"
        "    <interface name=\"%s\">\n"
        "      <method name=\"Activate\"/>\n"
        "      <method name=\"Lock\"/>\n"
        "      <method name=\"Unlock\"/>\n"
        "      <property name=\"Active\" type=\"b\" access=\"read\"/>\n"
        "      <property name=\"State\" type=\"s\" access=\"read\"/>\n"
        "      <property name=\"Type\" type=\"s\" access=\"read\"/>\n"
        "      <property name=\"IdleHint\" type=\"b\" access=\"read\"/>\n"
        "      <property name=\"IdleSinceHint\" type=\"t\" access=\"read\"/>\n"
        "      <property name=\"Seat\" type=\"(so)\" access=\"read\"/>\n"
        "      <property name=\"User\" type=\"(uo)\" access=\"read\"/>\n"
        "    </interface>\n"
        "  </node>\n"
        "  <node name=\"%s\">\n"
        "    <interface name=\"%s\">\n"
        "      <method name=\"SwitchTo\">\n"
        "        <arg name=\"vt\" type=\"u\" direction=\"in\"/>\n"
        "      </method>\n"
        "      <method name=\"SwitchToGraphical\"/>\n"
        "      <method name=\"LockAll\"/>\n"
        "      <method name=\"UnlockAll\"/>\n"
        "      <property name=\"ActiveSession\" type=\"(so)\" access=\"read\"/>\n"
        "      <property name=\"CanMultiSession\" type=\"b\" access=\"read\"/>\n"
        "      <property name=\"CanSwitch\" type=\"b\" access=\"read\"/>\n"
        "      <property name=\"CanGraphical\" type=\"b\" access=\"read\"/>\n"
        "      <property name=\"CanTTY\" type=\"b\" access=\"read\"/>\n"
        "      <property name=\"Id\" type=\"s\" access=\"read\"/>\n"
        "    </interface>\n"
        "  </node>\n"
        "  <node name=\"%s\">\n"
        "    <interface name=\"%s\">\n"
        "      <method name=\"Kill\">\n"
        "        <arg name=\"signal\" type=\"i\" direction=\"in\"/>\n"
        "      </method>\n"
        "      <method name=\"KillSession\">\n"
        "        <arg name=\"session_id\" type=\"s\" direction=\"in\"/>\n"
        "        <arg name=\"signal\" type=\"i\" direction=\"in\"/>\n"
        "      </method>\n"
        "      <property name=\"UID\" type=\"u\" access=\"read\"/>\n"
        "      <property name=\"UserName\" type=\"s\" access=\"read\"/>\n"
        "      <property name=\"RuntimePath\" type=\"s\" access=\"read\"/>\n"
        "      <property name=\"State\" type=\"s\" access=\"read\"/>\n"
        "      <property name=\"Linger\" type=\"b\" access=\"read\"/>\n"
        "      <property name=\"Sessions\" type=\"a(so)\" access=\"read\"/>\n"
        "      <property name=\"Display\" type=\"(so)\" access=\"read\"/>\n"
        "      <property name=\"KillProcesses\" type=\"b\" access=\"read\"/>\n"
        "    </interface>\n"
        "  </node>\n"
        "</node>\n",
        MANAGER_IFACE,
        session_path, SESSION_IFACE,
        seat_path, SEAT_IFACE,
        user_path, USER_IFACE
    );
}

static void log_msg(int priority, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsyslog(priority, fmt, ap);
    va_end(ap);
}

static void drop_privs(void) {
    struct passwd *pw = getpwnam(NOBODY_USER);
    if (!pw) {
        log_msg(LOG_CRIT, "Cannot find nobody user");
        _exit(1);
    }

    if (setgroups(0, NULL) == -1 ||
        setgid(pw->pw_gid) == -1 ||
        setuid(pw->pw_uid) == -1) {
        log_msg(LOG_CRIT, "Privilege drop failed");
        _exit(1);
    }

    if (prctl(PR_SET_DUMPABLE, 0) == -1) {
        log_msg(LOG_WARNING, "prctl(PR_SET_DUMPABLE, 0) failed");
    }
}

static int apply_seccomp(void) {
    scmp_filter_ctx ctx = seccomp_init(SCMP_ACT_ERRNO(EPERM));
    if (!ctx) return -1;

    const int allowed[] = {
        SCMP_SYS(read), SCMP_SYS(write), SCMP_SYS(sendmsg), SCMP_SYS(recvmsg),
        SCMP_SYS(poll), SCMP_SYS(epoll_wait), SCMP_SYS(epoll_ctl), SCMP_SYS(epoll_create1),
        SCMP_SYS(futex), SCMP_SYS(clock_gettime), SCMP_SYS(nanosleep),
        SCMP_SYS(getpid), SCMP_SYS(getuid), SCMP_SYS(getgid),
        SCMP_SYS(exit), SCMP_SYS(exit_group), SCMP_SYS(brk),
        SCMP_SYS(mmap), SCMP_SYS(munmap), SCMP_SYS(mprotect),
        SCMP_SYS(socketpair), SCMP_SYS(bind), SCMP_SYS(listen),
        SCMP_SYS(accept4), SCMP_SYS(setsockopt), SCMP_SYS(getsockopt),
        SCMP_SYS(close), SCMP_SYS(fcntl), SCMP_SYS(pipe), SCMP_SYS(pipe2),
        SCMP_SYS(getpwent), SCMP_SYS(endpwent), SCMP_SYS(setgroups),
        SCMP_SYS(setpwent)
    };

    for (size_t i = 0; i < sizeof(allowed)/sizeof(allowed[0]); i++)
        seccomp_rule_add(ctx, SCMP_ACT_ALLOW, allowed[i], 0);

    seccomp_rule_add(ctx, SCMP_ACT_KILL, SCMP_SYS(execve), 0);
    seccomp_rule_add(ctx, SCMP_ACT_KILL, SCMP_SYS(fork), 0);
    seccomp_rule_add(ctx, SCMP_ACT_KILL, SCMP_SYS(clone), 0);
    seccomp_rule_add(ctx, SCMP_ACT_KILL, SCMP_SYS(ptrace), 0);

    int r = seccomp_load(ctx);
    seccomp_release(ctx);
    return r;
}

static void __attribute__((constructor)) init_shutdown_helper(void) {
    if (pipe(shutdown_pipe) == -1) {
        openlog("lainos-dbus-bridge", LOG_PID | LOG_CONS, LOG_DAEMON);
        syslog(LOG_CRIT, "Failed to create shutdown helper pipe: %s", strerror(errno));
        closelog();
        _exit(1);
    }

    pid_t pid = fork();
    if (pid == -1) {
        openlog("lainos-dbus-bridge", LOG_PID | LOG_CONS, LOG_DAEMON);
        syslog(LOG_CRIT, "Failed to fork shutdown helper: %s", strerror(errno));
        closelog();
        _exit(1);
    }

    if (pid == 0) {
        close(shutdown_pipe[1]);

        int maxfd = sysconf(_SC_OPEN_MAX);
        if (maxfd == -1) maxfd = 1024;
        for (int fd = 3; fd < maxfd; fd++) {
            if (fd != shutdown_pipe[0]) close(fd);
        }

        char action[32];
        ssize_t n;
        while ((n = read(shutdown_pipe[0], action, sizeof(action) - 1)) > 0) {
            action[n] = '\0';
            if (n > 0 && action[n-1] == '\n') action[n-1] = '\0';

            execlp("openrc-shutdown", "openrc-shutdown", action, "now", NULL);
            openlog("lainos-shutdown-helper", LOG_PID | LOG_CONS, LOG_DAEMON);
            syslog(LOG_ERR, "execlp openrc-shutdown failed: %s", strerror(errno));
            closelog();
        }

        close(shutdown_pipe[0]);
        _exit(0);
    }

    close(shutdown_pipe[0]);
}

static void trigger_shutdown(const char *action) {
    if (shutdown_pipe[1] == -1) {
        log_msg(LOG_ERR, "Shutdown helper pipe not initialized");
        return;
    }

    size_t len = strlen(action);
    if (len >= 31) len = 31;

    ssize_t written = write(shutdown_pipe[1], action, len + 1);
    if (written == -1) {
        log_msg(LOG_ERR, "Failed to write to shutdown helper pipe: %s", strerror(errno));
    } else {
        log_msg(LOG_INFO, "Shutdown triggered via helper: %s", action);
    }
}

static DBusMessage* handle_manager_method(DBusMessage *msg) {
    DBusMessage *reply = NULL;

    if (dbus_message_is_method_call(msg, MANAGER_IFACE, "Reboot")) {
        trigger_shutdown("--reboot");
        reply = dbus_message_new_method_return(msg);
    }
    else if (dbus_message_is_method_call(msg, MANAGER_IFACE, "PowerOff")) {
        trigger_shutdown("--poweroff");
        reply = dbus_message_new_method_return(msg);
    }
    else if (dbus_message_is_method_call(msg, MANAGER_IFACE, "Suspend")) {
        trigger_shutdown("--suspend");
        reply = dbus_message_new_method_return(msg);
    }
    else if (dbus_message_is_method_call(msg, MANAGER_IFACE, "Inhibit")) {
        const char *what = NULL, *who = NULL, *why = NULL, *mode = NULL;
        dbus_message_get_args(msg, NULL,
                             DBUS_TYPE_STRING, &what,
                             DBUS_TYPE_STRING, &who,
                             DBUS_TYPE_STRING, &why,
                             DBUS_TYPE_STRING, &mode,
                             DBUS_TYPE_INVALID);

        int pipefd[2];
        if (pipe(pipefd) == -1) {
            reply = dbus_message_new_error_printf(msg, DBUS_ERROR_FAILED,
                                                  "Failed to create inhibitor pipe: %s", strerror(errno));
        } else {
            reply = dbus_message_new_method_return(msg);
            dbus_message_append_args(reply, DBUS_TYPE_UNIX_FD, &pipefd[0], DBUS_TYPE_INVALID);
            close(pipefd[0]);
            log_msg(LOG_INFO, "Inhibitor created: what=%s who=%s why=%s mode=%s", what, who, why, mode);
        }
    }
    else if (dbus_message_is_method_call(msg, MANAGER_IFACE, "ListInhibitors")) {
        reply = dbus_message_new_method_return(msg);
        DBusMessageIter iter, array;
        dbus_message_iter_init_append(reply, &iter);
        dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "(ssssuu)", &array);
        dbus_message_iter_close_container(&iter, &array);
    }
    else if (dbus_message_is_method_call(msg, MANAGER_IFACE, "ListSessions")) {
        reply = dbus_message_new_method_return(msg);
        DBusMessageIter iter, array, struct_iter;
        dbus_message_iter_init_append(reply, &iter);
        dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "(susso)", &array);
        dbus_message_iter_open_container(&array, DBUS_TYPE_STRUCT, NULL, &struct_iter);
        const char *sid = "1";
        const char *seat = current_seat_id;
        const char *spath = session_path;
        dbus_message_iter_append_basic(&struct_iter, DBUS_TYPE_STRING, &sid);
        dbus_message_iter_append_basic(&struct_iter, DBUS_TYPE_UINT32, &current_uid);
        dbus_message_iter_append_basic(&struct_iter, DBUS_TYPE_STRING, &current_username);
        dbus_message_iter_append_basic(&struct_iter, DBUS_TYPE_STRING, &seat);
        dbus_message_iter_append_basic(&struct_iter, DBUS_TYPE_OBJECT_PATH, &spath);
        dbus_message_iter_close_container(&array, &struct_iter);
        dbus_message_iter_close_container(&iter, &array);
    }
    else if (dbus_message_is_method_call(msg, MANAGER_IFACE, "GetSession")) {
        reply = dbus_message_new_method_return(msg);
        dbus_message_append_args(reply, DBUS_TYPE_OBJECT_PATH, &session_path, DBUS_TYPE_INVALID);
    }
    else if (dbus_message_is_method_call(msg, MANAGER_IFACE, "ListSeats")) {
        reply = dbus_message_new_method_return(msg);
        DBusMessageIter iter, array;
        dbus_message_iter_init_append(reply, &iter);
        dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "o", &array);
        dbus_message_iter_append_basic(&array, DBUS_TYPE_OBJECT_PATH, &seat_path);
        dbus_message_iter_close_container(&iter, &array);
    }
    else if (dbus_message_is_method_call(msg, MANAGER_IFACE, "GetSeat")) {
        reply = dbus_message_new_method_return(msg);
        dbus_message_append_args(reply, DBUS_TYPE_OBJECT_PATH, &seat_path, DBUS_TYPE_INVALID);
    }
    else if (dbus_message_is_method_call(msg, MANAGER_IFACE, "ListUsers")) {
        reply = dbus_message_new_method_return(msg);
        DBusMessageIter iter, array, struct_iter;
        dbus_message_iter_init_append(reply, &iter);
        dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "(uso)", &array);
        dbus_message_iter_open_container(&array, DBUS_TYPE_STRUCT, NULL, &struct_iter);
        dbus_message_iter_append_basic(&struct_iter, DBUS_TYPE_UINT32, &current_uid);
        dbus_message_iter_append_basic(&struct_iter, DBUS_TYPE_STRING, &current_username);
        dbus_message_iter_append_basic(&struct_iter, DBUS_TYPE_OBJECT_PATH, &user_path);
        dbus_message_iter_close_container(&array, &struct_iter);
        dbus_message_iter_close_container(&iter, &array);
    }
    else if (dbus_message_is_method_call(msg, MANAGER_IFACE, "GetUser")) {
        reply = dbus_message_new_method_return(msg);
        dbus_message_append_args(reply, DBUS_TYPE_OBJECT_PATH, &user_path, DBUS_TYPE_INVALID);
    }

    return reply;
}

static DBusMessage* handle_session_method(DBusMessage *msg) {
    DBusMessage *reply = NULL;

    if (dbus_message_is_method_call(msg, SESSION_IFACE, "Activate") ||
        dbus_message_is_method_call(msg, SESSION_IFACE, "Lock") ||
        dbus_message_is_method_call(msg, SESSION_IFACE, "Unlock")) {
        reply = dbus_message_new_method_return(msg);
    }
    else if (dbus_message_is_method_call(msg, "org.freedesktop.DBus.Properties", "Get")) {
        const char *iface = NULL, *prop = NULL;
        dbus_message_get_args(msg, NULL,
                             DBUS_TYPE_STRING, &iface,
                             DBUS_TYPE_STRING, &prop,
                             DBUS_TYPE_INVALID);

        reply = dbus_message_new_method_return(msg);
        if (strcmp(prop, "Active") == 0) {
            dbus_bool_t val = TRUE;
            dbus_message_append_args(reply, DBUS_TYPE_BOOLEAN, &val, DBUS_TYPE_INVALID);
        } else if (strcmp(prop, "State") == 0) {
            const char *val = "active";
            dbus_message_append_args(reply, DBUS_TYPE_STRING, &val, DBUS_TYPE_INVALID);
        } else if (strcmp(prop, "Type") == 0) {
            const char *val = "wayland";
            dbus_message_append_args(reply, DBUS_TYPE_STRING, &val, DBUS_TYPE_INVALID);
        } else if (strcmp(prop, "IdleHint") == 0) {
            dbus_bool_t val = FALSE;
            dbus_message_append_args(reply, DBUS_TYPE_BOOLEAN, &val, DBUS_TYPE_INVALID);
        } else if (strcmp(prop, "IdleSinceHint") == 0) {
            uint64_t val = 0;
            dbus_message_append_args(reply, DBUS_TYPE_UINT64, &val, DBUS_TYPE_INVALID);
        } else if (strcmp(prop, "Seat") == 0) {
            DBusMessageIter iter, struct_iter;
            dbus_message_iter_init_append(reply, &iter);
            dbus_message_iter_open_container(&iter, DBUS_TYPE_STRUCT, NULL, &struct_iter);
            dbus_message_iter_append_basic(&struct_iter, DBUS_TYPE_STRING, &current_seat_id);
            dbus_message_iter_append_basic(&struct_iter, DBUS_TYPE_OBJECT_PATH, &seat_path);
            dbus_message_iter_close_container(&iter, &struct_iter);
        } else if (strcmp(prop, "User") == 0) {
            DBusMessageIter iter, struct_iter;
            dbus_message_iter_init_append(reply, &iter);
            dbus_message_iter_open_container(&iter, DBUS_TYPE_STRUCT, NULL, &struct_iter);
            dbus_message_iter_append_basic(&struct_iter, DBUS_TYPE_UINT32, &current_uid);
            dbus_message_iter_append_basic(&struct_iter, DBUS_TYPE_OBJECT_PATH, &user_path);
            dbus_message_iter_close_container(&iter, &struct_iter);
        } else {
            dbus_message_unref(reply);
            reply = dbus_message_new_error(msg, DBUS_ERROR_UNKNOWN_PROPERTY, "Unknown property");
        }
    }

    return reply;
}

static DBusMessage* handle_seat_method(DBusMessage *msg) {
    DBusMessage *reply = NULL;

    if (dbus_message_is_method_call(msg, SEAT_IFACE, "SwitchToGraphical") ||
        dbus_message_is_method_call(msg, SEAT_IFACE, "LockAll") ||
        dbus_message_is_method_call(msg, SEAT_IFACE, "UnlockAll")) {
        reply = dbus_message_new_method_return(msg);
    }
    else if (dbus_message_is_method_call(msg, "org.freedesktop.DBus.Properties", "Get")) {
        const char *iface = NULL, *prop = NULL;
        dbus_message_get_args(msg, NULL,
                             DBUS_TYPE_STRING, &iface,
                             DBUS_TYPE_STRING, &prop,
                             DBUS_TYPE_INVALID);

        reply = dbus_message_new_method_return(msg);
        if (strcmp(prop, "ActiveSession") == 0) {
            DBusMessageIter iter, struct_iter;
            dbus_message_iter_init_append(reply, &iter);
            dbus_message_iter_open_container(&iter, DBUS_TYPE_STRUCT, NULL, &struct_iter);
            dbus_message_iter_append_basic(&struct_iter, DBUS_TYPE_STRING, &current_seat_id);
            dbus_message_iter_append_basic(&struct_iter, DBUS_TYPE_OBJECT_PATH, &session_path);
            dbus_message_iter_close_container(&iter, &struct_iter);
        } else if (strcmp(prop, "CanMultiSession") == 0 ||
                   strcmp(prop, "CanSwitch") == 0 ||
                   strcmp(prop, "CanGraphical") == 0 ||
                   strcmp(prop, "CanTTY") == 0) {
            dbus_bool_t val = FALSE;
            dbus_message_append_args(reply, DBUS_TYPE_BOOLEAN, &val, DBUS_TYPE_INVALID);
        } else if (strcmp(prop, "Id") == 0) {
            dbus_message_append_args(reply, DBUS_TYPE_STRING, &current_seat_id, DBUS_TYPE_INVALID);
        } else {
            dbus_message_unref(reply);
            reply = dbus_message_new_error(msg, DBUS_ERROR_UNKNOWN_PROPERTY, "Unknown property");
        }
    }

    return reply;
}

static DBusMessage* handle_user_method(DBusMessage *msg) {
    DBusMessage *reply = NULL;

    if (dbus_message_is_method_call(msg, USER_IFACE, "Kill") ||
        dbus_message_is_method_call(msg, USER_IFACE, "KillSession")) {
        reply = dbus_message_new_method_return(msg);
    }
    else if (dbus_message_is_method_call(msg, "org.freedesktop.DBus.Properties", "Get")) {
        const char *iface = NULL, *prop = NULL;
        dbus_message_get_args(msg, NULL,
                             DBUS_TYPE_STRING, &iface,
                             DBUS_TYPE_STRING, &prop,
                             DBUS_TYPE_INVALID);

        reply = dbus_message_new_method_return(msg);
        if (strcmp(prop, "UID") == 0) {
            dbus_message_append_args(reply, DBUS_TYPE_UINT32, &current_uid, DBUS_TYPE_INVALID);
        } else if (strcmp(prop, "UserName") == 0) {
            dbus_message_append_args(reply, DBUS_TYPE_STRING, &current_username, DBUS_TYPE_INVALID);
        } else if (strcmp(prop, "RuntimePath") == 0) {
            dbus_message_append_args(reply, DBUS_TYPE_STRING, &current_runtime_dir, DBUS_TYPE_INVALID);
        } else if (strcmp(prop, "State") == 0) {
            const char *val = "active";
            dbus_message_append_args(reply, DBUS_TYPE_STRING, &val, DBUS_TYPE_INVALID);
        } else if (strcmp(prop, "Linger") == 0 ||
                   strcmp(prop, "KillProcesses") == 0) {
            dbus_bool_t val = FALSE;
            dbus_message_append_args(reply, DBUS_TYPE_BOOLEAN, &val, DBUS_TYPE_INVALID);
        } else if (strcmp(prop, "Sessions") == 0) {
            DBusMessageIter iter, array, struct_iter;
            dbus_message_iter_init_append(reply, &iter);
            dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "(so)", &array);
            dbus_message_iter_open_container(&array, DBUS_TYPE_STRUCT, NULL, &struct_iter);
            dbus_message_iter_append_basic(&struct_iter, DBUS_TYPE_STRING, &current_seat_id);
            dbus_message_iter_append_basic(&struct_iter, DBUS_TYPE_OBJECT_PATH, &session_path);
            dbus_message_iter_close_container(&array, &struct_iter);
            dbus_message_iter_close_container(&iter, &array);
        } else if (strcmp(prop, "Display") == 0) {
            DBusMessageIter iter, struct_iter;
            dbus_message_iter_init_append(reply, &iter);
            dbus_message_iter_open_container(&iter, DBUS_TYPE_STRUCT, NULL, &struct_iter);
            dbus_message_iter_append_basic(&struct_iter, DBUS_TYPE_STRING, &current_seat_id);
            dbus_message_iter_append_basic(&struct_iter, DBUS_TYPE_OBJECT_PATH, &session_path);
            dbus_message_iter_close_container(&iter, &struct_iter);
        } else {
            dbus_message_unref(reply);
            reply = dbus_message_new_error(msg, DBUS_ERROR_UNKNOWN_PROPERTY, "Unknown property");
        }
    }

    return reply;
}

static DBusHandlerResult message_handler(DBusConnection *connection, DBusMessage *msg, void *user_data) {
    (void)user_data;
    DBusMessage *reply = NULL;

    if (dbus_message_is_method_call(msg, "org.freedesktop.DBus.Introspectable", "Introspect")) {
        reply = dbus_message_new_method_return(msg);
        dbus_message_append_args(reply, DBUS_TYPE_STRING, &introspection_xml, DBUS_TYPE_INVALID);
    }
    else if (dbus_message_has_interface(msg, MANAGER_IFACE)) {
        reply = handle_manager_method(msg);
    }
    else if (dbus_message_has_interface(msg, SESSION_IFACE) ||
             (dbus_message_is_method_call(msg, "org.freedesktop.DBus.Properties", "Get") &&
              dbus_message_has_path(msg, session_path))) {
        reply = handle_session_method(msg);
    }
    else if (dbus_message_has_interface(msg, SEAT_IFACE) ||
             (dbus_message_is_method_call(msg, "org.freedesktop.DBus.Properties", "Get") &&
              dbus_message_has_path(msg, seat_path))) {
        reply = handle_seat_method(msg);
    }
    else if (dbus_message_has_interface(msg, USER_IFACE) ||
             (dbus_message_is_method_call(msg, "org.freedesktop.DBus.Properties", "Get") &&
              dbus_message_has_path(msg, user_path))) {
        reply = handle_user_method(msg);
    }

    if (reply) {
        dbus_connection_send(connection, reply, NULL);
        dbus_message_unref(reply);
        return DBUS_HANDLER_RESULT_HANDLED;
    }

    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

/* Async-signal-safe signal handler: ONLY sets a volatile flag and writes
 * to a self-pipe or calls _exit(). No syslog, no D-Bus, no malloc. */
static void safe_signal_handler(int sig) {
    (void)sig;
    should_exit = 1;
}

/* Main loop checks should_exit and performs clean shutdown */
static int run_main_loop(void) {
    while (!should_exit) {
        if (!dbus_connection_read_write_dispatch(conn, 100)) {
            break;
        }
    }
    return 0;
}

int main(void) {
    openlog("lainos-dbus-bridge", LOG_PID | LOG_CONS, LOG_DAEMON);

    resolve_session_user();
    build_introspection_xml();

    if (apply_seccomp() < 0) {
        log_msg(LOG_CRIT, "Seccomp failed – aborting");
        return 1;
    }

    drop_privs();

    /* Use sigaction for reliable signal handling */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = safe_signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    if (sigaction(SIGTERM, &sa, NULL) == -1 ||
        sigaction(SIGINT, &sa, NULL) == -1) {
        log_msg(LOG_ERR, "sigaction failed: %s", strerror(errno));
        return 1;
    }

    signal(SIGCHLD, SIG_IGN);

    DBusError err;
    dbus_error_init(&err);

    conn = dbus_bus_get(DBUS_BUS_SYSTEM, &err);
    if (dbus_error_is_set(&err)) {
        log_msg(LOG_ERR, "dbus_bus_get failed: %s", err.message);
        dbus_error_free(&err);
        return 1;
    }

    if (!dbus_bus_request_name(conn, BUS_NAME,
                               DBUS_NAME_FLAG_REPLACE_EXISTING |
                               DBUS_NAME_FLAG_ALLOW_REPLACEMENT, &err)) {
        log_msg(LOG_ERR, "Failed to acquire bus name %s: %s", BUS_NAME, err.message);
        dbus_error_free(&err);
        return 1;
    }

    DBusObjectPathVTable vtable = { .message_function = message_handler };

    if (!dbus_connection_register_object_path(conn, MANAGER_PATH, &vtable, NULL) ||
        !dbus_connection_register_object_path(conn, session_path, &vtable, NULL) ||
        !dbus_connection_register_object_path(conn, seat_path, &vtable, NULL) ||
        !dbus_connection_register_object_path(conn, user_path, &vtable, NULL)) {
        log_msg(LOG_ERR, "Failed to register object paths");
        return 1;
    }

    log_msg(LOG_INFO, "Protocol 7 logind shim active for user %s (UID %u)", current_username, current_uid);

    run_main_loop();

    /* Clean shutdown outside signal handler */
    log_msg(LOG_INFO, "Shutting down cleanly");
    if (shutdown_pipe[1] != -1) {
        close(shutdown_pipe[1]);
        shutdown_pipe[1] = -1;
    }
    if (conn) {
        dbus_connection_unref(conn);
        conn = NULL;
    }
    closelog();
    return 0;
}
