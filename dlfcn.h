/* SPDX-License-Identifier: MIT */
/*
 * RinOS libc - dlfcn.h
 * 動的リンク関数 (POSIX互換)
 * カーネルdynlink.c連携 (RLL形式対応)
 */

#ifndef _DLFCN_H
#define _DLFCN_H

#include "stddef.h"
#include "errno.h"
#include "limits.h"
#include "link.h"
#include "../../src/shared/rin_dynlink_addr_abi.h"
#include "../../src/shared/rin_dynlink_flags.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════
 * dlopen フラグ
 * ═══════════════════════════════════════════════════════════════*/

#define RTLD_LAZY     RIN_DYNLINK_FLAG_LAZY      /* 遅延バインディング */
#define RTLD_NOW      RIN_DYNLINK_FLAG_NOW       /* 即時バインディング */
#define RTLD_GLOBAL   RIN_DYNLINK_FLAG_GLOBAL    /* シンボルをグローバルに公開 */
#define RTLD_LOCAL    0x0000  /* シンボルをローカルに保持 (デフォルト) */
#define RTLD_NOLOAD   RIN_DYNLINK_FLAG_NOLOAD    /* ロードせずに既存ハンドルを返す */
#define RTLD_NODELETE RIN_DYNLINK_FLAG_NODELETE  /* dlcloseでアンロードしない */
#define RTLD_DEEPBIND RIN_DYNLINK_FLAG_DEEPBIND  /* ライブラリ内シンボルを優先 */

/* 特殊ハンドル */
#define RTLD_DEFAULT  ((void*)0)   /* デフォルト検索 */
#define RTLD_NEXT     ((void*)-1)  /* 次のシンボルを検索 */
#define RIN_RTLD_MAIN ((void*)-2)  /* dlopen(NULL, ...) result */

/* ═══════════════════════════════════════════════════════════════
 * Dl_info構造体 (dladdr用)
 * ═══════════════════════════════════════════════════════════════*/

typedef struct {
    const char* dli_fname;   /* 共有ライブラリのパス名 */
    void*       dli_fbase;   /* 共有ライブラリのベースアドレス */
    const char* dli_sname;   /* シンボル名 */
    void*       dli_saddr;   /* シンボルのアドレス */
} Dl_info;

/* ═══════════════════════════════════════════════════════════════
 * カーネルdynlink API (syscall経由)
 * ═══════════════════════════════════════════════════════════════*/

#include "sys/syscall.h"

/* Hosted contract tests may inject the inventory syscall without enabling
 * the full freestanding target.  Remember that explicit hook so RTLD_NEXT
 * can exercise the product algorithm there; an ordinary hosted binary must
 * not execute Rin syscall numbers on its host kernel. */
#if defined(_RIN_DYNLINK_SYSCALL2) && \
    !defined(RIN_DYNLINK_SYSCALL2_DEFAULT_HOOK)
#define RIN_DYNLINK_CUSTOM_SYSCALL2 1
#endif

#ifndef _RIN_DYNLINK_SYSCALL0
#define _RIN_DYNLINK_SYSCALL0(number) _syscall0((uintptr_t)(number))
#endif
#ifndef _RIN_DYNLINK_SYSCALL1
#define _RIN_DYNLINK_SYSCALL1(number, argument) \
    _syscall1((uintptr_t)(number), (uintptr_t)(argument))
#endif
#ifndef _RIN_DYNLINK_SYSCALL2
#define RIN_DYNLINK_SYSCALL2_DEFAULT_HOOK 1
#define _RIN_DYNLINK_SYSCALL2(number, argument1, argument2) \
    _syscall2((uintptr_t)(number), (uintptr_t)(argument1), \
              (uintptr_t)(argument2))
#endif
#ifndef _RIN_DYNLINK_SYSCALL3
#define _RIN_DYNLINK_SYSCALL3(number, argument1, argument2, argument3) \
    _syscall3((uintptr_t)(number), (uintptr_t)(argument1), \
              (uintptr_t)(argument2), (uintptr_t)(argument3))
#endif

static inline void* _dynlink_pointer_result(intptr_t result) {
    if (result < 0) {
        errno = (result >= -4095) ? (int)-result : EIO;
        return NULL;
    }
    return (void*)(uintptr_t)result;
}

