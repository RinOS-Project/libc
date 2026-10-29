/* SPDX-License-Identifier: MIT */
#ifndef RIN_LIBC_INTERNAL_NETDB_DATABASE_H
#define RIN_LIBC_INTERNAL_NETDB_DATABASE_H

#ifndef RIN_NETDB_SERVICES_PATH
#define RIN_NETDB_SERVICES_PATH "/etc/services"
#endif
#ifndef RIN_NETDB_PROTOCOLS_PATH
#define RIN_NETDB_PROTOCOLS_PATH "/etc/protocols"
#endif
#ifndef RIN_NETDB_OPEN_SERVICES
#define RIN_NETDB_OPEN_SERVICES() fopen(RIN_NETDB_SERVICES_PATH, "r")
#endif
#ifndef RIN_NETDB_OPEN_PROTOCOLS
#define RIN_NETDB_OPEN_PROTOCOLS() fopen(RIN_NETDB_PROTOCOLS_PATH, "r")
#endif

typedef struct _NetdbServiceRecord {
    char name[RIN_NETDB_DATABASE_NAME_MAX];
    char aliases[RIN_NETDB_DATABASE_MAX_ALIASES]
                [RIN_NETDB_DATABASE_NAME_MAX];
    uint32_t alias_count;
    uint16_t port;
    char protocol[RIN_NETDB_DATABASE_NAME_MAX];
    int socket_type;
} _NetdbServiceRecord;

typedef struct _NetdbProtocolRecord {
    char name[RIN_NETDB_DATABASE_NAME_MAX];
    char aliases[RIN_NETDB_DATABASE_MAX_ALIASES]
                [RIN_NETDB_DATABASE_NAME_MAX];
    uint32_t alias_count;
    int number;
} _NetdbProtocolRecord;

static inline int _netdb_ascii_name_equal(const char* left,
                                           const char* right)
{
    size_t index = 0u;
    if (left == NULL || right == NULL) return 0;
    for (;;) {
        unsigned char l = (unsigned char)left[index];
        unsigned char r = (unsigned char)right[index];
        if (l >= (unsigned char)'A' && l <= (unsigned char)'Z')
            l = (unsigned char)(l + ((unsigned char)'a' - (unsigned char)'A'));
        if (r >= (unsigned char)'A' && r <= (unsigned char)'Z')
            r = (unsigned char)(r + ((unsigned char)'a' - (unsigned char)'A'));
        if (l != r) return 0;
        if (l == 0u) return 1;
        ++index;
    }
}

static inline int _netdb_copy_database_name(char* output,
                                             size_t capacity,
                                             const char* input)
{
    size_t length = 0u;
    if (output == NULL || capacity == 0u || input == NULL) return -1;
    while (input[length] != '\0') {
        if (length + 1u >= capacity) return -1;
        output[length] = input[length];
        ++length;
    }
    output[length] = '\0';
    return 0;
}

static inline int _netdb_database_space(unsigned char value)
{
    return value == (unsigned char)' ' || value == (unsigned char)'\t' ||
           value == (unsigned char)'\n' || value == (unsigned char)'\r' ||
           value == (unsigned char)'\v' || value == (unsigned char)'\f';
}

static inline char* _netdb_database_next_token(char** cursor)
{
    char* start;
    if (cursor == NULL || *cursor == NULL) return NULL;
    while (_netdb_database_space((unsigned char)**cursor)) ++*cursor;
    if (**cursor == '\0' || **cursor == '#') return NULL;
    start = *cursor;
    while (**cursor != '\0' && **cursor != '#' &&
           !_netdb_database_space((unsigned char)**cursor))
        ++*cursor;
    if (**cursor != '\0') {
        if (**cursor == '#') {
            **cursor = '\0';
            return start;
        }
        **cursor = '\0';
        ++*cursor;
    }
    return start;
}

