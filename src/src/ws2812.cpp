#include "driver/rmt.h"

#define WS2812_T0H_NS (400)
#define WS2812_T0L_NS (850)
#define WS2812_T1H_NS (800)
#define WS2812_T1L_NS (450)
#define RMT_LL_HW_BASE  (&RMT)

static uint32_t t0h_ticks = 0;
static uint32_t t1h_ticks = 0;
static uint32_t t0l_ticks = 0;
static uint32_t t1l_ticks = 0;


static void IRAM_ATTR ws2812_rmt_adapter(const void *src, rmt_item32_t *dest, size_t src_size,
        size_t wanted_num, size_t *translated_size, size_t *item_num)
{
    if (src == NULL || dest == NULL) {
        *translated_size = 0;
        *item_num = 0;
        return;
    }
    const rmt_item32_t bit0 = {{{ t0h_ticks, 1, t0l_ticks, 0 }}}; //Logical 0
    const rmt_item32_t bit1 = {{{ t1h_ticks, 1, t1l_ticks, 0 }}}; //Logical 1
    size_t size = 0;
    size_t num = 0;
    uint8_t *psrc = (uint8_t *)src;
    rmt_item32_t *pdest = dest;
    while (size < src_size && num < wanted_num) {
        for (int i = 0; i < 8; i++) {
            // MSB first
            if (*psrc & (1 << (7 - i))) {
                pdest->val =  bit1.val;
            } else {
                pdest->val =  bit0.val;
            }
            num++;
            pdest++;
        }
        size++;
        psrc++;
    }
    *translated_size = size;
    *item_num = num;
}

void ws2812Set(uint32_t rgb) 
{
    // Reserve channel
    uint8_t pixels[3];
    uint32_t numBytes = sizeof(pixels);
    gpio_num_t pin = (gpio_num_t)45;

    pixels[1] = 0xFF & (rgb>>16);
    pixels[0] = 0xFF & (rgb>>8);
    pixels[2] = 0xFF & (rgb);

    rmt_channel_t channel = (rmt_channel_t)0;
    rmt_config_t config = RMT_DEFAULT_CONFIG_TX((gpio_num_t)pin, channel);
    config.clk_div = 2;
    rmt_config(&config);
    rmt_driver_install(config.channel, 0, 0);

    // Convert NS timings to ticks
    uint32_t counter_clk_hz = 0;

    rmt_get_counter_clock(channel, &counter_clk_hz);

    // NS to tick converter
    float ratio = (float)counter_clk_hz / 1e9;

    t0h_ticks = (uint32_t)(ratio * WS2812_T0H_NS);
    t0l_ticks = (uint32_t)(ratio * WS2812_T0L_NS);
    t1h_ticks = (uint32_t)(ratio * WS2812_T1H_NS);
    t1l_ticks = (uint32_t)(ratio * WS2812_T1L_NS);

    // Initialize automatic timing translator
    rmt_translator_init(config.channel, ws2812_rmt_adapter);

    // Write and wait to finish
    rmt_write_sample(config.channel, pixels, (size_t)numBytes, true);
    rmt_wait_tx_done(config.channel, pdMS_TO_TICKS(100));

    // Free channel again
    rmt_driver_uninstall(config.channel);

    gpio_set_direction((gpio_num_t)pin, GPIO_MODE_OUTPUT);
}