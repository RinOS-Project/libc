/* SPDX-License-Identifier: MIT */
#include <sys/acl.h>
#include <sys/stat.h>
#include <sys/xattr.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RIN_ACL_WIRE_VERSION UINT32_C(2)
#define RIN_ACL_WIRE_HEADER UINT32_C(4)
#define RIN_ACL_WIRE_ENTRY UINT32_C(8)
#define RIN_ACL_WIRE_MAX UINT32_C(4096)
#define RIN_ACL_UNDEFINED_ID UINT32_MAX
#define RIN_ACL_ENTRY_MAX ((RIN_ACL_WIRE_MAX - RIN_ACL_WIRE_HEADER) / \
                          RIN_ACL_WIRE_ENTRY)

struct __rin_acl_entry {
    uint16_t tag;
    uint16_t permissions;
    uint32_t id;
};

struct __rin_acl {
    uint32_t count;
    uint32_t capacity;
    uint32_t cursor;
    uint32_t is_default;
    struct __rin_acl_entry entries[];
};

static uint16_t acl_wire_read16(const uint8_t* p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8u));
}

static uint32_t acl_wire_read32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8u) |
           ((uint32_t)p[2] << 16u) | ((uint32_t)p[3] << 24u);
}

static void acl_wire_write16(uint8_t* p, uint16_t value) {
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8u);
}

static void acl_wire_write32(uint8_t* p, uint32_t value) {
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8u);
    p[2] = (uint8_t)(value >> 16u);
    p[3] = (uint8_t)(value >> 24u);
}

static int acl_tag_valid(int tag) {
    return tag == ACL_USER_OBJ || tag == ACL_USER ||
           tag == ACL_GROUP_OBJ || tag == ACL_GROUP ||
           tag == ACL_MASK || tag == ACL_OTHER;
}

static size_t acl_allocation_size(uint32_t capacity) {
    return sizeof(struct __rin_acl) +
           (size_t)capacity * sizeof(struct __rin_acl_entry);
}

acl_t acl_init(int count) {
    acl_t acl;
    if (count < 0 || (uint32_t)count > RIN_ACL_ENTRY_MAX) {
        errno = EINVAL;
        return NULL;
    }
    acl = (acl_t)malloc(acl_allocation_size((uint32_t)count));
    if (!acl) return NULL;
    acl->count = 0u;
    acl->capacity = (uint32_t)count;
    acl->cursor = 0u;
    acl->is_default = 0u;
    return acl;
}

acl_t acl_dup(acl_t source) {
    acl_t copy;
    if (!source || source->count > source->capacity) {
        errno = EINVAL;
        return NULL;
    }
    copy = acl_init((int)source->count);
    if (!copy) return NULL;
    copy->count = source->count;
    copy->is_default = source->is_default;
    if (source->count)
        memcpy(copy->entries, source->entries,
               source->count * sizeof(source->entries[0]));
    return copy;
}

int acl_free(void* object) {
    free(object);
    return 0;
}

int acl_create_entry(acl_t* acl_p, acl_entry_t* entry_p) {
    acl_t acl;
    if (!acl_p || !*acl_p || !entry_p) {
        errno = EINVAL;
        return -1;
    }
    acl = *acl_p;
    if (acl->count > acl->capacity) {
        errno = EINVAL;
        return -1;
    }
    if (acl->count >= RIN_ACL_ENTRY_MAX) {
        errno = ENOSPC;
        return -1;
    }
    if (acl->count == acl->capacity) {
        uint32_t capacity = acl->capacity == 0u ? 4u : acl->capacity * 2u;
        struct __rin_acl* replacement;
        if (capacity > RIN_ACL_ENTRY_MAX) capacity = RIN_ACL_ENTRY_MAX;
        replacement = (struct __rin_acl*)realloc(
            acl, acl_allocation_size(capacity));
        if (!replacement) return -1;
        acl = replacement;
        *acl_p = acl;
        acl->capacity = capacity;
    }
    *entry_p = &acl->entries[acl->count++];
    (*entry_p)->tag = ACL_UNDEFINED_TAG;
    (*entry_p)->permissions = 0u;
    (*entry_p)->id = RIN_ACL_UNDEFINED_ID;
    return 0;
}