static inline int _netdb_database_parse_uint(const char* text,
                                              uint32_t maximum,
                                              uint32_t* value_out)
{
    uint32_t value = 0u;
    size_t index = 0u;
    if (text == NULL || text[0] == '\0' || value_out == NULL) return -1;
    for (; text[index] != '\0'; ++index) {
        uint32_t digit;
        if (text[index] < '0' || text[index] > '9') return -1;
        digit = (uint32_t)(text[index] - '0');
        if (value > (maximum - digit) / 10u) return -1;
        value = value * 10u + digit;
    }
    *value_out = value;
    return 0;
}

static inline int _netdb_parse_service_line(char* line,
                                             _NetdbServiceRecord* record)
{
    char* cursor = line;
    char* name = _netdb_database_next_token(&cursor);
    char* port_protocol = _netdb_database_next_token(&cursor);
    char* slash;
    char* alias;
    uint32_t port;
    if (name == NULL || port_protocol == NULL) return 0;
    slash = strchr(port_protocol, '/');
    if (slash == NULL || slash == port_protocol || slash[1] == '\0' ||
        strchr(slash + 1, '/') != NULL)
        return 0;
    *slash = '\0';
    if (_netdb_database_parse_uint(port_protocol, UINT16_MAX, &port) != 0)
        return 0;
    memset(record, 0, sizeof(*record));
    if (_netdb_copy_database_name(record->name, sizeof(record->name), name) !=
            0 ||
        _netdb_copy_database_name(record->protocol,
                                  sizeof(record->protocol), slash + 1) != 0)
        return 0;
    record->port = (uint16_t)port;
    if (_netdb_ascii_name_equal(record->protocol, "tcp"))
        record->socket_type = SOCK_STREAM;
    else if (_netdb_ascii_name_equal(record->protocol, "udp"))
        record->socket_type = SOCK_DGRAM;
    while ((alias = _netdb_database_next_token(&cursor)) != NULL) {
        uint32_t prior;
        if (record->alias_count >= RIN_NETDB_DATABASE_MAX_ALIASES ||
            _netdb_ascii_name_equal(alias, record->name))
            return 0;
        for (prior = 0u; prior < record->alias_count; ++prior)
            if (_netdb_ascii_name_equal(alias, record->aliases[prior]))
                return 0;
        if (_netdb_copy_database_name(
                record->aliases[record->alias_count],
                sizeof(record->aliases[record->alias_count]), alias) != 0)
            return 0;
        ++record->alias_count;
    }
    return 1;
}

static inline int _netdb_parse_protocol_line(char* line,
                                              _NetdbProtocolRecord* record)
{
    char* cursor = line;
    char* name = _netdb_database_next_token(&cursor);
    char* number_text = _netdb_database_next_token(&cursor);
    char* alias;
    uint32_t number;
    if (name == NULL || number_text == NULL ||
        _netdb_database_parse_uint(number_text, UINT8_MAX, &number) != 0)
        return 0;
    memset(record, 0, sizeof(*record));
    if (_netdb_copy_database_name(record->name, sizeof(record->name), name) !=
        0)
        return 0;
    record->number = (int)number;
    while ((alias = _netdb_database_next_token(&cursor)) != NULL) {
        uint32_t prior;
        if (record->alias_count >= RIN_NETDB_DATABASE_MAX_ALIASES ||
            _netdb_ascii_name_equal(alias, record->name))
            return 0;
        for (prior = 0u; prior < record->alias_count; ++prior)
            if (_netdb_ascii_name_equal(alias, record->aliases[prior]))
                return 0;
        if (_netdb_copy_database_name(
                record->aliases[record->alias_count],
                sizeof(record->aliases[record->alias_count]), alias) != 0)
            return 0;
        ++record->alias_count;
    }
    return 1;
}

