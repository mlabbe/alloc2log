#define _GNU_SOURCE

#include <dlfcn.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <execinfo.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <inttypes.h>
#include <pthread.h>

#define FTG_IMPLEMENT_CORE
#include "3rdparty/ftg_core.h"

#define FTG_IMPLEMENT_CONTAINERS
#include "3rdparty/ftg_containers.h"

#define A2L_TRACK_ALLOCS 1

#define MAX_FRAMES 32

// unity build -- dlsym calls malloc if .so is not compiled and linked in one stage
#include "trackallocs.c"


typedef struct{
    void *(*malloc)(size_t size);
    void *(*mmap)(void *addr, size_t length, int prot, int flags,
                  int fd, off_t offset);
}a2l_real_t;

// ptr addresses into a string with these attributes
typedef struct{
    char *bin, *bin_end;
    char *func, *func_end;
    char *offset, *offset_end;
    char *addr, *addr_end;
}a2l_parsedframe_t;



//
//
//

a2l_real_t a2l_real;

#define TAB "  "
#define TAB2 TAB TAB
#define TAB3 TAB TAB TAB
#define TAB4 TAB TAB TAB TAB

static int a2l__initialized = 0;
static int a2l__fd = -1;
#define A2L_ENSURE_INITIALIZED \
    if (!a2l__initialized) { a2l_initialize(); }

// toggle malloc logging.
// fixme: create a mutex for this
static int a2l__malloc_logging = 1;
static void a2l__enable_malloc_logging(void) {
    a2l__malloc_logging = 1;
}

static void a2l__disable_malloc_logging(void) {
    a2l__malloc_logging = 0;
}

void a2l_logstr(const char *msg) {
    write(a2l__fd, msg, strlen(msg));
}

static void
a2l_initialize(void) {
    write(1, "a\n", 2);

    char *logfile = getenv("A2L_LOGSTRFILE");
    char default_logfile[256];

    if (logfile == NULL) {
        sprintf(default_logfile, "a2l-%d.log", getpid());
        logfile = default_logfile;
    }

    write(1, "2\n", 2);

#define A2L_MAPSYM(x) \
    a2l_real.x = dlsym(RTLD_NEXT, #x);

    A2L_MAPSYM(malloc);
    A2L_MAPSYM(mmap);

#undef A2L_MAPSYM
    write(1, "t\n", 2);
#if A2L_TRACK_ALLOCS
    a2l__disable_malloc_logging();
    a2l_track_allocs_init();
    a2l__enable_malloc_logging();
#endif

    write(1, "s\n", 2);

    a2l__initialized = 1;
    a2l__fd = open(default_logfile, O_CREAT|O_WRONLY, S_IRUSR|S_IWUSR);

        write(1, "4\n", 2);
}

static void
a2l_log_frames(const char *calling_func, ssize_t alloc_bytes) {
    void *bt_buf[MAX_FRAMES];

    a2l__disable_malloc_logging();
    int trace_frames = backtrace(bt_buf, MAX_FRAMES);
    char **trace_frames_desc = backtrace_symbols(bt_buf, trace_frames);
    a2l__enable_malloc_logging();

    char buf[2048];
    char *p_end = &buf[2047];
    char *p_buf = &buf[0];

    // since our .so can't use buffered io, this function uses
    // stack space to buffer the structured log.
#define A2L_SPRINTF(MSG, ...)                       \
    if (!(p_buf + strlen(MSG) >= p_end))            \
        p_buf += sprintf(p_buf, MSG, ##__VA_ARGS__);

#define A2L_SPRINTF_STACKFRAME(COMPONENT, COMMA)      \
    A2L_SPRINTF(TAB2 #COMPONENT ": '%.*s'%c", \
                (int)(sf.COMPONENT##_end - sf.COMPONENT), sf.COMPONENT, COMMA)

    // derive hash from trace frames
    void *hash_beg = &trace_frames_desc[0];
    void *hash_end = &trace_frames_desc[trace_frames-1];
    uint32_t hash_id = ftg_hash_fast(hash_beg, hash_end - hash_beg);

    pthread_t thread_id = pthread_self();

    A2L_SPRINTF(TAB  "{\n" TAB2 "call: '%s',\n", calling_func);
    A2L_SPRINTF(TAB2 "bytes: %" FTG_SPEC_SSIZE_T ",\n", alloc_bytes);
    A2L_SPRINTF(TAB2 "hash_id: %"PRIu32",\n", hash_id);
    A2L_SPRINTF(TAB2 "thread_id: %" FTG_SPEC_SIZE_T ",\n", thread_id);
    A2L_SPRINTF(TAB2 "stack: [\n");

    for (int i = 2; i < trace_frames; i++) {
        // format:
        ///home/mlabbe/dev/alloc2log/bin/linux/alloc2log.so(malloc+0x4d) [0x7f8eef1c8ba8]

        a2l_parsedframe_t sf;
        char *p = trace_frames_desc[i];
        sf.bin = p;
        while (*p != '(') p++;
        sf.bin_end = p;

        sf.func = p+1;
        while (*p != '+') p++;
        sf.func_end = p;

        sf.offset = p+1;
        while (*p != ')') p++;
        sf.offset_end = p;

        while (*p != '[') p++;
        sf.addr = p+1;
        while (*p != ']') p++;
        sf.addr_end = p;

        A2L_SPRINTF(TAB3 "{");
        A2L_SPRINTF_STACKFRAME(func, ',');
        A2L_SPRINTF_STACKFRAME(bin, ',');
        A2L_SPRINTF_STACKFRAME(addr, ',');
        A2L_SPRINTF_STACKFRAME(offset, ' ');
        A2L_SPRINTF(TAB3 "}%c\n", i==trace_frames-1?' ':',');
    }

    A2L_SPRINTF(TAB2 "],\n");
    A2L_SPRINTF(TAB  "},\n");

    a2l_logstr(buf);

    a2l__disable_malloc_logging();
    free(trace_frames_desc);
    a2l__enable_malloc_logging();
}


//
// Wrapper Functions
//

void *
malloc(size_t size) {
    write(1, "b\n", 2);


    write(1, "B\n", 2);
    if (!a2l__malloc_logging) {
        return a2l_real.malloc(size);
    }

    A2L_ENSURE_INITIALIZED;

    a2l_log_frames("malloc", size);

    void *ptr = a2l_real.malloc(size);

#if A2L_TRACK_ALLOCS
    a2l_track_alloc(ptr);
#endif

    return ptr;
}

void *
mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset) {
    write(1, "c\n", 2);

    A2L_ENSURE_INITIALIZED;

    a2l_log_frames("mmap", (ssize_t)length);

    return a2l_real.mmap(addr, length, prot, flags, fd, offset);
}
