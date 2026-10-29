/* SPDX-License-Identifier: MIT */

#include "ftw.h"

#include "dirent.h"
#include "errno.h"
#include "fcntl.h"
#include "limits.h"
#include "pthread.h"
#include "stdlib.h"
#include "unistd.h"

typedef int (*RinFtwCallback3)(const char*, const struct stat*, int);
typedef int (*RinFtwCallback4)(const char*, const struct stat*, int,
                               struct FTW*);

typedef struct RinFtwNames {
    char** values;
    size_t count;
    size_t capacity;
} RinFtwNames;

typedef struct RinFtwAncestor {
    dev_t device;
    ino_t inode;
    const struct RinFtwAncestor* parent;
} RinFtwAncestor;

typedef struct RinFtwState {
    int flags;
    int is_nftw;
    int fd_limit;
    dev_t root_device;
    RinFtwCallback3 callback3;
    RinFtwCallback4 callback4;
    int stopped;
    int stop_value;
} RinFtwState;

static pthread_once_t g_rin_ftw_chdir_once = PTHREAD_ONCE_INIT;
static pthread_mutex_t g_rin_ftw_chdir_mutex;
static int g_rin_ftw_chdir_mutex_error;

static void rin_ftw_initialize_chdir_mutex(void) {
    pthread_mutexattr_t attributes;
    int result = pthread_mutexattr_init(&attributes);
    if (result != 0) {
        g_rin_ftw_chdir_mutex_error = result;
        return;
    }
    result = pthread_mutexattr_settype(&attributes, PTHREAD_MUTEX_RECURSIVE);
    if (result == 0)
        result = pthread_mutex_init(&g_rin_ftw_chdir_mutex, &attributes);
    (void)pthread_mutexattr_destroy(&attributes);
    g_rin_ftw_chdir_mutex_error = result;
}

static int rin_ftw_lock_chdir(void) {
    int result = pthread_once(&g_rin_ftw_chdir_once,
                              rin_ftw_initialize_chdir_mutex);
    if (result != 0) {
        errno = result;
        return -1;
    }
    if (g_rin_ftw_chdir_mutex_error != 0) {
        errno = g_rin_ftw_chdir_mutex_error;
        return -1;
    }
    result = pthread_mutex_lock(&g_rin_ftw_chdir_mutex);
    if (result != 0) {
        errno = result;
        return -1;
    }
    return 0;
}

static size_t rin_ftw_length(const char* text) {
    size_t length = 0u;
    while (text[length] != '\0') ++length;
    return length;
}

static char* rin_ftw_copy(const char* text, size_t length) {
    char* copy = (char*)malloc(length + 1u);
    size_t index;
    if (!copy) {
        errno = ENOMEM;
        return NULL;
    }
    for (index = 0u; index < length; ++index) copy[index] = text[index];
    copy[length] = '\0';
    return copy;
}

static void rin_ftw_names_clear(RinFtwNames* names) {
    size_t index;
    if (!names) return;
    for (index = 0u; index < names->count; ++index) free(names->values[index]);
    free(names->values);
    names->values = NULL;
    names->count = 0u;
    names->capacity = 0u;
}

static int rin_ftw_names_append(RinFtwNames* names, const char* name,
                                size_t length) {
    char* copy;
    if (names->count == names->capacity) {
        size_t capacity = names->capacity ? names->capacity * 2u : 16u;
        char** values;
        if (capacity < names->capacity ||
            capacity > ((size_t)-1) / sizeof(*values)) {
            errno = ENOMEM;
            return -1;
        }
        values = (char**)realloc(names->values, capacity * sizeof(*values));
        if (!values) {
            errno = ENOMEM;
            return -1;
        }
        names->values = values;
        names->capacity = capacity;
    }
    copy = rin_ftw_copy(name, length);
    if (!copy) return -1;
    names->values[names->count++] = copy;
    return 0;
}

