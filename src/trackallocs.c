
#if A2L_TRACK_ALLOCS

#define MAX_ALLOC_RECORDS_SOFT 4096  // hashindex soft limit

//
// Storage Records
//
typedef struct {
    void *heap_ptr;
    size_t bytes;
    uint32_t stack_hash_id;
}a2l_allocrecord_t;

static ftgc_hashindex_s a2l_allocrecord_hindex;
static a2l_allocrecord_t a2l_allocrecord_records[MAX_ALLOC_RECORDS_SOFT]; // todo: make this heap-based, realloc scheme

static void a2l_track_allocs_init(void) {
    ftgc_hashindex_init(&a2l_allocrecord_hindex, MAX_ALLOC_RECORDS_SOFT);
}

// todo:
// implement ptr to a2l_alloc_record dictionary

static void
a2l_track_alloc(void *ptr) {
    return;
    if (!ptr)
        return;

    // generate a key based on the ptr.  the main reason
    // this is done instead of using the hashindex key as the hash_id
    // for the end-user log is because hashindex collisions have
    // an in-program resolution, but hash_ids need collisions to be
    // improbable.
    ftgc_hashkey_t key;

    key = ftgc_hashindex_generate_key_ptr(&a2l_allocrecord_hindex, ptr);

    int record_index = 999; // todo: implement data structure

    ftgc_hashindex_add_key(&a2l_allocrecord_hindex, key, record_index);
}

#endif