static inline int _dynlink_status_result(intptr_t result) {
    result = __rin_syscall_posixize(result);
    if (result < 0) {
        if (result != -1) errno = EIO;
        return -1;
    }
    if (result != 0) {
        errno = (uintptr_t)result > (uintptr_t)INT_MAX ? EOVERFLOW : EIO;
        return -1;
    }
    return 0;
}

/* RIN64 v3 uses the process-owned signed dependency graph.  dlopen may
 * transactionally admit a signed library graph from the process-private
 * canonical namespace; it never expands a caller-supplied host search path or
 * loads an unsigned image. */
static inline int dynlink_init(void) {
    /* 初期化はカーネル起動時に完了、ユーザースペースからは不要 */
    return 0;
}

static inline void* dynlink_load_flags(const char* path, unsigned int flags) {
    return _dynlink_pointer_result(_RIN_DYNLINK_SYSCALL2(
        SYS_DLOPEN_FLAGS, (uintptr_t)path, (uintptr_t)flags));
}

static inline void* dynlink_load(const char* path) {
    return _dynlink_pointer_result(_RIN_DYNLINK_SYSCALL1(
        SYS_DLOPEN, (uintptr_t)path));
}

static inline int dynlink_unload(void* handle) {
    return _dynlink_status_result(_RIN_DYNLINK_SYSCALL1(
        SYS_DLCLOSE, (uintptr_t)handle));
}

static inline void* dynlink_resolve_global(const char* name) {
    return _dynlink_pointer_result(_RIN_DYNLINK_SYSCALL1(
        SYS_DLSYM, (uintptr_t)name));
}

static inline void* dynlink_resolve(void* handle, const char* name) {
    if (!handle || handle == RTLD_DEFAULT || handle == RIN_RTLD_MAIN) {
        return dynlink_resolve_global(name);
    }
    return _dynlink_pointer_result(_RIN_DYNLINK_SYSCALL2(
        SYS_DLSYM_FROM, (uintptr_t)handle, (uintptr_t)name));
}

static inline void* dynlink_resolve_versioned(void* handle, const char* name,
                                              const char* version) {
    return _dynlink_pointer_result(_RIN_DYNLINK_SYSCALL3(
        SYS_DLSYM_VERSIONED, (uintptr_t)handle, (uintptr_t)name,
        (uintptr_t)version));
}

static inline int dynlink_add_search_path(const char* path) {
    return _dynlink_status_result(_RIN_DYNLINK_SYSCALL1(
        SYS_DL_ADD_PATH, (uintptr_t)path));
}

static inline const char* dynlink_error(void) {
    return (const char*)_dynlink_pointer_result(
        _RIN_DYNLINK_SYSCALL0(SYS_DLERROR));
}

/* ═══════════════════════════════════════════════════════════════
 * エラーメッセージバッファ (ローカル用)
 * ═══════════════════════════════════════════════════════════════*/

#if defined(__cplusplus)
#define RIN_DLERROR_THREAD_LOCAL thread_local
#else
#define RIN_DLERROR_THREAD_LOCAL _Thread_local
#endif
/* Local diagnostics must not be shared by callers on different threads.
 * The kernel error channel is copied into this owner before publication. */
static RIN_DLERROR_THREAD_LOCAL char _dlerror_buf[256] = {0};
static RIN_DLERROR_THREAD_LOCAL int _dlerror_flag = 0;
#undef RIN_DLERROR_THREAD_LOCAL

static inline void _dl_set_error(const char* msg) {
    int i = 0;
    while (msg[i] && i < 255) {
        _dlerror_buf[i] = msg[i];
        i++;
    }
    _dlerror_buf[i] = '\0';
    _dlerror_flag = 1;
}

/* Forward declaration because the RTLD_NEXT helper is shared by dlsym and
 * dlvsym but is defined before the POSIX entry-point block below. */
static inline void* dlopen(const char* filename, int flags);

#if defined(__cplusplus)
#define RIN_RTLD_NEXT_THREAD_LOCAL thread_local
#else
#define RIN_RTLD_NEXT_THREAD_LOCAL _Thread_local
#endif
#define RIN_RTLD_NEXT_HANDLE_CAPACITY 64u
typedef struct RinRtldNextHandle {
    void* handle;
    char name[RIN_DLINVENTORY_V1_NAME_CAPACITY];
} RinRtldNextHandle;
/* A retained handle is the lifetime lease for a symbol returned from a
 * dynamic catalog.  Process teardown closes the object table; keeping the
 * lease here prevents a successful RTLD_NEXT address from being invalidated
 * by the loader's unused-tail reclamation immediately after lookup. */