static inline int _netdb_next_database_line(FILE* file, char* line,
                                             size_t capacity)
{
    while (fgets(line, (int)capacity, file) != NULL) {
        size_t length = strlen(line);
        if (length != 0u && line[length - 1u] != '\n' && !feof(file)) {
            int byte;
            byte = fgetc(file);
            if (byte == EOF && !ferror(file)) return 1;
            while ((byte = fgetc(file)) != EOF && byte != '\n') {
            }
            continue;
        }
        return 1;
    }
    return ferror(file) ? -1 : 0;
}

static inline int _netdb_next_service_record(FILE* file,
                                              _NetdbServiceRecord* record)
{
    char line[RIN_NETDB_DATABASE_LINE_MAX];
    int result;
    while ((result = _netdb_next_database_line(file, line, sizeof(line))) > 0)
        if (_netdb_parse_service_line(line, record) != 0) return 1;
    return result;
}

static inline int _netdb_next_protocol_record(FILE* file,
                                               _NetdbProtocolRecord* record)
{
    char line[RIN_NETDB_DATABASE_LINE_MAX];
    int result;
    while ((result = _netdb_next_database_line(file, line, sizeof(line))) > 0)
        if (_netdb_parse_protocol_line(line, record) != 0) return 1;
    return result;
}

static inline int _netdb_service_name_matches(
    const char* query, const _NetdbServiceRecord* record)
{
    uint32_t index;
    if (_netdb_ascii_name_equal(query, record->name)) return 1;
    for (index = 0u; index < record->alias_count; ++index)
        if (_netdb_ascii_name_equal(query, record->aliases[index])) return 1;
    return 0;
}

static inline int _netdb_protocol_name_matches(
    const char* query, const _NetdbProtocolRecord* record)
{
    uint32_t index;
    if (_netdb_ascii_name_equal(query, record->name)) return 1;
    for (index = 0u; index < record->alias_count; ++index)
        if (_netdb_ascii_name_equal(query, record->aliases[index])) return 1;
    return 0;
}

typedef FILE* (*_NetdbDatabaseOpen)(void);

static inline FILE* _netdb_open_services_database(void)
{
    return RIN_NETDB_OPEN_SERVICES();
}

static inline FILE* _netdb_open_protocols_database(void)
{
    return RIN_NETDB_OPEN_PROTOCOLS();
}

static inline int _netdb_database_lookup_begin(
    FILE** cached_file, long* database_offset, _NetdbDatabaseOpen open_file,
    FILE** file_out, long* restore_offset, int* opened_out)
{
    FILE* file;
    if (cached_file == NULL || database_offset == NULL || open_file == NULL ||
        file_out == NULL || restore_offset == NULL || opened_out == NULL)
        return -1;
    file = *cached_file;
    *opened_out = file == NULL;
    if (file == NULL) {
        file = open_file();
        if (file == NULL) return -1;
        *restore_offset = *database_offset;
    } else {
        *restore_offset = ftell(file);
        if (*restore_offset < 0) {
            int error = errno != 0 ? errno : EIO;
            (void)fclose(file);
            *cached_file = NULL;
            *database_offset = 0;
            errno = error;
            return -1;
        }
    }
    if (fseek(file, 0L, SEEK_SET) != 0) {
        int saved_errno = errno != 0 ? errno : EIO;
        if (*opened_out) {
            (void)fclose(file);
        } else {
            (void)fclose(file);
            *cached_file = NULL;
            *database_offset = 0;
        }
        errno = saved_errno;
        return -1;
    }
    *file_out = file;
    return 0;
}

static inline int _netdb_database_lookup_finish(
    FILE** cached_file, long* database_offset, int stayopen, FILE* file,
    long restore_offset, int opened, int status, int saved_errno)
{
    if (status < 0) {
        int error = errno == saved_errno ? EIO : errno;
        if (cached_file != NULL && *cached_file == file)
            *cached_file = NULL;
        if (database_offset != NULL) *database_offset = 0;
        (void)fclose(file);
        errno = error;
        return -1;
    }
    if (opened && stayopen) {
        if (fseek(file, *database_offset, SEEK_SET) != 0) {
            int error = errno == saved_errno ? EIO : errno;
            (void)fclose(file);
            errno = error;
            return -1;
        }
        *cached_file = file;
    } else if (opened) {
        (void)fclose(file);
    } else if (fseek(file, restore_offset, SEEK_SET) != 0) {
        int error = errno == saved_errno ? EIO : errno;
        (void)fclose(file);
        *cached_file = NULL;
        *database_offset = 0;
        errno = error;
        return -1;
    }
    errno = saved_errno;
    return status;
}

