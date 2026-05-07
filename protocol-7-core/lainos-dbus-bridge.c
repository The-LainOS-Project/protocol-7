/*
 * lainos-dbus-bridge.c – Protocol 7 login1 D-Bus Facade (v4.3-tuned)
 *
 * Implements org.freedesktop.login1 on dbus-openrc for AUR package compatibility.
 * Provides session, seat, and user management interfaces with realistic responses.
 *
 * Hardened: seccomp, privilege drop, no dynamic allocation in signal handler.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <syslog.h>
#include <signal.h>
#include <dbus/dbus.h>
#include <seccomp.h>
#include <pwd.h>
#include <grp.h>

#define DBUS_NAME       "org.freedesktop.login1"
#define DBUS_PATH       "/org/freedesktop/login1"
#define DBUS_INTERFACE  "org.freedesktop.login1.Manager"

static volatile sig_atomic_t shutdown_flag = 0;

static void signal_handler(int sig) {
    (void)sig;
    shutdown_flag = 1;
}

static void setup_signals(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

static void drop_privileges(void) {
    struct passwd *pw = getpwnam("nobody");
    if (!pw) return;
    if (setgid(pw->pw_gid) == -1 || setuid(pw->pw_uid) == -1) {
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
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(poll), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(epoll_wait), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(epoll_pwait), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(recvmsg), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(sendmsg), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(getpid), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(getuid), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(geteuid), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(getgid), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(getegid), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(sigreturn), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(rt_sigreturn), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(exit_group), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(exit), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(time), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(clock_gettime), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(gettimeofday), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(futex), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(madvise), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(munmap), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(mprotect), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(brk), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(ioctl), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(access), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(faccessat), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(stat), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(fstat), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(lstat), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(newfstatat), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(getdents64), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(lseek), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(dup), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(dup2), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(dup3), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(pipe), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(pipe2), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(socketpair), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(getrandom), 0);
    seccomp_load(ctx);
}

/* ─── Runtime user detection ─── */
static uid_t get_first_real_user(void) {
    FILE *fp = fopen("/etc/passwd", "r");
    if (!fp) return 1000;
    char line[256];
    uid_t uid = 1000;
    while (fgets(line, sizeof(line), fp)) {
        char *p = strchr(line, ':');
        if (!p) continue;
        *p = '\0';
        char *name = line;
        if (strcmp(name, "nobody") == 0 || strcmp(name, "root") == 0) continue;
        p = strchr(p + 1, ':');
        if (!p) continue;
        p = strchr(p + 1, ':');
        if (!p) continue;
        uid_t candidate = (uid_t)atoi(p + 1);
        if (candidate >= 1000 && candidate < 65534) {
            uid = candidate;
            break;
        }
    }
    fclose(fp);
    return uid;
}

static uid_t runtime_uid;
static char runtime_path[128];

/* ─── Inhibit fd tracking ───
 * Protocol 7 does not support real inhibition, but we must provide
 * valid file descriptors to callers. We keep pipes open for the
 * lifetime of the process. The write end is returned; the read end
 * is held to keep the pipe alive. This is a deliberate fd leak —
 * bounded by the number of Inhibit calls (typically < 10 per session).
 * When the limit is reached, we return LimitsExceeded so callers
 * can fall back gracefully.
 */
#define INHIBIT_MAX_PIPES 64
static int inhibit_pipes[INHIBIT_MAX_PIPES][2];
static int inhibit_pipe_count = 0;

static void init_runtime(void) {
    runtime_uid = get_first_real_user();
    snprintf(runtime_path, sizeof(runtime_path), "/run/user/%u", runtime_uid);
}