static RIN_RTLD_NEXT_THREAD_LOCAL RinRtldNextHandle
    _rin_rtld_next_handles[RIN_RTLD_NEXT_HANDLE_CAPACITY];
#undef RIN_RTLD_NEXT_THREAD_LOCAL

static inline int _rin_dl_retained_name_equal(
    const RinRtldNextHandle* retained, const char* name) {
    unsigned int index;
    if (!retained || !retained->handle || !name) return 0;
    for (index = 0u; index < RIN_DLINVENTORY_V1_NAME_CAPACITY; ++index) {
        if (retained->name[index] != name[index]) return 0;
        if (retained->name[index] == '\0') return 1;
    }
    return 0;
}

static inline int _rin_dl_name_equal(const char* first, const char* second) {
    unsigned int index;
    if (!first || !second) return 0;
    for (index = 0u; index < RIN_DLINVENTORY_V1_NAME_CAPACITY; ++index) {
        if (first[index] != second[index]) return 0;
        if (first[index] == '\0') return 1;
    }
    return 0;
}

static inline unsigned int _rin_dl_retained_find(const char* name) {
    for (unsigned int slot = 0u;
         slot < RIN_RTLD_NEXT_HANDLE_CAPACITY; ++slot) {
        if (_rin_dl_retained_name_equal(&_rin_rtld_next_handles[slot], name))
            return slot;
    }
    return RIN_RTLD_NEXT_HANDLE_CAPACITY;
}

static inline unsigned int _rin_dl_retained_add(void* handle,
                                                const char* name) {
    unsigned int slot;
    unsigned int index;
    if (!handle || !name) return RIN_RTLD_NEXT_HANDLE_CAPACITY;
    slot = _rin_dl_retained_find(name);
    if (slot < RIN_RTLD_NEXT_HANDLE_CAPACITY) return slot;
    for (slot = 0u; slot < RIN_RTLD_NEXT_HANDLE_CAPACITY; ++slot) {
        if (_rin_rtld_next_handles[slot].handle) continue;
        _rin_rtld_next_handles[slot].handle = handle;
        for (index = 0u; index < RIN_DLINVENTORY_V1_NAME_CAPACITY; ++index) {
            _rin_rtld_next_handles[slot].name[index] = name[index];
            if (name[index] == '\0') return slot;
        }
        /* A caller-owned name must be terminated within the inventory bound;
         * undo the slot if the caller violated that admission contract. */
        _rin_rtld_next_handles[slot].handle = NULL;
        _rin_rtld_next_handles[slot].name[0] = '\0';
        return RIN_RTLD_NEXT_HANDLE_CAPACITY;
    }
    return RIN_RTLD_NEXT_HANDLE_CAPACITY;
}

/* Check the authenticated process catalog without asking the loader to load
 * anything.  RTLD_NOLOAD depends on this distinction: a successful lookup
 * must acquire an existing object handle, while a missing name must not grow
 * the dynamic graph as a side effect. */
static inline int _rin_dl_inventory_contains(const char* name) {
    RinDlInventoryRecordV1 record;
    unsigned long index;
    unsigned long expected_count = 0UL;
    unsigned long long expected_generation = 0ULL;
    unsigned long long expected_adds = 0ULL;
    unsigned long long expected_subs = 0ULL;
    int fetch_result;

    if (!name || name[0] == '\0') {
        errno = EINVAL;
        return -1;
    }
    for (index = 0UL; index < RIN_DLINVENTORY_V1_NAME_CAPACITY; ++index) {
        if (name[index] == '\0') break;
    }
    if (index == 0UL || index == RIN_DLINVENTORY_V1_NAME_CAPACITY) {
        errno = EINVAL;
        return -1;
    }

#if !(defined(RIN_FREESTANDING) && RIN_FREESTANDING) && \
    !defined(RIN_DYNLINK_CUSTOM_SYSCALL2)
    errno = ENOSYS;
    return -1;
#endif

    for (index = 0UL; ; ++index) {
        fetch_result = __rin_dl_inventory_fetch(index, &record);
        if (fetch_result == 1) break;
        if (fetch_result != 0) return -1;
        if (index == 0UL) {
            expected_count = record.record_count;
            expected_generation = record.launch_generation;
            expected_adds = record.add_count;
            expected_subs = record.sub_count;
        } else if (record.record_count != expected_count ||
                   record.launch_generation != expected_generation ||
                   record.add_count != expected_adds ||
                   record.sub_count != expected_subs) {
            errno = EIO;
            return -1;
        }
        if (index >= expected_count) {
            errno = EIO;
            return -1;
        }
        if (_rin_dl_name_equal(record.name, name)) return 1;
        if (index + 1UL == expected_count) break;
    }
    errno = ENOENT;
    return 0;
}

