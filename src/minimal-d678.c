/** \file
 * Minimal test code for DIGIC 6
 * ROM dumper & other experiments but this is a minecraft server 
 */
#if !defined(CONFIG_200D)
#error "This is only for the 200D at version 1.0.1"
#endif
#include "dryos.h"
#include "bmp.h"
#include "customscreen.h"
#include "log-d678.h"
#include "avrcraft/demo_legacy_sock_dumbcraft/avrcraft.h"
extern void dump_file(char *name, uint32_t addr, uint32_t size);
extern void uart_printf(const char *fmt, ...);

extern void font_draw(uint32_t, uint32_t, uint32_t, uint32_t, char *);
extern void font_drawf(uint32_t *buf, uint32_t x_pos, uint32_t y_pos, uint32_t color, uint32_t scale, char *text, uint32_t height, uint32_t width);

extern uint32_t _AllocateMemory(uint32_t size);
extern void _FreeMemory(void *);

extern int wlanconnect(void *);

//#define SSID "turtius"
//#define PASSPHRASE "rocks@123"
//#define STATICIP "192.168.10.22" //IP for the camera.

#if !defined SSID || !defined PASSPHRASE || !defined STATICIP
#error "Wifi stuff not specified"
#endif
struct
{
    int a;
    int b;
    int c;
    char ssid[0x24];
    int d;
    int e;
    int f;
    int g;
    int h;
    char pass[0x3f];
} * wifisettings;

struct marv
{
    uint32_t signature;    // MARV - VRAM reversed
    uint8_t *bitmap_data;  // either UYVY or UYVY + opacity
    uint8_t *opacity_data; // optional; if missing, interleaved in bitmap_data
    uint32_t flags;        // (flags is a guess)
    uint32_t width;        // X resolution; may be larger than screen size
    uint32_t height;       // Y resolution; may be larger than screen size
    uint32_t pmem;         // pointer to PMEM (Permanent Memory) structure
};
static void led_blink(int times, int delay_on, int delay_off)
{
    for (int i = 0; i < times; i++)
    {
        MEM(CARD_LED_ADDRESS) = LEDON;
        msleep(delay_on);
        MEM(CARD_LED_ADDRESS) = LEDOFF;
        msleep(delay_off);
    }
}

/* used by font_draw */
static uint32_t rgb2yuv422(uint8_t r, uint8_t g, uint8_t b)
{
    float R = r;
    float G = g;
    float B = b;
    float Y, U, V;
    uint8_t y, u, v;

    Y = R * .299000 + G * .587000 + B * .114000;
    U = R * -.168736 + G * -.331264 + B * .500000 + 128;
    V = R * .500000 + G * -.418688 + B * -.081312 + 128;

    y = Y;
    u = U;
    v = V;

    return (u << 24) | (y << 16) | (v << 8) | y;
}

void disp_set_pixel(uint32_t x, uint32_t y, uint32_t color)
{

    uint8_t *bmp = bmp_vram_raw();

    struct marv *marv = bmp_marv();

    // UYVY display, must convert
    //uint32_t color = 0xFFFFFFFF;
    //uint32_t uyvy = rgb2yuv422(color >> 24,
    //                           (color >> 16) & 0xff,
      //                         (color >> 8) & 0xff);
      uint32_t uyvy = 0x9515952b;
    uint8_t alpha =  0xff;

    if (marv->opacity_data)
    {
        // 80D, 200D
        // adapted from names_are_hard, https://pastebin.com/Vt84t4z1
        uint32_t *offset = (uint32_t *)&bmp[(x & ~1) * 2 + y * 2 * marv->width];
        if (x % 2)
        {
            // set U, Y2, V, keep Y1
            *offset = (*offset & 0x0000FF00) | (uyvy & 0xFFFF00FF);
        }
        else
        {
            // set U, Y1, V, keep Y2
            *offset = (*offset & 0xFF000000) | (uyvy & 0x00FFFFFF);
        }
        marv->opacity_data[x + y * marv->width] = alpha;
    }
    else
    {
        // 5D4, M50
        // adapted from https://bitbucket.org/chris_miller/ml-fork/src/d1f1cdf978acc06c6fd558221962c827a7dc28f8/src/minimal-d678.c?fileviewer=file-view-default#minimal-d678.c-175
        // VRAM layout is UYVYAA (each character is one byte) for pixel pairs
        uint32_t *offset = (uint32_t *)&bmp[(x & ~1) * 3 + y * 3 * marv->width]; // unaligned pointer
        if (x % 2)
        {
            // set U, Y2, V, keep Y1
            *offset = (*offset & 0x0000FF00) | (uyvy & 0xFFFF00FF);
        }
        else
        {
            // set U, Y1, V, keep Y2
            *offset = (*offset & 0xFF000000) | (uyvy & 0x00FFFFFF);
        }
        uint8_t *opacity = (uint8_t *)offset + 4 + x % 2;
        *opacity = alpha;
    }
}

