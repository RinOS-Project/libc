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

#ifndef RIN_NETDB_HOSTS_PATH
#define RIN_NETDB_HOSTS_PATH "/etc/hosts"
#endif
#ifndef RIN_NETDB_NETWORKS_PATH
#define RIN_NETDB_NETWORKS_PATH "/etc/networks"
#endif
#ifndef RIN_NETDB_OPEN_HOSTS
#define RIN_NETDB_OPEN_HOSTS() fopen(RIN_NETDB_HOSTS_PATH, "r")
#endif
#ifndef RIN_NETDB_OPEN_NETWORKS
#define RIN_NETDB_OPEN_NETWORKS() fopen(RIN_NETDB_NETWORKS_PATH, "r")
#endif

typedef struct _NetdbHostRecord {
    int family;
    uint8_t address[16];
    uint32_t address_length;
    char name[RIN_NETDB_DATABASE_NAME_MAX];
    char aliases[RIN_NETDB_DATABASE_MAX_ALIASES]
                [RIN_NETDB_DATABASE_NAME_MAX];
    uint32_t alias_count;
} _NetdbHostRecord;

typedef struct _NetdbNetworkRecord {
    char name[RIN_NETDB_DATABASE_NAME_MAX];
    char aliases[RIN_NETDB_DATABASE_MAX_ALIASES]
                [RIN_NETDB_DATABASE_NAME_MAX];
    uint32_t alias_count;
    uint32_t net;
} _NetdbNetworkRecord;