/* ─── Introspection XML (v4.3-tuned) ─── */
static const char *introspection_xml =
    "<!DOCTYPE node PUBLIC "-//freedesktop//DTD D-BUS Object Introspection 1.0//EN"\n"
    " "http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd">\n"
    "<node>\n"
    "  <interface name="org.freedesktop.DBus.Introspectable">\n"
    "    <method name="Introspect">\n"
    "      <arg direction="out" type="s"/>\n"
    "    </method>\n"
    "  </interface>\n"
    "  <interface name="org.freedesktop.DBus.Properties">\n"
    "    <method name="Get">\n"
    "      <arg direction="in" type="s"/>\n"
    "      <arg direction="in" type="s"/>\n"
    "      <arg direction="out" type="v"/>\n"
    "    </method>\n"
    "    <method name="GetAll">\n"
    "      <arg direction="in" type="s"/>\n"
    "      <arg direction="out" type="a{sv}"/>\n"
    "    </method>\n"
    "  </interface>\n"
    "  <interface name="org.freedesktop.login1.Manager">\n"
    "    <method name="GetSession">\n"
    "      <arg direction="in" type="s"/>\n"
    "      <arg direction="out" type="o"/>\n"
    "    </method>\n"
    "    <method name="GetSeat">\n"
    "      <arg direction="in" type="s"/>\n"
    "      <arg direction="out" type="o"/>\n"
    "    </method>\n"
    "    <method name="GetUser">\n"
    "      <arg direction="in" type="u"/>\n"
    "      <arg direction="out" type="o"/>\n"
    "    </method>\n"
    "    <method name="ListSessions">\n"
    "      <arg direction="out" type="a(susso)"/>\n"
    "    </method>\n"
    "    <method name="ListSeats">\n"
    "      <arg direction="out" type="a(so)"/>\n"
    "    </method>\n"
    "    <method name="ListUsers">\n"
    "      <arg direction="out" type="a(uso)"/>\n"
    "    </method>\n"
    "    <method name="ListInhibitors">\n"
    "      <arg direction="out" type="a(ssssuu)"/>\n"
    "    </method>\n"
    "    <method name="Inhibit">\n"
    "      <arg direction="in" type="ssss"/>\n"
    "      <arg direction="out" type="h"/>\n"
    "    </method>\n"
    "    <method name="Reboot">\n"
    "      <arg direction="in" type="b"/>\n"
    "    </method>\n"
    "    <method name="PowerOff">\n"
    "      <arg direction="in" type="b"/>\n"
    "    </method>\n"
    "    <method name="Suspend">\n"
    "      <arg direction="in" type="b"/>\n"
    "    </method>\n"
    "    <method name="Hibernate">\n"
    "      <arg direction="in" type="b"/>\n"
    "    </method>\n"
    "    <method name="HybridSleep">\n"
    "      <arg direction="in" type="b"/>\n"
    "    </method>\n"
    "    <method name="CanReboot">\n"
    "      <arg direction="out" type="s"/>\n"
    "    </method>\n"
    "    <method name="CanPowerOff">\n"
    "      <arg direction="out" type="s"/>\n"
    "    </method>\n"
    "    <method name="CanSuspend">\n"
    "      <arg direction="out" type="s"/>\n"
    "    </method>\n"
    "    <method name="CanHibernate">\n"
    "      <arg direction="out" type="s"/>\n"
    "    </method>\n"
    "    <method name="CanHybridSleep">\n"
    "      <arg direction="out" type="s"/>\n"
    "    </method>\n"
    "    <property name="NAutoVTs" type="u" access="read"/>\n"
    "    <property name="KillOnlyUsers" type="as" access="read"/>\n"
    "    <property name="KillExcludeUsers" type="as" access="read"/>\n"
    "    <property name="IdleAction" type="s" access="read"/>\n"
    "    <property name="IdleActionUSec" type="t" access="read"/>\n"
    "    <property name="IdleHint" type="b" access="read"/>\n"
    "    <property name="IdleSinceHint" type="t" access="read"/>\n"
    "    <property name="IdleSinceHintMonotonic" type="t" access="read"/>\n"
    "    <property name="BlockInhibited" type="s" access="read"/>\n"
    "    <property name="DelayInhibited" type="s" access="read"/>\n"
    "    <property name="InhibitDelayMaxUSec" type="t" access="read"/>\n"
    "    <property name="HandlePowerKey" type="s" access="read"/>\n"
    "    <property name="HandleSuspendKey" type="s" access="read"/>\n"
    "    <property name="HandleHibernateKey" type="s" access="read"/>\n"
    "    <property name="HandleLidSwitch" type="s" access="read"/>\n"
    "    <property name="HandleLidSwitchExternalPower" type="s" access="read"/>\n"
    "    <property name="HandleLidSwitchDocked" type="s" access="read"/>\n"
    "    <property name="HoldoffTimeoutUSec" type="t" access="read"/>\n"
    "    <property name="IdleWatchdogPeriodUSec" type="t" access="read"/>\n"
    "    <property name="RuntimeDirectorySize" type="t" access="read"/>\n"
    "    <property name="RuntimeDirectoryInodesMax" type="t" access="read"/>\n"
    "    <property name="RemoveIPC" type="b" access="read"/>\n"
    "    <property name="InhibitorsMax" type="t" access="read"/>\n"
    "    <property name="NCurrentInhibitors" type="t" access="read"/>\n"
    "    <property name="SessionsMax" type="t" access="read"/>\n"
    "    <property name="NCurrentSessions" type="t" access="read"/>\n"
    "    <property name="UserTasksMax" type="t" access="read"/>\n"
    "    <signal name="SessionNew">\n"
    "      <arg type="s"/>\n"
    "      <arg type="o"/>\n"
    "    </signal>\n"
    "    <signal name="SessionRemoved">\n"
    "      <arg type="s"/>\n"
    "      <arg type="o"/>\n"
    "    </signal>\n"
    "    <signal name="SeatNew">\n"
    "      <arg type="s"/>\n"
    "      <arg type="o"/>\n"
    "    </signal>\n"
    "    <signal name="SeatRemoved">\n"
    "      <arg type="s"/>\n"
    "      <arg type="o"/>\n"
    "    </signal>\n"
    "    <signal name="UserNew">\n"
    "      <arg type="u"/>\n"
    "      <arg type="o"/>\n"
    "    </signal>\n"
    "    <signal name="UserRemoved">\n"
    "      <arg type="u"/>\n"
    "      <arg type="o"/>\n"
    "    </signal>\n"
    "    <signal name="PrepareForShutdown">\n"
    "      <arg type="b"/>\n"
    "    </signal>\n"
    "    <signal name="PrepareForSleep">\n"
    "      <arg type="b"/>\n"
    "    </signal>\n"
    "  </interface>\n"
    "  <interface name="org.freedesktop.login1.Session">\n"
    "    <method name="Activate">\n"
    "    </method>\n"
    "    <method name="Lock">\n"
    "    </method>\n"
    "    <method name="Unlock">\n"
    "    </method>\n"
    "    <method name="Terminate">\n"
    "    </method>\n"
    "    <method name="Kill">\n"
    "      <arg direction="in" type="s"/>\n"
    "      <arg direction="in" type="i"/>\n"
    "    </method>\n"
    "    <method name="SetIdleHint">\n"
    "      <arg direction="in" type="b"/>\n"
    "    </method>\n"
    "    <method name="SetType">\n"
    "      <arg direction="in" type="s"/>\n"
    "    </method>\n"
    "    <method name="SetClass">\n"
    "      <arg direction="in" type="s"/>\n"
    "    </method>\n"
    "    <method name="SetDesktop">\n"
    "      <arg direction="in" type="s"/>\n"
    "    </method>\n"
    "    <method name="SetDisplay">\n"
    "      <arg direction="in" type="s"/>\n"
    "    </method>\n"
    "    <method name="SetRemote">\n"
    "      <arg direction="in" type="b"/>\n"
    "    </method>\n"
    "    <method name="SetRemoteHost">\n"
    "      <arg direction="in" type="s"/>\n"
    "    </method>\n"
    "    <method name="SetRemoteUser">\n"
    "      <arg direction="in" type="s"/>\n"
    "    </method>\n"
    "    <property name="Id" type="s" access="read"/>\n"
    "    <property name="User" type="(uo)" access="read"/>\n"
    "    <property name="Name" type="s" access="read"/>\n"
    "    <property name="Timestamp" type="t" access="read"/>\n"
    "    <property name="TimestampMonotonic" type="t" access="read"/>\n"
    "    <property name="VTNr" type="u" access="read"/>\n"
    "    <property name="Seat" type="(so)" access="read"/>\n"
    "    <property name="TTY" type="s" access="read"/>\n"
    "    <property name="Display" type="s" access="read"/>\n"
    "    <property name="Remote" type="b" access="read"/>\n"
    "    <property name="RemoteHost" type="s" access="read"/>\n"
    "    <property name="RemoteUser" type="s" access="read"/>\n"
    "    <property name="Service" type="s" access="read"/>\n"
    "    <property name="Desktop" type="s" access="read"/>\n"
    "    <property name="Type" type="s" access="read"/>\n"
    "    <property name="Class" type="s" access="read"/>\n"
    "    <property name="Scope" type="s" access="read"/>\n"
    "    <property name="Leader" type="u" access="read"/>\n"
    "    <property name="Audit" type="u" access="read"/>\n"
    "    <property name="IdleHint" type="b" access="read"/>\n"
    "    <property name="IdleSinceHint" type="t" access="read"/>\n"
    "    <property name="IdleSinceHintMonotonic" type="t" access="read"/>\n"
    "    <property name="LockedHint" type="b" access="read"/>\n"
    "  </interface>\n"
    "  <interface name="org.freedesktop.login1.Seat">\n"
    "    <method name="ActivateSession">\n"
    "      <arg direction="in" type="s"/>\n"
    "    </method>\n"
    "    <method name="SwitchTo">\n"
    "      <arg direction="in" type="u"/>\n"
    "    </method>\n"
    "    <method name="SwitchToNext">\n"
    "    </method>\n"
    "    <method name="SwitchToPrevious">\n"
    "    </method>\n"
    "    <method name="Terminate">\n"
    "    </method>\n"
    "    <property name="Id" type="s" access="read"/>\n"
    "    <property name="ActiveSession" type="(so)" access="read"/>\n"
    "    <property name="CanMultiSession" type="b" access="read"/>\n"
    "    <property name="CanTTY" type="b" access="read"/>\n"
    "    <property name="CanGraphical" type="b" access="read"/>\n"
    "    <property name="Sessions" type="a(so)" access="read"/>\n"
    "    <property name="IdleHint" type="b" access="read"/>\n"
    "    <property name="IdleSinceHint" type="t" access="read"/>\n"
    "    <property name="IdleSinceHintMonotonic" type="t" access="read"/>\n"
    "  </interface>\n"
    "  <interface name="org.freedesktop.login1.User">\n"
    "    <method name="Terminate">\n"
    "    </method>\n"
    "    <method name="Kill">\n"
    "      <arg direction="in" type="s"/>\n"
    "      <arg direction="in" type="i"/>\n"
    "    </method>\n"
    "    <property name="UID" type="u" access="read"/>\n"
    "    <property name="GID" type="u" access="read"/>\n"
    "    <property name="Name" type="s" access="read"/>\n"
    "    <property name="Timestamp" type="t" access="read"/>\n"
    "    <property name="TimestampMonotonic" type="t" access="read"/>\n"
    "    <property name="RuntimePath" type="s" access="read"/>\n"
    "    <property name="Service" type="s" access="read"/>\n"
    "    <property name="Slice" type="s" access="read"/>\n"
    "    <property name="Display" type="(so)" access="read"/>\n"
    "    <property name="State" type="s" access="read"/>\n"
    "    <property name="Sessions" type="a(so)" access="read"/>\n"
    "    <property name="IdleHint" type="b" access="read"/>\n"
    "    <property name="IdleSinceHint" type="t" access="read"/>\n"
    "    <property name="IdleSinceHintMonotonic" type="t" access="read"/>\n"
    "    <property name="Linger" type="b" access="read"/>\n"
    "    <property name="Wallpaper" type="s" access="read"/>\n"
    "  </interface>\n"
    "</node>\n";