/* Resolve RTLD_NEXT from the call site rather than guessing a global symbol.
 * RinOS publishes a generation-consistent inventory in load order; the
 * address immediately after the dlsym call identifies the image that owns
 * the caller, and only later images are eligible.  This keeps the lookup
 * inside the authenticated graph instead of implementing RTLD_NEXT as an
 * alias for RTLD_DEFAULT. */
static inline void* _rin_dlsym_next(const char* symbol, int versioned,
                                    const char* version) {
    RinDlInventoryRecordV1 record;
    unsigned long index;
    unsigned long caller_index = ~0UL;
    unsigned long expected_count = 0UL;
    unsigned long long expected_generation = 0ULL;
    unsigned long long expected_adds = 0ULL;
    unsigned long long expected_subs = 0ULL;
    uintptr_t caller_address = 0u;
    int fetch_result;

#if !(defined(RIN_FREESTANDING) && RIN_FREESTANDING) && \
    !defined(RIN_DYNLINK_CUSTOM_SYSCALL2)
    (void)symbol;
    (void)versioned;
    (void)version;
    errno = ENOSYS;
    return NULL;
#endif

#if defined(__GNUC__) || defined(__clang__)
    caller_address = (uintptr_t)__builtin_return_address(0);
#endif
    if (caller_address == 0u) {
        errno = EINVAL;
        return NULL;
    }

    /* First walk identifies the caller image and validates one immutable
     * inventory generation.  A second walk below revalidates the same
     * generation while resolving candidates, so detach/attach cannot turn a
     * stale name into an address from a different graph. */
    for (index = 0UL; ; ++index) {
        fetch_result = __rin_dl_inventory_fetch(index, &record);
        if (fetch_result == 1) break;
        if (fetch_result != 0) return NULL;
        if (index == 0UL) {
            expected_count = record.record_count;
            expected_generation = record.launch_generation;
            expected_adds = record.add_count;
            expected_subs = record.sub_count;
        } else if (record.record_count != expected_count ||
                   record.launch_generation != expected_generation ||
                   record.add_count != expected_adds ||
                   record.sub_count != expected_subs) {
            errno = EIO;
            return NULL;
        }
        if (index >= expected_count ||
            record.load_address > (uint64_t)caller_address ||
            (uint64_t)caller_address - record.load_address >=
                record.mapped_size)
            goto next_inventory_record;
        if (caller_index != ~0UL) {
            /* Overlapping image ranges would make the caller identity
             * ambiguous; the kernel catalog must never publish that graph. */
            errno = EIO;
            return NULL;
        }
        caller_index = index;
next_inventory_record:
        if (index + 1UL == expected_count) break;
    }
    if (caller_index == ~0UL) {
        errno = ENOENT;
        return NULL;
    }

    for (index = caller_index + 1UL; index < expected_count; ++index) {
        void* candidate_handle;
        void* address;
        int candidate_error;
        unsigned int retained_slot = RIN_RTLD_NEXT_HANDLE_CAPACITY;

        fetch_result = __rin_dl_inventory_fetch(index, &record);
        if (fetch_result != 0) return NULL;
        if (record.record_count != expected_count ||
            record.launch_generation != expected_generation ||
            record.add_count != expected_adds ||
            record.sub_count != expected_subs) {
            errno = EIO;
            return NULL;
        }

        /* Re-evaluate a retained lease after fetching the current record so a
         * concurrent inventory mutation cannot select a handle for a
         * different image. */
        retained_slot = _rin_dl_retained_find(record.name);

        /* The inventory says this image is already mapped.  dlopen therefore
         * acquires only its process-local query handle; it does not bypass
         * signed admission or expand an untrusted search path.  Keep that
         * handle alive for the returned address: a dynamic tail may be
         * detached as soon as its last handle closes. */
        candidate_handle = retained_slot < RIN_RTLD_NEXT_HANDLE_CAPACITY
            ? _rin_rtld_next_handles[retained_slot].handle
            : dlopen(record.name, RTLD_NOW);
        if (!candidate_handle) {
            if (errno == ENOENT) continue;
            return NULL;
        }
        errno = 0;
        address = versioned
            ? dynlink_resolve_versioned(candidate_handle, symbol, version)
            : dynlink_resolve(candidate_handle, symbol);
        if (address) {
            if (retained_slot == RIN_RTLD_NEXT_HANDLE_CAPACITY) {
                retained_slot = _rin_dl_retained_add(candidate_handle,
                                                     record.name);
                if (retained_slot == RIN_RTLD_NEXT_HANDLE_CAPACITY) {
                    (void)dynlink_unload(candidate_handle);
                    errno = EAGAIN;
                    return NULL;
                }
            }
            return address;
        }
        candidate_error = errno != 0 ? errno : ENOENT;
        if (candidate_error != ENOENT) {
            errno = candidate_error;
            return NULL;
        }
        if (retained_slot == RIN_RTLD_NEXT_HANDLE_CAPACITY &&
            dynlink_unload(candidate_handle) != 0)
            return NULL;
    }
    errno = ENOENT;
    return NULL;
}

