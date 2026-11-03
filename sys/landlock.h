/* SPDX-License-Identifier: MIT */
/* Linux-compatible Landlock userspace ABI. */
#ifndef _SYS_LANDLOCK_H
#define _SYS_LANDLOCK_H

#include "../stddef.h"
#include "../stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LANDLOCK_CREATE_RULESET_VERSION (1u << 0)
#define LANDLOCK_CREATE_RULESET_ERRATA (1u << 1)

#define LANDLOCK_ADD_RULE_QUIET (1u << 0)

#define LANDLOCK_RULE_PATH_BENEATH 1u
#define LANDLOCK_RULE_NET_PORT 2u

#define LANDLOCK_ACCESS_FS_EXECUTE       (1ull << 0)
#define LANDLOCK_ACCESS_FS_WRITE_FILE    (1ull << 1)
#define LANDLOCK_ACCESS_FS_READ_FILE     (1ull << 2)
#define LANDLOCK_ACCESS_FS_READ_DIR      (1ull << 3)
#define LANDLOCK_ACCESS_FS_REMOVE_DIR    (1ull << 4)
#define LANDLOCK_ACCESS_FS_REMOVE_FILE   (1ull << 5)
#define LANDLOCK_ACCESS_FS_MAKE_CHAR     (1ull << 6)
#define LANDLOCK_ACCESS_FS_MAKE_DIR      (1ull << 7)
#define LANDLOCK_ACCESS_FS_MAKE_REG      (1ull << 8)
#define LANDLOCK_ACCESS_FS_MAKE_SOCK     (1ull << 9)
#define LANDLOCK_ACCESS_FS_MAKE_FIFO     (1ull << 10)
#define LANDLOCK_ACCESS_FS_MAKE_BLOCK    (1ull << 11)
#define LANDLOCK_ACCESS_FS_MAKE_SYM      (1ull << 12)
#define LANDLOCK_ACCESS_FS_REFER         (1ull << 13)
#define LANDLOCK_ACCESS_FS_TRUNCATE      (1ull << 14)
#define LANDLOCK_ACCESS_FS_IOCTL_DEV     (1ull << 15)
#define LANDLOCK_ACCESS_FS_RESOLVE_UNIX  (1ull << 16)

#define LANDLOCK_ACCESS_NET_BIND_TCP     (1ull << 0)
#define LANDLOCK_ACCESS_NET_CONNECT_TCP  (1ull << 1)
#define LANDLOCK_ACCESS_NET_BIND_UDP     (1ull << 2)
#define LANDLOCK_ACCESS_NET_CONNECT_SEND_UDP (1ull << 3)

#define LANDLOCK_SCOPE_ABSTRACT_UNIX_SOCKET (1ull << 0)
#define LANDLOCK_SCOPE_SIGNAL               (1ull << 1)

#define LANDLOCK_PERMISSION_NAMESPACE_USE  (1ull << 0)
#define LANDLOCK_PERMISSION_CAPABILITY_USE (1ull << 1)

#define LANDLOCK_RESTRICT_SELF_LOG_SAME_EXEC_OFF    (1u << 0)
#define LANDLOCK_RESTRICT_SELF_LOG_NEW_EXEC_ON      (1u << 1)
#define LANDLOCK_RESTRICT_SELF_LOG_SUBDOMAINS_OFF   (1u << 2)
#define LANDLOCK_RESTRICT_SELF_TSYNC                (1u << 3)
#define LANDLOCK_RESTRICT_SELF_NO_NEW_PRIVS         (1u << 4)

struct landlock_ruleset_attr {
    uint64_t handled_access_fs;
    uint64_t handled_access_net;
    uint64_t scoped;
    uint64_t quiet_access_fs;
    uint64_t quiet_access_net;
    uint64_t quiet_scoped;
    uint64_t handled_permissions;
};

struct landlock_path_beneath_attr {
    uint64_t allowed_access;
    int32_t parent_fd;
} __attribute__((packed));

struct landlock_net_port_attr {
    uint64_t allowed_access;
    uint64_t port;
};

struct landlock_namespace_attr {
    uint64_t permissions;
    uint64_t allowed_namespace_types;
    uint64_t quiet_namespace_types;
};

struct landlock_capability_attr {
    uint64_t allowed_capabilities;
    uint64_t quiet_capabilities;
};

int landlock_create_ruleset(const struct landlock_ruleset_attr* attr,
                            size_t size, uint32_t flags);
int landlock_add_rule(int ruleset_fd, uint32_t rule_type,
                      const void* rule_attr, uint32_t flags);
int landlock_restrict_self(int ruleset_fd, uint32_t flags);

#ifdef __cplusplus
}
#endif

#endif /* _SYS_LANDLOCK_H */