/* ─── Method dispatch ─── */
static void handle_method(DBusConnection *conn, DBusMessage *msg) {
    const char *iface = dbus_message_get_interface(msg);
    const char *member = dbus_message_get_member(msg);
    const char *path = dbus_message_get_path(msg);

    if (!iface || !member) return;

    if (strcmp(iface, "org.freedesktop.DBus.Introspectable") == 0 &&
        strcmp(member, "Introspect") == 0) {
        DBusMessage *reply = dbus_message_new_method_return(msg);
        dbus_message_append_args(reply,
            DBUS_TYPE_STRING, &introspection_xml,
            DBUS_TYPE_INVALID);
        dbus_connection_send(conn, reply, NULL);
        dbus_message_unref(reply);
        return;
    }

    if (strcmp(iface, "org.freedesktop.login1.Manager") != 0) {
        DBusMessage *err = dbus_message_new_error(msg,
            "org.freedesktop.DBus.Error.UnknownInterface",
            "Unknown interface");
        dbus_connection_send(conn, err, NULL);
        dbus_message_unref(err);
        return;
    }

    if (strcmp(member, "GetSession") == 0) {
        const char *sid = "";
        dbus_message_get_args(msg, NULL,
            DBUS_TYPE_STRING, &sid,
            DBUS_TYPE_INVALID);
        char objpath[128];
        snprintf(objpath, sizeof(objpath), "/org/freedesktop/login1/session/%s", sid);
        DBusMessage *reply = dbus_message_new_method_return(msg);
        dbus_message_append_args(reply,
            DBUS_TYPE_OBJECT_PATH, &objpath,
            DBUS_TYPE_INVALID);
        dbus_connection_send(conn, reply, NULL);
        dbus_message_unref(reply);
    }
    else if (strcmp(member, "GetSeat") == 0) {
        const char *seat_id = "";
        dbus_message_get_args(msg, NULL,
            DBUS_TYPE_STRING, &seat_id,
            DBUS_TYPE_INVALID);
        char objpath[128];
        snprintf(objpath, sizeof(objpath), "/org/freedesktop/login1/seat/%s", seat_id);
        DBusMessage *reply = dbus_message_new_method_return(msg);
        dbus_message_append_args(reply,
            DBUS_TYPE_OBJECT_PATH, &objpath,
            DBUS_TYPE_INVALID);
        dbus_connection_send(conn, reply, NULL);
        dbus_message_unref(reply);
    }
    else if (strcmp(member, "GetUser") == 0) {
        uint32_t uid = 0;
        dbus_message_get_args(msg, NULL,
            DBUS_TYPE_UINT32, &uid,
            DBUS_TYPE_INVALID);
        char objpath[128];
        snprintf(objpath, sizeof(objpath), "/org/freedesktop/login1/user/_%u", uid);
        DBusMessage *reply = dbus_message_new_method_return(msg);
        dbus_message_append_args(reply,
            DBUS_TYPE_OBJECT_PATH, &objpath,
            DBUS_TYPE_INVALID);
        dbus_connection_send(conn, reply, NULL);
        dbus_message_unref(reply);
    }
    else if (strcmp(member, "ListSessions") == 0) {
        DBusMessage *reply = dbus_message_new_method_return(msg);
        DBusMessageIter iter, arr;
        dbus_message_iter_init_append(reply, &iter);
        dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "(susso)", &arr);
        char sid[] = "1";
        uint32_t uid = runtime_uid;
        char uname[] = "user";
        char seat[] = "seat0";
        char objpath[128];
        snprintf(objpath, sizeof(objpath), "/org/freedesktop/login1/session/%s", sid);
        DBusMessageIter struc;
        dbus_message_iter_open_container(&arr, DBUS_TYPE_STRUCT, NULL, &struc);
        dbus_message_iter_append_basic(&struc, DBUS_TYPE_STRING, &sid);
        dbus_message_iter_append_basic(&struc, DBUS_TYPE_UINT32, &uid);
        dbus_message_iter_append_basic(&struc, DBUS_TYPE_STRING, &uname);
        dbus_message_iter_append_basic(&struc, DBUS_TYPE_STRING, &seat);
        dbus_message_iter_append_basic(&struc, DBUS_TYPE_OBJECT_PATH, &objpath);
        dbus_message_iter_close_container(&arr, &struc);
        dbus_message_iter_close_container(&iter, &arr);
        dbus_connection_send(conn, reply, NULL);
        dbus_message_unref(reply);
    }
    else if (strcmp(member, "ListSeats") == 0) {
        DBusMessage *reply = dbus_message_new_method_return(msg);
        DBusMessageIter iter, arr;
        dbus_message_iter_init_append(reply, &iter);
        dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "(so)", &arr);
        char seat[] = "seat0";
        char objpath[] = "/org/freedesktop/login1/seat/seat0";
        DBusMessageIter struc;
        dbus_message_iter_open_container(&arr, DBUS_TYPE_STRUCT, NULL, &struc);
        dbus_message_iter_append_basic(&struc, DBUS_TYPE_STRING, &seat);
        dbus_message_iter_append_basic(&struc, DBUS_TYPE_OBJECT_PATH, &objpath);
        dbus_message_iter_close_container(&arr, &struc);
        dbus_message_iter_close_container(&iter, &arr);
        dbus_connection_send(conn, reply, NULL);
        dbus_message_unref(reply);
    }
    else if (strcmp(member, "ListUsers") == 0) {
        DBusMessage *reply = dbus_message_new_method_return(msg);
        DBusMessageIter iter, arr;
        dbus_message_iter_init_append(reply, &iter);
        dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "(uso)", &arr);
        uint32_t uid = runtime_uid;
        char uname[] = "user";
        char objpath[128];
        snprintf(objpath, sizeof(objpath), "/org/freedesktop/login1/user/_%u", uid);
        DBusMessageIter struc;
        dbus_message_iter_open_container(&arr, DBUS_TYPE_STRUCT, NULL, &struc);
        dbus_message_iter_append_basic(&struc, DBUS_TYPE_UINT32, &uid);
        dbus_message_iter_append_basic(&struc, DBUS_TYPE_STRING, &uname);
        dbus_message_iter_append_basic(&struc, DBUS_TYPE_OBJECT_PATH, &objpath);
        dbus_message_iter_close_container(&arr, &struc);
        dbus_message_iter_close_container(&iter, &arr);
        dbus_connection_send(conn, reply, NULL);
        dbus_message_unref(reply);
    }
    else if (strcmp(member, "ListInhibitors") == 0) {
        DBusMessage *reply = dbus_message_new_method_return(msg);
        DBusMessageIter iter, arr;
        dbus_message_iter_init_append(reply, &iter);
        dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "(ssssuu)", &arr);
        dbus_message_iter_close_container(&iter, &arr);
        dbus_connection_send(conn, reply, NULL);
        dbus_message_unref(reply);
    }
    else if (strcmp(member, "Inhibit") == 0) {
        if (inhibit_pipe_count >= INHIBIT_MAX_PIPES) {
            DBusMessage *err = dbus_message_new_error(msg,
                "org.freedesktop.DBus.Error.LimitsExceeded",
                "Maximum inhibitor count reached");
            dbus_connection_send(conn, err, NULL);
            dbus_message_unref(err);
            return;
        }
        int pipefd[2];
        if (pipe(pipefd) == -1) {
            DBusMessage *err = dbus_message_new_error(msg,
                "org.freedesktop.DBus.Error.Failed",
                "pipe() failed");
            dbus_connection_send(conn, err, NULL);
            dbus_message_unref(err);
            return;
        }
        DBusMessage *reply = dbus_message_new_method_return(msg);
        dbus_message_append_args(reply,
            DBUS_TYPE_UNIX_FD, &pipefd[1],
            DBUS_TYPE_INVALID);
        dbus_connection_send(conn, reply, NULL);
        dbus_message_unref(reply);
        /* Keep both ends open — caller gets valid fd, pipe stays alive.
         * This is a bounded fd leak (max 64 pairs). The read end is
         * never read from; it exists solely to prevent SIGPIPE/EPIPE
         * when the caller writes to the write end. */
        inhibit_pipes[inhibit_pipe_count][0] = pipefd[0];
        inhibit_pipes[inhibit_pipe_count][1] = pipefd[1];
        inhibit_pipe_count++;
    }
    else if (strcmp(member, "Reboot") == 0 || strcmp(member, "PowerOff") == 0) {
        const char *action = (strcmp(member, "Reboot") == 0) ? "reboot" : "poweroff";
        int pipefd[2];
        if (pipe(pipefd) == -1) {
            DBusMessage *err = dbus_message_new_error(msg,
                "org.freedesktop.DBus.Error.Failed",
                "pipe() failed");
            dbus_connection_send(conn, err, NULL);
            dbus_message_unref(err);
            return;
        }
        pid_t pid = fork();
        if (pid == -1) {
            close(pipefd[0]);
            close(pipefd[1]);
            DBusMessage *err = dbus_message_new_error(msg,
                "org.freedesktop.DBus.Error.Failed",
                "fork() failed");
            dbus_connection_send(conn, err, NULL);
            dbus_message_unref(err);
            return;
        }
        if (pid == 0) {
            close(pipefd[1]);
            char buf[16];
            ssize_t n = read(pipefd[0], buf, sizeof(buf));
            close(pipefd[0]);
            (void)n;
            execlp("openrc-shutdown", "openrc-shutdown", action, (char *)NULL);
            _exit(127);
        }
        close(pipefd[0]);
        DBusMessage *reply = dbus_message_new_method_return(msg);
        dbus_connection_send(conn, reply, NULL);
        dbus_message_unref(reply);
        close(pipefd[1]);
    }
    else if (strcmp(member, "Suspend") == 0) {
        /* EXPLICIT CONTRACT EXCEPTION — see hard-scope-contract.md §4.3
         * Suspend returns success (no-op) rather than failure.
         * Rationale: Most callers check CanSuspend first; returning failure
         * here breaks more apps than it helps. The system does not actually
         * suspend — this is a compatibility shim, not a real implementation.
         * This is the ONLY exception to the "return failure" rule. */
        DBusMessage *reply = dbus_message_new_method_return(msg);
        dbus_connection_send(conn, reply, NULL);
        dbus_message_unref(reply);
    }
    else if (strcmp(member, "Hibernate") == 0 || strcmp(member, "HybridSleep") == 0) {
        DBusMessage *err = dbus_message_new_error(msg,
            "org.freedesktop.DBus.Error.NotSupported",
            "Hibernation is not supported on this system");
        dbus_connection_send(conn, err, NULL);
        dbus_message_unref(err);
    }
    else if (strcmp(member, "CanReboot") == 0 || strcmp(member, "CanPowerOff") == 0 ||
             strcmp(member, "CanSuspend") == 0) {
        const char *yes = "yes";
        DBusMessage *reply = dbus_message_new_method_return(msg);
        dbus_message_append_args(reply,
            DBUS_TYPE_STRING, &yes,
            DBUS_TYPE_INVALID);
        dbus_connection_send(conn, reply, NULL);
        dbus_message_unref(reply);
    }
    else if (strcmp(member, "CanHibernate") == 0 || strcmp(member, "CanHybridSleep") == 0) {
        const char *no = "no";
        DBusMessage *reply = dbus_message_new_method_return(msg);
        dbus_message_append_args(reply,
            DBUS_TYPE_STRING, &no,
            DBUS_TYPE_INVALID);
        dbus_connection_send(conn, reply, NULL);
        dbus_message_unref(reply);
    }
    else if (strcmp(member, "Get") == 0 || strcmp(member, "GetAll") == 0) {
        DBusMessage *reply = dbus_message_new_method_return(msg);
        DBusMessageIter iter;
        dbus_message_iter_init_append(reply, &iter);
        if (strcmp(member, "GetAll") == 0) {
            DBusMessageIter dict;
            dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "{sv}", &dict);
            dbus_message_iter_close_container(&iter, &dict);
        } else {
            DBusMessageIter var;
            dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "s", &var);
            const char *val = "";
            dbus_message_iter_append_basic(&var, DBUS_TYPE_STRING, &val);
            dbus_message_iter_close_container(&iter, &var);
        }
        dbus_connection_send(conn, reply, NULL);
        dbus_message_unref(reply);
    }
    else {
        DBusMessage *err = dbus_message_new_error(msg,
            "org.freedesktop.DBus.Error.UnknownMethod",
            "Unknown method");
        dbus_connection_send(conn, err, NULL);
        dbus_message_unref(err);
    }
}

