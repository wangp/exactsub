/*
** exactsub FROM TO
**
** Replace all occurrences of FROM with TO, from standard input to standard
** output.  The FROM and TO arguments may contain any bytes (except NUL);
** there are no special characters.
*/

#define _GNU_SOURCE     /* memmem() */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void die(const char *msg)
{
    fprintf(stderr, "%s\n", msg);
    exit(1);
}

typedef struct
{
    char    *ptr;
    int     cap;
    int     len;
} buf_t;

void buf_check(buf_t *buf)
{
    (void)buf;
    assert(buf->cap >= 0);
    assert(buf->len >= 0);
    assert(buf->len <= buf->cap);
}

void buf_init(buf_t *buf, int cap)
{
    buf->ptr = malloc(cap);
    buf->cap = cap;
    buf->len = 0;

    if (buf->ptr == NULL) {
        die("malloc failed\n");
    }
}

void buf_free(buf_t *buf)
{
    free(buf->ptr);
    buf->ptr = 0;
    buf->cap = 0;
    buf->len = 0;
}

void buf_shift(buf_t *buf, int n)
{
    int     newlen;

    newlen = buf->len - n;
    assert(newlen >= 0);

    memmove(buf->ptr, buf->ptr + n, newlen);
    buf->len = newlen;
    buf_check(buf);
}

int buf_search(const buf_t *buf, const int start,
    const void *needle, int needlelen)
{
    char    *p;

    assert(start >= 0);
    assert(start <= buf->len);

    p = memmem(buf->ptr + start, buf->len - start, needle, needlelen);
    if (p == NULL)
        return -1;
    else
        return p - buf->ptr;
}

int buf_fread(buf_t *buf, FILE *stream)
{
    char    *ptr;
    int     avail;
    size_t  n;

    ptr = buf->ptr + buf->len;
    avail = buf->cap - buf->len;
    n = fread(ptr, 1, avail, stream);
    if (n > 0) {
        buf->len += n;
        buf_check(buf);
    }
    return ((int)n == avail);
}

void checked_buf_fread(buf_t *buf, FILE *stream)
{
    if (!buf_fread(buf, stream)) {
        if (ferror(stream)) {
            die("read error\n");
        }
    }
}

void checked_fwrite(FILE *stream, const void *ptr, size_t num_bytes)
{
    size_t  n;

    n = fwrite(ptr, 1, num_bytes, stream);
    if (n < num_bytes) {
        fprintf(stderr, "write error\n");
        exit(1);
    }
}

int checked_fgetc(FILE *stream)
{
    int     c;

    c = fgetc(stream);
    if (c == EOF && ferror(stream)) {
        die("read error\n");
    }
    return c;
}

void checked_fputc(FILE *stream, int c)
{
    if (fputc(c, stream) == EOF) {
        die("write error\n");
    }
}

int exactsubst0(const char *to, const int to_len, FILE *inp, FILE *out)
{
    int     occurs = 0;

    for (;;) {
        int c = checked_fgetc(inp);
        checked_fwrite(out, to, to_len);
        occurs++;
        if (c == EOF)
            break;
        checked_fputc(out, c);
    }

    return occurs;
}

static unsigned nextpowerof2(unsigned x)
{
    unsigned y = 1;

    while (y < x) {
        unsigned y2 = y * 2;
        if (y2 < y) {
            die("integer overflow\n");
        }
        y = y2;
    }

    return y;
}

int exactsubst(const char *from, const int from_len,
                const char *to, const int to_len,
                FILE *inp, FILE *out)
{
    const int   MINBUFCAP = 1024;
    int         choosecap;
    buf_t       buf;
    int         occurs = 0;

    if (from_len == 0) {
        return exactsubst0(to, to_len, inp, out);
    }

    if (from_len < MINBUFCAP) {
        choosecap = MINBUFCAP;
    } else {
        choosecap = (int) nextpowerof2(from_len);
    }
    if (choosecap < from_len) {
        die("insufficient buffer capacity\n");
    }

    buf_init(&buf, choosecap);

    while (!feof(inp)) {
        checked_buf_fread(&buf, inp);

        if (buf.len >= from_len) {
            int start = 0;
            int found;

            while ((found = buf_search(&buf, start, from, from_len)) >= 0) {
                checked_fwrite(out, buf.ptr + start, found - start);
                checked_fwrite(out, to, to_len);
                start = found + from_len;
                occurs++;
            }

            /*
            ** We can safely consume everything up to this point,
            ** if we have not already done so.
            */
            int skipto = buf.len - (from_len - 1);
            if (skipto > start) {
                checked_fwrite(out, buf.ptr + start, skipto - start);
                buf_shift(&buf, skipto);
            } else {
                buf_shift(&buf, start);
            }
        }
    }

    checked_fwrite(out, buf.ptr, buf.len);

    buf_free(&buf);

    return occurs;
}

int main(int argc, char *argv[])
{
    char    *from;
    int     from_len;
    char    *to;
    int     to_len;
    int     occurs;

    if (argc != 3) {
        fprintf(stderr, "Usage: exactsub FROM TO\n");
        return 1;
    }

    from = argv[1];
    from_len = strlen(from);
    to = argv[2];
    to_len = strlen(to);
    occurs = exactsubst(from, from_len, to, to_len, stdin, stdout);
    if (0) {
        fprintf(stderr, "occurrences: %d\n", occurs);
    }
    return 0;
}