/* ═══════════════════════════════════════════════════════════════
 * POSIX dlfcn関数
 * ═══════════════════════════════════════════════════════════════*/

/*
 * dlopen - 動的ライブラリをロード
 *
 * @param filename  ライブラリのパス (NULLならメインプログラム)
 * @param flags     RTLD_LAZY, RTLD_NOW, RTLD_NOLOAD, RTLD_NODELETE,
 *                  RTLD_GLOBAL, RTLD_LOCAL
 * @return          ライブラリハンドル、失敗時はNULL
 */
static inline void* dlopen(const char* filename, int flags) {
    const int binding_flags = flags & (RTLD_LAZY | RTLD_NOW);
    const int known_flags = RTLD_LAZY | RTLD_NOW | RTLD_GLOBAL |
                            RTLD_NOLOAD | RTLD_NODELETE | RTLD_DEEPBIND;

    if ((flags & ~known_flags) != 0 || binding_flags == 0 ||
        binding_flags == (RTLD_LAZY | RTLD_NOW)) {
        errno = EINVAL;
        _dl_set_error("dlopen: invalid flags");
        return NULL;
    }
    /* RinOS's signed-library owner resolves the dependency graph while the
     * image is admitted; it does not expose a deferred relocation queue.
     * RTLD_LAZY is therefore a valid request with eager binding semantics,
     * which is permitted when the implementation elects to resolve symbols
     * before returning.  Scope-bearing requests use the native two-word
     * loader ABI so RTLD_GLOBAL is published to the process namespace and
     * RTLD_DEEPBIND is retained as an explicit policy bit. */

    /* 初期化 (初回のみ) */
    static int initialized = 0;
    if (!initialized) {
        dynlink_init();
        initialized = 1;
    }

    if (!filename) {
        /* A successful dlopen must never return the NULL failure sentinel. */
        return RIN_RTLD_MAIN;
    }

    if ((flags & RTLD_NOLOAD) != 0) {
        const int loaded = _rin_dl_inventory_contains(filename);
        if (loaded <= 0) {
            if (loaded == 0)
                _dl_set_error("dlopen: library is not loaded");
            else if (errno == ENOSYS)
                _dl_set_error("dlopen: loaded-library inventory unavailable");
            else
                _dl_set_error("dlopen: cannot inspect loaded libraries");
            return NULL;
        }
    }

    void* handle = (flags & (RTLD_GLOBAL | RTLD_DEEPBIND)) != 0
        ? dynlink_load_flags(filename,
                             (unsigned int)(flags &
                                             (RTLD_GLOBAL | RTLD_DEEPBIND)))
        : dynlink_load(filename);
    if (!handle) {
        _dl_set_error("dlopen: failed to load library");
        return NULL;
    }

    if ((flags & RTLD_NODELETE) != 0 &&
        _rin_dl_retained_find(filename) == RIN_RTLD_NEXT_HANDLE_CAPACITY) {
        /* The public handle and the permanent lease are separate kernel
         * object handles.  Closing the public one must not release the
         * NODELETE lease; acquire the second handle before returning. */
        void* retained_handle = dynlink_load(filename);
        unsigned int retained_slot;
        if (!retained_handle) {
            (void)dynlink_unload(handle);
            _dl_set_error("dlopen: cannot retain NODELETE library");
            return NULL;
        }
        retained_slot = _rin_dl_retained_add(retained_handle, filename);
        if (retained_slot == RIN_RTLD_NEXT_HANDLE_CAPACITY) {
            (void)dynlink_unload(retained_handle);
            (void)dynlink_unload(handle);
            errno = EAGAIN;
            _dl_set_error("dlopen: NODELETE lease table is full");
            return NULL;
        }
    }

    return handle;
}

