/**
 * @file kz_xmodem.c
 * @author Shinichiro Nakamura
 * @brief XMODEM for KOZOS.
 */

/*
 * ===============================================================
 *  XMODEM for KOZOS
 *  Version 0.0.2
 * ===============================================================
 * Copyright (c) 2012 Shinichiro Nakamura
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 * ===============================================================
 */

#include <stdio.h>
#include <string.h>
#include "serial.h"
#include "kz_xmodem.h"

#ifdef WIN32
#   include <windows.h>
#   define SLEEP(SEC)       Sleep((SEC) * 1000)
#else
#   include <unistd.h>
#   define SLEEP(SEC)       sleep((SEC))
#endif

#define CODE_SOH            (0x01)
#define CODE_EOT            (0x04)
#define CODE_ACK            (0x06)
#define CODE_NAK            (0x15)
#define CODE_CAN            (0x18)
#define CODE_EOF            (0x1A)
#define CODE_EOF            (0x1A)
#define XMODEM_DATA_BLKSIZ  (128)
#define XMODEM_SEND_BLKSIZ  (3 + XMODEM_DATA_BLKSIZ + 1)

#define TIMEOUT_SHORT_MS    (10)
#define TIMEOUT_LONG_MS     (1000)

#define NAK_WAIT_SEC        (20)
#define NAK_WAIT_TIMES      ((NAK_WAIT_SEC * 1000) / TIMEOUT_SHORT_MS)

/**
 * @brief シリアルポートに残留するデータをフラッシュする。
 * @details
 * ターゲット側には、接続前に何らかの入力が行なわれている可能性がある。
 * ホスト側には、ターゲット側からやってきたエコーバック文字列や
 * プロンプトの表示などが残っているかもしれない。
 * リターンコードを送ってターゲット側のコマンド解釈層に残っている
 * データをフラッシュする。
 *
 * @param serial シリアルハンドラ。
 *
 * @return エラーコード。
 */
ErrorCode flush_serial(SERIAL *serial)
{
    unsigned char buf[64];

    /*
     * リターンコードをターゲットに送信する。
     */
    if (serial_write(serial, (unsigned char *)"\n", 1) != 0) {
        return SerialWriteError;
    }

    /*
     * リターンに対するエコーバックとプロンプトを飲み込む。
     * もしかしたら、シリアルポートドライバが抱えている
     * バッファリングされたデータも読み出されるかもしれない。
     * 必ず受信するわけではないので、ここではエラーを見ない。
     */
    serial_read_with_timeout(serial, buf, sizeof(buf), TIMEOUT_SHORT_MS);

    /*
     * どんなものを受信したか知りたい場合、
     * ここでbufを表示させてみれば良い。
     */

    return NoError;
}

/**
 * @brief KOZOSブートローダをロード状態に遷移させる。
 * @details
 * KOZOSブートローダは、loadというコマンドによってロード状態に遷移する。
 *
 * @param serial シリアルハンドラ。
 *
 * @return エラーコード。
 */
ErrorCode setup_load_condition(SERIAL *serial)
{
    unsigned char buf[64];

    /*
     * loadコマンドを発行する。
     */
    if (serial_write(serial, (unsigned char *)"load\n", 5) != 0) {
        return SerialWriteError;
    }

    /*
     * エコーバックを飲み込む。
     * 与えた受信要求長さを満たすわけでないのでエラーを見ない。
     */
    serial_read_with_timeout(serial, buf, sizeof(buf), TIMEOUT_SHORT_MS);

    return NoError;
}

/**
 * @brief ターゲットからのNAKコード受信を待つ。
 * @details
 * ターゲットからのNAKコードは数秒おきにやってくる事になっている。
 * あまり長くやってこない時にはターゲット側が期待している状態に
 * 遷移していないと判定してエラーとする。
 *
 * @param serial シリアルハンドラ。
 *
 * @return エラーコード。
 */