static int acl_entry_index(acl_t acl, acl_entry_t entry,
                           uint32_t* index_out) {
    uintptr_t base, candidate, bytes;
    if (!acl || !entry || !index_out || acl->count > acl->capacity)
        return 0;
    base = (uintptr_t)&acl->entries[0];
    candidate = (uintptr_t)entry;
    bytes = (uintptr_t)acl->count * sizeof(acl->entries[0]);
    if (candidate < base || candidate - base >= bytes ||
        (candidate - base) % sizeof(acl->entries[0]) != 0u)
        return 0;
    *index_out = (uint32_t)((candidate - base) /
                            sizeof(acl->entries[0]));
    return 1;
}

int acl_delete_entry(acl_t acl, acl_entry_t entry) {
    uint32_t index;
    if (!acl_entry_index(acl, entry, &index)) {
        errno = EINVAL;
        return -1;
    }
    if (index + 1u < acl->count)
        memmove(&acl->entries[index], &acl->entries[index + 1u],
                (acl->count - index - 1u) * sizeof(acl->entries[0]));
    --acl->count;
    if (acl->cursor > index) --acl->cursor;
    return 0;
}

int acl_copy_entry(acl_entry_t dest, acl_entry_t source) {
    if (!dest || !source) {
        errno = EINVAL;
        return -1;
    }
    *dest = *source;
    return 0;
}

int acl_get_entry(acl_t acl, int entry_id, acl_entry_t* entry_p) {
    if (!acl || acl->count > acl->capacity || !entry_p ||
        (entry_id != ACL_FIRST_ENTRY && entry_id != ACL_NEXT_ENTRY)) {
        errno = EINVAL;
        return -1;
    }
    if (entry_id == ACL_FIRST_ENTRY) acl->cursor = 0u;
    if (acl->cursor >= acl->count) {
        *entry_p = NULL;
        return 0;
    }
    *entry_p = &acl->entries[acl->cursor++];
    return 1;
}

int acl_get_tag_type(acl_entry_t entry, acl_tag_t* tag_type_p) {
    if (!entry || !tag_type_p || !acl_tag_valid(entry->tag)) {
        errno = EINVAL;
        return -1;
    }
    *tag_type_p = entry->tag;
    return 0;
}

int acl_set_tag_type(acl_entry_t entry, acl_tag_t tag_type) {
    if (!entry || !acl_tag_valid(tag_type)) {
        errno = EINVAL;
        return -1;
    }
    entry->tag = (uint16_t)tag_type;
    if (tag_type != ACL_USER && tag_type != ACL_GROUP)
        entry->id = RIN_ACL_UNDEFINED_ID;
    return 0;
}

int acl_get_permset(acl_entry_t entry, acl_permset_t* permset_p) {
    if (!entry || !permset_p) {
        errno = EINVAL;
        return -1;
    }
    *permset_p = &entry->permissions;
    return 0;
}

int acl_set_permset(acl_entry_t entry, acl_permset_t permset) {
    if (!entry || !permset || (*permset & ~UINT16_C(7)) != 0u) {
        errno = EINVAL;
        return -1;
    }
    entry->permissions = *permset;
    return 0;
}

int acl_clear_perms(acl_permset_t permset) {
    if (!permset) { errno = EINVAL; return -1; }
    *permset = 0u;
    return 0;
}

int acl_add_perm(acl_permset_t permset, acl_perm_t perm) {
    if (!permset || (perm & ~UINT32_C(7)) != 0u) {
        errno = EINVAL;
        return -1;
    }
    *permset |= (uint16_t)perm;
    return 0;
}

