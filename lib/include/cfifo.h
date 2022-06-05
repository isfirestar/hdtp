#if !defined LIB_CKFIFO_H
#define LIB_CKFIFO_H

#include "compiler.h"
#include "threading.h"

struct ckfifo {
    unsigned char      *buffer;
    uint32_t     	    size;
    uint32_t     	    in;
    uint32_t       	    out;
    lwp_mutex_t         mutex;   /* relate to memory copy, it's not adapt to spinlock */
};

PORTABLEAPI(struct ckfifo*) ckfifo_init(void *buffer, uint32_t size);
PORTABLEAPI(void) ckfifo_uninit(struct ckfifo *ckfifo_ring_buffer);
PORTABLEAPI(uint32_t) ckfifo_len(struct ckfifo *ckfifo_ring_buffer);
PORTABLEAPI(uint32_t) ckfifo_get(struct ckfifo *ckfifo_ring_buffer, void *buffer, uint32_t size);
PORTABLEAPI(uint32_t) ckfifo_put(struct ckfifo *ckfifo_ring_buffer, const void *buffer, uint32_t size);

#endif
