/* RinOS libc string runtime state shared by all translation units. */

#include "string.h"

_Thread_local char* _rin_strtok_last = NULL;