int acl_delete_perm(acl_permset_t permset, acl_perm_t perm) {
    if (!permset || (perm & ~UINT32_C(7)) != 0u) {
        errno = EINVAL;
        return -1;
    }
    *permset &= (uint16_t)~perm;
    return 0;
}

int acl_get_perm(acl_permset_t permset, acl_perm_t perm) {
    if (!permset || perm == 0u || (perm & (perm - 1u)) != 0u ||
        (perm & ~UINT32_C(7)) != 0u) {
        errno = EINVAL;
        return -1;
    }
    return (*permset & perm) != 0u;
}

void* acl_get_qualifier(acl_entry_t entry) {
    if (!entry || (entry->tag != ACL_USER && entry->tag != ACL_GROUP) ||
        entry->id == RIN_ACL_UNDEFINED_ID) {
        errno = EINVAL;
        return NULL;
    }
    if (entry->tag == ACL_USER) {
        uid_t* uid = (uid_t*)malloc(sizeof(*uid));
        if (uid) *uid = (uid_t)entry->id;
        return uid;
    }
    {
        gid_t* gid = (gid_t*)malloc(sizeof(*gid));
        if (gid) *gid = (gid_t)entry->id;
        return gid;
    }
}

int acl_set_qualifier(acl_entry_t entry, const void* qualifier) {
    uint64_t id;
    if (!entry || !qualifier ||
        (entry->tag != ACL_USER && entry->tag != ACL_GROUP)) {
        errno = EINVAL;
        return -1;
    }
    id = entry->tag == ACL_USER ? (uint64_t)*(const uid_t*)qualifier
                                : (uint64_t)*(const gid_t*)qualifier;
    if (id >= RIN_ACL_UNDEFINED_ID) { errno = EINVAL; return -1; }
    entry->id = (uint32_t)id;
    return 0;
}

int acl_valid(acl_t acl) {
    uint32_t user_obj = 0u, group_obj = 0u, mask = 0u, other = 0u;
    uint32_t named = 0u;
    if (!acl || acl->count < 3u || acl->count > acl->capacity ||
        acl->count > RIN_ACL_ENTRY_MAX) {
        errno = EINVAL;
        return -1;
    }
    for (uint32_t i = 0u; i < acl->count; ++i) {
        const struct __rin_acl_entry* entry = &acl->entries[i];
        if (!acl_tag_valid(entry->tag) ||
            (entry->permissions & ~UINT16_C(7)) != 0u) {
            errno = EINVAL;
            return -1;
        }
        if (entry->tag == ACL_USER_OBJ) {
            if (entry->id != RIN_ACL_UNDEFINED_ID || ++user_obj != 1u)
                goto invalid;
        } else if (entry->tag == ACL_GROUP_OBJ) {
            if (entry->id != RIN_ACL_UNDEFINED_ID || ++group_obj != 1u)
                goto invalid;
        } else if (entry->tag == ACL_OTHER) {
            if (entry->id != RIN_ACL_UNDEFINED_ID || ++other != 1u)
                goto invalid;
        } else if (entry->tag == ACL_MASK) {
            if (entry->id != RIN_ACL_UNDEFINED_ID || ++mask != 1u)
                goto invalid;
        } else {
            if (entry->id == RIN_ACL_UNDEFINED_ID) goto invalid;
            ++named;
            for (uint32_t j = 0u; j < i; ++j)
                if (acl->entries[j].tag == entry->tag &&
                    acl->entries[j].id == entry->id)
                    goto invalid;
        }
    }
    if (user_obj == 1u && group_obj == 1u && other == 1u && mask <= 1u &&
        (named == 0u || mask == 1u))
        return 0;
invalid:
    errno = EINVAL;
    return -1;
}