void hexDump(char *desc, void *addr, int len)
{
    int i;
    unsigned char buff[17];
    unsigned char *pc = (unsigned char *)addr;

    // Output description if given.
    if (desc != NULL)
        uart_printf("%s:\n", desc);

    // Process every byte in the data.
    for (i = 0; i < len; i++)
    {
        // Multiple of 16 means new line (with line offset).

        if ((i % 16) == 0)
        {
            // Just don't print ASCII for the zeroth line.
            if (i != 0)
                uart_printf("  %s\n", buff);

            // Output the offset.
            uart_printf("  %04x ", i);
        }

        // Now the hex code for the specific character.
        uart_printf(" %02x", pc[i]);

        // And store a printable ASCII character for later.
        if ((pc[i] < 0x20) || (pc[i] > 0x7e))
        {
            buff[i % 16] = '.';
        }
        else
        {
            buff[i % 16] = pc[i];
        }

        buff[(i % 16) + 1] = '\0';
    }

    // Pad out last line if not exactly 16 characters.
    while ((i % 16) != 0)
    {
        uart_printf("   ");
        i++;
    }

    // And print the final ASCII bit.
    uart_printf("  %s\n", buff);
}

static void DUMP_ASM my_DebugMsg(int class, int level, char *fmt, ...)
{
    int lr = read_lr();
    if (class == 0x82)
    {
        char buf[1000];
        int len;
        va_list ap;
        va_start(ap, fmt);
        len += vsnprintf(buf + len, 1000 - len - 1, fmt, ap);
        va_end(ap);
        len += snprintf(buf + len, 1000 - len, " LR: %x", lr);
        len += snprintf(buf + len, 1000 - len, "\n");
        uart_printf("%s", buf);
    }
    else
    {
        return;
    }
}

static void DUMP_ASM server()
{

    //wait for screen to turn on before doing anything
    while (!bmp_vram_raw())
    {
        msleep(100);
    }

    msleep(1000);

    bmp_printf_auto("Switching WIFI on...\n");
    msleep(1000);
    bmp_printf_auto("Waiting for response\n");
    wifisettings = _AllocateMemory(0xfc);
    bmp_printf_auto("Allocated 0x%x bytes at 0x%x", 0xfc, wifisettings);
    memset(wifisettings, 0, 0xfc);
    strcpy(wifisettings->ssid, SSID);
    strcpy(wifisettings->pass, PASSPHRASE);
    char *ip = STATICIP;
    wifisettings->a = 0;
    wifisettings->b = 2;
    wifisettings->c = 2;
    wifisettings->d = 0;
    wifisettings->e = 6;
    wifisettings->f = 4;
    wifisettings->g = 0;
    wifisettings->h = 6;
    // hexDump("A",&wifisettings,0xfc);
    hexDump("A", wifisettings, 0xfc);
    //Turn lime core on
    call("NwLimeInit");
    call("NwLimeOn");
    call("wlanpoweron");
    call("wlanup");
    call("wlanchk");
    call("wlanipset", ip);
    if (wlanconnect(wifisettings) != 0)
    {
        _FreeMemory(wifisettings);
        bmp_printf_auto("Cant connect to WIFI!\n");
        return;
    }
    while (MEM(0x1d90c) == 0xe073631f)
    {
        msleep(100);
    }              //wait for lime core to power on
    msleep(10000); //wait for the Lime core to init.
    bmp_printf_auto("IP: %s\n", ip);
    msleep(100);
    StartServer();
    while (true)
    {

        msleep(100);
    }
}

/* called before Canon's init_task */
void boot_pre_init_task(void)
{
#ifdef LOG_EARLY_STARTUP
    log_start();
#endif
}

/* called right after Canon's init_task, while their initialization continues in background */
void boot_post_init_task(void)
{
#ifndef LOG_EARLY_STARTUP

#endif
    msleep(1000);
    task_create("server", 0x1f, 0x1000, server, 0);
    task_create("draw", 0x1e, 0x1000, screen, 0);
}

#ifndef CONFIG_5D4
/* dummy */
int FIO_WriteFile(FILE *stream, const void *ptr, size_t count){};
#endif

void ml_assert_handler(char *msg, char *file, int line, const char *func){};
