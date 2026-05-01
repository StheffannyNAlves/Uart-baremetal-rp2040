#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define CLOCKS_BASE      0x40008000
#define RESETS_BASE      0x4000c000
#define IO_BANK0_BASE    0x40014000
#define PADS_BANK0_BASE  0x4001c000
#define XOSC_BASE        0x40024000
#define UART0_BASE       0x40034000
#define SIO_BASE         0xd0000000

#define XOSC_CTRL        *(volatile uint32_t *)(XOSC_BASE + 0x00)
#define XOSC_STATUS      *(volatile uint32_t *)(XOSC_BASE + 0x04)
#define XOSC_STARTUP     *(volatile uint32_t *)(XOSC_BASE + 0x0C)

#define XOSC_ENABLE      0xFABAA0
#define CMD_ENABLE       0xB
#define STABLE_BIT       (1u << 31)

#define CLK_REF_CTRL     *(volatile uint32_t *)(CLOCKS_BASE + 0x30)
#define CLK_SYS_CTRL     *(volatile uint32_t *)(CLOCKS_BASE + 0x3C)
#define CLK_PERI_CTRL    *(volatile uint32_t *)(CLOCKS_BASE + 0x48)

#define SRC_XOSC         2u
#define SRC_AUX          0u

#define LED              25u

#define RESETS_RESET      *(volatile uint32_t *)(RESETS_BASE + 0x00)
#define RESETS_RESET_DONE *(volatile uint32_t *)(RESETS_BASE + 0x08)

#define RST_UART0        (1u << 22)
#define RST_IO_BANK0     (1u << 5)
#define RST_PADS_BANK0   (1u << 8)

#define GPIO0_CTRL       *(volatile uint32_t *)(IO_BANK0_BASE + 0x004) // TX
#define GPIO01_CTRL      *(volatile uint32_t *)(IO_BANK0_BASE + 0x00C) // RX
#define GPIO25_CTRL      *(volatile uint32_t *)(IO_BANK0_BASE + 0x0CC) // LED

#define FUNC_UART        2u
#define FUNC_SIO         5u

#define GPIO_OUT_XOR     *(volatile uint32_t *)(SIO_BASE + 0x01C)
#define GPIO_OE_SET      *(volatile uint32_t *)(SIO_BASE + 0x024)
#define GPIO_OUT_CLR     *(volatile uint32_t *)(SIO_BASE + 0x018)


#define UART0_DR         *(volatile uint32_t *)(UART0_BASE + 0x000)
#define UART0_FR         *(volatile uint32_t *)(UART0_BASE + 0x018)
#define UART0_IBRD       *(volatile uint32_t *)(UART0_BASE + 0x024)
#define UART0_FBRD       *(volatile uint32_t *)(UART0_BASE + 0x028)
#define UART0_LCR_H      *(volatile uint32_t *)(UART0_BASE + 0x02C)
#define UART0_CR         *(volatile uint32_t *)(UART0_BASE + 0x030)

#define TXFF_BIT         (1u << 5)
#define RXFE_BIT         (1u << 4)

/* PADS_BANK0 */
#define PADS_GPIO0       *(volatile uint32_t *)(PADS_BANK0_BASE + 0x04)
#define PADS_GPIO01      *(volatile uint32_t *)(PADS_BANK0_BASE + 0x08)

#define PAD_OD           (1u << 7)
#define PAD_IE           (1u << 6)
#define PAD_DRIVE_MASK   (3u << 4)
#define PAD_PUE          (1u << 3)
#define PAD_PDE          (1u << 2)
#define PAD_SCHMITT      (1u << 1)
#define PAD_SLEWFAST     (1u << 0)

#define CLK_PERI         12000000u
#define BAUD_RATE        115200u

static void delay(volatile uint32_t n)
{
    while (n--) {
        __asm volatile ("nop");
    }
}

void xosc_init(void)
{
    XOSC_STARTUP = 47u;
    XOSC_CTRL = (XOSC_ENABLE | CMD_ENABLE);
    while (!(XOSC_STATUS & STABLE_BIT));

    CLK_REF_CTRL = SRC_XOSC;
    CLK_SYS_CTRL = SRC_AUX;

    /* mantém enable como no seu código original */
    CLK_PERI_CTRL = (1u << 11);
}

static void pads_uart_init(void)
{
    /*
     * TX (GPIO0):
     * - sem pull interno
     * - mantém input enable desligado para não sobrar ruído
     */
    PADS_GPIO0 &= ~(PAD_PUE | PAD_PDE | PAD_IE);

    /*
     * RX (GPIO13):
     * - input enable ligado
     * - pull-up interno ligado
     * - schmitt ligado
     * - pull-down desligado
     */
    PADS_GPIO01 &= ~PAD_PDE;
    PADS_GPIO01 |= (PAD_IE | PAD_PUE | PAD_SCHMITT);
}

void uart_init(void)
{
    RESETS_RESET &= ~RST_UART0;
    while (!(RESETS_RESET_DONE & RST_UART0));

    GPIO0_CTRL = FUNC_UART;
    GPIO01_CTRL = FUNC_UART;

    pads_uart_init();

    uint32_t div_x64 = ((CLK_PERI * 4u) + (BAUD_RATE / 2u)) / BAUD_RATE;

    UART0_CR = 0u;
    UART0_IBRD = div_x64 >> 6;
    UART0_FBRD = div_x64 & 0x3Fu;
    UART0_LCR_H = (1u << 4) | (3u << 5);   // FIFO + 8 bits
    UART0_CR = (1u << 0) | (1u << 8) | (1u << 9); // UARTEN | TXE | RXE
}

bool uartrx_disponivel(void)
{
    return !(UART0_FR & RXFE_BIT);
}

char uart_getc(void)
{
    while (UART0_FR & RXFE_BIT);
    return (char)(UART0_DR & 0xFFu);
}

void uart_putc(char data)
{
    while (UART0_FR & TXFF_BIT);
    UART0_DR = (uint32_t)data;
}

void uart_puts(const char *s)
{
    while (*s) {
        uart_putc(*s++);
    }
}

int main(void)
{
    xosc_init();

    uint32_t rst = (RST_IO_BANK0 | RST_PADS_BANK0);
    RESETS_RESET &= ~rst;
    while ((RESETS_RESET_DONE & rst) != rst);

    GPIO25_CTRL = FUNC_SIO;
    GPIO_OE_SET = (1u << LED);

    for (int j = 0; j < 6; j++) {
        GPIO_OUT_XOR = (1u << LED);
        delay(1000000u);
    }

    uart_init();
    uart_puts("UART echo ready\r\n");

    while (1) {
        char c = uart_getc();   // espera bloquear até chegar algo no RX
        uart_putc(c);           // devolve no TX

        if (c == 'S' || c == 't' || c == 'h' || c == 'e') {
            GPIO_OUT_XOR = (1u << LED);
        } else if (c == '\r' || c == '\n') {
            // ignora
        } else {
            GPIO_OUT_CLR = (1u << LED);
        }
    }
}