int acl_calc_mask(acl_t* acl_p) {
    acl_t acl;
    uint16_t mask = 0u;
    acl_entry_t entry;
    int found = 0;
    if (!acl_p || !*acl_p) { errno = EINVAL; return -1; }
    acl = *acl_p;
    for (uint32_t i = 0u; i < acl->count; ++i) {
        struct __rin_acl_entry* current = &acl->entries[i];
        if (current->tag == ACL_MASK) {
            entry = current;
            found = 1;
        }
        if (current->tag == ACL_GROUP_OBJ || current->tag == ACL_GROUP ||
            current->tag == ACL_USER)
            mask |= current->permissions;
    }
    if (!found && acl_create_entry(acl_p, &entry) != 0) return -1;
    if (!found && acl_set_tag_type(entry, ACL_MASK) != 0) return -1;
    entry->permissions = mask;
    return 0;
}

static int acl_wire_decode(const uint8_t* wire, uint32_t size, acl_t* out) {
    uint32_t count;
    acl_t acl;
    if (!wire || !out || size < RIN_ACL_WIRE_HEADER ||
        size > RIN_ACL_WIRE_MAX ||
        (size - RIN_ACL_WIRE_HEADER) % RIN_ACL_WIRE_ENTRY != 0u ||
        acl_wire_read32(wire) != RIN_ACL_WIRE_VERSION) {
        errno = EINVAL;
        return -1;
    }
    count = (size - RIN_ACL_WIRE_HEADER) / RIN_ACL_WIRE_ENTRY;
    acl = acl_init((int)count);
    if (!acl) return -1;
    acl->count = count;
    for (uint32_t i = 0u; i < count; ++i) {
        const uint8_t* source = wire + RIN_ACL_WIRE_HEADER +
                                i * RIN_ACL_WIRE_ENTRY;
        acl->entries[i].tag = acl_wire_read16(source);
        acl->entries[i].permissions = acl_wire_read16(source + 2u);
        acl->entries[i].id = acl_wire_read32(source + 4u);
    }
    if (acl_valid(acl) != 0) {
        acl_free(acl);
        return -1;
    }
    *out = acl;
    return 0;
}

static int acl_wire_encode(acl_t acl, uint8_t* wire, uint32_t* size_out) {
    uint32_t size;
    if (!wire || !size_out || acl_valid(acl) != 0) return -1;
    size = RIN_ACL_WIRE_HEADER + acl->count * RIN_ACL_WIRE_ENTRY;
    if (size > RIN_ACL_WIRE_MAX) { errno = EINVAL; return -1; }
    acl_wire_write32(wire, RIN_ACL_WIRE_VERSION);
    for (uint32_t i = 0u; i < acl->count; ++i) {
        uint8_t* entry = wire + RIN_ACL_WIRE_HEADER +
                         i * RIN_ACL_WIRE_ENTRY;
        acl_wire_write16(entry, acl->entries[i].tag);
        acl_wire_write16(entry + 2u, acl->entries[i].permissions);
        acl_wire_write32(entry + 4u, acl->entries[i].id);
    }
    *size_out = size;
    return 0;
}

static const char* acl_xattr_name(int type) {
    if (type == ACL_TYPE_ACCESS) return "system.posix_acl_access";
    if (type == ACL_TYPE_DEFAULT) return "system.posix_acl_default";
    errno = EINVAL;
    return NULL;
}

static acl_t acl_from_mode(mode_t mode) {
    static const acl_tag_t tags[] = {
        ACL_USER_OBJ, ACL_GROUP_OBJ, ACL_OTHER
    };
    static const uint32_t shifts[] = { 6u, 3u, 0u };
    acl_t acl = acl_init(3);
    if (!acl) return NULL;
    for (uint32_t index = 0u; index < 3u; ++index) {
        acl_entry_t entry;
        if (acl_create_entry(&acl, &entry) != 0 ||
            acl_set_tag_type(entry, tags[index]) != 0) {
            acl_free(acl);
            return NULL;
        }
        entry->permissions = (uint16_t)((mode >> shifts[index]) & 7u);
    }
    return acl;
}

