/*
 * libdbus-mock.c – Protocol 7 Sovereign D-Bus ABI Mock (libdbus-1.so.3)
 *
 * Minimal ABI-compatible stub to prevent dlopen/linkage crashes in Electron,
 * Flatpak runtimes, xdg-desktop-portal, and legacy desktop code.
 * Provides no real D-Bus functionality — pure compatibility layer.
 *
 * Hardened: -fPIC -shared, ELF note for audit, distinct fake pointers,
 *           minimal symbol surface, stripped output.
 *
 * Compile (for PKGBUILD):
 *   gcc -fPIC -shared -O2 -flto -s -Wl,-soname,libdbus-1.so.3 \
 *       -o libdbus-1.so.3 libdbus-mock.c
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

// Linux ELF note – visible via readelf -n
#ifdef __linux__
#include <elf.h>

__attribute__((section(".note.lainos.protocol7")))
__attribute__((used))
static const struct {
    Elf64_Word n_namesz;
    Elf64_Word n_descsz;
    Elf64_Word n_type;
    char       name[8];
    char       desc[64];
} note __attribute__((used)) = {
    .n_namesz = 7,
    .n_descsz = 64,
    .n_type   = 0x5057,
    .name     = "LainOS\0\0",
    .desc     = "P7-ABI-2026.01|libdbus-mock|no-real-bus|crash-prevention\0"
                "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
};
#endif

const char protocol7_libdbus_mock_version[] __attribute__((used)) =
    "P7-ABI-2026.01|libdbus-mock|no-real-bus";

/* ─── Minimal DBusError struct for ABI compatibility ─── */
typedef struct {
    const char *name;
    const char *message;
    unsigned int dummy1;
    unsigned int dummy2;
    unsigned int dummy3;
    int          padding;
    void        *free_func;
} DBusError;

typedef struct DBusConnection DBusConnection;
typedef struct DBusMessage DBusMessage;
typedef struct DBusPendingCall DBusPendingCall;
typedef int (*DBusHandleMessageFunction)(DBusConnection *connection, DBusMessage *message, void *user_data);
typedef void (*DBusFreeFunction)(void *memory);

typedef int dbus_bool_t;
typedef unsigned int dbus_uint32_t;
typedef int DBusBusType;

#define DBUS_TYPE_STRING   ((int) 's')
#define DBUS_TYPE_INVALID  ((int) '\0')
#define DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER 1

/* ─── Core stubs ─── */

void dbus_error_init(DBusError *error) {
    if (error) {
        error->name      = NULL;
        error->message   = NULL;
        error->dummy1    = 0;
        error->dummy2    = 0;
        error->free_func = NULL;
    }
}

dbus_bool_t dbus_error_is_set(const DBusError *error) {
    return 0;
}

void dbus_connection_unref(DBusConnection *connection) {
    (void)connection;
}

void dbus_message_unref(DBusMessage *message) {
    (void)message;
}

DBusConnection *dbus_bus_get(DBusBusType type, DBusError *error) {
    (void)type;
    if (error) dbus_error_init(error);
    return (DBusConnection *)0xdead0001;
}

DBusConnection *dbus_bus_get_private(DBusBusType type, DBusError *error) {
    return dbus_bus_get(type, error);
}

dbus_bool_t dbus_bus_name_has_owner(DBusConnection *connection,
                                    const char *name,
                                    DBusError *error) {
    (void)connection; (void)name;
    if (error) dbus_error_init(error);
    return 0;
}

dbus_bool_t dbus_bus_request_name(DBusConnection *connection,
                                  const char *name,
                                  unsigned int flags,
                                  DBusError *error) {
    (void)connection; (void)name; (void)flags;
    if (error) dbus_error_init(error);
    return DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER;
}

DBusMessage *dbus_message_new_method_call(const char *bus_name,
                                          const char *path,
                                          const char *interface,
                                          const char *method) {
    (void)bus_name; (void)path; (void)interface; (void)method;
    return (DBusMessage *)0xbeef0001;
}

dbus_bool_t dbus_message_append_args(DBusMessage *message,
                                     int first_arg_type,
                                     ...) {
    (void)message; (void)first_arg_type;
    return 1;
}

dbus_bool_t dbus_connection_send(DBusConnection *connection,
                                 DBusMessage *message,
                                 dbus_uint32_t *serial) {
    (void)connection; (void)message;
    if (serial) *serial = 1;
    return 1;
}

dbus_bool_t dbus_connection_send_with_reply(DBusConnection *connection,
                                            DBusMessage *message,
                                            DBusPendingCall **pending_return,
                                            int timeout_milliseconds) {
    (void)connection; (void)message; (void)timeout_milliseconds;
    if (pending_return) *pending_return = NULL;
    return 1;
}

dbus_bool_t dbus_connection_open(const char *address, DBusError *error) {
    (void)address;
    if (error) dbus_error_init(error);
    return 1;
}

dbus_bool_t dbus_connection_open_private(const char *address, DBusError *error) {
    return dbus_connection_open(address, error);
}

dbus_bool_t dbus_connection_add_filter(DBusConnection *connection,
                                       DBusHandleMessageFunction function,
                                       void *user_data,
                                       DBusFreeFunction free_data_function) {
    (void)connection; (void)function; (void)user_data; (void)free_data_function;
    return 1;
}