static inline int _netdb_find_service_by_name(
    const char* name, const char* protocol, int socket_type,
    _NetdbServiceRecord* output)
{
    __rin_netdb_runtime_state_v1* state = &__rin_netdb_runtime_state;
    FILE* file = NULL;
    _NetdbServiceRecord candidate;
    long restore_offset;
    int opened;
    int status;
    int saved_errno = errno;
    if (name == NULL || output == NULL) return 0;
    if (_netdb_database_lookup_begin(
            &state->service_file, &state->service_file_offset,
            _netdb_open_services_database, &file, &restore_offset,
            &opened) != 0)
        return -1;
    while ((status = _netdb_next_service_record(file, &candidate)) > 0) {
        if (!_netdb_service_name_matches(name, &candidate) ||
            (protocol != NULL &&
             !_netdb_ascii_name_equal(protocol, candidate.protocol)) ||
            (socket_type != 0 && socket_type != candidate.socket_type))
            continue;
        *output = candidate;
        status = 1;
        break;
    }
    status = _netdb_database_lookup_finish(
        &state->service_file, &state->service_file_offset,
        state->service_stayopen, file, restore_offset, opened, status,
        saved_errno);
    return status;
}

static inline int _netdb_find_service_by_port(
    uint16_t port, const char* protocol, _NetdbServiceRecord* output)
{
    __rin_netdb_runtime_state_v1* state = &__rin_netdb_runtime_state;
    FILE* file = NULL;
    _NetdbServiceRecord candidate;
    long restore_offset;
    int opened;
    int status;
    int saved_errno = errno;
    if (output == NULL) return 0;
    if (_netdb_database_lookup_begin(
            &state->service_file, &state->service_file_offset,
            _netdb_open_services_database, &file, &restore_offset,
            &opened) != 0)
        return -1;
    while ((status = _netdb_next_service_record(file, &candidate)) > 0) {
        if (candidate.port != port ||
            (protocol != NULL &&
             !_netdb_ascii_name_equal(protocol, candidate.protocol)))
            continue;
        *output = candidate;
        status = 1;
        break;
    }
    return _netdb_database_lookup_finish(
        &state->service_file, &state->service_file_offset,
        state->service_stayopen, file, restore_offset, opened, status,
        saved_errno);
}

static inline int _netdb_find_protocol_by_name(
    const char* name, _NetdbProtocolRecord* output)
{
    __rin_netdb_runtime_state_v1* state = &__rin_netdb_runtime_state;
    FILE* file = NULL;
    _NetdbProtocolRecord candidate;
    long restore_offset;
    int opened;
    int status;
    int saved_errno = errno;
    if (name == NULL || output == NULL) return 0;
    if (_netdb_database_lookup_begin(
            &state->protocol_file, &state->protocol_file_offset,
            _netdb_open_protocols_database, &file, &restore_offset,
            &opened) != 0)
        return -1;
    while ((status = _netdb_next_protocol_record(file, &candidate)) > 0) {
        if (!_netdb_protocol_name_matches(name, &candidate)) continue;
        *output = candidate;
        status = 1;
        break;
    }
    return _netdb_database_lookup_finish(
        &state->protocol_file, &state->protocol_file_offset,
        state->protocol_stayopen, file, restore_offset, opened, status,
        saved_errno);
}