/* ─── Session/Seat/User property dispatch ─── */
static void handle_properties(DBusConnection *conn, DBusMessage *msg) {
    const char *iface = dbus_message_get_interface(msg);
    const char *member = dbus_message_get_member(msg);
    const char *path = dbus_message_get_path(msg);

    if (!iface || !member || !path) return;

    DBusMessage *reply = dbus_message_new_method_return(msg);
    DBusMessageIter iter;
    dbus_message_iter_init_append(reply, &iter);

    if (strcmp(iface, "org.freedesktop.DBus.Properties") == 0 &&
        strcmp(member, "Get") == 0) {
        const char *prop_iface = NULL;
        const char *prop_name = NULL;
        dbus_message_get_args(msg, NULL,
            DBUS_TYPE_STRING, &prop_iface,
            DBUS_TYPE_STRING, &prop_name,
            DBUS_TYPE_INVALID);

        if (strcmp(prop_iface, "org.freedesktop.login1.Manager") == 0) {
            if (strcmp(prop_name, "NAutoVTs") == 0) {
                uint32_t v = 6;
                DBusMessageIter var;
                dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "u", &var);
                dbus_message_iter_append_basic(&var, DBUS_TYPE_UINT32, &v);
                dbus_message_iter_close_container(&iter, &var);
            }
            else if (strcmp(prop_name, "IdleHint") == 0) {
                dbus_bool_t v = 0;
                DBusMessageIter var;
                dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "b", &var);
                dbus_message_iter_append_basic(&var, DBUS_TYPE_BOOLEAN, &v);
                dbus_message_iter_close_container(&iter, &var);
            }
            else if (strcmp(prop_name, "BlockInhibited") == 0 ||
                     strcmp(prop_name, "DelayInhibited") == 0) {
                const char *v = "";
                DBusMessageIter var;
                dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "s", &var);
                dbus_message_iter_append_basic(&var, DBUS_TYPE_STRING, &v);
                dbus_message_iter_close_container(&iter, &var);
            }
            else if (strcmp(prop_name, "HandlePowerKey") == 0 ||
                     strcmp(prop_name, "HandleSuspendKey") == 0 ||
                     strcmp(prop_name, "HandleHibernateKey") == 0 ||
                     strcmp(prop_name, "HandleLidSwitch") == 0 ||
                     strcmp(prop_name, "HandleLidSwitchExternalPower") == 0 ||
                     strcmp(prop_name, "HandleLidSwitchDocked") == 0) {
                const char *v = "ignore";
                DBusMessageIter var;
                dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "s", &var);
                dbus_message_iter_append_basic(&var, DBUS_TYPE_STRING, &v);
                dbus_message_iter_close_container(&iter, &var);
            }
            else if (strcmp(prop_name, "RemoveIPC") == 0) {
                dbus_bool_t v = 1;
                DBusMessageIter var;
                dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "b", &var);
                dbus_message_iter_append_basic(&var, DBUS_TYPE_BOOLEAN, &v);
                dbus_message_iter_close_container(&iter, &var);
            }
            else if (strcmp(prop_name, "RuntimeDirectorySize") == 0 ||
                     strcmp(prop_name, "RuntimeDirectoryInodesMax") == 0 ||
                     strcmp(prop_name, "InhibitDelayMaxUSec") == 0 ||
                     strcmp(prop_name, "HoldoffTimeoutUSec") == 0 ||
                     strcmp(prop_name, "IdleActionUSec") == 0 ||
                     strcmp(prop_name, "IdleWatchdogPeriodUSec") == 0 ||
                     strcmp(prop_name, "SessionsMax") == 0 ||
                     strcmp(prop_name, "NCurrentSessions") == 0 ||
                     strcmp(prop_name, "NCurrentInhibitors") == 0 ||
                     strcmp(prop_name, "InhibitorsMax") == 0 ||
                     strcmp(prop_name, "UserTasksMax") == 0) {
                uint64_t v = 0;
                DBusMessageIter var;
                dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "t", &var);
                dbus_message_iter_append_basic(&var, DBUS_TYPE_UINT64, &v);
                dbus_message_iter_close_container(&iter, &var);
            }
            else {
                const char *v = "";
                DBusMessageIter var;
                dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "s", &var);
                dbus_message_iter_append_basic(&var, DBUS_TYPE_STRING, &v);
                dbus_message_iter_close_container(&iter, &var);
            }
        }
        else if (strcmp(prop_iface, "org.freedesktop.login1.Session") == 0) {
            if (strcmp(prop_name, "Id") == 0) {
                const char *v = "1";
                DBusMessageIter var;
                dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "s", &var);
                dbus_message_iter_append_basic(&var, DBUS_TYPE_STRING, &v);
                dbus_message_iter_close_container(&iter, &var);
            }
            else if (strcmp(prop_name, "User") == 0) {
                DBusMessageIter var, struc;
                dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "(uo)", &var);
                dbus_message_iter_open_container(&var, DBUS_TYPE_STRUCT, NULL, &struc);
                uint32_t uid = runtime_uid;
                char objpath[128];
                snprintf(objpath, sizeof(objpath), "/org/freedesktop/login1/user/_%u", uid);
                dbus_message_iter_append_basic(&struc, DBUS_TYPE_UINT32, &uid);
                dbus_message_iter_append_basic(&struc, DBUS_TYPE_OBJECT_PATH, &objpath);
                dbus_message_iter_close_container(&var, &struc);
                dbus_message_iter_close_container(&iter, &var);
            }
            else if (strcmp(prop_name, "Name") == 0) {
                const char *v = "user";
                DBusMessageIter var;
                dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "s", &var);
                dbus_message_iter_append_basic(&var, DBUS_TYPE_STRING, &v);
                dbus_message_iter_close_container(&iter, &var);
            }
            else if (strcmp(prop_name, "VTNr") == 0) {
                uint32_t v = 1;
                DBusMessageIter var;
                dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "u", &var);
                dbus_message_iter_append_basic(&var, DBUS_TYPE_UINT32, &v);
                dbus_message_iter_close_container(&iter, &var);
            }
            else if (strcmp(prop_name, "Seat") == 0) {
                DBusMessageIter var, struc;
                dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "(so)", &var);
                dbus_message_iter_open_container(&var, DBUS_TYPE_STRUCT, NULL, &struc);
                const char *seat = "seat0";
                char objpath[] = "/org/freedesktop/login1/seat/seat0";
                dbus_message_iter_append_basic(&struc, DBUS_TYPE_STRING, &seat);
                dbus_message_iter_append_basic(&struc, DBUS_TYPE_OBJECT_PATH, &objpath);
                dbus_message_iter_close_container(&var, &struc);
                dbus_message_iter_close_container(&iter, &var);
            }
            else if (strcmp(prop_name, "TTY") == 0 || strcmp(prop_name, "Display") == 0 ||
                     strcmp(prop_name, "RemoteHost") == 0 || strcmp(prop_name, "RemoteUser") == 0 ||
                     strcmp(prop_name, "Service") == 0) {
                const char *v = "";
                DBusMessageIter var;
                dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "s", &var);
                dbus_message_iter_append_basic(&var, DBUS_TYPE_STRING, &v);
                dbus_message_iter_close_container(&iter, &var);
            }
            else if (strcmp(prop_name, "Desktop") == 0) {
                const char *v = "sway";
                DBusMessageIter var;
                dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "s", &var);
                dbus_message_iter_append_basic(&var, DBUS_TYPE_STRING, &v);
                dbus_message_iter_close_container(&iter, &var);
            }
            else if (strcmp(prop_name, "Type") == 0) {
                const char *v = "wayland";
                DBusMessageIter var;
                dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "s", &var);
                dbus_message_iter_append_basic(&var, DBUS_TYPE_STRING, &v);
                dbus_message_iter_close_container(&iter, &var);
            }
            else if (strcmp(prop_name, "Class") == 0) {
                const char *v = "user";
                DBusMessageIter var;
                dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "s", &var);
                dbus_message_iter_append_basic(&var, DBUS_TYPE_STRING, &v);
                dbus_message_iter_close_container(&iter, &var);
            }
            else if (strcmp(prop_name, "Scope") == 0) {
                const char *v = "session-1.scope";
                DBusMessageIter var;
                dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "s", &var);
                dbus_message_iter_append_basic(&var, DBUS_TYPE_STRING, &v);
                dbus_message_iter_close_container(&iter, &var);
            }
            else if (strcmp(prop_name, "Leader") == 0) {
                uint32_t v = (uint32_t)getpid();
                DBusMessageIter var;
                dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "u", &var);
                dbus_message_iter_append_basic(&var, DBUS_TYPE_UINT32, &v);
                dbus_message_iter_close_container(&iter, &var);
            }
            else if (strcmp(prop_name, "Audit") == 0) {
                uint32_t v = 0;
                DBusMessageIter var;
                dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "u", &var);
                dbus_message_iter_append_basic(&var, DBUS_TYPE_UINT32, &v);
                dbus_message_iter_close_container(&iter, &var);
            }
            else if (strcmp(prop_name, "IdleHint") == 0 || strcmp(prop_name, "LockedHint") == 0 ||
                     strcmp(prop_name, "Remote") == 0) {
                dbus_bool_t v = 0;
                DBusMessageIter var;
                dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "b", &var);
                dbus_message_iter_append_basic(&var, DBUS_TYPE_BOOLEAN, &v);
                dbus_message_iter_close_container(&iter, &var);
            }
            else if (strcmp(prop_name, "Timestamp") == 0 ||
                     strcmp(prop_name, "TimestampMonotonic") == 0 ||
                     strcmp(prop_name, "IdleSinceHint") == 0 ||
                     strcmp(prop_name, "IdleSinceHintMonotonic") == 0) {
                uint64_t v = 0;
                DBusMessageIter var;
                dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "t", &var);
                dbus_message_iter_append_basic(&var, DBUS_TYPE_UINT64, &v);
                dbus_message_iter_close_container(&iter, &var);
            }
            else {
                const char *v = "";
                DBusMessageIter var;
                dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "s", &var);
                dbus_message_iter_append_basic(&var, DBUS_TYPE_STRING, &v);
                dbus_message_iter_close_container(&iter, &var);
            }
        }
        else if (strcmp(prop_iface, "org.freedesktop.login1.Seat") == 0) {
            if (strcmp(prop_name, "Id") == 0) {
                const char *v = "seat0";
                DBusMessageIter var;
                dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "s", &var);
                dbus_message_iter_append_basic(&var, DBUS_TYPE_STRING, &v);
                dbus_message_iter_close_container(&iter, &var);
            }
            else if (strcmp(prop_name, "ActiveSession") == 0) {
                DBusMessageIter var, struc;
                dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "(so)", &var);
                dbus_message_iter_open_container(&var, DBUS_TYPE_STRUCT, NULL, &struc);
                const char *sid = "1";
                char objpath[] = "/org/freedesktop/login1/session/1";
                dbus_message_iter_append_basic(&struc, DBUS_TYPE_STRING, &sid);
                dbus_message_iter_append_basic(&struc, DBUS_TYPE_OBJECT_PATH, &objpath);
                dbus_message_iter_close_container(&var, &struc);
                dbus_message_iter_close_container(&iter, &var);
            }
            else if (strcmp(prop_name, "CanMultiSession") == 0) {
                dbus_bool_t v = 0;
                DBusMessageIter var;
                dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "b", &var);
                dbus_message_iter_append_basic(&var, DBUS_TYPE_BOOLEAN, &v);
                dbus_message_iter_close_container(&iter, &var);
            }
            else if (strcmp(prop_name, "CanTTY") == 0 || strcmp(prop_name, "CanGraphical") == 0) {
                dbus_bool_t v = 1;
                DBusMessageIter var;
                dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "b", &var);
                dbus_message_iter_append_basic(&var, DBUS_TYPE_BOOLEAN, &v);
                dbus_message_iter_close_container(&iter, &var);
            }
            else if (strcmp(prop_name, "Sessions") == 0) {
                DBusMessageIter var, arr;
                dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "a(so)", &var);
                dbus_message_iter_open_container(&var, DBUS_TYPE_ARRAY, "(so)", &arr);
                dbus_message_iter_close_container(&var, &arr);
                dbus_message_iter_close_container(&iter, &var);
            }
            else if (strcmp(prop_name, "IdleHint") == 0) {
                dbus_bool_t v = 0;
                DBusMessageIter var;
                dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "b", &var);
                dbus_message_iter_append_basic(&var, DBUS_TYPE_BOOLEAN, &v);
                dbus_message_iter_close_container(&iter, &var);
            }
            else if (strcmp(prop_name, "IdleSinceHint") == 0 ||
                     strcmp(prop_name, "IdleSinceHintMonotonic") == 0) {
                uint64_t v = 0;
                DBusMessageIter var;
                dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "t", &var);
                dbus_message_iter_append_basic(&var, DBUS_TYPE_UINT64, &v);
                dbus_message_iter_close_container(&iter, &var);
            }
            else {
                const char *v = "";
                DBusMessageIter var;
                dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "s", &var);
                dbus_message_iter_append_basic(&var, DBUS_TYPE_STRING, &v);
                dbus_message_iter_close_container(&iter, &var);
            }
        }
        else if (strcmp(prop_iface, "org.freedesktop.login1.User") == 0) {
            if (strcmp(prop_name, "UID") == 0) {
                uint32_t v = runtime_uid;
                DBusMessageIter var;
                dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "u", &var);
                dbus_message_iter_append_basic(&var, DBUS_TYPE_UINT32, &v);
                dbus_message_iter_close_container(&iter, &var);
            }
            else if (strcmp(prop_name, "GID") == 0) {
                uint32_t v = (uint32_t)getgid();
                DBusMessageIter var;
                dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "u", &var);
                dbus_message_iter_append_basic(&var, DBUS_TYPE_UINT32, &v);
                dbus_message_iter_close_container(&iter, &var);
            }
            else if (strcmp(prop_name, "Name") == 0) {
                const char *v = "user";
                DBusMessageIter var;
                dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "s", &var);
                dbus_message_iter_append_basic(&var, DBUS_TYPE_STRING, &v);
                dbus_message_iter_close_container(&iter, &var);
            }
            else if (strcmp(prop_name, "RuntimePath") == 0) {
                DBusMessageIter var;
                dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "s", &var);
                dbus_message_iter_append_basic(&var, DBUS_TYPE_STRING, &runtime_path);
                dbus_message_iter_close_container(&iter, &var);
            }
            else if (strcmp(prop_name, "Service") == 0 || strcmp(prop_name, "Slice") == 0) {
                const char *v = "";
                DBusMessageIter var;
                dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "s", &var);
                dbus_message_iter_append_basic(&var, DBUS_TYPE_STRING, &v);
                dbus_message_iter_close_container(&iter, &var);
            }
            else if (strcmp(prop_name, "Display") == 0) {
                DBusMessageIter var, struc;
                dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "(so)", &var);
                dbus_message_iter_open_container(&var, DBUS_TYPE_STRUCT, NULL, &struc);
                const char *sid = "1";
                char objpath[] = "/org/freedesktop/login1/session/1";
                dbus_message_iter_append_basic(&struc, DBUS_TYPE_STRING, &sid);
                dbus_message_iter_append_basic(&struc, DBUS_TYPE_OBJECT_PATH, &objpath);
                dbus_message_iter_close_container(&var, &struc);
                dbus_message_iter_close_container(&iter, &var);
            }
            else if (strcmp(prop_name, "State") == 0) {
                const char *v = "active";
                DBusMessageIter var;
                dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "s", &var);
                dbus_message_iter_append_basic(&var, DBUS_TYPE_STRING, &v);
                dbus_message_iter_close_container(&iter, &var);
            }
            else if (strcmp(prop_name, "Sessions") == 0) {
                DBusMessageIter var, arr;
                dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "a(so)", &var);
                dbus_message_iter_open_container(&var, DBUS_TYPE_ARRAY, "(so)", &arr);
                dbus_message_iter_close_container(&var, &arr);
                dbus_message_iter_close_container(&iter, &var);
            }
            else if (strcmp(prop_name, "IdleHint") == 0 || strcmp(prop_name, "Linger") == 0) {
                dbus_bool_t v = 0;
                DBusMessageIter var;
                dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "b", &var);
                dbus_message_iter_append_basic(&var, DBUS_TYPE_BOOLEAN, &v);
                dbus_message_iter_close_container(&iter, &var);
            }
            else if (strcmp(prop_name, "Timestamp") == 0 ||
                     strcmp(prop_name, "TimestampMonotonic") == 0 ||
                     strcmp(prop_name, "IdleSinceHint") == 0 ||
                     strcmp(prop_name, "IdleSinceHintMonotonic") == 0) {
                uint64_t v = 0;
                DBusMessageIter var;
                dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "t", &var);
                dbus_message_iter_append_basic(&var, DBUS_TYPE_UINT64, &v);
                dbus_message_iter_close_container(&iter, &var);
            }
            else if (strcmp(prop_name, "Wallpaper") == 0) {
                const char *v = "";
                DBusMessageIter var;
                dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "s", &var);
                dbus_message_iter_append_basic(&var, DBUS_TYPE_STRING, &v);
                dbus_message_iter_close_container(&iter, &var);
            }
            else {
                const char *v = "";
                DBusMessageIter var;
                dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "s", &var);
                dbus_message_iter_append_basic(&var, DBUS_TYPE_STRING, &v);
                dbus_message_iter_close_container(&iter, &var);
            }
        }
        else {
            const char *v = "";
            DBusMessageIter var;
            dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "s", &var);
            dbus_message_iter_append_basic(&var, DBUS_TYPE_STRING, &v);
            dbus_message_iter_close_container(&iter, &var);
        }
    }
    else if (strcmp(member, "GetAll") == 0) {
        DBusMessageIter dict;
        dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "{sv}", &dict);
        dbus_message_iter_close_container(&iter, &dict);
    }

    dbus_connection_send(conn, reply, NULL);
    dbus_message_unref(reply);
}

