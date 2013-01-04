#ifndef _KOZOS_H_INCLUDED_
#define _KOZOS_H_INCLUDED_

#include "defines.h"
#include "syscall.h"

/* システム・コール */

/* スレッド起動のシステム・コール */
kz_thread_id_t kz_run(kz_func_t func, char *name, int priority, int stacksize,
        int argc, char *argv[]);
/* スレッド終了のシステム・コール */
void kz_exit(void);

int kz_wait(void);
int kz_sleep(void);
int kz_wakeup(kz_thread_id_t id);
kz_thread_id_t kz_getid(void);
int kz_chpri(int priority);

/* ライブラリ関数 */

/* 初期スレッドを起動し、OSの動作を開始する */
void kz_start(kz_func_t func, char *name, int priority, int stacksize,
        int argc, char *argv[]);
/* 致命的エラーの時に呼び出す */
void kz_sysdown(void);
/* システム・コールを実行する */
void kz_syscall(kz_syscall_type_t type, kz_syscall_param_t *param);

/* ユーザスレッド */

/* ユーザスレッドのメイン関数 */
int test09_1_main(int argc, char *argv[]);
int test09_2_main(int argc, char *argv[]);
int test09_3_main(int argc, char *argv[]);
extern kz_thread_id_t test09_1_id;
extern kz_thread_id_t test09_2_id;
extern kz_thread_id_t test09_3_id;

#endif