/*
 * dlclose - 動的ライブラリをアンロード
 *
 * @param handle    dlopenで取得したハンドル
 * @return          成功時0、失敗時非0
 */
static inline int dlclose(void* handle) {
    if (handle == RIN_RTLD_MAIN) {
        return 0;
    }
    if (!handle || handle == RTLD_DEFAULT || handle == RTLD_NEXT) {
        errno = EINVAL;
        _dl_set_error("dlclose: invalid handle");
        return -1;
    }

    int ret = dynlink_unload(handle);
    if (ret != 0) {
        if (ret > 0) errno = EIO;
        _dl_set_error("dlclose: failed to unload library");
        return -1;
    }
    return 0;
}

/*
 * dlsym - シンボルのアドレスを取得
 *
 * @param handle    ライブラリハンドル (RTLD_DEFAULT, RTLD_NEXTも可)
 * @param symbol    シンボル名
 * @return          シンボルのアドレス、失敗時はNULL
 */
static inline void* dlsym(void* handle, const char* symbol) {
    if (!symbol) {
        errno = EINVAL;
        _dl_set_error("dlsym: NULL symbol name");
        return NULL;
    }

    void* addr = NULL;

    if (handle == RTLD_DEFAULT) {
        /* グローバル検索 */
        addr = dynlink_resolve_global(symbol);
    } else if (handle == RIN_RTLD_MAIN) {
        addr = dynlink_resolve_global(symbol);
    } else if (handle == RTLD_NEXT) {
        addr = _rin_dlsym_next(symbol, 0, NULL);
    } else {
        /* 特定ライブラリ内検索 */
        addr = dynlink_resolve(handle, symbol);
    }

    if (!addr) {
        _dl_set_error("dlsym: symbol not found");
    }

    return addr;
}

/*
 * dlerror - 最後のエラーメッセージを取得
 *
 * @return  エラーメッセージ、エラーがなければNULL
 */
static inline char* dlerror(void) {
    /* まずカーネルエラーをチェック */
    const char* kernel_err = dynlink_error();
    if (kernel_err && kernel_err[0]) {
        /* カーネルエラーをローカルバッファにコピー */
        int i = 0;
        while (kernel_err[i] && i < 255) {
            _dlerror_buf[i] = kernel_err[i];
            i++;
        }
        _dlerror_buf[i] = '\0';
        return _dlerror_buf;
    }

    /* ローカルエラーをチェック */
    if (_dlerror_flag) {
        _dlerror_flag = 0;
        return _dlerror_buf;
    }
    return NULL;
}

/* `Dl_info` returns pointers into these scratch buffers.  They must be
 * thread-local: a concurrent dladdr() call in another thread is allowed to
 * continue using the first result until its next call, and a process-global
 * buffer would otherwise make the first result change behind the caller. */
#if defined(__cplusplus)
#define RIN_DLFCN_THREAD_LOCAL thread_local
#else
#define RIN_DLFCN_THREAD_LOCAL _Thread_local
#endif
static RIN_DLFCN_THREAD_LOCAL char
    _rin_dladdr_name[RIN_DLINVENTORY_V1_NAME_CAPACITY];
static RIN_DLFCN_THREAD_LOCAL char
    _rin_dladdr_rll_name[RIN_DLADDR_RLL_V1_LIBRARY_NAME_CAPACITY];
