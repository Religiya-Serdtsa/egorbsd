#include <sys/libttak/ttak_uma.h>
#include <sys/sysctl.h>
#include <sys/kdb.h>

/* Global list of active egor_ttak_arena_t instances */
static LIST_HEAD(, egor_ttak_arena) egor_ttak_arena_list = LIST_HEAD_INITIALIZER(egor_ttak_arena_list);
static struct mtx egor_ttak_arena_list_mtx;

static bool egor_ttak_global_enable = false; // Default to disabled
SYSCTL_DECL(_hw_libttak);
SYSCTL_NODE(_hw, OID_AUTO, libttak, CTLFLAG_RW | CTLFLAG_MPSAFE, 0, "libttak kernel integration");
SYSCTL_BOOL(_hw_libttak, OID_AUTO, enable, CTLFLAG_RWTUN, &egor_ttak_global_enable, 0,
    "Enable/Disable libttak integration for UMA zones");

/*
 * Initialize libttak's global epoch manager. This should ideally be called once
 * during kernel boot.
 */
static void
egor_ttak_init_globals(void *dummy)
{
    (void)dummy; // Suppress unused parameter warning
    
    /* Initialize libttak's global epoch manager if it's not already */
    /* This might be redundant if libttak itself has a global init, but good to ensure */
    /* ttak_epoch_manager_t is declared as extern in libttak. */
    /* For now, assume libttak handles its own global init or we don't need to touch it directly here. */

    mtx_init(&egor_ttak_arena_list_mtx, "egor_ttak_arena_list_mtx", NULL, MTX_DEF);
}
SYSINIT(egor_ttak_globals, SI_SUB_TUNABLES, SI_ORDER_FIRST, egor_ttak_init_globals, NULL);


/*
 * @brief Initialize an egor_ttak_arena_t instance.
 */
static void
egor_ttak_arena_init(egor_ttak_arena_t *arena)
{
    // Initialize libttak arena environment
    ttak_arena_env_config_t config;
    ttak_arena_env_config_init(&config);
    config.generation_bytes = PAGE_SIZE * 4; // Example: 16KB per generation
    config.chunk_bytes = 0; // Will be set by ttak_arena_generation_claim if not set.
                            // For UMA, each allocation is an item.

    if (!ttak_arena_env_init(&arena->env, &config)) {
        printf("WARNING: egor_ttak_arena_init failed to initialize ttak_arena_env\n");
        arena->initialized = false;
        return;
    }

    // Begin the first generation
    // Use an arbitrary epoch_id for now, in a real scenario this might be coordinated
    // ttak_arena_generation_begin allocates and registers a new generation buffer.
    if (!ttak_arena_generation_begin(&arena->env, &arena->current_generation, 0)) {
        printf("WARNING: egor_ttak_arena_init failed to begin first arena generation\n");
        ttak_arena_env_destroy(&arena->env); // Clean up the environment
        arena->initialized = false;
        return;
    }

    mtx_init(&arena->lock, "egor_ttak_arena_mtx", NULL, MTX_DEF);
    arena->initialized = true;

    mtx_lock(&egor_ttak_arena_list_mtx);
    LIST_INSERT_HEAD(&egor_ttak_arena_list, arena, list_entry);
    mtx_unlock(&egor_ttak_arena_list_mtx);
}

/*
 * @brief Destroy an egor_ttak_arena_t instance.
 */
static void
egor_ttak_arena_destroy(egor_ttak_arena_t *arena)
{
    if (!arena->initialized)
        return;

    mtx_lock(&egor_ttak_arena_list_mtx);
    LIST_REMOVE(arena, list_entry);
    mtx_unlock(&egor_ttak_arena_list_mtx);

    // Retire the current generation if it's active
    if (arena->current_generation.base != NULL) { // Check if a generation was begun
        ttak_arena_generation_retire(&arena->env, &arena->current_generation);
    }
    ttak_arena_env_destroy(&arena->env);
    mtx_destroy(&arena->lock);
    arena->initialized = false;
}

/*
 * @brief Initialize an egor_ttak_zone_t.
 */
void
egor_ttak_zone_init(egor_ttak_zone_t *egor_zone, const char *name, size_t size,
                    uma_ctor ctor, uma_dtor dtor, uma_init init, uma_fini fini,
                    uma_align align, uint16_t flags, bool use_ttak_arena_flag)
{
    egor_zone->use_ttak_arena = use_ttak_arena_flag && egor_ttak_global_enable;
    
    if (egor_zone->use_ttak_arena) {
        egor_ttak_arena_init(&egor_zone->ttak_arena);
        if (!egor_zone->ttak_arena.initialized) {
            printf("WARNING: libttak arena initialization failed for zone %s, falling back to UMA.\n", name);
            egor_zone->use_ttak_arena = false; // Fallback
        } else {
             printf("INFO: libttak arena enabled for zone %s\n", name);
        }
    }

    // Create the underlying UMA zone, always
    // This will be used as fallback if libttak arena fails or is disabled
    // Or if libttak arena is enabled, this is still the source for the arena itself
    egor_zone->uma_zone = uma_zcreate(name, size, ctor, dtor, init, fini, align, flags);
    if (egor_zone->uma_zone == NULL) {
        // Handle critical error: UMA zone creation failed
        panic("Failed to create UMA zone for %s\n", name);
    }

    mtx_init(&egor_zone->zone_lock, name, NULL, MTX_DEF);

    atomic_store(&egor_zone->alloc_count, 0);
    atomic_store(&egor_zone->free_count, 0);
    atomic_store(&egor_zone->reset_count, 0);
    atomic_store(&egor_zone->reclaimed_bytes, 0);
}

