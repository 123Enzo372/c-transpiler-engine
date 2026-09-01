#include "main.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static void free_lines(char **lines)
{
    if (lines == NULL)
        return;

    for (int i = 0; lines[i] != NULL; i++)
    {
        free(lines[i]);
    }
    free(lines);
}

static int append_line(char ***lines, int *count, int *capacity, const char *buffer, size_t len)
{
    char *line;

    if (*count >= *capacity - 1)
    {
        int old_capacity = *capacity;
        int new_capacity = *capacity + 64;
        char **tmp = realloc(*lines, sizeof(char *) * new_capacity);
        if (tmp == NULL)
        {
            report_message("ERREUR SYSTEME : Échec de réallocation mémoire pour le tableau de lignes.\n",
                           "SYSTEM ERROR : Memory reallocation failed for line list.\n");
            return 0;
        }
        *lines = tmp;
        for (int i = old_capacity; i < new_capacity; i++)
        {
            (*lines)[i] = NULL;
        }
        *capacity = new_capacity;
    }

    line = malloc(len + 1);
    if (line == NULL)
    {
        report_message("ERREUR SYSTEME : Échec d'allocation mémoire pour une ligne source.\n",
                       "SYSTEM ERROR : Memory allocation failed for source line.\n");
        return 0;
    }

    memcpy(line, buffer, len);
    line[len] = '\0';
    (*lines)[(*count)++] = line;
    (*lines)[*count] = NULL;
    return 1;
}

int parser(FILE *file, char ***text_ptr)
{
    if (file == NULL || text_ptr == NULL)
    {
        report_message("ERREUR : Paramètres invalides fournis au parser.\n",
                       "ERROR : Invalid parameters provided to parser.\n");
        return 1;
    }
    *text_ptr = NULL;

    int line_capacity = 64;
    int line_count = 0;
    char **lines = calloc(line_capacity, sizeof(char *));
    if (lines == NULL)
    {
        report_message("ERREUR SYSTEME : Échec d'allocation mémoire pour le tableau de lignes.\n",
                       "SYSTEM ERROR : Memory allocation failed for line list.\n");
        return 1;
    }

    size_t buffer_capacity = 256;
    size_t buffer_len = 0;
    char *buffer = malloc(buffer_capacity);
    if (buffer == NULL)
    {
        report_message("ERREUR SYSTEME : Échec d'allocation mémoire pour le tampon de ligne.\n",
                       "SYSTEM ERROR : Memory allocation failed for line buffer.\n");
        free(lines);
        return 1;
    }

    int c;
    while ((c = fgetc(file)) != EOF)
    {
        if (buffer_len >= buffer_capacity - 2)
        {
            size_t new_capacity = buffer_capacity * 2;
            char *tmp = realloc(buffer, new_capacity);
            if (tmp == NULL)
            {
                report_message("ERREUR SYSTEME : Échec de réallocation mémoire pour le tampon de ligne.\n",
                               "SYSTEM ERROR : Memory reallocation failed for line buffer.\n");
                free(buffer);
                free_lines(lines);
                return 1;
            }
            buffer = tmp;
            buffer_capacity = new_capacity;
        }

        buffer[buffer_len++] = (char)c;
        if (c == '\n')
        {
            if (!append_line(&lines, &line_count, &line_capacity, buffer, buffer_len))
            {
                free(buffer);
                free_lines(lines);
                return 1;
            }
            buffer_len = 0;
        }
    }

    if (buffer_len > 0)
    {
        if (buffer_len >= buffer_capacity - 1)
        {
            char *tmp = realloc(buffer, buffer_capacity + 2);
            if (tmp == NULL)
            {
                report_message("ERREUR SYSTEME : Échec de réallocation mémoire pour le tampon de ligne.\n",
                               "SYSTEM ERROR : Memory reallocation failed for line buffer.\n");
                free(buffer);
                free_lines(lines);
                return 1;
            }
            buffer = tmp;
        }

        buffer[buffer_len++] = '\n';
        if (!append_line(&lines, &line_count, &line_capacity, buffer, buffer_len))
        {
            free(buffer);
            free_lines(lines);
            return 1;
        }
    }

    free(buffer);
    *text_ptr = lines;
    return 0;
}
