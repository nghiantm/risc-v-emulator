/* Shared by the emulator and the firmware. Must contain only #defines so it
 * compiles identically in hosted C and freestanding firmware. */
#ifndef MEMORY_MAP_H
#define MEMORY_MAP_H

/* Main RAM */
#define MAIN_RAM_BASE      0x80000000u
#define MAIN_RAM_SIZE      0x00400000u   /* 4 MiB */
/* DUT RAM (fault-injectable) */
#define DUT_RAM_BASE       0x90000000u
#define DUT_RAM_SIZE       0x00010000u   /* 64 KiB */
/* SYSCON */
#define SYSCON_BASE        0x00100000u
#define SYSCON_SIZE        0x00001000u
#define SYSCON_PASS        0x5555u
#define SYSCON_FAIL_TAG    0x3333u       /* fail word = (code << 16) | SYSCON_FAIL_TAG */
/* CLINT */
#define CLINT_BASE         0x02000000u
#define CLINT_SIZE         0x00010000u
#define CLINT_MTIMECMP_LO  0x4000u
#define CLINT_MTIMECMP_HI  0x4004u
#define CLINT_MTIME_LO     0xBFF8u
#define CLINT_MTIME_HI     0xBFFCu
/* UART */
#define UART_BASE          0x10000000u
#define UART_SIZE          0x00001000u
#define UART_TX            0x0u
#define UART_STATUS        0x4u
#define UART_STATUS_TXRDY  0x1u
/* Sensor */
#define SENSOR_BASE        0x10001000u
#define SENSOR_SIZE        0x00001000u
#define SENSOR_ID_REG      0x00u
#define SENSOR_CTRL_REG    0x04u
#define SENSOR_STATUS_REG  0x08u
#define SENSOR_DATA_REG    0x0Cu
#define SENSOR_COUNT_REG   0x10u
#define SENSOR_ID_VALUE    0x5E450001u
#define SENSOR_CTRL_START  (1u << 0)
#define SENSOR_CTRL_IRQ_EN (1u << 1)
#define SENSOR_STATUS_READY (1u << 0)
#define SENSOR_STATUS_ERROR (1u << 1)    /* reserved: never set by any fault; always 0 */
#define SENSOR_TEMP_MIN_CC (-4000)       /* centi-degrees C */
#define SENSOR_TEMP_MAX_CC 12500
#define SENSOR_LATENCY_INSTS 2000u

#endif /* MEMORY_MAP_H */
