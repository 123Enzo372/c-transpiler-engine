#ifndef LANGUAGE_RUNTIME_H
#define LANGUAGE_RUNTIME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static inline char *nl_input(const char *prompt)
{
    char *buffer = malloc(256 * sizeof(char));
    if (buffer == NULL)
        return NULL;

    if (prompt != NULL)
    {
        printf("%s", prompt);
        fflush(stdout);
    }

    if (fgets(buffer, 256, stdin) == NULL)
    {
        buffer[0] = '\0';
        return buffer;
    }

    buffer[strcspn(buffer, "\n")] = '\0';
    return buffer;
}

static inline char *nl_str_copy(const char *text)
{
    size_t len;
    char *copy;

    if (text == NULL)
        text = "";

    len = strlen(text);
    copy = malloc(len + 1);
    if (copy == NULL)
        return NULL;

    memcpy(copy, text, len + 1);
    return copy;
}

static inline char *nl_str_concat(const char *left, const char *right)
{
    size_t left_len;
    size_t right_len;
    char *result;

    if (left == NULL)
        left = "";
    if (right == NULL)
        right = "";

    left_len = strlen(left);
    right_len = strlen(right);
    result = malloc(left_len + right_len + 1);
    if (result == NULL)
        return NULL;

    memcpy(result, left, left_len);
    memcpy(result + left_len, right, right_len + 1);
    return result;
}

static inline int nl_str_len(const char *text)
{
    return text == NULL ? 0 : (int)strlen(text);
}

static inline bool nl_str_eq(const char *left, const char *right)
{
    if (left == NULL || right == NULL)
        return left == right;
    return strcmp(left, right) == 0;
}

#define NL_APPEND_DECL(TYPE, NAME) \
static inline bool nl_append_##NAME(TYPE **list, int *len, TYPE value) \
{ \
    TYPE *tmp; \
    if (list == NULL || len == NULL || *len < 0) \
        return false; \
    tmp = realloc(*list, (size_t)(*len + 1) * sizeof(TYPE)); \
    if (tmp == NULL) \
        return false; \
    tmp[*len] = value; \
    *list = tmp; \
    (*len)++; \
    return true; \
} \
static inline bool nl_insert_##NAME(TYPE **list, int *len, int index, TYPE value) \
{ \
    TYPE *tmp; \
    if (list == NULL || len == NULL || *len < 0 || index < 0 || index > *len) \
        return false; \
    tmp = realloc(*list, (size_t)(*len + 1) * sizeof(TYPE)); \
    if (tmp == NULL) \
        return false; \
    memmove(tmp + index + 1, tmp + index, (size_t)(*len - index) * sizeof(TYPE)); \
    tmp[index] = value; \
    *list = tmp; \
    (*len)++; \
    return true; \
} \
static inline bool nl_contains_##NAME(const TYPE *list, int len, TYPE value) \
{ \
    if (list == NULL || len <= 0) \
        return false; \
    for (int i = 0; i < len; i++) \
    { \
        if (list[i] == value) \
            return true; \
    } \
    return false; \
}

NL_APPEND_DECL(int, int)
NL_APPEND_DECL(float, float)
NL_APPEND_DECL(bool, bool)

static inline bool nl_append_str(char ***list, int *len, char *value)
{
    char **tmp;
    if (list == NULL || len == NULL || *len < 0)
        return false;
    tmp = realloc(*list, (size_t)(*len + 1) * sizeof(char *));
    if (tmp == NULL)
        return false;
    tmp[*len] = value;
    *list = tmp;
    (*len)++;
    return true;
}

static inline bool nl_insert_str(char ***list, int *len, int index, char *value)
{
    char **tmp;
    if (list == NULL || len == NULL || *len < 0 || index < 0 || index > *len)
        return false;
    tmp = realloc(*list, (size_t)(*len + 1) * sizeof(char *));
    if (tmp == NULL)
        return false;
    memmove(tmp + index + 1, tmp + index, (size_t)(*len - index) * sizeof(char *));
    tmp[index] = value;
    *list = tmp;
    (*len)++;
    return true;
}

static inline bool nl_contains_str(char **list, int len, const char *value)
{
    if (list == NULL || len <= 0 || value == NULL)
        return false;
    for (int i = 0; i < len; i++)
    {
        if (list[i] != NULL && strcmp(list[i], value) == 0)
            return true;
    }
    return false;
}

#undef NL_APPEND_DECL

#endif
