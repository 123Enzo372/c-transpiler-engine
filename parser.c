#include "main.h"
#include <stdio.h>
#include <stdlib.h>


int parser(FILE *file, char ***text_ptr)
{
    if (file == NULL || text_ptr == NULL) 
    {
        if (flag_french)
            fprintf(stderr, "ERREUR : Paramètres invalides fournis au parser.\n");
        else
            fprintf(stderr, "ERROR : Invalid parameters provided to parser.\n");
        return 1;
    }

    int size_c = 100;
    int size_s = 100;

    char **text = malloc(sizeof(char *) * size_s);    
    if (text == NULL)
    {
        if (flag_french)
            fprintf(stderr, "ERREUR SYSTEME : Échec d'allocation mémoire pour 'text'.\n");
        else
            fprintf(stderr, "SYSTEM ERROR : Memory allocation failed for 'text'.\n");
        return 1;
    }
    
    text[0] = malloc(sizeof(char) * size_c);
    if (text[0] == NULL)
    {
        if (flag_french)
            fprintf(stderr, "ERREUR SYSTEME : Échec d'allocation mémoire pour le premier élément de 'text'.\n");
        else
            fprintf(stderr, "SYSTEM ERROR : Memory allocation failed for first item of 'text'.\n");
        free(text);
        return 1;
    }

    int index_s = 0;
    int index_c = 0;
    int c;

    while ((c = fgetc(file)) != EOF)
    {
        if (c != ' ' && c != '\n' && c != '\t')
        {
            if (index_c >= size_c - 1)
            {
                size_c += 100;
                char *tmp = realloc(text[index_s], sizeof(char) * size_c); 
                if (tmp == NULL)
                {
                    if (flag_french)
                        fprintf(stderr, "ERREUR SYSTEME : Échec de réallocation mémoire pour un mot.\n");
                    else
                        fprintf(stderr, "SYSTEM ERROR : Memory reallocation failed for word.\n");
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
                    size_s += 100;
                    char **tmp = realloc(text, sizeof(char *) * size_s);
                    if (tmp == NULL)
                    {
                        if (flag_french)
                            fprintf(stderr, "ERREUR SYSTEME : Échec de réallocation mémoire pour le tableau de mots.\n");
                        else
                            fprintf(stderr, "SYSTEM ERROR : Memory reallocation failed for word list.\n");
                        goto error_cleanup;
                    }
                    text = tmp;
                }
                
                text[index_s] = malloc(sizeof(char) * 2);
                if (text[index_s] == NULL)
                {
                    if (flag_french)
                        fprintf(stderr, "ERREUR SYSTEME : Échec d'allocation mémoire pour un séparateur.\n");
                    else
                        fprintf(stderr, "SYSTEM ERROR : Memory allocation failed for separator.\n");
                    goto error_cleanup;
                }
            }

            text[index_s][0] = (char)c;
            text[index_s][1] = '\0';

            index_s++;
            if (index_s >= size_s - 1)
            {
                size_s += 100;
                char **tmp = realloc(text, sizeof(char *) * size_s);
                if (tmp == NULL)
                {
                    if (flag_french)
                        fprintf(stderr, "ERREUR SYSTEME : Échec de réallocation mémoire pour le tableau de mots.\n");
                    else
                        fprintf(stderr, "SYSTEM ERROR : Memory reallocation failed for word list.\n");
                    goto error_cleanup;
                }
                text = tmp;
            }

            index_c = 0;
            size_c = 100;
            text[index_s] = malloc(sizeof(char) * size_c);
            if (text[index_s] == NULL)
            {
                if (flag_french)
                    fprintf(stderr, "ERREUR SYSTEME : Échec d'allocation mémoire pour un nouveau mot.\n");
                else
                    fprintf(stderr, "SYSTEM ERROR : Memory allocation failed for new word.\n");
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
