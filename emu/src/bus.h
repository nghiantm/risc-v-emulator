#ifndef BUS_H
#define BUS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct Device Device;
struct Device {
    const char *name;
    uint32_t    base;                 /* absolute bus address */
    uint32_t    size;                 /* bytes */
    /* offset is relative to base. size_bytes is 1, 2 or 4. Return false => access fault. */
    bool (*read )(Device *d, uint32_t offset, unsigned size_bytes, uint32_t *out);
    bool (*write)(Device *d, uint32_t offset, unsigned size_bytes, uint32_t value);
    void (*tick )(Device *d);         /* called once per machine loop iteration; may be NULL */
    void *state;                      /* device-private */
};

#define BUS_MAX_DEVICES 8

typedef struct {
    Device *devices[BUS_MAX_DEVICES];
    size_t  count;
    bool     tohost_enabled;          /* set by ELF loader if symbol `tohost` found */
    uint32_t tohost_addr;
    bool     tohost_written;          /* latched on first write */
    uint32_t tohost_value;
} Bus;

void bus_init(Bus *b);
bool bus_add(Bus *b, Device *d);      /* false if full */

/* Return false => access fault (unmapped, device refused, or straddles two devices). */
bool bus_read (Bus *b, uint32_t addr, unsigned size_bytes, uint32_t *out);
bool bus_write(Bus *b, uint32_t addr, unsigned size_bytes, uint32_t value);

void bus_tick(Bus *b);                /* tick every device */

/* MMIO devices call this: only RAM accepts misaligned access. */
static inline bool access_aligned(uint32_t offset, unsigned size_bytes)
{
    return (offset & (size_bytes - 1u)) == 0;
}

#endif /* BUS_H */
