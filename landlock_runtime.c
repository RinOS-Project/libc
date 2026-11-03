/* SPDX-License-Identifier: MIT */
#include <sys/landlock.h>
#include <sys/syscall.h>

int landlock_create_ruleset(const struct landlock_ruleset_attr* attr,
                            size_t size, uint32_t flags)
{
    return (int)syscall(SYS_LANDLOCK_CREATE_RULESET, attr, size, flags);
}

int landlock_add_rule(int ruleset_fd, uint32_t rule_type,
                      const void* rule_attr, uint32_t flags)
{
    return (int)syscall(SYS_LANDLOCK_ADD_RULE, ruleset_fd, rule_type,
                        rule_attr, flags);
}

int landlock_restrict_self(int ruleset_fd, uint32_t flags)
{
    return (int)syscall(SYS_LANDLOCK_RESTRICT_SELF, ruleset_fd, flags);
}
