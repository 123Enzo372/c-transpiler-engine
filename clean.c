#include "main.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


void remove_created_files(char **created_filenames, char **new_option)
{
    if (created_filenames == NULL)
        return;

    int keep_c = 0;
    int keep_h = 0;

    if (new_option != NULL)
    {
        for (int i = 0; new_option[i] != NULL; i++)
        {
            if (strcmp(new_option[i], "-keep_c") == 0)
                keep_c = 1;
            else if (strcmp(new_option[i], "-keep_h") == 0)
                keep_h = 1;
        }
    }

    for (int i = 0; created_filenames[i] != NULL; i++)
    {
        size_t len = strlen(created_filenames[i]);
        if (len < 2)
            continue;

        if (!keep_c && strcmp(created_filenames[i] + len - 2, ".c") == 0)
        {
            if (remove(created_filenames[i]) != 0)
            {
                if (flag_french)
                    fprintf(stderr, "ERREUR SYSTEME : Impossible de supprimer le fichier temporaire '%s'.\n", created_filenames[i]);
                else
                    fprintf(stderr, "SYSTEM ERROR : Failed to remove temporary file '%s'.\n", created_filenames[i]);
            }
        }
        else if (!keep_h && strcmp(created_filenames[i] + len - 2, ".h") == 0)
        {
            size_t gch_len = len + 5;
            char *gch_filename = malloc(gch_len);
            if (gch_filename != NULL)
            {
                snprintf(gch_filename, gch_len, "%s.gch", created_filenames[i]);
                remove(gch_filename);
                free(gch_filename);
            }
            else
            {
                if (flag_french)
                    fprintf(stderr, "ERREUR SYSTEME : Échec d'allocation mémoire pour 'gch_filename'.\n");
                else
                    fprintf(stderr, "SYSTEM ERROR : Memory allocation failed for 'gch_filename'.\n");
            }

            if (remove(created_filenames[i]) != 0)
            {
                if (flag_french)
                    fprintf(stderr, "ERREUR SYSTEME : Impossible de supprimer le fichier temporaire '%s'.\n", created_filenames[i]);
                else
                    fprintf(stderr, "SYSTEM ERROR : Failed to remove temporary file '%s'.\n", created_filenames[i]);
            }
        }
    }
}

void remove_H_files(char **filename)
{
    if (filename == NULL)
        return;

    for (int i = 0; filename[i] != NULL; i++)
    {
        size_t len = strlen(filename[i]);
        if (len >= 2 && strcmp(filename[i] + len - 2, ".H") == 0)
        {
            if (remove(filename[i]) != 0)
            {
                if (flag_french)
                    fprintf(stderr, "ERREUR SYSTEME : Impossible de supprimer le fichier source '%s'.\n", filename[i]);
                else
                    fprintf(stderr, "SYSTEM ERROR : Failed to remove source file '%s'.\n", filename[i]);
            }
        }
    }
}

void remove_l_files(char **filename)
{
    if (filename == NULL)
        return;

    for (int i = 0; filename[i] != NULL; i++)
    {
        size_t len = strlen(filename[i]);
        if (len >= 2 && strcmp(filename[i] + len - 2, ".l") == 0)
        {
            if (remove(filename[i]) != 0)
            {
                if (flag_french)
                    fprintf(stderr, "ERREUR SYSTEME : Impossible de supprimer le fichier source '%s'.\n", filename[i]);
                else
                    fprintf(stderr, "SYSTEM ERROR : Failed to remove source file '%s'.\n", filename[i]);
            }
        }
    }
}