/* 0は一覧取得成功、1は読み取り不可、-1はその他のエラーを表す。
 * 子へ進む前にdirectory streamを閉じる。 */
static int rin_ftw_read_names_stream(DIR* directory, RinFtwNames* names) {
    int read_error = 0;
    int close_error = 0;

    if (!directory || !names) {
        errno = EFAULT;
        return -1;
    }
    names->values = NULL;
    names->count = 0u;
    names->capacity = 0u;

    for (;;) {
        struct dirent* entry;
        size_t length = 0u;
        errno = 0;
        entry = readdir(directory);
        if (!entry) {
            read_error = errno;
            break;
        }
        while (length <= NAME_MAX && entry->d_name[length] != '\0') ++length;
        if (length == 0u || length > NAME_MAX) {
            read_error = EIO;
            break;
        }
        if ((length == 1u && entry->d_name[0] == '.') ||
            (length == 2u && entry->d_name[0] == '.' &&
             entry->d_name[1] == '.'))
            continue;
        if (rin_ftw_names_append(names, entry->d_name, length) != 0) {
            read_error = errno;
            break;
        }
    }

    if (closedir(directory) != 0) close_error = errno;
    if (read_error != 0) {
        rin_ftw_names_clear(names);
        errno = read_error;
        return read_error == EACCES ? 1 : -1;
    }
    if (close_error != 0) {
        rin_ftw_names_clear(names);
        errno = close_error;
        return -1;
    }
    return 0;
}

static int rin_ftw_read_names(const char* path, RinFtwNames* names) {
    DIR* directory;
    if (!path || !names) {
        errno = EFAULT;
        return -1;
    }
    directory = opendir(path);
    if (!directory) return errno == EACCES ? 1 : -1;
    return rin_ftw_read_names_stream(directory, names);
}

static int rin_ftw_read_names_fd(int descriptor, RinFtwNames* names) {
    DIR* directory;
    if (descriptor < 0 || !names) {
        if (descriptor >= 0) (void)close(descriptor);
        errno = EFAULT;
        return -1;
    }
    directory = fdopendir(descriptor);
    if (!directory) {
        int saved_errno = errno;
        (void)close(descriptor);
        errno = saved_errno;
        return saved_errno == EACCES ? 1 : -1;
    }
    return rin_ftw_read_names_stream(directory, names);
}

static char* rin_ftw_child_path(const char* parent, const char* child) {
    size_t parent_length = rin_ftw_length(parent);
    size_t child_length = rin_ftw_length(child);
    int needs_separator = parent_length != 0u &&
                          parent[parent_length - 1u] != '/';
    size_t total = parent_length + (needs_separator ? 1u : 0u) + child_length;
    char* result;
    size_t index;
    if (total >= PATH_MAX) {
        errno = ENAMETOOLONG;
        return NULL;
    }
    result = (char*)malloc(total + 1u);
    if (!result) {
        errno = ENOMEM;
        return NULL;
    }
    for (index = 0u; index < parent_length; ++index) result[index] = parent[index];
    if (needs_separator) result[parent_length++] = '/';
    for (index = 0u; index < child_length; ++index)
        result[parent_length + index] = child[index];
    result[total] = '\0';
    return result;
}

static size_t rin_ftw_base(const char* path) {
    size_t length = rin_ftw_length(path);
    size_t base;
    while (length > 1u && path[length - 1u] == '/') --length;
    if (length == 1u && path[0] == '/') return 0u;
    base = length;
    while (base > 0u && path[base - 1u] != '/') --base;
    return base;
}

static char* rin_ftw_parent_path(const char* path) {
    size_t base = rin_ftw_base(path);
    size_t length;
    if (base == 0u) return rin_ftw_copy(path, 1u);
    length = base - 1u;
    if (length == 0u) length = 1u;
    return rin_ftw_copy(path, length);
}

