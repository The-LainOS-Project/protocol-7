/*
 * libdbus-mock.c – Protocol 7 D-Bus Compatibility Stub (v4.3-tuned)
 *
 * ABI-compatible stub for libdbus-1.so.3 — prevents dlopen/linkage crashes
 * in Electron, Flatpak, xdg-desktop-portal, and other D-Bus consumers.
 *
 * Hardened: no-op heavy, distinct fake pointers, minimal surface.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>

/* ─── Type definitions ─── */
typedef struct DBusConnection DBusConnection;
typedef struct DBusMessage DBusMessage;
typedef struct DBusMessageIter DBusMessageIter;
typedef struct DBusPendingCall DBusPendingCall;
typedef struct DBusError DBusError;
typedef struct DBusWatch DBusWatch;
typedef struct DBusTimeout DBusTimeout;

struct DBusError {
    const char *name;
    const char *message;
    unsigned int dummy1 : 1;
    unsigned int dummy2 : 1;
    unsigned int dummy3 : 1;
    unsigned int dummy4 : 1;
    unsigned int dummy5 : 1;
    void *padding1;
};

struct DBusMessageIter {
    void *dummy1;
    void *dummy2;
    dbus_uint32_t dummy3;
    int dummy4;
    int dummy5;
    int dummy6;
    int dummy7;
    int dummy8;
    int dummy9;
    int dummy10;
    int dummy11;
    int pad1;
    void *pad2;
    void *pad3;
};

typedef enum {
    DBUS_BUS_SESSION,
    DBUS_BUS_SYSTEM,
    DBUS_BUS_STARTER
} DBusBusType;

typedef enum {
    DBUS_HANDLER_RESULT_HANDLED,
    DBUS_HANDLER_RESULT_NOT_YET_HANDLED,
    DBUS_HANDLER_RESULT_NEED_MEMORY
} DBusHandlerResult;

typedef enum {
    DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER = 1,
    DBUS_REQUEST_NAME_REPLY_IN_QUEUE,
    DBUS_REQUEST_NAME_REPLY_EXISTS,
    DBUS_REQUEST_NAME_REPLY_ALREADY_OWNER
} DBusRequestNameReply;

typedef enum {
    DBUS_WATCH_READABLE = 1,
    DBUS_WATCH_WRITABLE = 2,
    DBUS_WATCH_ERROR = 4,
    DBUS_WATCH_HANGUP = 8
} DBusWatchFlags;

typedef int (*DBusHandleMessageFunction)(DBusConnection *connection,
                                          DBusMessage *message,
                                          void *user_data);
typedef void (*DBusFreeFunction)(void *memory);
typedef void (*DBusPendingCallNotifyFunction)(DBusPendingCall *pending,
                                               void *user_data);

/* ─── Fake pointers (non-canonical hole) ─── */
#define FAKE_CONN  ((DBusConnection *)0x0000cafe00000001UL)
#define FAKE_MSG   ((DBusMessage *)0x0000cafe00000002UL)
#define FAKE_ITER  ((DBusMessageIter *)0x0000cafe00000003UL)
#define FAKE_PEND  ((DBusPendingCall *)0x0000cafe00000004UL)
#define FAKE_WATCH ((DBusWatch *)0x0000cafe00000005UL)
#define FAKE_TIMEOUT ((DBusTimeout *)0x0000cafe00000006UL)

/* ─── Error handling ─── */
void dbus_error_init(DBusError *error) {
    if (error) memset(error, 0, sizeof(DBusError));
}

void dbus_error_free(DBusError *error) {
    if (error) {
        free((void *)error->name);
        free((void *)error->message);
        dbus_error_init(error);
    }
}

void dbus_set_error(DBusError *error, const char *name, const char *format, ...) {
    if (!error) return;
    dbus_error_free(error);
    error->name = name ? strdup(name) : NULL;
    if (format) {
        char buf[1024];
        va_list ap;
        va_start(ap, format);
        vsnprintf(buf, sizeof(buf), format, ap);
        va_end(ap);
        error->message = strdup(buf);
    }
}

void dbus_set_error_const(DBusError *error, const char *name, const char *message) {
    if (!error) return;
    dbus_error_free(error);
    error->name = name;
    error->message = message;
    error->dummy1 = 0;
}

int dbus_error_is_set(const DBusError *error) {
    return error && error->name != NULL;
}

const char *dbus_error_name(const DBusError *error) {
    return error ? error->name : NULL;
}