/* ─── Main loop ─── */
int main(void) {
    openlog("lainos-dbus-bridge", LOG_PID, LOG_DAEMON);
    syslog(LOG_INFO, "Protocol 7 login1 facade starting");

    init_runtime();

    DBusError err;
    dbus_error_init(&err);
    DBusConnection *conn = dbus_bus_get(DBUS_BUS_SYSTEM, &err);
    if (!conn) {
        syslog(LOG_ERR, "Failed to connect to system bus: %s", err.message);
        dbus_error_free(&err);
        return 1;
    }

    int ret = dbus_bus_request_name(conn, DBUS_NAME,
        DBUS_NAME_FLAG_REPLACE_EXISTING, &err);
    if (ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
        syslog(LOG_ERR, "Failed to acquire name %s: %s", DBUS_NAME, err.message);
        dbus_error_free(&err);
        dbus_connection_unref(conn);
        return 1;
    }

    drop_privileges();
    apply_seccomp();
    setup_signals();

    syslog(LOG_INFO, "login1 facade active on %s", DBUS_NAME);

    while (!shutdown_flag) {
        dbus_connection_read_write(conn, 100);
        DBusMessage *msg = dbus_connection_pop_message(conn);
        if (!msg) continue;

        int type = dbus_message_get_type(msg);
        if (type == DBUS_MESSAGE_TYPE_METHOD_CALL) {
            const char *iface = dbus_message_get_interface(msg);
            if (iface && strcmp(iface, "org.freedesktop.DBus.Properties") == 0) {
                handle_properties(conn, msg);
            } else {
                handle_method(conn, msg);
            }
        }

        dbus_message_unref(msg);
    }

    syslog(LOG_INFO, "login1 facade shutting down");
    dbus_connection_unref(conn);
    closelog();
    return 0;
}