/*
 * @brief Destroy an egor_ttak_zone_t.
 */
void
egor_ttak_zone_destroy(egor_ttak_zone_t *egor_zone)
{
    if (egor_zone->use_ttak_arena) {
        egor_ttak_arena_destroy(&egor_zone->ttak_arena);
    }
    if (egor_zone->uma_zone != NULL) {
        uma_zdestroy(egor_zone->uma_zone);
    }
    mtx_destroy(&egor_zone->zone_lock);
}

/*
 * @brief Allocate from an egor_ttak_zone_t.
 */
void *
egor_ttak_zone_alloc(egor_ttak_zone_t *egor_zone, int m_flags)
{
    void *item = NULL;
    size_t size = egor_zone->uma_zone->uz_size; // Get item size from underlying UMA zone

    mtx_lock(&egor_zone->zone_lock);
    if (egor_zone->use_ttak_arena && egor_zone->ttak_arena.initialized) {
        // Use libttak arena to allocate
        // For simplicity, we'll use a fixed 'lifetime_ticks' and 'now' for now.
        // In a real scenario, these would be dynamic or configured per zone/object.
        // For kernel context, ttak_mem_alloc might be called with KERN_NOW or similar.
        // For now, epoch_id 0 and lifetime 0 (or infinite).
        item = ttak_arena_generation_claim(&egor_zone->ttak_arena.env, 
                                           &egor_zone->ttak_arena.current_generation, 
                                           size);
        if (item) {
            atomic_fetch_add(&egor_zone->alloc_count, 1);
        } else {
            // Fallback to UMA if libttak allocation fails
            printf("WARNING: libttak arena allocation failed for zone %s, falling back to UMA.\n", egor_zone->uma_zone->uz_name);
            item = uma_zalloc(egor_zone->uma_zone, m_flags);
        }
    } else {
        item = uma_zalloc(egor_zone->uma_zone, m_flags);
    }
    mtx_unlock(&egor_zone->zone_lock);

    return item;
}

/*
 * @brief Free to an egor_ttak_zone_t.
 */
void
egor_ttak_zone_free(egor_ttak_zone_t *egor_zone, void *item)
{
    mtx_lock(&egor_zone->zone_lock);
    if (egor_zone->use_ttak_arena && egor_zone->ttak_arena.initialized) {
        // For arena-backed allocations, freeing is a no-op as cleanup
        // happens during arena reset.
        atomic_fetch_add(&egor_zone->free_count, 1);
        // No actual free operation here, it will be done by arena reset
    } else {
        uma_zfree(egor_zone->uma_zone, item);
    }
    mtx_unlock(&egor_zone->zone_lock);
}

/*
 * @brief Reset an egor_ttak_zone_t's arena, reclaiming all allocated memory.
 */
void
egor_ttak_zone_reset(egor_ttak_zone_t *egor_zone)
{
    mtx_lock(&egor_zone->zone_lock);
    if (egor_zone->use_ttak_arena && egor_zone->ttak_arena.initialized) {
        // This is where bulk reclamation would happen by retiring the current generation
        size_t reclaimed = egor_zone->ttak_arena.current_generation.used; // Assuming this tracks current usage
        if (ttak_arena_generation_retire(&egor_zone->ttak_arena.env, &egor_zone->ttak_arena.current_generation)) {
            // If successfully retired, start a new generation for future allocations
            // Use the same epoch_id for now, or derive a new one if needed
            if (!ttak_arena_generation_begin(&egor_zone->ttak_arena.env, &egor_zone->ttak_arena.current_generation, 0)) {
                printf("WARNING: egor_ttak_zone_reset failed to begin new arena generation after retire.\n");
                // What to do here? Fallback to UMA for subsequent allocations?
                // For now, the arena will remain in a non-functional state until re-init or disable.
                egor_zone->ttak_arena.initialized = false;
            } else {
                atomic_fetch_add(&egor_zone->reset_count, 1);
                atomic_fetch_add(&egor_zone->reclaimed_bytes, reclaimed);
                printf("DEBUG: Resetting egor_ttak_zone_t arena for %s, reclaimed %zu bytes.\n", egor_zone->uma_zone->uz_name, reclaimed);
            }
        } else {
            printf("WARNING: egor_ttak_zone_reset failed to retire current arena generation for %s.\n", egor_zone->uma_zone->uz_name);
        }
    }
    mtx_unlock(&egor_zone->zone_lock);
}