ErrorCode wait_target_nak(SERIAL *serial)
{
    ErrorCode retval = NoError;
    int cnt = 0;

    while (1) {
        /*
         * タイムアウトを設定して読み込みを行なう。
         */
        unsigned char c = 0x00;
        serial_read_with_timeout(serial, &c, 1, TIMEOUT_SHORT_MS);

        /*
         * ユーザを不安にさせないための出力。
         */
        if ((cnt % (1000 / TIMEOUT_SHORT_MS)) == 0) {
            fprintf(stderr, ".");
        }

        /*
         * NAKを受信したら終了。
         */
        if (c == CODE_NAK) {
            retval = NoError;
            break;
        }

        /*
         * タイムアウトとして扱うかどうかを判定する。
         */
        cnt++;
        if (cnt > NAK_WAIT_TIMES) {
            retval = TargetIllegalState;
            break;
        }
    }
    fprintf(stderr, "\n");

    return retval;
}

/**
 * @brief 与えられたバッファのチェックサムを計算する。
 *
 * @param buf バッファへのポインタ。
 * @param siz バッファに格納されているデータバイト数。
 *
 * @return チェックサム計算結果。
 */
unsigned char CALC_CHECKSUM(const unsigned char *buf, const int siz)
{
    int i;
    unsigned char cs = 0;
    for (i = 0; i < siz; i++) {
        cs += buf[i];
    }
    return cs;
}

/**
 * @brief 転送を実行する。
 *
 * @param serial シリアルハンドラ。
 * @param buf バッファへのポインタ。
 * @param siz バッファに格納されているデータバイト数。
 *
 * @return ターゲットから受信した結果コード。
 */
unsigned char TRANSMIT_BLOCK(
        SERIAL *serial, const unsigned char *buf, const int siz)
{
    unsigned char c = 0;
    serial_write(serial, buf, siz);
    serial_read_with_timeout(serial, &c, 1, TIMEOUT_LONG_MS);
    return c;
}

/**
 * @brief ファイルをターゲットに送信する。
 *
 * @param serial シリアルハンドラ。
 * @param filename ファイル名。
 *
 * @return エラーコード。
 */
ErrorCode transmit_file(SERIAL *serial, const char *filename)
{
    ErrorCode retval = NoError;

    /*
     * ファイルをオープンする。
     */
    FILE *fp = fopen(filename, "rb");
    if (fp == NULL) {
        return FileOpenError;
    }

    /*
     * ファイルの長さを取得する。
     */
    if (fseek(fp, 0L, SEEK_END) != 0) {
        retval = FileSeekError;
        goto out;
    }
    long file_size = ftell(fp);
    int block_count = file_size / XMODEM_DATA_BLKSIZ;
    int block_remainder = file_size % XMODEM_DATA_BLKSIZ;
    if (fseek(fp, 0L, SEEK_SET) != 0) {
        retval = FileSeekError;
        goto out;
    }
    fprintf(stderr, "File(%s): %d blocks + %d bytes\n",
            filename, block_count, block_remainder);

    /*
     * ブロック転送を開始する。
     */
    unsigned char block_number = 1;
    {
        int i;
        for (i = 0; i < block_count; i++) {
            unsigned char c;
            unsigned char buf[XMODEM_SEND_BLKSIZ];
            buf[0] = CODE_SOH;
            buf[1] = block_number;
            buf[2] = ~block_number;
            if (fread(buf + 3, XMODEM_DATA_BLKSIZ, 1, fp) != 1) {
                retval = FileReadError;
                goto out;
            }
            buf[3 + XMODEM_DATA_BLKSIZ] =
                CALC_CHECKSUM(&buf[3], XMODEM_DATA_BLKSIZ);
            do {
                c = TRANSMIT_BLOCK(serial, buf, sizeof(buf));
                fprintf(stderr, "%c", (c == CODE_ACK) ? '.' : 'x');
            } while (c == CODE_NAK);
            if (c != CODE_ACK) {
                retval = TargetIllegalResponse;
                goto out;
            }
            block_number++;
        }
    }
    if (block_remainder > 0) {
        unsigned char c;
        unsigned char buf[XMODEM_SEND_BLKSIZ];
        buf[0] = CODE_SOH;
        buf[1] = block_number;
        buf[2] = ~block_number;
        memset(buf + 3, CODE_EOF, XMODEM_DATA_BLKSIZ);
        if (fread(buf + 3, block_remainder, 1, fp) != 1) {
            retval = FileReadError;
            goto out;
        }
        buf[3 + XMODEM_DATA_BLKSIZ] =
            CALC_CHECKSUM(&buf[3], XMODEM_DATA_BLKSIZ);
        do {
            c = TRANSMIT_BLOCK(serial, buf, sizeof(buf));
            fprintf(stderr, "%c", (c == CODE_ACK) ? '.' : 'x');
        } while (c == CODE_NAK);
        if (c != CODE_ACK) {
            retval = TargetIllegalResponse;
            goto out;
        }
    }

    /*
     * 終了コードを送信して結果コードを取得する。
     */
    {
        unsigned char c = CODE_EOT;
        serial_write(serial, &c, 1);
        serial_read_with_timeout(serial, &c, 1, TIMEOUT_LONG_MS);
        if (c != CODE_ACK) {
            retval = TargetIllegalResponse;
            goto out;
        }
    }

out:
    fprintf(stderr, "\n");
    fclose(fp);
    return retval;
}

