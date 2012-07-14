#ifndef _LIB_H_INCLUDED_
#define _LIB_H_INCLUDED_

void *memset(void *b, int c, long len); /* メモリを特定のバイトデータで埋める */
void *memcpy(void *dst, const void *src, long len); /* メモリのコピー */
int memcmp(const void *b1, const void *b2, long len);   /* メモリの比較 */
int strlen(const char *s);  /* 文字列の長さ */
char *strcpy(char *dst, const char *src);   /* 文字列のコピー */
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, int len);   /* 長さ指定での文字列の比較 */

int putc(unsigned char c);  /* 1文字送信 */
int puts(unsigned char *str);   /* 文字列送信 */
int putxval(unsigned long value, int column);   /* 数値の16進表示 */

#endif
