#include "cfifo.h"

#include "zmalloc.h"

static uint32_t __ckfifo_len(const struct ckfifo *ckfifo_ring_buffer)
{
    return (ckfifo_ring_buffer->in - ckfifo_ring_buffer->out);
}

static uint32_t __ckfifo_get(struct ckfifo *ckfifo_ring_buffer, unsigned char *buffer, uint32_t size)
{
    uint32_t len, n;

    n  = min(size, ckfifo_ring_buffer->in - ckfifo_ring_buffer->out);
    len = min(n, ckfifo_ring_buffer->size - (ckfifo_ring_buffer->out & (ckfifo_ring_buffer->size - 1)));
    memcpy(buffer, ckfifo_ring_buffer->buffer + (ckfifo_ring_buffer->out & (ckfifo_ring_buffer->size - 1)), len);
    memcpy(buffer + len, ckfifo_ring_buffer->buffer, n - len);
    ckfifo_ring_buffer->out += n;
    return n;
}

static uint32_t __ckfifo_put(struct ckfifo *ckfifo_ring_buffer, const unsigned char *buffer, uint32_t size)
{
    uint32_t len, n;

    n = min(size, ckfifo_ring_buffer->size - ckfifo_ring_buffer->in + ckfifo_ring_buffer->out);
    len  = min(n, ckfifo_ring_buffer->size - (ckfifo_ring_buffer->in & (ckfifo_ring_buffer->size - 1)));
    memcpy(ckfifo_ring_buffer->buffer + (ckfifo_ring_buffer->in & (ckfifo_ring_buffer->size - 1)), buffer, len);
    memcpy(ckfifo_ring_buffer->buffer, buffer + len, n - len);
    ckfifo_ring_buffer->in += n;
    return n;
}

PORTABLEIMPL(struct ckfifo*) ckfifo_init(void *buffer, uint32_t size)
{
    struct ckfifo *ckfifo_ring_buffer;

    if (!is_powerof_2(size) || unlikely(!buffer)) {
        return NULL;
    }

    ckfifo_ring_buffer = (struct ckfifo *)ztrymalloc(sizeof(struct ckfifo));
    if ( unlikely(!ckfifo_ring_buffer) ) {
        return NULL;
    }

    memset(ckfifo_ring_buffer, 0, sizeof(struct ckfifo));
    ckfifo_ring_buffer->buffer = buffer;
    ckfifo_ring_buffer->size = size;
    ckfifo_ring_buffer->in = 0;
    ckfifo_ring_buffer->out = 0;
    lwp_mutex_init(&ckfifo_ring_buffer->mutex, YES);
    return ckfifo_ring_buffer;
}

PORTABLEIMPL(void) ckfifo_uninit(struct ckfifo *ckfifo_ring_buffer)
{
    if (unlikely(!ckfifo_ring_buffer)) {
        return;
    }

    zfree(ckfifo_ring_buffer);
}

PORTABLEIMPL(uint32_t) ckfifo_len(struct ckfifo *ckfifo_ring_buffer)
{
    uint32_t len;

    if (unlikely(!ckfifo_ring_buffer)) {
        return 0;
    }

    lwp_mutex_lock(&ckfifo_ring_buffer->mutex);
    len = __ckfifo_len(ckfifo_ring_buffer);
    lwp_mutex_unlock(&ckfifo_ring_buffer->mutex);
    return len;
}

PORTABLEIMPL(uint32_t) ckfifo_get(struct ckfifo *ckfifo_ring_buffer, void *buffer, uint32_t size)
{
    uint32_t n;

    if (unlikely(!ckfifo_ring_buffer || !buffer)) {
        return 0;
    }

    lwp_mutex_lock(&ckfifo_ring_buffer->mutex);
    n = __ckfifo_get(ckfifo_ring_buffer, buffer, size);
    if (ckfifo_ring_buffer->in == ckfifo_ring_buffer->out) {
        ckfifo_ring_buffer->in = ckfifo_ring_buffer->out = 0;
    }
    lwp_mutex_unlock(&ckfifo_ring_buffer->mutex);
    return n;
}

PORTABLEIMPL(uint32_t) ckfifo_put(struct ckfifo *ckfifo_ring_buffer, const void *buffer, uint32_t size)
{
    uint32_t n;

    if (unlikely(!ckfifo_ring_buffer || !buffer)) {
        return 0;
    }

    lwp_mutex_lock(&ckfifo_ring_buffer->mutex);
    n = __ckfifo_put(ckfifo_ring_buffer, buffer, size);
    lwp_mutex_unlock(&ckfifo_ring_buffer->mutex);
    return n;
}