const char *dbus_error_message(const DBusError *error) {
    return error ? error->message : NULL;
}

/* ─── Connection ─── */
DBusConnection *dbus_bus_get(DBusBusType type, DBusError *error) {
    (void)type;
    if (error) dbus_error_init(error);
    return FAKE_CONN;
}

DBusConnection *dbus_bus_get_private(DBusBusType type, DBusError *error) {
    (void)type;
    if (error) dbus_error_init(error);
    return FAKE_CONN;
}

DBusConnection *dbus_connection_open(const char *address, DBusError *error) {
    (void)address;
    if (error) dbus_error_init(error);
    return FAKE_CONN;
}

DBusConnection *dbus_connection_open_private(const char *address, DBusError *error) {
    (void)address;
    if (error) dbus_error_init(error);
    return FAKE_CONN;
}

void dbus_connection_ref(DBusConnection *connection) {
    (void)connection;
}

void dbus_connection_unref(DBusConnection *connection) {
    (void)connection;
}

void dbus_connection_close(DBusConnection *connection) {
    (void)connection;
}

int dbus_connection_get_is_connected(DBusConnection *connection) {
    (void)connection;
    return 1;
}

int dbus_connection_get_is_authenticated(DBusConnection *connection) {
    (void)connection;
    return 1;
}

int dbus_connection_get_is_anonymous(DBusConnection *connection) {
    (void)connection;
    return 0;
}

const char *dbus_connection_get_server_id(DBusConnection *connection) {
    (void)connection;
    return "protocol7-mock";
}

int dbus_connection_can_send_type(DBusConnection *connection, int type) {
    (void)connection; (void)type;
    return 1;
}

/* ─── Message ─── */
DBusMessage *dbus_message_new(int message_type) {
    (void)message_type;
    return FAKE_MSG;
}

DBusMessage *dbus_message_new_method_call(const char *destination,
                                            const char *path,
                                            const char *iface,
                                            const char *method) {
    (void)destination; (void)path; (void)iface; (void)method;
    return FAKE_MSG;
}

DBusMessage *dbus_message_new_method_return(DBusMessage *method_call) {
    (void)method_call;
    return FAKE_MSG;
}

DBusMessage *dbus_message_new_signal(const char *path,
                                      const char *iface,
                                      const char *name) {
    (void)path; (void)iface; (void)name;
    return FAKE_MSG;
}

DBusMessage *dbus_message_new_error(DBusMessage *reply_to,
                                     const char *error_name,
                                     const char *error_message) {
    (void)reply_to; (void)error_name; (void)error_message;
    return FAKE_MSG;
}

DBusMessage *dbus_message_ref(DBusMessage *message) {
    return message;
}

void dbus_message_unref(DBusMessage *message) {
    (void)message;
}

int dbus_message_get_type(DBusMessage *message) {
    (void)message;
    return 1; /* DBUS_MESSAGE_TYPE_METHOD_CALL */
}

int dbus_message_is_method_call(DBusMessage *message,
                                 const char *iface,
                                 const char *method) {
    (void)message; (void)iface; (void)method;
    return 1;
}

int dbus_message_is_signal(DBusMessage *message,
                            const char *iface,
                            const char *signal_name) {
    (void)message; (void)iface; (void)signal_name;
    return 0;
}

int dbus_message_is_error(DBusMessage *message, const char *error_name) {
    (void)message; (void)error_name;
    return 0;
}

const char *dbus_message_get_destination(DBusMessage *message) {
    (void)message;
    return NULL;
}

const char *dbus_message_get_path(DBusMessage *message) {
    (void)message;
    return "/org/freedesktop/DBus";
}

const char *dbus_message_get_interface(DBusMessage *message) {
    (void)message;
    return "org.freedesktop.DBus";
}

const char *dbus_message_get_member(DBusMessage *message) {
    (void)message;
    return "MockMethod";
}

const char *dbus_message_get_sender(DBusMessage *message) {
    (void)message;
    return ":1.42";
}

const char *dbus_message_get_error_name(DBusMessage *message) {
    (void)message;
    return NULL;
}

int dbus_message_get_reply_serial(DBusMessage *message) {
    (void)message;
    return 0;
}

int dbus_message_get_serial(DBusMessage *message) {
    (void)message;
    return 42;
}

/* ─── Message Iterators ─── */
void dbus_message_iter_init_append(DBusMessage *message, DBusMessageIter *iter) {
    (void)message;
    if (iter) memset(iter, 0, sizeof(DBusMessageIter));
}

