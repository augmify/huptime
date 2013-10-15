/*
 * fdinfo.h
 */

#ifndef _FDINFO_H_
#define _FDINFO_H_

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>

typedef enum
{
    /* BOUND FDs are the sockets that have been
     * bound. These are the thing of most interest,
     * since we will ensure these are not closed and
     * are transparently passed between copies of the
     * application. */
    BOUND = 1, 

    /* TRACKED FDs are descriptors that have been
     * returned from BOUND FDs. Essentially we must
     * wait until all TRACKED FDs have been closed in
     * the application before we can cleanly exit. */
    TRACKED = 2,

    /* SAVED FDs are descriptors that we have saved
     * from startup. Because the program may go through
     * and close some of the file descriptors it had open
     * at start-up, we stuff them somewhere so that we can
     * recreate the environment as accurately as possible.
     * This may lead to some problems with open terminal
     * FDs, etc. but we'll see what happens. */
    SAVED = 3,

    /* INITIAL FDs are simply the initial open file
     * descriptors that the process has. These are closed
     * on a fork-based restart if they have not been closed
     * already. This is because they will belong to the
     * new process after this point. */
    INITIAL = 4,
} fdtype_t;

struct fdinfo;
typedef struct fdinfo fdinfo_t;

typedef
struct boundinfo
{
    int stub_listened :1;
    int real_listened :1;
    int is_ghost :1;
    struct sockaddr addr;
    socklen_t addrlen;
} __attribute__((packed)) boundinfo_t;

typedef
struct trackedinfo
{
    fdinfo_t *bound;
} trackedinfo_t;

typedef
struct savedinfo
{
    int fd;
} savedinfo_t;

typedef
struct initialinfo
{
} initialinfo_t;

struct fdinfo
{
    fdtype_t type;
    int refs;
    union
    {
        boundinfo_t bound;
        trackedinfo_t tracked;
        savedinfo_t saved;
        initialinfo_t initial;
    };
};

/* Statistics. */
extern int total_bound;
extern int total_tracked;
extern int total_saved;
extern int total_initial;

static inline fdinfo_t*
alloc_info(fdtype_t type)
{
    fdinfo_t *info = (fdinfo_t*)malloc(sizeof(fdinfo_t));
    memset(info, 0, sizeof(fdinfo_t));
    info->type = type;
    info->refs = 1;
    switch( type )
    {
        case BOUND:
            __sync_fetch_and_add(&total_bound, 1);
            break;
        case TRACKED:
            __sync_fetch_and_add(&total_tracked, 1);
            break;
        case SAVED:
            __sync_fetch_and_add(&total_saved, 1);
            break;
        case INITIAL:
            __sync_fetch_and_add(&total_initial, 1);
            break;
    }
    return info;
}

static void dec_ref(fdinfo_t* info);
static inline void
free_info(fdinfo_t* info)
{
    switch( info->type )
    {
        case BOUND:
            __sync_fetch_and_add(&total_bound, -1);
            break;
        case TRACKED:
            if( info->tracked.bound != NULL )
            {
                dec_ref(info->tracked.bound);
            }
            __sync_fetch_and_add(&total_tracked, -1);
            break;
        case SAVED:
            __sync_fetch_and_add(&total_saved, -1);
            break;
        case INITIAL:
            __sync_fetch_and_add(&total_initial, -1);
            break;
    }
    free(info);
}

static inline void
inc_ref(fdinfo_t* info)
{
    __sync_fetch_and_add(&info->refs, 1);
}

static inline void
dec_ref(fdinfo_t* info)
{
    if( __sync_fetch_and_add(&info->refs, -1) == 1 )
    {
        free_info(info);
    }
}

int info_decode(int pipe, int *fd, fdinfo_t **info);
int info_encode(int pipe, int fd, fdinfo_t *info);

#endif
