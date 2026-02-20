#ifndef _SYS_LIBTTAK_TTAK_UMA_H_
#define _SYS_LIBTTAK_TTAK_UMA_H_

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <vm/uma.h>

#include <ttak/mem/arena_helper.h>
#include <ttak/mem/epoch_gc.h>

/*
 * @brief Egor-specific libttak arena wrapper for kernel UMA integration.
 * This structure manages a libttak arena environment for a specific purpose
 * within the kernel.
 */
typedef struct egor_ttak_arena {
    ttak_arena_env_t        env;            /**< libttak arena environment */
    ttak_arena_generation_t current_generation; /**< Currently active generation */
    struct mtx              lock;           /**< Lock for arena operations */
    bool                    initialized;    /**< True if arena is initialized */
    LIST_ENTRY(egor_ttak_arena) list_entry; /**< Entry for global list of arenas */
} egor_ttak_arena_t;

/*
 * @brief Egor-specific UMA zone wrapper with optional libttak arena backing.
 * This allows a UMA zone to be optionally backed by a libttak arena for
 * allocations, enabling scope-based reclamation.
 */
typedef struct egor_ttak_zone {
    uma_zone_t              uma_zone;       /**< Original UMA zone (fallback or underlying) */
    egor_ttak_arena_t       ttak_arena;     /**< libttak arena (if enabled) */
    bool                    use_ttak_arena; /**< Flag to enable/disable libttak arena backing */
    struct mtx              zone_lock;      /**< Lock for egor_ttak_zone operations */

    /* Statistics and tunables */
    atomic_long             alloc_count;    /**< Number of allocations via ttak arena */
    atomic_long             free_count;     /**< Number of frees via ttak arena (often no-op) */
    atomic_long             reset_count;    /**< Number of arena resets */
    atomic_long             reclaimed_bytes;/**< Bytes reclaimed by arena resets */
} egor_ttak_zone_t;

/* Function prototypes for egor_ttak_zone */
void egor_ttak_zone_init(egor_ttak_zone_t *egor_zone, const char *name, size_t size,
                         uma_ctor ctor, uma_dtor dtor, uma_init init, uma_fini fini,
                         uma_align align, uint16_t flags, bool use_ttak_arena);
void egor_ttak_zone_destroy(egor_ttak_zone_t *egor_zone);
void *egor_ttak_zone_alloc(egor_ttak_zone_t *egor_zone, int m_flags);
void egor_ttak_zone_free(egor_ttak_zone_t *egor_zone, void *item);
void egor_ttak_zone_reset(egor_ttak_zone_t *egor_zone);

#endif /* _SYS_LIBTTAK_TTAK_UMA_H_ */
