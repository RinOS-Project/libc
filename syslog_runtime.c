/* RinOS libc syslog process-owned configuration. */

#include "syslog.h"
#include "pthread.h"

#define RIN_SYSLOG_IDENT_MAX 255u

static pthread_mutex_t g_syslog_lock = PTHREAD_MUTEX_INITIALIZER;
static char g_syslog_ident[RIN_SYSLOG_IDENT_MAX + 1u];

const char* _syslog_ident = NULL;
int _syslog_option = 0;
int _syslog_facility = LOG_USER;
int _syslog_mask = 0xFF;

void _rin_syslog_lock(void) {
    (void)pthread_mutex_lock(&g_syslog_lock);
}

void _rin_syslog_unlock(void) {
    (void)pthread_mutex_unlock(&g_syslog_lock);
}

/* Called under g_syslog_lock. Copy the identifier so subsequent logging does
 * not depend on caller-owned storage remaining alive after openlog(). */
void _rin_syslog_set_ident_locked(const char* ident) {
    if (!ident) {
        g_syslog_ident[0] = '\0';
        _syslog_ident = NULL;
        return;
    }

    size_t length = 0u;
    while (length < RIN_SYSLOG_IDENT_MAX && ident[length] != '\0') {
        g_syslog_ident[length] = ident[length];
        ++length;
    }
    g_syslog_ident[length] = '\0';
    _syslog_ident = g_syslog_ident;
}
