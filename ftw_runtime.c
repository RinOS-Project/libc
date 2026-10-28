/* SPDX-License-Identifier: MIT */

#include "ftw.h"

#include "dirent.h"
#include "errno.h"
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

/* Return zero for a complete snapshot, one when the directory is unreadable,
 * and minus one for all other errors. The stream is always closed before the
 * caller descends, so a one-descriptor limit is sufficient at every depth. */
static int rin_ftw_read_names(const char* path, RinFtwNames* names) {
    DIR* directory;
    int read_error = 0;
    int close_error = 0;

    names->values = NULL;
    names->count = 0u;
    names->capacity = 0u;
    directory = opendir(path);
    if (!directory) return errno == EACCES ? 1 : -1;

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

static char* rin_ftw_child_path(const char* parent, const char* child) {
    size_t parent_length = rin_ftw_length(parent);
    size_t child_length = rin_ftw_length(child);
    int needs_separator = !(parent_length == 1u && parent[0] == '/');
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

static int rin_ftw_invoke(RinFtwState* state, const char* path,
                          const struct stat* status, int type, int level) {
    struct FTW information;
    int result;
    int callback_errno;
    if ((state->flags & FTW_CHDIR) != 0) {
        char* parent = rin_ftw_parent_path(path);
        if (!parent) return -1;
        if (chdir(parent) != 0) {
            free(parent);
            return -1;
        }
        free(parent);
    }

    information.base = (int)rin_ftw_base(path);
    information.level = level;
    errno = 0;
    if (state->is_nftw)
        result = state->callback4(path, status, type, &information);
    else
        result = state->callback3(path, status, type);
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

static int rin_ftw_stat_error(RinFtwState* state, const char* path,
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
        lstat(path, &link_status) == 0 && S_ISLNK(link_status.st_mode)) {
        type = FTW_SLN;
        callback_status = &link_status;
    } else if (stat_error != EACCES) {
        errno = stat_error;
        return -1;
    }
    return rin_ftw_invoke(state, path, callback_status, type, level);
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

/* Return zero to continue, one when a callback requested a stop, and minus
 * one for a traversal error. */
static int rin_ftw_walk(RinFtwState* state, const char* path, int level,
                        const RinFtwAncestor* ancestors, int is_root) {
    struct stat status;
    RinFtwNames names;
    int read_result;
    int depth_first = (state->flags & FTW_DEPTH) != 0;
    int physical = (state->flags & FTW_PHYS) != 0;
    int directory_cycle;
    int result;
    size_t index;

    if ((physical ? lstat(path, &status) : stat(path, &status)) != 0)
        return rin_ftw_stat_error(state, path, level, errno);

    if (!is_root && (state->flags & FTW_MOUNT) != 0 &&
        status.st_dev != state->root_device)
        return 0;

    if (S_ISLNK(status.st_mode) && physical)
        return rin_ftw_invoke(state, path, &status, FTW_SL, level);

    if (!S_ISDIR(status.st_mode))
        return rin_ftw_invoke(state, path, &status, FTW_F, level);

    directory_cycle = !physical && rin_ftw_seen_ancestor(ancestors, &status);
    if (directory_cycle) {
        if (depth_first) return 0;
        return rin_ftw_invoke(state, path, &status, FTW_D, level);
    }

    read_result = rin_ftw_read_names(path, &names);
    if (read_result == 1)
        return rin_ftw_invoke(state, path, &status, FTW_DNR, level);
    if (read_result < 0) return -1;

    if (!depth_first) {
        result = rin_ftw_invoke(state, path, &status, FTW_D, level);
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
            char* child_path = rin_ftw_child_path(path, names.values[index]);
            if (!child_path) {
                rin_ftw_names_clear(&names);
                return -1;
            }
            result = level == INT_MAX
                ? (errno = EOVERFLOW, -1)
                : rin_ftw_walk(state, child_path, level + 1, &current, 0);
            free(child_path);
            if (result != 0) {
                rin_ftw_names_clear(&names);
                return result;
            }
        }
    }
    rin_ftw_names_clear(&names);

    if (depth_first)
        return rin_ftw_invoke(state, path, &status, FTW_DP, level);
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
        separator = !(cwd_length == 1u && cwd[0] == '/');
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

    path_length = rin_ftw_length(result);
    while (path_length > 1u && result[path_length - 1u] == '/')
        result[--path_length] = '\0';
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
    int traversal_locked = 0;
    int result;
    int saved_errno;

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
    /* Absolute walks without FTW_CHDIR neither depend on nor change cwd.
     * Relative walks retain the lock so FTW_CHDIR callbacks cannot change
     * their path base while they are traversing. */
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
    } else {
        size_t length = rin_ftw_length(path);
        while (length > 1u && path[length - 1u] == '/') --length;
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

    state.flags = flags;
    state.is_nftw = is_nftw;
    state.root_device = 0;
    state.callback3 = callback3;
    state.callback4 = callback4;
    state.stopped = 0;
    state.stop_value = 0;
    if ((flags & FTW_MOUNT) != 0) {
        if ((flags & FTW_PHYS) != 0) {
            if (lstat(walk_path, &root_status) != 0) {
                result = rin_ftw_stat_error(&state, walk_path, 0, errno);
                if (state.stopped) result = state.stop_value;
                goto finished_walk;
            }
        } else if (stat(walk_path, &root_status) != 0) {
            result = rin_ftw_stat_error(&state, walk_path, 0, errno);
            if (state.stopped) result = state.stop_value;
            goto finished_walk;
        }
        state.root_device = root_status.st_dev;
    }

    result = rin_ftw_walk(&state, walk_path, 0, NULL, 1);
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
