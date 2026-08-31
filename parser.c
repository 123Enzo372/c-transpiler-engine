#include "main.h"
#include <stdio.h>
#include <stdlib.h>


int parser(FILE *file, char ***text_ptr)
{
    if (file == NULL || text_ptr == NULL) 
    {
        report_message("ERREUR : Paramètres invalides fournis au parser.\n",
                       "ERROR : Invalid parameters provided to parser.\n");
        return 1;
    }
    *text_ptr = NULL;

    int size_c = 100;
    int size_s = 100;

    char **text = calloc(size_s, sizeof(char *));
    if (text == NULL)
    {
        report_message("ERREUR SYSTEME : Échec d'allocation mémoire pour 'text'.\n",
                       "SYSTEM ERROR : Memory allocation failed for 'text'.\n");
        return 1;
    }
    
    text[0] = malloc(sizeof(char) * size_c);
    if (text[0] == NULL)
    {
        report_message("ERREUR SYSTEME : Échec d'allocation mémoire pour le premier élément de 'text'.\n",
                       "SYSTEM ERROR : Memory allocation failed for first item of 'text'.\n");
        free(text);
        return 1;
    }

    int index_s = 0;
    int index_c = 0;
    int c;
    int last_c = 0;

    while ((c = fgetc(file)) != EOF)
    {
        last_c = c;
        if (c != ' ' && c != '\n' && c != '\t')
        {
            if (index_c >= size_c - 1)
            {
                size_c += 100;
                char *tmp = realloc(text[index_s], sizeof(char) * size_c); 
                if (tmp == NULL)
                {
                    report_message("ERREUR SYSTEME : Échec de réallocation mémoire pour un mot.\n",
                                   "SYSTEM ERROR : Memory reallocation failed for word.\n");
                    goto error_cleanup;
                }
                text[index_s] = tmp;
            }
            text[index_s][index_c++] = (char)c;
        }
        else
        {
            if (index_c > 0)
            {
                text[index_s][index_c] = '\0';
                index_s++;

                if (index_s >= size_s - 2)
                {
                    int old_size = size_s;
                    size_s += 100;
                    char **tmp = realloc(text, sizeof(char *) * size_s);
                    if (tmp == NULL)
                    {
                        report_message("ERREUR SYSTEME : Échec de réallocation mémoire pour le tableau de mots.\n",
                                       "SYSTEM ERROR : Memory reallocation failed for word list.\n");
                        goto error_cleanup;
                    }
                    text = tmp;
                    for (int j = old_size; j < size_s; j++)
                    {
                        text[j] = NULL;
                    }
                }
                
                text[index_s] = malloc(sizeof(char) * 2);
                if (text[index_s] == NULL)
                {
                    report_message("ERREUR SYSTEME : Échec d'allocation mémoire pour un séparateur.\n",
                                   "SYSTEM ERROR : Memory allocation failed for separator.\n");
                    goto error_cleanup;
                }
            }

            text[index_s][0] = (char)c;
            text[index_s][1] = '\0';

            index_s++;
            if (index_s >= size_s - 1)
            {
                int old_size = size_s;
                size_s += 100;
                char **tmp = realloc(text, sizeof(char *) * size_s);
                if (tmp == NULL)
                {
                    report_message("ERREUR SYSTEME : Échec de réallocation mémoire pour le tableau de mots.\n",
                                   "SYSTEM ERROR : Memory reallocation failed for word list.\n");
                    goto error_cleanup;
                }
                text = tmp;
                for (int j = old_size; j < size_s; j++)
                {
                    text[j] = NULL;
                }
            }

            index_c = 0;
            size_c = 100;
            text[index_s] = malloc(sizeof(char) * size_c);
            if (text[index_s] == NULL)
            {
                report_message("ERREUR SYSTEME : Échec d'allocation mémoire pour un nouveau mot.\n",
                               "SYSTEM ERROR : Memory allocation failed for new word.\n");
                goto error_cleanup;
            }
        }
    }

    if (index_c > 0)
    {
        text[index_s][index_c] = '\0';
        index_s++;
    }
    else 
    {
        free(text[index_s]);
        text[index_s] = NULL;
    }

    if (last_c != 0 && last_c != '\n')
    {
        if (index_s >= size_s - 1)
        {
            int old_size = size_s;
            size_s += 100;
            char **tmp = realloc(text, sizeof(char *) * size_s);
            if (tmp == NULL)
            {
                report_message("ERREUR SYSTEME : Échec de réallocation mémoire pour le tableau de mots.\n",
                               "SYSTEM ERROR : Memory reallocation failed for word list.\n");
                goto error_cleanup;
            }
            text = tmp;
            for (int j = old_size; j < size_s; j++)
            {
                text[j] = NULL;
            }
        }

        text[index_s] = malloc(sizeof(char) * 2);
        if (text[index_s] == NULL)
        {
            report_message("ERREUR SYSTEME : Échec d'allocation mémoire pour un séparateur final.\n",
                           "SYSTEM ERROR : Memory allocation failed for final separator.\n");
            goto error_cleanup;
        }
        text[index_s][0] = '\n';
        text[index_s][1] = '\0';
        index_s++;
    }

    text[index_s] = NULL;
    *text_ptr = text;
    return 0;

error_cleanup:
    for (int i = 0; i <= index_s; i++)
    {
        free(text[i]);
    }
    free(text);
    return 1;
}
