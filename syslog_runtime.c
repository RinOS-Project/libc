/* RinOS libc syslog process-owned configuration. */

#include "syslog.h"

const char* _syslog_ident = NULL;
int _syslog_option = 0;
int _syslog_facility = LOG_USER;
int _syslog_mask = 0xFF;