/**
 * @brief エラーメッセージを表示する。
 *
 * @param ec エラーコード。
 */
void ERROR_MESSAGE(ErrorCode ec)
{
    char *txt;
    switch (ec) {
        case NoError:
            txt = "No error.";
            break;
        case SerialOpenError:
            txt = "Serial open error.";
            break;
        case SerialWriteError:
            txt = "Serial write error.";
            break;
        case TargetIllegalState:
            txt = "Illegal target state found.";
            break;
        case TargetIllegalResponse:
            txt = "Illegal target response found.";
            break;
        case FileOpenError:
            txt = "File open error.";
            break;
        case FileSeekError:
            txt = "File seek error.";
            break;
        case FileReadError:
            txt = "File read error.";
            break;
        default:
            txt = "Unknown error.";
            break;
    }
    fprintf(stderr, "%s\n", txt);
}

/**
 * @brief エントリポイント。
 *
 * @param argc 引数の数。
 * @param argv 引数のポインタのポインタ。
 *
 * @return シェルに返す値。
 */
int main(int argc, char **argv)
{
    ErrorCode ec;

    fprintf(stderr, "=================================================\n");
    fprintf(stderr, " XMODEM for KOZOS H8/3069F (Version %d.%d.%d)\n",
            VERSION_MAJOR,
            VERSION_MINOR,
            VERSION_RELEASE);
    fprintf(stderr, " Copyright(C) 2012 Shinichiro Nakamura\n");
    fprintf(stderr, "=================================================\n");

    if (argc != 3) {
        fprintf(stderr, "kz_xmodem [elf file] [interface]\n");
        return 1;
    }

    /*
     * シリアルをオープンする。
     */
    SERIAL *serial = serial_open(argv[2], SerialBaud9600);
    if (serial == NULL) {
        fprintf(stderr, "Serial open failed.\n");
        return 2;
    }

    /*
     * ホストとターゲットのシリアルポートに滞留しているデータを吐き出す。
     */
    fprintf(stderr, "Flushing serial port.\n");
    ec = flush_serial(serial);
    if (ec != NoError) {
        goto failure;
    }

    /*
     * バッファリングによってコマンドが行き違いになるのを防ぐ。
     */
    fprintf(stderr, "Wait.\n");
    SLEEP(1);

    /*
     * KOZOSブートローダをload状態に遷移させる。
     */
    fprintf(stderr, "Setup load condition.\n");
    ec = setup_load_condition(serial);
    if (ec != NoError) {
        goto failure;
    }

    /*
     * ターゲットからのNAK着信を待つ。
     */
    fprintf(stderr, "Wait a NAK.\n");
    ec = wait_target_nak(serial);
    if (ec != NoError) {
        goto failure;
    }

    /*
     * ELFファイルをターゲットに転送する。
     */
    fprintf(stderr, "Transmit the target ELF file.\n");
    ec = transmit_file(serial, argv[1]);
    if (ec != NoError) {
        goto failure;
    }

    /*
     * ターゲットからやってくる最後のメッセージを待つ。
     */
    fprintf(stderr, "Wait a message from the target.\n");
    SLEEP(1);

    /*
     * ターゲットからやってくる最後のメッセージを飲み込む。
     */
    flush_serial(serial);

    /*
     * シリアルポートを閉じる。
     */
    serial_close(serial);

    /*
     * 正常終了を示す文字列を出力して終了。
     */
    fprintf(stderr, "Complete.\n");
    return (int)NoError;

failure:
    /*
     * シリアルポートを閉じる。
     */
    serial_close(serial);

    /*
     * エラーメッセージを出力して終了。
     */
    fprintf(stderr, "Error: ");
    ERROR_MESSAGE(ec);
    return (int)ec;
}