static inline int _netdb_find_protocol_by_number(
    int number, _NetdbProtocolRecord* output)
{
    __rin_netdb_runtime_state_v1* state = &__rin_netdb_runtime_state;
    FILE* file = NULL;
    _NetdbProtocolRecord candidate;
    long restore_offset;
    int opened;
    int status;
    int saved_errno = errno;
    if (output == NULL) return 0;
    if (_netdb_database_lookup_begin(
            &state->protocol_file, &state->protocol_file_offset,
            _netdb_open_protocols_database, &file, &restore_offset,
            &opened) != 0)
        return -1;
    while ((status = _netdb_next_protocol_record(file, &candidate)) > 0) {
        if (candidate.number != number) continue;
        *output = candidate;
        status = 1;
        break;
    }
    return _netdb_database_lookup_finish(
        &state->protocol_file, &state->protocol_file_offset,
        state->protocol_stayopen, file, restore_offset, opened, status,
        saved_errno);
}

static inline struct servent* _netdb_publish_service(
    const _NetdbServiceRecord* record)
{
    __rin_netdb_runtime_state_v1* state = &__rin_netdb_runtime_state;
    uint32_t index;
    if (record == NULL ||
        _netdb_copy_database_name(state->service_name,
                                  sizeof(state->service_name),
                                  record->name) != 0 ||
        _netdb_copy_database_name(state->service_protocol,
                                  sizeof(state->service_protocol),
                                  record->protocol) != 0)
        return NULL;
    for (index = 0u; index < RIN_NETDB_DATABASE_MAX_ALIASES; ++index) {
        state->service_aliases[index] = NULL;
        if (index < record->alias_count) {
            if (_netdb_copy_database_name(
                    state->service_alias_storage[index],
                    sizeof(state->service_alias_storage[index]),
                    record->aliases[index]) != 0)
                return NULL;
            state->service_aliases[index] =
                state->service_alias_storage[index];
        }
    }
    state->service_aliases[RIN_NETDB_DATABASE_MAX_ALIASES] = NULL;
    state->service_result.s_name = state->service_name;
    state->service_result.s_aliases = state->service_aliases;
    state->service_result.s_port = (int)htons(record->port);
    state->service_result.s_proto = state->service_protocol;
    return &state->service_result;
}

static inline struct protoent* _netdb_publish_protocol(
    const _NetdbProtocolRecord* record)
{
    __rin_netdb_runtime_state_v1* state = &__rin_netdb_runtime_state;
    uint32_t index;
    if (record == NULL ||
        _netdb_copy_database_name(state->protocol_name,
                                  sizeof(state->protocol_name),
                                  record->name) != 0)
        return NULL;
    for (index = 0u; index < RIN_NETDB_DATABASE_MAX_ALIASES; ++index) {
        state->protocol_aliases[index] = NULL;
        if (index < record->alias_count) {
            if (_netdb_copy_database_name(
                    state->protocol_alias_storage[index],
                    sizeof(state->protocol_alias_storage[index]),
                    record->aliases[index]) != 0)
                return NULL;
            state->protocol_aliases[index] =
                state->protocol_alias_storage[index];
        }
    }
    state->protocol_aliases[RIN_NETDB_DATABASE_MAX_ALIASES] = NULL;
    state->protocol_result.p_name = state->protocol_name;
    state->protocol_result.p_aliases = state->protocol_aliases;
    state->protocol_result.p_proto = record->number;
    return &state->protocol_result;
}