int dbus_message_iter_init(DBusMessage *message, DBusMessageIter *iter) {
    (void)message;
    if (iter) memset(iter, 0, sizeof(DBusMessageIter));
    return 1;
}

int dbus_message_iter_has_next(DBusMessageIter *iter) {
    (void)iter;
    return 0;
}

int dbus_message_iter_next(DBusMessageIter *iter) {
    (void)iter;
    return 0;
}

int dbus_message_iter_get_arg_type(DBusMessageIter *iter) {
    (void)iter;
    return 0;
}

int dbus_message_iter_get_element_type(DBusMessageIter *iter) {
    (void)iter;
    return 0;
}

void dbus_message_iter_recurse(DBusMessageIter *iter, DBusMessageIter *sub) {
    (void)iter;
    if (sub) memset(sub, 0, sizeof(DBusMessageIter));
}

void dbus_message_iter_get_basic(DBusMessageIter *iter, void *value) {
    (void)iter; (void)value;
}

void dbus_message_iter_get_fixed_array(DBusMessageIter *iter, void *value, int *n_elements) {
    (void)iter;
    if (value) *(void **)value = NULL;
    if (n_elements) *n_elements = 0;
}

void dbus_message_iter_append_basic(DBusMessageIter *iter, int type, const void *value) {
    (void)iter; (void)type; (void)value;
}

int dbus_message_iter_open_container(DBusMessageIter *iter, int type, const char *contained_signature, DBusMessageIter *sub) {
    (void)iter; (void)type; (void)contained_signature;
    if (sub) memset(sub, 0, sizeof(DBusMessageIter));
    return 1;
}

int dbus_message_iter_close_container(DBusMessageIter *iter, DBusMessageIter *sub) {
    (void)iter; (void)sub;
    return 1;
}

void dbus_message_iter_append_fixed_array(DBusMessageIter *iter, int element_type, const void *value, int n_elements) {
    (void)iter; (void)element_type; (void)value; (void)n_elements;
}

/* ─── Message args ─── */
int dbus_message_get_args(DBusMessage *message, DBusError *error, int first_arg_type, ...) {
    (void)message;
    if (error) dbus_error_init(error);
    (void)first_arg_type;
    return 1;
}

int dbus_message_get_args_valist(DBusMessage *message, DBusError *error, int first_arg_type, va_list var_args) {
    (void)message;
    if (error) dbus_error_init(error);
    (void)first_arg_type; (void)var_args;
    return 1;
}

int dbus_message_append_args(DBusMessage *message, int first_arg_type, ...) {
    (void)message;
    (void)first_arg_type;
    return 1;
}

int dbus_message_append_args_valist(DBusMessage *message, int first_arg_type, va_list var_args) {
    (void)message;
    (void)first_arg_type; (void)var_args;
    return 1;
}

/* ─── Message setters ─── */
int dbus_message_set_destination(DBusMessage *message, const char *destination) {
    (void)message; (void)destination;
    return 1;
}

int dbus_message_set_path(DBusMessage *message, const char *object_path) {
    (void)message; (void)object_path;
    return 1;
}

int dbus_message_set_interface(DBusMessage *message, const char *iface) {
    (void)message; (void)iface;
    return 1;
}

int dbus_message_set_member(DBusMessage *message, const char *member) {
    (void)message; (void)member;
    return 1;
}

/* ─── Sending ─── */
int dbus_connection_send(DBusConnection *connection, DBusMessage *message, dbus_uint32_t *serial) {
    (void)connection; (void)message;
    if (serial) *serial = 42;
    return 1;
}

DBusMessage *dbus_connection_send_with_reply_and_block(DBusConnection *connection,
                                                        DBusMessage *message,
                                                        int timeout_milliseconds,
                                                        DBusError *error) {
    (void)connection; (void)message; (void)timeout_milliseconds;
    if (error) dbus_error_init(error);
    return FAKE_MSG;
}

int dbus_connection_send_with_reply(DBusConnection *connection,
                                     DBusMessage *message,
                                     DBusPendingCall **pending_return,
                                     int timeout_milliseconds) {
    (void)connection; (void)message; (void)timeout_milliseconds;
    if (pending_return) *pending_return = FAKE_PEND;
    return 1;
}

/* ─── Pending call ─── */
void dbus_pending_call_ref(DBusPendingCall *pending) {
    (void)pending;
}

