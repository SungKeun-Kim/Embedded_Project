/**
 * @file  syscalls.c
 * @brief Newlib 시스템 콜 스텁 (nosys/nano.specs 보완)
 *
 * printf → USART1 리다이렉트 (디버그 빌드 시 활성화 가능).
 */
#include <sys/stat.h>
#include <errno.h>

/* _write: stdout/stderr → 디버그 UART (필요 시 구현) */
int _write(int file, char *ptr, int len)
{
    (void)file;
    (void)ptr;
    /* 디버그 UART 미연결 시 버림 */
    return len;
}

int _read(int file, char *ptr, int len)
{
    (void)file;
    (void)ptr;
    (void)len;
    return 0;
}

int _close(int file)
{
    (void)file;
    return -1;
}

int _fstat(int file, struct stat *st)
{
    (void)file;
    st->st_mode = S_IFCHR;
    return 0;
}

int _isatty(int file)
{
    (void)file;
    return 1;
}

int _lseek(int file, int ptr, int dir)
{
    (void)file;
    (void)ptr;
    (void)dir;
    return 0;
}

/* _sbrk: 힙 확장 (동적 할당 미사용이나 printf 내부에서 필요할 수 있음) */
extern char _end;  /* 링커 스크립트 심볼 */

void *_sbrk(int incr)
{
    static char *heap_end = 0;
    char *prev;

    if (heap_end == 0) {
        heap_end = &_end;
    }
    prev = heap_end;
    heap_end += incr;
    return (void *)prev;
}