static int rin_ftw_invoke_checked(RinFtwState* state,
                                  const char* display_path,
                                  const char* walk_path,
                                  const struct stat* status, int type,
                                  int level,
                                  const struct stat* expected_parent) {
    struct FTW information;
    int result;
    int callback_errno;
    if ((state->flags & FTW_CHDIR) != 0) {
        char* parent = rin_ftw_parent_path(walk_path);
        if (!parent) return -1;
        if (chdir(parent) != 0) {
            free(parent);
            return -1;
        }
        free(parent);
        if (expected_parent) {
            struct stat current_directory;
            if (stat(".", &current_directory) != 0) return -1;
            if (current_directory.st_dev != expected_parent->st_dev ||
                current_directory.st_ino != expected_parent->st_ino) {
                errno = EAGAIN;
                return -1;
            }
        }
    }

    information.base = (int)rin_ftw_base(display_path);
    information.level = level;
    errno = 0;
    if (state->is_nftw)
        result = state->callback4(display_path, status, type, &information);
    else
        result = state->callback3(display_path, status, type);
    callback_errno = errno;
    if (result != 0) {
        if (result == -1 && callback_errno == 0) {
            errno = EACCES;
            callback_errno = EACCES;
        }
        state->stopped = 1;
        state->stop_value = result;
        return 1;
    }
    errno = callback_errno;
    return 0;
}

static int rin_ftw_invoke(RinFtwState* state, const char* display_path,
                          const char* walk_path,
                          const struct stat* status, int type, int level) {
    return rin_ftw_invoke_checked(state, display_path, walk_path, status,
                                  type, level, NULL);
}

static int rin_ftw_stat_error(RinFtwState* state, const char* walk_path,
                              const char* display_path,
                              int level, int stat_error) {
    struct stat link_status;
    struct stat empty_status;
    int type = FTW_NS;
    const struct stat* callback_status = &empty_status;
    size_t index;
    unsigned char* bytes = (unsigned char*)&empty_status;

    for (index = 0u; index < sizeof(empty_status); ++index) bytes[index] = 0u;
    if ((state->flags & FTW_PHYS) == 0 &&
        (stat_error == ENOENT || stat_error == ENOTDIR) &&
        lstat(walk_path, &link_status) == 0 &&
        S_ISLNK(link_status.st_mode)) {
        type = FTW_SLN;
        callback_status = &link_status;
    } else if (stat_error != EACCES) {
        errno = stat_error;
        return -1;
    }
    return rin_ftw_invoke(state, display_path, walk_path, callback_status,
                          type, level);
}

static int rin_ftw_seen_ancestor(const RinFtwAncestor* ancestor,
                                 const struct stat* status) {
    while (ancestor) {
        if (ancestor->device == status->st_dev &&
            ancestor->inode == status->st_ino)
            return 1;
        ancestor = ancestor->parent;
    }
    return 0;
}

static int rin_ftw_same_object(const struct stat* left,
                               const struct stat* right) {
    return left && right && left->st_dev == right->st_dev &&
           left->st_ino == right->st_ino;
}

