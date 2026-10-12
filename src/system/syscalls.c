/**
 * @file    syscalls.c
 * @brief   Minimal newlib system calls for a bare-metal target.
 * @author  32-bit-bender
 * @date    2026-10-08
 *
 * Newlib's stdio (snprintf and friends) references these functions even
 * though this project never opens files or reads a console. Defining them
 * here replaces the stubs from --specs=nosys.specs, which print the
 * "... is not implemented and will always fail" linker warnings.
 *
 * Nothing here talks to hardware. Output written through stdout (a stray
 * printf) is discarded; use LOG_INF() and friends to see output on the UART.
 */
#include <errno.h>

/* Discard everything and report success, so stdio never retries or fails. */
int _write(int fd, const char *buf, int len)
{
    (void)fd;
    (void)buf;
    return len;
}

/* There is no input device: always end-of-file. */
int _read(int fd, char *buf, int len)
{
    (void)fd;
    (void)buf;
    (void)len;
    return 0;
}

/* No files can be opened, so no descriptor is ever valid to close. */
int _close(int fd)
{
    (void)fd;
    errno = EBADF;
    return -1;
}

/* Seeking is meaningless on a stream with no storage. */
int _lseek(int fd, int offset, int whence)
{
    (void)fd;
    (void)offset;
    (void)whence;
    return 0;
}