static inline int _netdb_parse_host_line(char* line,
                                         _NetdbHostRecord* record)
{
    char* cursor = line;
    char* address_text = _netdb_database_next_token(&cursor);
    char* name = _netdb_database_next_token(&cursor);
    char* alias;
    struct in_addr v4;
    struct in6_addr v6;
    if (address_text == NULL || name == NULL) return 0;
    memset(record, 0, sizeof(*record));
    if (inet_pton(AF_INET, address_text, &v4) == 1) {
        record->family = AF_INET;
        record->address_length = 4u;
        memcpy(record->address, &v4.s_addr, 4u);
    } else if (inet_pton(AF_INET6, address_text, &v6) == 1) {
        record->family = AF_INET6;
        record->address_length = 16u;
        memcpy(record->address, v6.s6_addr, 16u);
    } else {
        return 0;
    }
    if (_netdb_copy_database_name(record->name, sizeof(record->name), name) !=
        0)
        return 0;
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

static inline int _netdb_parse_network_line(char* line,
                                            _NetdbNetworkRecord* record)
{
    char* cursor = line;
    char* name = _netdb_database_next_token(&cursor);
    char* number_text = _netdb_database_next_token(&cursor);
    char* alias;
    struct in_addr v4;
    if (name == NULL || number_text == NULL) return 0;
    memset(record, 0, sizeof(*record));
    if (inet_pton(AF_INET, number_text, &v4) == 1) {
        record->net = ntohl(v4.s_addr);
    } else {
        char* end = NULL;
        unsigned long value = strtoul(number_text, &end, 0);
        if (end == number_text || *end != '\0' || value > UINT32_MAX)
            return 0;
        record->net = (uint32_t)value;
    }
    if (_netdb_copy_database_name(record->name, sizeof(record->name), name) !=
        0)
        return 0;
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

static inline int _netdb_next_host_record(FILE* file,
                                          _NetdbHostRecord* record)
{
    char line[RIN_NETDB_DATABASE_LINE_MAX];
    int result;
    while ((result = _netdb_next_database_line(file, line, sizeof(line))) > 0)
        if (_netdb_parse_host_line(line, record) != 0) return 1;
    return result;
}

static inline int _netdb_next_network_record(FILE* file,
                                             _NetdbNetworkRecord* record)
{
    char line[RIN_NETDB_DATABASE_LINE_MAX];
    int result;
    while ((result = _netdb_next_database_line(file, line, sizeof(line))) > 0)
        if (_netdb_parse_network_line(line, record) != 0) return 1;
    return result;
}

static inline FILE* _netdb_open_hosts_database(void)
{
    return RIN_NETDB_OPEN_HOSTS();
}

static inline FILE* _netdb_open_networks_database(void)
{
    return RIN_NETDB_OPEN_NETWORKS();
}

static inline int _netdb_host_name_matches(const char* query,
                                           const _NetdbHostRecord* record)
{
    uint32_t index;
    if (_netdb_ascii_name_equal(query, record->name)) return 1;
    for (index = 0u; index < record->alias_count; ++index)
        if (_netdb_ascii_name_equal(query, record->aliases[index])) return 1;
    return 0;
}

static inline int _netdb_network_name_matches(
    const char* query, const _NetdbNetworkRecord* record)
{
    uint32_t index;
    if (_netdb_ascii_name_equal(query, record->name)) return 1;
    for (index = 0u; index < record->alias_count; ++index)
        if (_netdb_ascii_name_equal(query, record->aliases[index])) return 1;
    return 0;
}

static inline int _netdb_find_network_by_name(
    const char* name, _NetdbNetworkRecord* output)
{
    __rin_netdb_runtime_state_v1* state = &__rin_netdb_runtime_state;
    FILE* file = NULL;
    _NetdbNetworkRecord candidate;
    long restore_offset;
    int opened;
    int status;
    int saved_errno = errno;
    if (name == NULL || output == NULL) return 0;
    if (_netdb_database_lookup_begin(
            &state->network_file, &state->network_file_offset,
            _netdb_open_networks_database, &file, &restore_offset,
            &opened) != 0)
        return -1;
    while ((status = _netdb_next_network_record(file, &candidate)) > 0) {
        if (!_netdb_network_name_matches(name, &candidate)) continue;
        *output = candidate;
        status = 1;
        break;
    }
    return _netdb_database_lookup_finish(
        &state->network_file, &state->network_file_offset,
        state->network_stayopen, file, restore_offset, opened, status,
        saved_errno);
}

static inline int _netdb_find_network_by_number(
    uint32_t net, _NetdbNetworkRecord* output)
{
    __rin_netdb_runtime_state_v1* state = &__rin_netdb_runtime_state;
    FILE* file = NULL;
    _NetdbNetworkRecord candidate;
    long restore_offset;
    int opened;
    int status;
    int saved_errno = errno;
    if (output == NULL) return 0;
    if (_netdb_database_lookup_begin(
            &state->network_file, &state->network_file_offset,
            _netdb_open_networks_database, &file, &restore_offset,
            &opened) != 0)
        return -1;
    while ((status = _netdb_next_network_record(file, &candidate)) > 0) {
        if (candidate.net != net) continue;
        *output = candidate;
        status = 1;
        break;
    }
    return _netdb_database_lookup_finish(
        &state->network_file, &state->network_file_offset,
        state->network_stayopen, file, restore_offset, opened, status,
        saved_errno);
}

static inline struct hostent* _netdb_publish_host(
    const _NetdbHostRecord* record)
{
    __rin_netdb_runtime_state_v1* state = &__rin_netdb_runtime_state;
    uint32_t index;
    if (record == NULL) return NULL;
    if (record->family == AF_INET && record->address_length == 4u) {
        memcpy(&state->addr, record->address, 4u);
        state->addr_list[0] = (char*)&state->addr;
    } else if (record->family == AF_INET6 && record->address_length == 16u) {
        memcpy(&state->host_addr6, record->address, 16u);
        state->addr_list[0] = (char*)&state->host_addr6;
    } else {
        return NULL;
    }
    state->addr_list[1] = NULL;
    /* The shared hostent aliases slot holds only the terminator, matching
     * gethostbyname.  The re-entrant gethostent_r publishes aliases. */
    state->aliases[0] = NULL;
    for (index = 0u; index < 256u; ++index) {
        state->hostname[index] = record->name[index];
        if (record->name[index] == '\0') break;
    }
    state->hostname[255] = '\0';
    state->hostent.h_name = state->hostname;
    state->hostent.h_aliases = state->aliases;
    state->hostent.h_addrtype = record->family;
    state->hostent.h_length = (int)record->address_length;
    state->hostent.h_addr_list = state->addr_list;
    (void)index;
    return &state->hostent;
}

static inline struct netent* _netdb_publish_network(
    const _NetdbNetworkRecord* record)
{
    __rin_netdb_runtime_state_v1* state = &__rin_netdb_runtime_state;
    uint32_t index;
    if (record == NULL ||
        _netdb_copy_database_name(state->network_name,
                                  sizeof(state->network_name),
                                  record->name) != 0)
        return NULL;
    for (index = 0u; index < RIN_NETDB_DATABASE_MAX_ALIASES; ++index) {
        state->network_aliases[index] = NULL;
        if (index < record->alias_count) {
            if (_netdb_copy_database_name(
                    state->network_alias_storage[index],
                    sizeof(state->network_alias_storage[index]),
                    record->aliases[index]) != 0)
                return NULL;
            state->network_aliases[index] =
                state->network_alias_storage[index];
        }
    }
    state->network_aliases[RIN_NETDB_DATABASE_MAX_ALIASES] = NULL;
    state->network_result.n_name = state->network_name;
    state->network_result.n_aliases = state->network_aliases;
    state->network_result.n_addrtype = AF_INET;
    state->network_result.n_net = record->net;
    return &state->network_result;
}

static inline struct hostent* gethostent(void)
{
    __rin_netdb_runtime_state_v1* state = &__rin_netdb_runtime_state;
    _NetdbHostRecord record;
    FILE* file = state->host_file;
    int local_file = 0;
    int status;
    long next_offset;
    int saved_errno = errno;
    if (file == NULL) {
        file = RIN_NETDB_OPEN_HOSTS();
        if (file == NULL) {
            h_errno = NO_RECOVERY;
            return NULL;
        }
        local_file = 1;
        if (fseek(file, state->host_file_offset, SEEK_SET) != 0) {
            int error = errno != 0 ? errno : EIO;
            (void)fclose(file);
            errno = error;
            h_errno = NO_RECOVERY;
            return NULL;
        }
    }
    status = _netdb_next_host_record(file, &record);
    next_offset = ftell(file);
    if (status > 0 && next_offset < 0) status = -1;
    if (local_file && state->host_stayopen != 0) {
        state->host_file = file;
    } else if (local_file) {
        (void)fclose(file);
    }
    if (status <= 0) {
        if (status < 0) {
            state->host_file_offset = 0;
            if (state->host_file != NULL) {
                (void)fclose(state->host_file);
                state->host_file = NULL;
            }
            if (errno == saved_errno) errno = EIO;
            h_errno = NO_RECOVERY;
        } else {
            if (next_offset >= 0) state->host_file_offset = next_offset;
            errno = saved_errno;
        }
        return NULL;
    }
    state->host_file_offset = next_offset;
    {
        struct hostent* result = _netdb_publish_host(&record);
        if (result == NULL) {
            h_errno = NO_RECOVERY;
            return NULL;
        }
        errno = saved_errno;
        h_errno = 0;
        return result;
    }
}

static inline void sethostent(int stayopen)
{
    __rin_netdb_runtime_state_v1* state = &__rin_netdb_runtime_state;
    if (state->host_file != NULL) {
        (void)fclose(state->host_file);
        state->host_file = NULL;
    }
    state->host_file_offset = 0;
    state->host_stayopen = stayopen != 0;
}

static inline void endhostent(void)
{
    __rin_netdb_runtime_state_v1* state = &__rin_netdb_runtime_state;
    if (state->host_file != NULL) {
        (void)fclose(state->host_file);
        state->host_file = NULL;
    }
    state->host_file_offset = 0;
    state->host_stayopen = 0;
}

static inline struct netent* getnetent(void)
{
    __rin_netdb_runtime_state_v1* state = &__rin_netdb_runtime_state;
    _NetdbNetworkRecord record;
    FILE* file = state->network_file;
    int local_file = 0;
    int status;
    long next_offset;
    int saved_errno = errno;
    if (file == NULL) {
        file = RIN_NETDB_OPEN_NETWORKS();
        if (file == NULL) {
            h_errno = NO_RECOVERY;
            return NULL;
        }
        local_file = 1;
        if (fseek(file, state->network_file_offset, SEEK_SET) != 0) {
            int error = errno != 0 ? errno : EIO;
            (void)fclose(file);
            errno = error;
            h_errno = NO_RECOVERY;
            return NULL;
        }
    }
    status = _netdb_next_network_record(file, &record);
    next_offset = ftell(file);
    if (status > 0 && next_offset < 0) status = -1;
    if (local_file && state->network_stayopen != 0) {
        state->network_file = file;
    } else if (local_file) {
        (void)fclose(file);
    }
    if (status <= 0) {
        if (status < 0) {
            state->network_file_offset = 0;
            if (state->network_file != NULL) {
                (void)fclose(state->network_file);
                state->network_file = NULL;
            }
            if (errno == saved_errno) errno = EIO;
            h_errno = NO_RECOVERY;
        } else {
            if (next_offset >= 0) state->network_file_offset = next_offset;
            errno = saved_errno;
        }
        return NULL;
    }
    state->network_file_offset = next_offset;
    {
        struct netent* result = _netdb_publish_network(&record);
        if (result == NULL) {
            h_errno = NO_RECOVERY;
            return NULL;
        }
        errno = saved_errno;
        h_errno = 0;
        return result;
    }
}

static inline void setnetent(int stayopen)
{
    __rin_netdb_runtime_state_v1* state = &__rin_netdb_runtime_state;
    if (state->network_file != NULL) {
        (void)fclose(state->network_file);
        state->network_file = NULL;
    }
    state->network_file_offset = 0;
    state->network_stayopen = stayopen != 0;
}

static inline void endnetent(void)
{
    __rin_netdb_runtime_state_v1* state = &__rin_netdb_runtime_state;
    if (state->network_file != NULL) {
        (void)fclose(state->network_file);
        state->network_file = NULL;
    }
    state->network_file_offset = 0;
    state->network_stayopen = 0;
}

static inline struct netent* getnetbyname(const char* name)
{
    _NetdbNetworkRecord record;
    int saved_errno = errno;
    int status;
    if (!name) {
        errno = EINVAL;
        h_errno = NO_RECOVERY;
        return NULL;
    }
    status = _netdb_find_network_by_name(name, &record);
    if (status <= 0) {
        if (status == 0) {
            errno = ENOENT;
            h_errno = HOST_NOT_FOUND;
        } else {
            h_errno = NO_RECOVERY;
        }
        return NULL;
    }
    {
        struct netent* result = _netdb_publish_network(&record);
        if (result == NULL) {
            h_errno = NO_RECOVERY;
            return NULL;
        }
        errno = saved_errno;
        h_errno = 0;
        return result;
    }
}

static inline struct netent* getnetbyaddr(uint32_t net, int type)
{
    _NetdbNetworkRecord record;
    int saved_errno = errno;
    int status;
    if (type != AF_INET) {
        errno = EINVAL;
        h_errno = NO_RECOVERY;
        return NULL;
    }
    status = _netdb_find_network_by_number(net, &record);
    if (status <= 0) {
        if (status == 0) {
            errno = ENOENT;
            h_errno = HOST_NOT_FOUND;
        } else {
            h_errno = NO_RECOVERY;
        }
        return NULL;
    }
    {
        struct netent* result = _netdb_publish_network(&record);
        if (result == NULL) {
            h_errno = NO_RECOVERY;
            return NULL;
        }
        errno = saved_errno;
        h_errno = 0;
        return result;
    }
}

static inline size_t _netdb_align_ptr(size_t offset)
{
    size_t mask = sizeof(void*) - 1u;
    return (offset + mask) & ~mask;
}

static inline size_t _netdb_string_length(const char* text)
{
    size_t length = 0u;
    while (text[length] != '\0') ++length;
    return length + 1u;
}

static inline uint32_t _netdb_string_list_count(char* const* list)
{
    uint32_t count = 0u;
    if (list == NULL) return 0u;
    while (list[count] != NULL) ++count;
    return count;
}

/* Pack a servent into a caller buffer.  Layout: name, proto, alias
 * strings, then the aligned alias pointer array. */
static inline int _netdb_pack_servent(char* buf, size_t buflen,
                                      const struct servent* src,
                                      struct servent* ret,
                                      struct servent** result)
{
    size_t name_len;
    size_t proto_len;
    size_t alias_total = 0u;
    size_t strings;
    size_t pointers;
    size_t off;
    size_t offsets[RIN_NETDB_DATABASE_MAX_ALIASES];
    uint32_t alias_count;
    uint32_t i;
    char** aliases;
    *result = NULL;
    if (buf == NULL || src == NULL || ret == NULL) return EINVAL;
    name_len = _netdb_string_length(src->s_name);
    proto_len = _netdb_string_length(src->s_proto);
    alias_count = _netdb_string_list_count(src->s_aliases);
    if (alias_count > RIN_NETDB_DATABASE_MAX_ALIASES) return ERANGE;
    for (i = 0u; i < alias_count; ++i)
        alias_total += _netdb_string_length(src->s_aliases[i]);
    strings = name_len + proto_len + alias_total;
    pointers = (size_t)(alias_count + 1u) * sizeof(char*);
    if (_netdb_align_ptr(strings) + pointers > buflen) return ERANGE;
    off = 0u;
    ret->s_name = buf + off;
    memcpy(ret->s_name, src->s_name, name_len);
    off += name_len;
    ret->s_proto = buf + off;
    memcpy(ret->s_proto, src->s_proto, proto_len);
    off += proto_len;
    for (i = 0u; i < alias_count; ++i) {
        size_t len = _netdb_string_length(src->s_aliases[i]);
        offsets[i] = off;
        memcpy(buf + off, src->s_aliases[i], len);
        off += len;
    }
    off = _netdb_align_ptr(off);
    aliases = (char**)(void*)(buf + off);
    for (i = 0u; i < alias_count; ++i) aliases[i] = buf + offsets[i];
    aliases[alias_count] = NULL;
    ret->s_aliases = aliases;
    ret->s_port = src->s_port;
    *result = ret;
    return 0;
}

static inline int _netdb_pack_protoent(char* buf, size_t buflen,
                                       const struct protoent* src,
                                       struct protoent* ret,
                                       struct protoent** result)
{
    size_t name_len;
    size_t alias_total = 0u;
    size_t strings;
    size_t pointers;
    size_t off;
    size_t offsets[RIN_NETDB_DATABASE_MAX_ALIASES];
    uint32_t alias_count;
    uint32_t i;
    char** aliases;
    *result = NULL;
    if (buf == NULL || src == NULL || ret == NULL) return EINVAL;
    name_len = _netdb_string_length(src->p_name);
    alias_count = _netdb_string_list_count(src->p_aliases);
    if (alias_count > RIN_NETDB_DATABASE_MAX_ALIASES) return ERANGE;
    for (i = 0u; i < alias_count; ++i)
        alias_total += _netdb_string_length(src->p_aliases[i]);
    strings = name_len + alias_total;
    pointers = (size_t)(alias_count + 1u) * sizeof(char*);
    if (_netdb_align_ptr(strings) + pointers > buflen) return ERANGE;
    off = 0u;
    ret->p_name = buf + off;
    memcpy(ret->p_name, src->p_name, name_len);
    off += name_len;
    for (i = 0u; i < alias_count; ++i) {
        size_t len = _netdb_string_length(src->p_aliases[i]);
        offsets[i] = off;
        memcpy(buf + off, src->p_aliases[i], len);
        off += len;
    }
    off = _netdb_align_ptr(off);
    aliases = (char**)(void*)(buf + off);
    for (i = 0u; i < alias_count; ++i) aliases[i] = buf + offsets[i];
    aliases[alias_count] = NULL;
    ret->p_aliases = aliases;
    ret->p_proto = src->p_proto;
    *result = ret;
    return 0;
}

/* Pack a hostent: name, one address image, alias strings, then the
 * aligned address-list and alias pointer arrays. */
static inline int _netdb_pack_hostent(char* buf, size_t buflen,
                                      const struct hostent* src,
                                      struct hostent* ret,
                                      struct hostent** result)
{
    size_t name_len;
    size_t addr_len;
    size_t alias_total = 0u;
    size_t strings;
    size_t pointers;
    size_t off;
    size_t addr_off;
    size_t offsets[RIN_NETDB_DATABASE_MAX_ALIASES];
    uint32_t alias_count;
    uint32_t i;
    char** addr_list;
    char** aliases;
    *result = NULL;
    if (buf == NULL || src == NULL || ret == NULL ||
        src->h_addr_list == NULL || src->h_addr_list[0] == NULL ||
        src->h_length <= 0 ||
        (size_t)src->h_length > sizeof(struct in6_addr))
        return EINVAL;
    name_len = _netdb_string_length(src->h_name);
    addr_len = (size_t)src->h_length;
    alias_count = _netdb_string_list_count(src->h_aliases);
    if (alias_count > RIN_NETDB_DATABASE_MAX_ALIASES) return ERANGE;
    for (i = 0u; i < alias_count; ++i)
        alias_total += _netdb_string_length(src->h_aliases[i]);
    strings = name_len + addr_len + alias_total;
    /* addr_list has exactly one address plus the terminator. */
    pointers = 2u * sizeof(char*) +
               (size_t)(alias_count + 1u) * sizeof(char*);
    if (_netdb_align_ptr(strings) + pointers +
            (sizeof(void*) - 1u) * 2u > buflen)
        return ERANGE;
    off = 0u;
    ret->h_name = buf + off;
    memcpy(ret->h_name, src->h_name, name_len);
    off += name_len;
    addr_off = off;
    memcpy(buf + off, src->h_addr_list[0], addr_len);
    off += addr_len;
    for (i = 0u; i < alias_count; ++i) {
        size_t len = _netdb_string_length(src->h_aliases[i]);
        offsets[i] = off;
        memcpy(buf + off, src->h_aliases[i], len);
        off += len;
    }
    off = _netdb_align_ptr(off);
    addr_list = (char**)(void*)(buf + off);
    addr_list[0] = buf + addr_off;
    addr_list[1] = NULL;
    off += 2u * sizeof(char*);
    off = _netdb_align_ptr(off);
    aliases = (char**)(void*)(buf + off);
    for (i = 0u; i < alias_count; ++i) aliases[i] = buf + offsets[i];
    aliases[alias_count] = NULL;
    ret->h_aliases = aliases;
    ret->h_addrtype = src->h_addrtype;
    ret->h_length = src->h_length;
    ret->h_addr_list = addr_list;
    *result = ret;
    return 0;
}

static inline int _netdb_pack_netent(char* buf, size_t buflen,
                                     const struct netent* src,
                                     struct netent* ret,
                                     struct netent** result)
{
    size_t name_len;
    size_t alias_total = 0u;
    size_t strings;
    size_t pointers;
    size_t off;
    size_t offsets[RIN_NETDB_DATABASE_MAX_ALIASES];
    uint32_t alias_count;
    uint32_t i;
    char** aliases;
    *result = NULL;
    if (buf == NULL || src == NULL || ret == NULL) return EINVAL;
    name_len = _netdb_string_length(src->n_name);
    alias_count = _netdb_string_list_count(src->n_aliases);
    if (alias_count > RIN_NETDB_DATABASE_MAX_ALIASES) return ERANGE;
    for (i = 0u; i < alias_count; ++i)
        alias_total += _netdb_string_length(src->n_aliases[i]);
    strings = name_len + alias_total;
    pointers = (size_t)(alias_count + 1u) * sizeof(char*);
    if (_netdb_align_ptr(strings) + pointers > buflen) return ERANGE;
    off = 0u;
    ret->n_name = buf + off;
    memcpy(ret->n_name, src->n_name, name_len);
    off += name_len;
    for (i = 0u; i < alias_count; ++i) {
        size_t len = _netdb_string_length(src->n_aliases[i]);
        offsets[i] = off;
        memcpy(buf + off, src->n_aliases[i], len);
        off += len;
    }
    off = _netdb_align_ptr(off);
    aliases = (char**)(void*)(buf + off);
    for (i = 0u; i < alias_count; ++i) aliases[i] = buf + offsets[i];
    aliases[alias_count] = NULL;
    ret->n_aliases = aliases;
    ret->n_addrtype = src->n_addrtype;
    ret->n_net = src->n_net;
    *result = ret;
    return 0;
}

#endif /* RIN_LIBC_INTERNAL_NETDB_DATABASE_H */