/* 絶対pathをcomponentごとに開き、symlinkを経由しないdirectory FDを返す。 */
static int rin_ftw_open_directory_path(const char* path, int fd_limit,
                                       const struct stat* expected) {
    const int directory_flags = O_RDONLY | O_DIRECTORY | O_CLOEXEC |
                                O_NOFOLLOW;
    struct stat status;
    size_t cursor = 0u;
    int descriptor;
    if (!path || path[0] != '/') {
        errno = EINVAL;
        return -1;
    }
    descriptor = open("/", directory_flags);
    if (descriptor < 0) return -1;
    while (path[cursor] == '/') ++cursor;
    while (path[cursor] != '\0') {
        char component[NAME_MAX + 1u];
        size_t start = cursor;
        size_t length;
        int next_descriptor;
        while (path[cursor] != '\0' && path[cursor] != '/') ++cursor;
        length = cursor - start;
        while (path[cursor] == '/') ++cursor;
        if (length == 0u ||
            (length == 1u && path[start] == '.'))
            continue;
        if (length > NAME_MAX) {
            (void)close(descriptor);
            errno = ENAMETOOLONG;
            return -1;
        }
        if (fd_limit < 2) {
            (void)close(descriptor);
            errno = EMFILE;
            return -1;
        }
        for (size_t index = 0u; index < length; ++index)
            component[index] = path[start + index];
        component[length] = '\0';
        next_descriptor = openat(descriptor, component, directory_flags);
        if (next_descriptor < 0) {
            int saved_errno = errno;
            (void)close(descriptor);
            errno = saved_errno;
            return -1;
        }
        if (fstat(next_descriptor, &status) != 0) {
            int saved_errno = errno;
            (void)close(next_descriptor);
            (void)close(descriptor);
            errno = saved_errno;
            return -1;
        }
        if (close(descriptor) != 0) {
            int saved_errno = errno;
            (void)close(next_descriptor);
            errno = saved_errno;
            return -1;
        }
        descriptor = next_descriptor;
    }
    if (fstat(descriptor, &status) != 0) {
        int saved_errno = errno;
        (void)close(descriptor);
        errno = saved_errno;
        return -1;
    }
    if (expected && !rin_ftw_same_object(&status, expected)) {
        (void)close(descriptor);
        errno = EAGAIN;
        return -1;
    }
    return descriptor;
}

/* 親directoryをno-followで開き、必要なら既知のinode identityを確認する。 */
static int rin_ftw_open_parent_path(const char* path, int fd_limit,
                                    const struct stat* expected_parent,
                                    int* parent_descriptor,
                                    struct stat* parent_status,
                                    char leaf[NAME_MAX + 1u]) {
    size_t length;
    size_t leaf_start;
    size_t leaf_length;
    size_t slash;
    size_t parent_length;
    char* parent_path;
    int descriptor;
    if (!path || !parent_descriptor || !parent_status || !leaf ||
        path[0] != '/') {
        errno = EINVAL;
        return -1;
    }
    length = rin_ftw_length(path);
    while (length > 1u && path[length - 1u] == '/') --length;
    if (length == 1u && path[0] == '/') {
        *parent_descriptor = -1;
        leaf[0] = '\0';
        (void)fd_limit;
        (void)parent_status;
        return 0;
    }
    slash = length;
    while (slash > 0u && path[slash - 1u] != '/') --slash;
    leaf_start = slash;
    leaf_length = length - leaf_start;
    if (leaf_length == 0u || leaf_length > NAME_MAX) {
        errno = leaf_length == 0u ? EINVAL : ENAMETOOLONG;
        return -1;
    }
    for (size_t index = 0u; index < leaf_length; ++index)
        leaf[index] = path[leaf_start + index];
    leaf[leaf_length] = '\0';
    parent_length = slash <= 1u ? 1u : slash - 1u;
    parent_path = rin_ftw_copy(path, parent_length);
    if (!parent_path) return -1;
    descriptor = rin_ftw_open_directory_path(parent_path, fd_limit,
                                              expected_parent);
    free(parent_path);
    if (descriptor < 0) return -1;
    if (fstat(descriptor, parent_status) != 0) {
        int saved_errno = errno;
        (void)close(descriptor);
        errno = saved_errno;
        return -1;
    }
    *parent_descriptor = descriptor;
    return 0;
}