static int acl_xattr_missing(int error) {
    return error == ENODATA || error == ENOTSUP;
}

acl_t acl_get_file(const char* path, int type) {
    const char* name = acl_xattr_name(type);
    uint8_t wire[RIN_ACL_WIRE_MAX];
    ssize_t size;
    acl_t acl = NULL;
    if (!path || !name) { errno = EINVAL; return NULL; }
    size = getxattr(path, name, wire, sizeof(wire));
    if (size < 0) {
        int saved_errno = errno;
        struct stat status;
        if (type != ACL_TYPE_ACCESS || !acl_xattr_missing(saved_errno))
            return NULL;
        if (stat(path, &status) != 0) return NULL;
        return acl_from_mode(status.st_mode);
    }
    if (acl_wire_decode(wire, (uint32_t)size, &acl) != 0) return NULL;
    acl->is_default = type == ACL_TYPE_DEFAULT;
    return acl;
}

acl_t acl_get_fd(int fd) {
    uint8_t wire[RIN_ACL_WIRE_MAX];
    ssize_t size = fgetxattr(fd, "system.posix_acl_access", wire,
                             sizeof(wire));
    acl_t acl = NULL;
    if (size < 0) {
        int saved_errno = errno;
        struct stat status;
        if (!acl_xattr_missing(saved_errno)) return NULL;
        if (fstat(fd, &status) != 0) return NULL;
        return acl_from_mode(status.st_mode);
    }
    if (acl_wire_decode(wire, (uint32_t)size, &acl) != 0) return NULL;
    return acl;
}

int acl_set_file(const char* path, int type, acl_t acl) {
    const char* name = acl_xattr_name(type);
    uint8_t wire[RIN_ACL_WIRE_MAX];
    uint32_t size = 0u;
    if (!path || !name || acl_wire_encode(acl, wire, &size) != 0)
        return -1;
    return setxattr(path, name, wire, size, 0);
}

int acl_set_fd(int fd, acl_t acl) {
    uint8_t wire[RIN_ACL_WIRE_MAX];
    uint32_t size = 0u;
    if (acl_wire_encode(acl, wire, &size) != 0) return -1;
    return fsetxattr(fd, "system.posix_acl_access", wire, size, 0);
}

int acl_delete_def_file(const char* path) {
    if (!path) { errno = EINVAL; return -1; }
    return removexattr(path, "system.posix_acl_default");
}

