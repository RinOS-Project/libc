/* SPDX-License-Identifier: MIT */
/* POSIX access-control list API for RinOS libc. */
#ifndef _SYS_ACL_H
#define _SYS_ACL_H

#include <sys/types.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct __rin_acl* acl_t;
typedef struct __rin_acl_entry* acl_entry_t;
typedef uint16_t* acl_permset_t;
typedef int acl_tag_t;
typedef uint32_t acl_perm_t;

#define ACL_UNDEFINED_TAG 0
#define ACL_USER_OBJ 0x01
#define ACL_USER 0x02
#define ACL_GROUP_OBJ 0x04
#define ACL_GROUP 0x08
#define ACL_MASK 0x10
#define ACL_OTHER 0x20
#define ACL_READ 0x04
#define ACL_WRITE 0x02
#define ACL_EXECUTE 0x01
#define ACL_FIRST_ENTRY 0
#define ACL_NEXT_ENTRY 1
#define ACL_TYPE_ACCESS 0x8000
#define ACL_TYPE_DEFAULT 0x4000
#define ACL_TEXT_SIZE 4096

acl_t acl_init(int count);
acl_t acl_dup(acl_t acl);
int acl_free(void* object);
int acl_valid(acl_t acl);
int acl_create_entry(acl_t* acl_p, acl_entry_t* entry_p);
int acl_delete_entry(acl_t acl, acl_entry_t entry);
int acl_copy_entry(acl_entry_t dest, acl_entry_t src);
int acl_get_entry(acl_t acl, int entry_id, acl_entry_t* entry_p);
int acl_get_tag_type(acl_entry_t entry, acl_tag_t* tag_type_p);
int acl_set_tag_type(acl_entry_t entry, acl_tag_t tag_type);
int acl_get_permset(acl_entry_t entry, acl_permset_t* permset_p);
int acl_set_permset(acl_entry_t entry, acl_permset_t permset);
int acl_clear_perms(acl_permset_t permset);
int acl_add_perm(acl_permset_t permset, acl_perm_t perm);
int acl_delete_perm(acl_permset_t permset, acl_perm_t perm);
int acl_get_perm(acl_permset_t permset, acl_perm_t perm);
void* acl_get_qualifier(acl_entry_t entry);
int acl_set_qualifier(acl_entry_t entry, const void* qualifier);
int acl_calc_mask(acl_t* acl_p);
acl_t acl_from_text(const char* text);
char* acl_to_text(acl_t acl, ssize_t* len_p);
acl_t acl_get_file(const char* path, int type);
acl_t acl_get_fd(int fd);
int acl_set_file(const char* path, int type, acl_t acl);
int acl_set_fd(int fd, acl_t acl);
int acl_delete_def_file(const char* path);

#ifdef __cplusplus
}
#endif
#endif
