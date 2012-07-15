#include "defines.h"
#include "intr.h"
#include "interrupt.h"
#include "serial.h"
#include "lib.h"

static void intr(softvec_type_t type, unsigned long sp)
{
    int c;
    static char buf[32];
    static int len;

    c = getc();

    if(c != '\n') {
        buf[len++] = c;
    } else {
        buf[len++] = '\0';
        if(!strncmp(buf, "echo", 4)) {
            puts(buf + 4);
            puts("\n");
        } else {
            puts("unknown command.\n");
        }
        puts("> ");
        len = 0;
    }
}

int main(void)
{
    INTR_DISABLE;   /* 割込み無効にする */

    puts("kozos boot succeed!\n");

    /* シリアル割込みハンドラを登録 */
    softvec_setintr(SOFTVEC_TYPE_SERINTR, intr);
    /* シリアル受信割込みを有効化 */
    serial_intr_recv_enable(SERIAL_DEFAULT_DEVICE);

    puts("> ");

    INTR_ENABLE;    /* 割込み有効にする */
    while(1) {
        asm volatile ("sleep"); /* 省電力モードに移行 */
        /* 割込み待ち */
    }

    return 0;
}

