#ifndef _INTERRUPT_H_INCLUDED_
#define _INTERRUPT_H_INCLUDED_

/* 以下はリンカ・スクリプトで定義してあるシンボル */
extern char softvec;
#define SOFTVEC_ADDR (&softvec)

/* ソフトウェア・割込みベクタの種別を表す型の定義 */
typedef short softvec_type_t;

/* 割込みハンドラの型の定義 */
typedef void (*softvec_handler_t)(softvec_type_t type, unsigned long sp);

/* ソフトウェア・割込みベクタの位置 */
#define SOFTVECS ((softvec_handler_t *)SOFTVEC_ADDR)

/* 割込み有効化 */
#define INTR_ENABLE  asm volatile ("andc.b #0x3f,ccr")
/* 割込み無効化 */
#define INTR_DISABLE asm volatile ("orc.b #0xc0,ccr")

/* ソフトウェア・割込みベクタの初期化 */
int softvec_init(void);

/* ソフトウェア・割込みベクタの設定 */
int softvec_setintr(softvec_type_t type, softvec_handler_t handler);

/* 共通割込みハンドラ */
void interrupt(softvec_type_t type, unsigned long sp);

#endif
