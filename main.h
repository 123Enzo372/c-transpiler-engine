#ifndef MAIN_H
#define MAIN_H

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int flag_french;

static inline void report_message(const char *fr_format, const char *en_format, ...)
{
    va_list args;
    const char *format = flag_french ? fr_format : en_format;

    va_start(args, en_format);
    vfprintf(stderr, format, args);
    va_end(args);
}

static inline int safe_copy(char *dest, size_t dest_size, const char *src)
{
    size_t len;

    if (dest == NULL || src == NULL || dest_size == 0)
        return 0;

    len = strlen(src);
    if (len >= dest_size)
    {
        dest[0] = '\0';
        return 0;
    }

    memcpy(dest, src, len + 1);
    return 1;
}

static inline char *duplicate_string(const char *src)
{
    size_t len;
    char *copy;

    if (src == NULL)
        return NULL;

    len = strlen(src);
    copy = malloc(len + 1);
    if (copy == NULL)
        return NULL;

    memcpy(copy, src, len + 1);
    return copy;
}

int parser(FILE *file, char ***text_ptr);
int trad_h(char *filename, char ***text_ptr);
int trad_c(char *filename, char ***text_ptr);
int execute(char **filename, char *output, char **options);

void remove_created_files(char **created_filenames, char **new_option);
void remove_H_files(char **filename);
void remove_l_files(char **filename);

#endif /* MAIN_H */