/* FTW_PHYSは親directoryのidentityを確認したdescriptor相対走査を使う。 */
static int rin_ftw_walk_physical(RinFtwState* state, const char* walk_path,
                                 const char* display_path, int level,
                                 const RinFtwAncestor* ancestors,
                                 int is_root) {
    struct stat expected_parent_storage;
    const struct stat* expected_parent = NULL;
    struct stat parent_status;
    struct stat status;
    struct stat opened_status;
    char leaf[NAME_MAX + 1u];
    int parent_descriptor = -1;
    int node_descriptor = -1;
    int parent_result;
    int result;
    int depth_first = (state->flags & FTW_DEPTH) != 0;
    int root_is_slash;
    int directory_cycle;
    if (ancestors) {
        unsigned char* bytes = (unsigned char*)&expected_parent_storage;
        for (size_t index = 0u; index < sizeof(expected_parent_storage); ++index)
            bytes[index] = 0u;
        expected_parent_storage.st_dev = ancestors->device;
        expected_parent_storage.st_ino = ancestors->inode;
        expected_parent = &expected_parent_storage;
    }
    parent_result = rin_ftw_open_parent_path(
        walk_path, state->fd_limit, expected_parent, &parent_descriptor,
        &parent_status, leaf);
    if (parent_result < 0) return -1;
    root_is_slash = parent_descriptor < 0;

    if (root_is_slash) {
        node_descriptor = rin_ftw_open_directory_path(
            walk_path, state->fd_limit, NULL);
        if (node_descriptor < 0) return -1;
        if (fstat(node_descriptor, &status) != 0) {
            int saved_errno = errno;
            (void)close(node_descriptor);
            errno = saved_errno;
            return -1;
        }
        parent_status = status;
    } else if (fstatat(parent_descriptor, leaf, &status,
                      AT_SYMLINK_NOFOLLOW) != 0) {
        int stat_error = errno;
        if (stat_error == EACCES) {
            struct stat empty_status;
            unsigned char* bytes = (unsigned char*)&empty_status;
            for (size_t index = 0u; index < sizeof(empty_status); ++index)
                bytes[index] = 0u;
            result = rin_ftw_invoke_checked(state, display_path, walk_path,
                &empty_status, FTW_NS, level, &parent_status);
            (void)close(parent_descriptor);
            return result;
        }
        (void)close(parent_descriptor);
        errno = stat_error;
        return -1;
    }

    if (is_root && (state->flags & FTW_MOUNT) != 0)
        state->root_device = status.st_dev;
    if (!is_root && (state->flags & FTW_MOUNT) != 0 &&
        status.st_dev != state->root_device) {
        if (parent_descriptor >= 0) (void)close(parent_descriptor);
        return 0;
    }
    if (S_ISLNK(status.st_mode)) {
        result = rin_ftw_invoke_checked(state, display_path, walk_path,
            &status, FTW_SL, level, &parent_status);
        if (parent_descriptor >= 0) (void)close(parent_descriptor);
        return result;
    }
    if (!S_ISDIR(status.st_mode)) {
        result = rin_ftw_invoke_checked(state, display_path, walk_path,
            &status, FTW_F, level, &parent_status);
        if (parent_descriptor >= 0) (void)close(parent_descriptor);
        return result;
    }

    directory_cycle = rin_ftw_seen_ancestor(ancestors, &status);
    if (directory_cycle) {
        if (parent_descriptor >= 0) (void)close(parent_descriptor);
        if (depth_first) return 0;
        return rin_ftw_invoke_checked(state, display_path, walk_path,
            &status, FTW_D, level, &parent_status);
    }

    if (!root_is_slash) {
        if (state->fd_limit < 2) {
            (void)close(parent_descriptor);
            errno = EMFILE;
            return -1;
        }
        node_descriptor = openat(parent_descriptor, leaf,
            O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
    }
    if (node_descriptor < 0) {
        int open_error = errno;
        struct stat replacement;
        if (!root_is_slash &&
            fstatat(parent_descriptor, leaf, &replacement,
                    AT_SYMLINK_NOFOLLOW) == 0 &&
            S_ISLNK(replacement.st_mode)) {
            result = rin_ftw_invoke_checked(state, display_path, walk_path,
                &replacement, FTW_SL, level, &parent_status);
            (void)close(parent_descriptor);
            return result;
        }
        if (open_error == EACCES) {
            result = rin_ftw_invoke_checked(state, display_path, walk_path,
                &status, FTW_DNR, level, &parent_status);
            (void)close(parent_descriptor);
            return result;
        }
        if (parent_descriptor >= 0) (void)close(parent_descriptor);
        errno = open_error;
        return -1;
    }
    if (fstat(node_descriptor, &opened_status) != 0) {
        int saved_errno = errno;
        (void)close(node_descriptor);
        if (parent_descriptor >= 0) (void)close(parent_descriptor);
        errno = saved_errno;
        return -1;
    }
    if (!rin_ftw_same_object(&status, &opened_status)) {
        (void)close(node_descriptor);
        if (parent_descriptor >= 0) (void)close(parent_descriptor);
        errno = EAGAIN;
        return -1;
    }
    status = opened_status;
    if (parent_descriptor >= 0) {
        if (close(parent_descriptor) != 0) {
            int saved_errno = errno;
            (void)close(node_descriptor);
            errno = saved_errno;
            return -1;
        }
        parent_descriptor = -1;
    }

    {
        RinFtwNames names;
        RinFtwAncestor current;
        int read_result = rin_ftw_read_names_fd(node_descriptor, &names);
        node_descriptor = -1;
        if (read_result == 1)
            return rin_ftw_invoke_checked(state, display_path, walk_path,
                &status, FTW_DNR, level, &parent_status);
        if (read_result < 0) return -1;
        if (!depth_first) {
            result = rin_ftw_invoke_checked(state, display_path, walk_path,
                &status, FTW_D, level, &parent_status);
            if (result != 0) {
                rin_ftw_names_clear(&names);
                return result;
            }
        }
        current.device = status.st_dev;
        current.inode = status.st_ino;
        current.parent = ancestors;
        for (size_t index = 0u; index < names.count; ++index) {
            char* child_walk_path = rin_ftw_child_path(walk_path,
                                                       names.values[index]);
            char* child_display_path;
            if (!child_walk_path) {
                rin_ftw_names_clear(&names);
                return -1;
            }
            child_display_path = rin_ftw_child_path(display_path,
                                                     names.values[index]);
            if (!child_display_path) {
                free(child_walk_path);
                rin_ftw_names_clear(&names);
                return -1;
            }
            result = level == INT_MAX
                ? (errno = EOVERFLOW, -1)
                : rin_ftw_walk_physical(state, child_walk_path,
                    child_display_path, level + 1, &current, 0);
            free(child_display_path);
            free(child_walk_path);
            if (result != 0) {
                rin_ftw_names_clear(&names);
                return result;
            }
        }
        rin_ftw_names_clear(&names);
    }
    if (depth_first)
        return rin_ftw_invoke_checked(state, display_path, walk_path,
            &status, FTW_DP, level, &parent_status);
    return 0;
}

/* Return zero to continue, one when a callback requested a stop, and minus
 * one for a traversal error. */
static int rin_ftw_walk(RinFtwState* state, const char* walk_path,
                        const char* display_path, int level,
                        const RinFtwAncestor* ancestors, int is_root) {
    struct stat status;
    RinFtwNames names;
    int read_result;
    int depth_first = (state->flags & FTW_DEPTH) != 0;
    int physical = (state->flags & FTW_PHYS) != 0;
    int directory_cycle;
    int result;
    size_t index;

    if (physical)
        return rin_ftw_walk_physical(state, walk_path, display_path, level,
                                     ancestors, is_root);

    if ((physical ? lstat(walk_path, &status) : stat(walk_path, &status)) != 0)
        return rin_ftw_stat_error(state, walk_path, display_path, level,
                                  errno);

    if (!is_root && (state->flags & FTW_MOUNT) != 0 &&
        status.st_dev != state->root_device)
        return 0;

    if (S_ISLNK(status.st_mode) && physical)
        return rin_ftw_invoke(state, display_path, walk_path, &status,
                              FTW_SL, level);

    if (!S_ISDIR(status.st_mode))
        return rin_ftw_invoke(state, display_path, walk_path, &status,
                              FTW_F, level);

    directory_cycle = !physical && rin_ftw_seen_ancestor(ancestors, &status);
    if (directory_cycle) {
        if (depth_first) return 0;
        return rin_ftw_invoke(state, display_path, walk_path, &status,
                              FTW_D, level);
    }

    read_result = rin_ftw_read_names(walk_path, &names);
    if (read_result == 1)
        return rin_ftw_invoke(state, display_path, walk_path, &status,
                              FTW_DNR, level);
    if (read_result < 0) return -1;

    if (!depth_first) {
        result = rin_ftw_invoke(state, display_path, walk_path, &status,
                                FTW_D, level);
        if (result != 0) {
            rin_ftw_names_clear(&names);
            return result;
        }
    }

    {
        RinFtwAncestor current;
        current.device = status.st_dev;
        current.inode = status.st_ino;
        current.parent = ancestors;
        for (index = 0u; index < names.count; ++index) {
            char* child_walk_path = rin_ftw_child_path(walk_path,
                                                        names.values[index]);
            char* child_display_path;
            if (!child_walk_path) {
                rin_ftw_names_clear(&names);
                return -1;
            }
            child_display_path = rin_ftw_child_path(display_path,
                                                      names.values[index]);
            if (!child_display_path) {
                free(child_walk_path);
                rin_ftw_names_clear(&names);
                return -1;
            }
            result = level == INT_MAX
                ? (errno = EOVERFLOW, -1)
                : rin_ftw_walk(state, child_walk_path, child_display_path,
                               level + 1, &current, 0);
            free(child_display_path);
            free(child_walk_path);
            if (result != 0) {
                rin_ftw_names_clear(&names);
                return result;
            }
        }
    }
    rin_ftw_names_clear(&names);

    if (depth_first)
        return rin_ftw_invoke(state, display_path, walk_path, &status,
                              FTW_DP, level);
    return 0;
}

static char* rin_ftw_absolute_path(const char* path, const char* cwd) {
    size_t path_length = rin_ftw_length(path);
    size_t cwd_length;
    size_t total;
    int separator;
    char* result;
    size_t index;
    if (path[0] == '/') {
        if (path_length >= PATH_MAX) {
            errno = ENAMETOOLONG;
            return NULL;
        }
        result = rin_ftw_copy(path, path_length);
    } else {
        cwd_length = rin_ftw_length(cwd);
        separator = cwd_length != 0u && cwd[cwd_length - 1u] != '/';
        total = cwd_length + (separator ? 1u : 0u) + path_length;
        if (total >= PATH_MAX) {
            errno = ENAMETOOLONG;
            return NULL;
        }
        result = (char*)malloc(total + 1u);
        if (!result) {
            errno = ENOMEM;
            return NULL;
        }
        for (index = 0u; index < cwd_length; ++index) result[index] = cwd[index];
        if (separator) result[cwd_length++] = '/';
        for (index = 0u; index < path_length; ++index)
            result[cwd_length + index] = path[index];
        result[total] = '\0';
    }
    if (!result) return NULL;

    return result;
}

static int rin_ftw_run(const char* path, int fd_limit, int flags,
                       RinFtwCallback3 callback3,
                       RinFtwCallback4 callback4, int is_nftw) {
    RinFtwState state;
    struct stat root_status;
    char cwd[PATH_MAX];
    char* saved_cwd = NULL;
    char* walk_path = NULL;
    char* display_path = NULL;
    int traversal_locked = 0;
    int result;
    int saved_errno;
    int mutex_result;

    if (!path || (!callback3 && !callback4)) {
        errno = EFAULT;
        return -1;
    }
    if (fd_limit < 1 || (!is_nftw && fd_limit > OPEN_MAX) ||
        (flags & ~(FTW_PHYS | FTW_MOUNT | FTW_CHDIR | FTW_DEPTH)) != 0) {
        errno = EINVAL;
        return -1;
    }

    if (path[0] == '\0') {
        errno = ENOENT;
        return -1;
    }
    /* FTW_CHDIR changes the process cwd during callbacks, so serialize those
     * walks. Relative walks without FTW_CHDIR only need a cwd snapshot; make
     * their root absolute before releasing the lock so callbacks and other
     * ftw walks cannot change the base used by later path operations. */
    if ((flags & FTW_CHDIR) != 0 || path[0] != '/') {
        if (rin_ftw_lock_chdir() != 0) return -1;
        traversal_locked = 1;
    }

    if ((flags & FTW_CHDIR) != 0) {
        if (!getcwd(cwd, sizeof(cwd))) {
            result = -1;
            goto done;
        }
        saved_cwd = rin_ftw_copy(cwd, rin_ftw_length(cwd));
        if (!saved_cwd) {
            result = -1;
            goto done;
        }
        walk_path = rin_ftw_absolute_path(path, saved_cwd);
        if (!walk_path) {
            result = -1;
            goto done;
        }
    } else if (path[0] != '/') {
        if (!getcwd(cwd, sizeof(cwd))) {
            result = -1;
            goto done;
        }
        walk_path = rin_ftw_absolute_path(path, cwd);
        if (!walk_path) {
            result = -1;
            goto done;
        }
        mutex_result = pthread_mutex_unlock(&g_rin_ftw_chdir_mutex);
        if (mutex_result != 0) {
            errno = mutex_result;
            result = -1;
            goto done;
        }
        traversal_locked = 0;
    } else {
        size_t length = rin_ftw_length(path);
        if (length >= PATH_MAX) {
            errno = ENAMETOOLONG;
            result = -1;
            goto done;
        }
        walk_path = rin_ftw_copy(path, length);
        if (!walk_path) {
            result = -1;
            goto done;
        }
    }

    display_path = rin_ftw_copy(path, rin_ftw_length(path));
    if (!display_path) {
        result = -1;
        goto done;
    }

    state.flags = flags;
    state.is_nftw = is_nftw;
    state.fd_limit = fd_limit;
    state.root_device = 0;
    state.callback3 = callback3;
    state.callback4 = callback4;
    state.stopped = 0;
    state.stop_value = 0;
    if ((flags & FTW_MOUNT) != 0 && (flags & FTW_PHYS) == 0) {
        if (stat(walk_path, &root_status) != 0) {
            result = rin_ftw_stat_error(&state, walk_path, display_path,
                                        0, errno);
            if (state.stopped) result = state.stop_value;
            goto finished_walk;
        }
        state.root_device = root_status.st_dev;
    }

    result = rin_ftw_walk(&state, walk_path, display_path, 0, NULL, 1);
    if (state.stopped) result = state.stop_value;

finished_walk:
    saved_errno = errno;
    if (traversal_locked && saved_cwd && chdir(saved_cwd) != 0) {
        result = -1;
        saved_errno = errno;
    }
    errno = saved_errno;
done:
    free(walk_path);
    free(display_path);
    free(saved_cwd);
    if (traversal_locked) {
        int unlock_result = pthread_mutex_unlock(&g_rin_ftw_chdir_mutex);
        if (unlock_result != 0 && result >= 0) {
            errno = unlock_result;
            result = -1;
        }
    }
    return result;
}

int nftw(const char* path,
         int (*fn)(const char*, const struct stat*, int, struct FTW*),
         int fd_limit, int flags) {
    return rin_ftw_run(path, fd_limit, flags, NULL, fn, 1);
}

int ftw(const char* path,
        int (*fn)(const char*, const struct stat*, int), int fd_limit) {
    if (!fn) {
        errno = EFAULT;
        return -1;
    }
    return rin_ftw_run(path, fd_limit, 0, fn, NULL, 0);
}