static RIN_DLFCN_THREAD_LOCAL char
    _rin_dladdr_rll_symbol[RIN_DLADDR_RLL_V1_SYMBOL_NAME_CAPACITY];
#undef RIN_DLFCN_THREAD_LOCAL

static inline void _rin_dladdr_clear(Dl_info* info) {
    if (!info) return;
    info->dli_fname = NULL;
    info->dli_fbase = NULL;
    info->dli_sname = NULL;
    info->dli_saddr = NULL;
}

/* SYS_DLADDR carries both legacy RLL and native RIN v3 records.  Native
 * ENOSYS remains the signal to use the launch-inventory image-only fallback. */
static inline int _rin_dladdr_rll_result(const void* addr, Dl_info* info) {
    RinDlAddrRllRecordV1 record;
    intptr_t result;
    uint32_t index;
    uint64_t target;
    if (!addr || !info) return -1;
    result = _RIN_DYNLINK_SYSCALL2(
        SYS_DLADDR, (uintptr_t)addr, (uintptr_t)&record);
    if (result < 0) {
        const int error = result >= -4095 ? (int)-result : EIO;
        if (error == ENOSYS) return 0; /* native RIN64 fallback */
        errno = error;
        return -1;
    }
    if (result != 0) {
        errno = result > (intptr_t)INT_MAX ? EOVERFLOW : EIO;
        return -1;
    }
    target = (uint64_t)(uintptr_t)addr;
    if (record.abi_version != RIN_DLADDR_RLL_V1_ABI_VERSION ||
        (record.flags & ~(RIN_DLADDR_RLL_V1_FLAG_DATA |
                          RIN_DLADDR_RLL_V1_FLAG_NATIVE)) != 0u ||
        record.library_name_length == 0u ||
        record.library_name_length >=
            RIN_DLADDR_RLL_V1_LIBRARY_NAME_CAPACITY ||
        record.symbol_name_length >=
            RIN_DLADDR_RLL_V1_SYMBOL_NAME_CAPACITY ||
        record.library_name[record.library_name_length] != '\0' ||
        record.symbol_name[record.symbol_name_length] != '\0' ||
        record.load_address == 0u ||
        record.mapped_size == 0u ||
        record.load_address > UINT64_MAX - record.mapped_size ||
        target < record.load_address ||
        target - record.load_address >= record.mapped_size ||
        ((record.symbol_address == 0u) !=
         (record.symbol_name_length == 0u)) ||
        (record.symbol_name_length != 0u &&
         (record.symbol_address < record.load_address ||
          record.symbol_address > target))) {
        errno = EIO;
        return -1;
    }
    for (index = 0u; index < record.library_name_length; ++index) {
        if (record.library_name[index] == '\0') {
            errno = EIO;
            return -1;
        }
    }
    for (index = 0u; index < record.symbol_name_length; ++index) {
        if (record.symbol_name[index] == '\0') {
            errno = EIO;
            return -1;
        }
    }
    for (uint32_t index = 0u;
         index <= record.library_name_length; ++index)
        _rin_dladdr_rll_name[index] = record.library_name[index];
    for (uint32_t index = 0u;
         index <= record.symbol_name_length; ++index)
        _rin_dladdr_rll_symbol[index] = record.symbol_name[index];
    info->dli_fname = _rin_dladdr_rll_name;
    info->dli_fbase = (void*)(uintptr_t)record.load_address;
    info->dli_sname = record.symbol_name_length != 0u
        ? _rin_dladdr_rll_symbol : NULL;
    info->dli_saddr = record.symbol_name_length != 0u
        ? (void*)(uintptr_t)record.symbol_address : NULL;
    return 1;
}

/* dladdr v1 resolves a launch-verified image.  Native RIN v3 records may
 * additionally carry the nearest-lower verified export from either the main
 * image or an authenticated library; the inventory fallback remains
 * image-only when the native carrier reports ENOSYS. */