static inline struct servent* getservent(void)
{
    __rin_netdb_runtime_state_v1* state = &__rin_netdb_runtime_state;
    _NetdbServiceRecord record;
    FILE* file = state->service_file;
    int local_file = 0;
    int status;
    long next_offset;
    int saved_errno = errno;
    if (file == NULL) {
        file = RIN_NETDB_OPEN_SERVICES();
        if (file == NULL) return NULL;
        local_file = 1;
        if (fseek(file, state->service_file_offset, SEEK_SET) != 0) {
            int error = errno != 0 ? errno : EIO;
            (void)fclose(file);
            errno = error;
            return NULL;
        }
    }
    status = _netdb_next_service_record(file, &record);
    next_offset = ftell(file);
    if (status > 0 && next_offset < 0) status = -1;
    if (local_file && state->service_stayopen != 0) {
        state->service_file = file;
    } else if (local_file) {
        (void)fclose(file);
    }
    if (status <= 0) {
        if (status < 0) {
            state->service_file_offset = 0;
            if (state->service_file != NULL) {
                (void)fclose(state->service_file);
                state->service_file = NULL;
            }
            if (errno == saved_errno) errno = EIO;
        } else {
            if (next_offset >= 0) state->service_file_offset = next_offset;
            errno = saved_errno;
        }
        return NULL;
    }
    state->service_file_offset = next_offset;
    {
        struct servent* result = _netdb_publish_service(&record);
        if (result == NULL) return NULL;
        errno = saved_errno;
        return result;
    }
}

static inline void setservent(int stayopen)
{
    __rin_netdb_runtime_state_v1* state = &__rin_netdb_runtime_state;
    if (state->service_file != NULL) {
        (void)fclose(state->service_file);
        state->service_file = NULL;
    }
    state->service_file_offset = 0;
    state->service_stayopen = stayopen != 0;
}

static inline void endservent(void)
{
    __rin_netdb_runtime_state_v1* state = &__rin_netdb_runtime_state;
    if (state->service_file != NULL) {
        (void)fclose(state->service_file);
        state->service_file = NULL;
    }
    state->service_file_offset = 0;
    state->service_stayopen = 0;
}

static inline struct protoent* getprotoent(void)
{
    __rin_netdb_runtime_state_v1* state = &__rin_netdb_runtime_state;
    _NetdbProtocolRecord record;
    FILE* file = state->protocol_file;
    int local_file = 0;
    int status;
    long next_offset;
    int saved_errno = errno;
    if (file == NULL) {
        file = RIN_NETDB_OPEN_PROTOCOLS();
        if (file == NULL) return NULL;
        local_file = 1;
        if (fseek(file, state->protocol_file_offset, SEEK_SET) != 0) {
            int error = errno != 0 ? errno : EIO;
            (void)fclose(file);
            errno = error;
            return NULL;
        }
    }
    status = _netdb_next_protocol_record(file, &record);
    next_offset = ftell(file);
    if (status > 0 && next_offset < 0) status = -1;
    if (local_file && state->protocol_stayopen != 0) {
        state->protocol_file = file;
    } else if (local_file) {
        (void)fclose(file);
    }
    if (status <= 0) {
        if (status < 0) {
            state->protocol_file_offset = 0;
            if (state->protocol_file != NULL) {
                (void)fclose(state->protocol_file);
                state->protocol_file = NULL;
            }
            if (errno == saved_errno) errno = EIO;
        } else {
            if (next_offset >= 0) state->protocol_file_offset = next_offset;
            errno = saved_errno;
        }
        return NULL;
    }
    state->protocol_file_offset = next_offset;
    {
        struct protoent* result = _netdb_publish_protocol(&record);
        if (result == NULL) return NULL;
        errno = saved_errno;
        return result;
    }
}

static inline void setprotoent(int stayopen)
{
    __rin_netdb_runtime_state_v1* state = &__rin_netdb_runtime_state;
    if (state->protocol_file != NULL) {
        (void)fclose(state->protocol_file);
        state->protocol_file = NULL;
    }
    state->protocol_file_offset = 0;
    state->protocol_stayopen = stayopen != 0;
}

static inline void endprotoent(void)
{
    __rin_netdb_runtime_state_v1* state = &__rin_netdb_runtime_state;
    if (state->protocol_file != NULL) {
        (void)fclose(state->protocol_file);
        state->protocol_file = NULL;
    }
    state->protocol_file_offset = 0;
    state->protocol_stayopen = 0;
}

#endif /* RIN_LIBC_INTERNAL_NETDB_DATABASE_H */