void dbus_pending_call_unref(DBusPendingCall *pending) {
    (void)pending;
}

int dbus_pending_call_set_notify(DBusPendingCall *pending,
                                  DBusPendingCallNotifyFunction function,
                                  void *user_data,
                                  DBusFreeFunction free_user_data) {
    (void)pending; (void)function; (void)user_data; (void)free_user_data;
    return 1;
}

int dbus_pending_call_get_completed(DBusPendingCall *pending) {
    (void)pending;
    return 1;
}

DBusMessage *dbus_pending_call_steal_reply(DBusPendingCall *pending) {
    (void)pending;
    return FAKE_MSG;
}

void dbus_pending_call_block(DBusPendingCall *pending) {
    (void)pending;
}

/* ─── Bus name ─── */
int dbus_bus_request_name(DBusConnection *connection,
                            const char *name,
                            unsigned int flags,
                            DBusError *error) {
    (void)connection; (void)name; (void)flags;
    if (error) dbus_error_init(error);
    return DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER;
}

int dbus_bus_release_name(DBusConnection *connection,
                            const char *name,
                            DBusError *error) {
    (void)connection; (void)name;
    if (error) dbus_error_init(error);
    return 1;
}

/* ─── Match rules ─── */
int dbus_bus_add_match(DBusConnection *connection,
                          const char *rule,
                          DBusError *error) {
    (void)connection; (void)rule;
    if (error) dbus_error_init(error);
    return 1;
}

int dbus_bus_remove_match(DBusConnection *connection,
                            const char *rule,
                            DBusError *error) {
    (void)connection; (void)rule;
    if (error) dbus_error_init(error);
    return 1;
}

/* ─── Filters / Handlers ─── */
void dbus_connection_add_filter(DBusConnection *connection,
                                 DBusHandleMessageFunction function,
                                 void *user_data,
                                 DBusFreeFunction free_data_function) {
    (void)connection; (void)function; (void)user_data; (void)free_data_function;
}

void dbus_connection_remove_filter(DBusConnection *connection,
                                    DBusHandleMessageFunction function,
                                    void *user_data) {
    (void)connection; (void)function; (void)user_data;
}

/* ─── Watch / Timeout ─── */
int dbus_connection_set_watch_functions(DBusConnection *connection,
                                         DBusAddWatchFunction add_function,
                                         DBusRemoveWatchFunction remove_function,
                                         DBusWatchToggledFunction toggled_function,
                                         void *data,
                                         DBusFreeFunction free_data_function) {
    (void)connection; (void)add_function; (void)remove_function;
    (void)toggled_function; (void)data; (void)free_data_function;
    return 1;
}

int dbus_connection_set_timeout_functions(DBusConnection *connection,
                                            DBusAddTimeoutFunction add_function,
                                            DBusRemoveTimeoutFunction remove_function,
                                            DBusTimeoutToggledFunction toggled_function,
                                            void *data,
                                            DBusFreeFunction free_data_function) {
    (void)connection; (void)add_function; (void)remove_function;
    (void)toggled_function; (void)data; (void)free_data_function;
    return 1;
}

/* ─── Misc ─── */
void dbus_shutdown(void) {
}

const char *dbus_get_local_machine_id(void) {
    return "protocol7";
}

/* ─── Signature / Validation ─── */
int dbus_validate_path(const char *path, DBusError *error) {
    (void)path;
    if (error) dbus_error_init(error);
    return 1;
}

int dbus_validate_interface(const char *name, DBusError *error) {
    (void)name;
    if (error) dbus_error_init(error);
    return 1;
}

int dbus_validate_member(const char *name, DBusError *error) {
    (void)name;
    if (error) dbus_error_init(error);
    return 1;
}

int dbus_validate_error_name(const char *name, DBusError *error) {
    (void)name;
    if (error) dbus_error_init(error);
    return 1;
}

int dbus_validate_bus_name(const char *name, DBusError *error) {
    (void)name;
    if (error) dbus_error_init(error);
    return 1;
}

/* ─── Memory ─── */
void *dbus_malloc(size_t bytes) {
    return malloc(bytes);
}

void *dbus_malloc0(size_t bytes) {
    return calloc(1, bytes);
}

void *dbus_realloc(void *memory, size_t bytes) {
    return realloc(memory, bytes);
}

void dbus_free(void *memory) {
    free(memory);
}

/* ─── Thread init (no-op, we are not thread-safe by design) ─── */
void dbus_threads_init_default(void) {
}