static inline int dladdr(const void* addr, Dl_info* info) {
    RinDlInventoryRecordV1 record;
    char matched_name[RIN_DLINVENTORY_V1_NAME_CAPACITY];
    unsigned long index;
    unsigned long expected_count = 0UL;
    unsigned long long expected_generation = 0ULL;
    unsigned long long expected_adds = 0ULL;
    unsigned long long expected_subs = 0ULL;
    uint64_t address;
    uint64_t matched_base = 0u;
    unsigned int matched_name_length = 0u;
    int matched = 0;
    if (!addr || !info) {
        _rin_dladdr_clear(info);
        errno = EINVAL;
        return 0;
    }
    _rin_dladdr_clear(info);
    {
        const int legacy_result = _rin_dladdr_rll_result(addr, info);
        if (legacy_result < 0) {
            _rin_dladdr_clear(info);
            return 0;
        }
        if (legacy_result > 0) return 1;
    }
    address = (uint64_t)(uintptr_t)addr;
    for (index = 0UL; ; ++index) {
        unsigned int name_index;
        if (__rin_dl_inventory_fetch(index, &record) != 0) return 0;
        if (index == 0UL) {
            expected_count = record.record_count;
            expected_generation = record.launch_generation;
            expected_adds = record.add_count;
            expected_subs = record.sub_count;
        } else if (record.record_count != expected_count ||
                   record.launch_generation != expected_generation ||
                   record.add_count != expected_adds ||
                   record.sub_count != expected_subs) {
            errno = EIO;
            return 0;
        }
        if (index >= expected_count) {
            errno = EIO;
            return 0;
        }
        if (record.load_address <= address &&
            address - record.load_address < record.mapped_size) {
            if (matched) {
                errno = EIO;
                return 0;
            }
            matched = 1;
            matched_base = record.load_address;
            matched_name_length = record.name_length;
            for (name_index = 0u; name_index <= matched_name_length;
                 ++name_index)
                matched_name[name_index] = record.name[name_index];
        }
        if (index + 1UL == expected_count) break;
    }
    if (!matched) {
        errno = ENOENT;
        return 0;
    }
    for (unsigned int name_index = 0u; name_index <= matched_name_length;
         ++name_index)
        _rin_dladdr_name[name_index] = matched_name[name_index];
    info->dli_fname = _rin_dladdr_name;
    info->dli_fbase = (void*)(uintptr_t)matched_base;
    return 1;
}

/*
 * dlvsym - バージョン付きシンボル検索 (GNU拡張)
 *
 * @param handle    ライブラリハンドル
 * @param symbol    シンボル名
 * @param version   バージョン文字列
 * @return          シンボルのアドレス
 */
static inline void* dlvsym(void* handle, const char* symbol, const char* version) {
    if (!symbol || !version) {
        errno = EINVAL;
        _dl_set_error("dlvsym: NULL symbol or version");
        return NULL;
    }
    if (handle == RTLD_NEXT) {
        void* address = _rin_dlsym_next(symbol, 1, version);
        if (!address) {
            if (errno == ENOSYS)
                _dl_set_error("dlvsym: RTLD_NEXT inventory unavailable");
            else if (errno == ENOENT)
                _dl_set_error("dlvsym: symbol/version not found");
            else
                _dl_set_error("dlvsym: RTLD_NEXT lookup failed");
        }
        return address;
    }
    /* Both RTLD_DEFAULT and the successful main-image handle use the global
     * namespace in the kernel ABI.  A missing export is distinguishable from
     * an unavailable versioned syscall so callers do not observe a false
     * success for an unversioned symbol. */
    if (handle == RTLD_DEFAULT || handle == RIN_RTLD_MAIN) handle = NULL;
    errno = 0;
    void* address = dynlink_resolve_versioned(handle, symbol, version);
    if (!address) {
        if (errno == 0) errno = ENOENT;
        if (errno == ENOSYS)
            _dl_set_error("dlvsym: symbol versioning is not implemented");
        else
            _dl_set_error("dlvsym: symbol/version not found");
    }
    return address;
}

/* ═══════════════════════════════════════════════════════════════
 * RinOS拡張関数
 * ═══════════════════════════════════════════════════════════════*/

/*
 * dl_add_search_path - ライブラリ検索パスを追加
 *
 * @param path      検索パス
 * @return          成功時0、失敗時-1
 */
static inline int dl_add_search_path(const char* path) {
    return dynlink_add_search_path(path);
}

#ifdef __cplusplus
}
#endif

#undef RIN_DYNLINK_CUSTOM_SYSCALL2

#endif /* _DLFCN_H */
