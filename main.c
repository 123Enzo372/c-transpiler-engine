#include "main.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int flag_french = 0;

int main(int argc, char *argv[]) 
{
    /* Première passe pour détecter le flag -french préventivement */
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-french") == 0)
        {
            flag_french = 1;
            break;
        }
    }

    if (argc < 2)
    {
        if (flag_french)
            fprintf(stderr, "ERREUR : Nom de fichier ou argument manquant.\n");
        else
            fprintf(stderr, "ERROR : Missing file name or argument.\n");
        return 1;
    }
    
    int index_file = 0;    
    int size_file = 10;
    char **filename = malloc(sizeof(char *) * size_file);
    if (filename == NULL)
    {
        if (flag_french)
            fprintf(stderr, "ERREUR SYSTEME : Échec d'allocation mémoire pour 'filename'.\n");
        else
            fprintf(stderr, "SYSTEM ERROR : Memory allocation failed for 'filename'.\n");
        return 1;
    }
    
    int index_option = 0;
    int size_option = 20;
    char **options = malloc(sizeof(char *) * size_option);
    if (options == NULL)
    {
        if (flag_french)
            fprintf(stderr, "ERREUR SYSTEME : Échec d'allocation mémoire pour 'options'.\n");
        else
            fprintf(stderr, "SYSTEM ERROR : Memory allocation failed for 'options'.\n");
        free(filename);
        return 1;
    }

    int index_new_option = 0;
    int size_new_option = 10;
    char **new_option = malloc(sizeof(char *) * size_new_option);
    if (new_option == NULL)
    {
        if (flag_french)
            fprintf(stderr, "ERREUR SYSTEME : Échec d'allocation mémoire pour 'new_option'.\n");
        else
            fprintf(stderr, "SYSTEM ERROR : Memory allocation failed for 'new_option'.\n");
        free(filename);
        free(options);
        return 1;
    }

    char *output = NULL;

    for (int i = 1; i < argc; i++)
    {
        size_t len = strlen(argv[i]);
        
        if (len >= 2 && (strcmp(argv[i] + len - 2, ".l") == 0 || strcmp(argv[i] + len - 2, ".H") == 0))
        {
            if (index_file >= size_file - 1)
            {
                size_file += 10;
                char **tmp = realloc(filename, sizeof(char *) * size_file);  
                if (tmp == NULL)
                {
                    if (flag_french)
                        fprintf(stderr, "ERREUR SYSTEME : Échec de réallocation mémoire pour 'filename'.\n");
                    else
                        fprintf(stderr, "SYSTEM ERROR : Memory reallocation failed for 'filename'.\n");
                    goto cleanup_and_exit;
                }
                filename = tmp;
            }
            filename[index_file++] = argv[i];
        }
        else if (argv[i][0] == '-') 
        {
            if (strcmp(argv[i], "-o") == 0)
            {
                if (i + 1 < argc)
                {
                    output = argv[i + 1];
                    i++;
                }
                else
                {
                    if (flag_french)
                        fprintf(stderr, "ERREUR : L'option '-o' nécessite un nom de fichier cible.\n");
                    else
                        fprintf(stderr, "ERROR : Option '-o' requires a target filename.\n");
                    goto cleanup_and_exit;
                }
            }
            else if (strcmp(argv[i], "-rm_H") == 0 || strcmp(argv[i], "-rm_l") == 0 ||
                     strcmp(argv[i], "-keep_c") == 0 || strcmp(argv[i], "-keep_h") == 0 ||
                     strcmp(argv[i], "-without-binary") == 0 || strcmp(argv[i], "-french") == 0)
            {
                if (index_new_option >= size_new_option - 1)
                {
                    size_new_option += 10;
                    char **tmp = realloc(new_option, sizeof(char *) * size_new_option);
                    if (tmp == NULL)
                    {
                        if (flag_french)
                            fprintf(stderr, "ERREUR SYSTEME : Échec de réallocation mémoire pour 'new_option'.\n");
                        else
                            fprintf(stderr, "SYSTEM ERROR : Memory reallocation failed for 'new_option'.\n");
                        goto cleanup_and_exit;
                    }
                    new_option = tmp;
                }
                new_option[index_new_option++] = argv[i];
            }
            else 
            {
                if (index_option >= size_option - 1)
                {
                    size_option += 10;
                    char **tmp = realloc(options, sizeof(char *) * size_option);  
                    if (tmp == NULL)
                    {
                        if (flag_french)
                            fprintf(stderr, "ERREUR SYSTEME : Échec de réallocation mémoire pour 'options'.\n");
                        else
                            fprintf(stderr, "SYSTEM ERROR : Memory reallocation failed for 'options'.\n");
                        goto cleanup_and_exit;
                    }
                    options = tmp;
                }
                options[index_option++] = argv[i];
            }
        }
        else if (len >= 2 && (strcmp(argv[i] + len - 2, ".c") == 0 || strcmp(argv[i] + len - 2, ".h") == 0))
        {
            if (index_option >= size_option - 1)
            {
                size_option += 10;
                char **tmp = realloc(options, sizeof(char *) * size_option);  
                if (tmp == NULL)
                {
                    if (flag_french)
                        fprintf(stderr, "ERREUR SYSTEME : Échec de réallocation mémoire pour 'options'.\n");
                    else
                        fprintf(stderr, "SYSTEM ERROR : Memory reallocation failed for 'options'.\n");
                    goto cleanup_and_exit;
                }
                options = tmp;
            }
            options[index_option++] = argv[i];
        }
        else 
        {
            if (flag_french)
                fprintf(stderr, "ERREUR : L'argument '%s' n'est pas reconnu.\n", argv[i]);
            else
                fprintf(stderr, "ERROR : Unrecognized argument '%s'.\n", argv[i]);
            goto cleanup_and_exit;
        }
    }

    filename[index_file] = NULL;
    options[index_option] = NULL;
    new_option[index_new_option] = NULL;

    if (index_file == 0)
    {
        if (flag_french)
            fprintf(stderr, "ERREUR : Aucun fichier source (extension .l ou .H) n'a été fourni.\n");
        else
            fprintf(stderr, "ERROR : No source file (.l or .H extension) was provided.\n");
        goto cleanup_and_exit;
    }

    char **created_filenames = malloc(sizeof(char *) * (index_file + 1));
    if (created_filenames == NULL)
    {
        if (flag_french)
            fprintf(stderr, "ERREUR SYSTEME : Échec d'allocation mémoire pour 'created_filenames'.\n");
        else
            fprintf(stderr, "SYSTEM ERROR : Memory allocation failed for 'created_filenames'.\n");
        goto cleanup_and_exit;
    }
    for (int k = 0; k <= index_file; k++) 
    {
        created_filenames[k] = NULL;
    }

    int status = 0;
    int created_count = 0;

    for (int i = 0; filename[i] != NULL; i++)
    {
        FILE *file = fopen(filename[i], "r");
        if (file == NULL)
        {
            if (flag_french)
                fprintf(stderr, "ERREUR FICHIER : Impossible d'ouvrir le fichier '%s'.\n", filename[i]);
            else
                fprintf(stderr, "FILE ERROR : Cannot open file '%s'.\n", filename[i]);
            status = 1;
            break;
        }
        
        char **text_ptr = NULL;
        status = parser(file, &text_ptr);
        fclose(file);

        if (status != 0)
        {
            if (flag_french)
                fprintf(stderr, "ERREUR : Échec de l'analyse syntaxique (parsing) du fichier '%s'.\n", filename[i]);
            else
                fprintf(stderr, "ERROR : Parsing failed for file '%s'.\n", filename[i]);
            break;
        }

        if (text_ptr != NULL) 
        {
            size_t len = strlen(filename[i]);
            
            if (len >= 2 && strcmp(filename[i] + len - 2, ".H") == 0)
            {
                status = trad_h(filename[i], &text_ptr);
            }
            else if (len >= 2 && strcmp(filename[i] + len - 2, ".l") == 0)
            {
                status = trad_c(filename[i], &text_ptr);
            }

            for (int j = 0; text_ptr[j] != NULL; j++) 
            {
                free(text_ptr[j]);
            }
            free(text_ptr);

            if (status != 0)
            {
                if (flag_french)
                    fprintf(stderr, "ERREUR : Échec lors de la traduction du fichier '%s'.\n", filename[i]);
                else
                    fprintf(stderr, "ERROR : Translation failed for file '%s'.\n", filename[i]);
                break;
            }

            char *gen_name = strdup(filename[i]);
            if (gen_name == NULL)
            {
                if (flag_french)
                    fprintf(stderr, "ERREUR SYSTEME : Échec d'allocation mémoire.\n");
                else
                    fprintf(stderr, "SYSTEM ERROR : Memory allocation failed.\n");
                status = 1;
                break;
            }
            if (len >= 2)
            {
                if (strcmp(filename[i] + len - 2, ".l") == 0)
                    gen_name[len - 1] = 'c';
                else if (strcmp(filename[i] + len - 2, ".H") == 0)
                    gen_name[len - 1] = 'h';
            }
            created_filenames[created_count++] = gen_name;
        }
    }

    if (status != 0)
    {
        for (int i = 0; created_filenames[i] != NULL; i++)
        {
            remove(created_filenames[i]);
            free(created_filenames[i]);
        }
        free(created_filenames);
        goto cleanup_and_exit;
    }

    int without_binary = 0;
    for (int i = 0; new_option[i] != NULL; i++)
    {
        if (strcmp(new_option[i], "-without-binary") == 0)
        {
            without_binary = 1;
            break;
        }
    }

    int res = 0;
    if (!without_binary)
    {
        res = execute(created_filenames, output, options);
    }

    if (res != 0)
    {
        for (int i = 0; created_filenames[i] != NULL; i++)
        {
            remove(created_filenames[i]);
            free(created_filenames[i]);
        }
        free(created_filenames);
        goto cleanup_and_exit;
    }

    remove_created_files(created_filenames, new_option);

    for (int i = 0; new_option[i] != NULL; i++)
    {
        if (strcmp(new_option[i], "-rm_H") == 0)
        {
            remove_H_files(filename);
        }
        else if (strcmp(new_option[i], "-rm_l") == 0)
        {
            remove_l_files(filename);
        }
    }

    for (int i = 0; created_filenames[i] != NULL; i++)
    {
        free(created_filenames[i]);
    }
    free(created_filenames);
    free(filename);
    free(options);
    free(new_option);

    return res;

cleanup_and_exit:
    free(filename);
    free(options);
    free(new_option);
    return 1;
}
