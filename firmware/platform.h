#ifndef T8L_PLATFORM_H
#define T8L_PLATFORM_H
#include <stdint.h>
#include <stddef.h>

#ifdef T8L_HOST_TEST
uint8_t board_u8(uint32_t address);
uint16_t board_u16(uint32_t address);
uint32_t board_u32(uint32_t address);
uint32_t board_irq_save(void);
void board_irq_restore(uint32_t saved);
uint32_t stock_init(void);
uint32_t stock_loop(void);
uint32_t stock_keys(uint8_t *events);
uint32_t stock_crsf(const uint8_t *frame, uint32_t size);
uint32_t stock_leds(const uint8_t *leds);
uint32_t stock_beep(uint8_t count, uint16_t duration);
#else
static inline uint8_t board_u8(uint32_t a) { return *(volatile uint8_t *)a; }
static inline uint16_t board_u16(uint32_t a) { return *(volatile uint16_t *)a; }
static inline uint32_t board_u32(uint32_t a) { return *(volatile uint32_t *)a; }
static inline uint32_t board_irq_save(void) {
    uint32_t saved;
    __asm__ volatile("mrs %0, primask\n cpsid i" : "=r"(saved) :: "memory");
    return saved;
}
static inline void board_irq_restore(uint32_t saved) {
    __asm__ volatile("msr primask, %0" :: "r"(saved) : "memory");
}
#define stock_init ((uint32_t (*)(void))0x0800f6fdu)
/* 0x08016934 contains the entire infinite runtime loop. Hook one returning
 * iteration at its 0x0801725e call to 0x08014aa8, NOT its outer entry. */
#define stock_loop ((uint32_t (*)(void))0x08014aa9u)
#define stock_keys ((uint32_t (*)(uint8_t *))0x08011d21u)
#define stock_crsf ((uint32_t (*)(const uint8_t *,uint32_t))0x08017fc1u)
#define stock_leds ((uint32_t (*)(const uint8_t *))0x0800f995u)
/* count=1 queues the stock short tone and its stop; duration ignored in that branch. */
#define stock_beep ((uint32_t (*)(uint8_t,uint16_t))0x08010199u)
#endif

uint32_t hook_init(void);
uint32_t hook_loop(void);
uint32_t hook_keys(uint8_t *events);
uint32_t hook_crsf(const uint8_t *frame, uint32_t size);
uint32_t hook_leds(const uint8_t *leds);
#endif