acl_t acl_from_text(const char* text) {
    acl_t acl;
    const char* cursor;
    int prefix_seen = 0;
    int is_default = 0;
    if (!text) { errno = EINVAL; return NULL; }
    acl = acl_init(0);
    if (!acl) return NULL;
    cursor = text;
    while (*cursor) {
        char line[256];
        size_t length = 0u;
        char* first;
        char* second;
        char* tag_text;
        char* qualifier;
        char* permissions;
        acl_entry_t entry;
        acl_tag_t tag;
        uint32_t id = RIN_ACL_UNDEFINED_ID;
        while (*cursor == '\n' || *cursor == '\r' || *cursor == ' ' ||
               *cursor == '\t') ++cursor;
        if (!*cursor) break;
        while (cursor[length] && cursor[length] != '\n' &&
               cursor[length] != '\r') {
            if (length + 1u >= sizeof(line)) goto invalid_text;
            line[length] = cursor[length];
            ++length;
        }
        line[length] = '\0';
        cursor += length;
        while (*cursor == '\n' || *cursor == '\r') ++cursor;
        while (length && (line[length - 1u] == ' ' ||
                          line[length - 1u] == '\t'))
            line[--length] = '\0';
        {
            int line_is_default = strncmp(line, "default:", 8u) == 0;
            if (!prefix_seen) {
                prefix_seen = 1;
                is_default = line_is_default;
            } else if (is_default != line_is_default) {
                goto invalid_text;
            }
        }
        if (strncmp(line, "default:", 8u) == 0) {
            memmove(line, line + 8u, length - 7u);
            length -= 8u;
        }
        first = strchr(line, ':');
        if (!first) goto invalid_text;
        *first++ = '\0';
        second = strchr(first, ':');
        if (!second) goto invalid_text;
        *second++ = '\0';
        tag_text = line;
        qualifier = first;
        permissions = second;
        if (strcmp(tag_text, "user") == 0)
            tag = *qualifier ? ACL_USER : ACL_USER_OBJ;
        else if (strcmp(tag_text, "group") == 0)
            tag = *qualifier ? ACL_GROUP : ACL_GROUP_OBJ;
        else if (strcmp(tag_text, "mask") == 0 && !*qualifier)
            tag = ACL_MASK;
        else if (strcmp(tag_text, "other") == 0 && !*qualifier)
            tag = ACL_OTHER;
        else goto invalid_text;
        if (*qualifier) {
            char* end = NULL;
            unsigned long parsed = strtoul(qualifier, &end, 10);
            if (!end || *end || parsed >= RIN_ACL_UNDEFINED_ID)
                goto invalid_text;
            id = (uint32_t)parsed;
        }
        if (strlen(permissions) != 3u) goto invalid_text;
        if (acl_create_entry(&acl, &entry) != 0) goto allocation_error;
        entry->tag = (uint16_t)tag;
        entry->id = id;
        entry->permissions = 0u;
        if (permissions[0] == 'r') entry->permissions |= ACL_READ;
        else if (permissions[0] != '-') goto invalid_text;
        if (permissions[1] == 'w') entry->permissions |= ACL_WRITE;
        else if (permissions[1] != '-') goto invalid_text;
        if (permissions[2] == 'x') entry->permissions |= ACL_EXECUTE;
        else if (permissions[2] != '-') goto invalid_text;
    }
    if (acl_valid(acl) != 0) goto invalid_text;
    acl->is_default = (uint32_t)is_default;
    return acl;
allocation_error:
    acl_free(acl);
    return NULL;
invalid_text:
    acl_free(acl);
    errno = EINVAL;
    return NULL;
}

char* acl_to_text(acl_t acl, ssize_t* len_p) {
    size_t capacity;
    size_t used = 0u;
    char* text;
    if (len_p) *len_p = 0;
    if (acl_valid(acl) != 0) return NULL;
    capacity = (size_t)acl->count * 80u + 1u;
    text = (char*)malloc(capacity);
    if (!text) return NULL;
    for (uint32_t i = 0u; i < acl->count; ++i) {
        const struct __rin_acl_entry* entry = &acl->entries[i];
        const char* tag;
        char qualifier[32] = "";
        int written;
        switch (entry->tag) {
            case ACL_USER_OBJ: tag = "user"; break;
            case ACL_USER:
                tag = "user";
                snprintf(qualifier, sizeof(qualifier), "%u", entry->id);
                break;
            case ACL_GROUP_OBJ: tag = "group"; break;
            case ACL_GROUP:
                tag = "group";
                snprintf(qualifier, sizeof(qualifier), "%u", entry->id);
                break;
            case ACL_MASK: tag = "mask"; break;
            default: tag = "other"; break;
        }
        written = snprintf(text + used, capacity - used,
                           "%s%s:%s:%c%c%c\n",
                           acl->is_default ? "default:" : "", tag, qualifier,
                           (entry->permissions & ACL_READ) ? 'r' : '-',
                           (entry->permissions & ACL_WRITE) ? 'w' : '-',
                           (entry->permissions & ACL_EXECUTE) ? 'x' : '-');
        if (written < 0 || (size_t)written >= capacity - used) {
            free(text);
            errno = EOVERFLOW;
            return NULL;
        }
        used += (size_t)written;
    }
    if (len_p) *len_p = (ssize_t)used;
    return text;
}
