#include "main.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int flag_french = 0;

static void print_help(void)
{
    if (flag_french)
    {
        printf("Usage : ./compilateur [sources] [options-gcc] [options] [-o sortie]\n\n"
               "Sources :\n"
               "  fichier.l              Traduit un fichier source en C.\n"
               "  fichier.H              Traduit un header avec garde d'inclusion.\n"
               "  fichier.c / fichier.h  Transmis directement à gcc.\n\n"
               "Options :\n"
               "  -o <nom>          Nom du binaire final.\n"
               "  -without-binary   Traduit sans lancer gcc.\n"
               "  -keep_c           Conserve les fichiers .c générés.\n"
               "  -keep_h           Conserve les fichiers .h générés.\n"
               "  -rm_l             Supprime les sources .l après succès.\n"
               "  -rm_H             Supprime les sources .H après succès.\n"
               "  -french           Affiche les diagnostics en français.\n"
               "  -h, --help        Affiche cette aide.\n\n"
               "Exemples :\n"
               "  ./compilateur main.l -o app\n"
               "  ./compilateur main.l api.H -Wall -Wextra -o app\n"
               "  ./compilateur main.l -without-binary -keep_c\n");
    }
    else
    {
        printf("Usage: ./compilateur [sources] [gcc-options] [options] [-o output]\n\n"
               "Sources:\n"
               "  file.l              Translate a source file to C.\n"
               "  file.H              Translate a header with an include guard.\n"
               "  file.c / file.h     Passed directly to gcc.\n\n"
               "Options:\n"
               "  -o <name>         Final executable name.\n"
               "  -without-binary   Translate without running gcc.\n"
               "  -keep_c           Keep generated .c files.\n"
               "  -keep_h           Keep generated .h files.\n"
               "  -rm_l             Delete .l sources after success.\n"
               "  -rm_H             Delete .H sources after success.\n"
               "  -french           Print diagnostics in French.\n"
               "  -h, --help        Show this help message.\n\n"
               "Examples:\n"
               "  ./compilateur main.l -o app\n"
               "  ./compilateur main.l api.H -Wall -Wextra -o app\n"
               "  ./compilateur main.l -without-binary -keep_c\n");
    }
}

static int append_argument(char ***items, int *count, int *capacity, char *value, const char *name)
{
    if (*count >= *capacity - 1)
    {
        int new_capacity = *capacity + 10;
        char **tmp = realloc(*items, sizeof(char *) * new_capacity);
        if (tmp == NULL)
        {
            report_message("ERREUR SYSTEME : Échec de réallocation mémoire pour '%s'.\n",
                           "SYSTEM ERROR : Memory reallocation failed for '%s'.\n", name);
            return 0;
        }
        *items = tmp;
        *capacity = new_capacity;
    }

    (*items)[(*count)++] = value;
    (*items)[*count] = NULL;
    return 1;
}

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

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0)
        {
            print_help();
            return 0;
        }
    }

    if (argc < 2)
    {
        report_message("ERREUR : Nom de fichier ou argument manquant.\n",
                       "ERROR : Missing file name or argument.\n");
        return 1;
    }
    
    int index_file = 0;    
    int size_file = 10;
    char **filename = calloc(size_file, sizeof(char *));
    if (filename == NULL)
    {
        report_message("ERREUR SYSTEME : Échec d'allocation mémoire pour 'filename'.\n",
                       "SYSTEM ERROR : Memory allocation failed for 'filename'.\n");
        return 1;
    }
    
    int index_option = 0;
    int size_option = 20;
    char **options = calloc(size_option, sizeof(char *));
    if (options == NULL)
    {
        report_message("ERREUR SYSTEME : Échec d'allocation mémoire pour 'options'.\n",
                       "SYSTEM ERROR : Memory allocation failed for 'options'.\n");
        free(filename);
        return 1;
    }

    int index_new_option = 0;
    int size_new_option = 10;
    char **new_option = calloc(size_new_option, sizeof(char *));
    if (new_option == NULL)
    {
        report_message("ERREUR SYSTEME : Échec d'allocation mémoire pour 'new_option'.\n",
                       "SYSTEM ERROR : Memory allocation failed for 'new_option'.\n");
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
            if (!append_argument(&filename, &index_file, &size_file, argv[i], "filename"))
                goto cleanup_and_exit;
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
                    report_message("ERREUR : L'option '-o' nécessite un nom de fichier cible.\n",
                                   "ERROR : Option '-o' requires a target filename.\n");
                    goto cleanup_and_exit;
                }
            }
            else if (strcmp(argv[i], "-rm_H") == 0 || strcmp(argv[i], "-rm_l") == 0 ||
                     strcmp(argv[i], "-keep_c") == 0 || strcmp(argv[i], "-keep_h") == 0 ||
                     strcmp(argv[i], "-without-binary") == 0 || strcmp(argv[i], "-french") == 0)
            {
                if (!append_argument(&new_option, &index_new_option, &size_new_option, argv[i], "new_option"))
                    goto cleanup_and_exit;
            }
            else 
            {
                if (!append_argument(&options, &index_option, &size_option, argv[i], "options"))
                    goto cleanup_and_exit;
            }
        }
        else if (len >= 2 && (strcmp(argv[i] + len - 2, ".c") == 0 || strcmp(argv[i] + len - 2, ".h") == 0))
        {
            if (!append_argument(&options, &index_option, &size_option, argv[i], "options"))
                goto cleanup_and_exit;
        }
        else 
        {
            report_message("ERREUR : L'argument '%s' n'est pas reconnu.\n",
                           "ERROR : Unrecognized argument '%s'.\n", argv[i]);
            goto cleanup_and_exit;
        }
    }

    filename[index_file] = NULL;
    options[index_option] = NULL;
    new_option[index_new_option] = NULL;

    if (index_file == 0)
    {
        report_message("ERREUR : Aucun fichier source (extension .l ou .H) n'a été fourni.\n",
                       "ERROR : No source file (.l or .H extension) was provided.\n");
        goto cleanup_and_exit;
    }

    char **created_filenames = calloc(index_file + 1, sizeof(char *));
    if (created_filenames == NULL)
    {
        report_message("ERREUR SYSTEME : Échec d'allocation mémoire pour 'created_filenames'.\n",
                       "SYSTEM ERROR : Memory allocation failed for 'created_filenames'.\n");
        goto cleanup_and_exit;
    }
    int status = 0;
    int created_count = 0;

    for (int i = 0; filename[i] != NULL; i++)
    {
        FILE *file = fopen(filename[i], "r");
        if (file == NULL)
        {
            report_message("ERREUR FICHIER : Impossible d'ouvrir le fichier '%s'.\n",
                           "FILE ERROR : Cannot open file '%s'.\n", filename[i]);
            status = 1;
            break;
        }
        
        char **text_ptr = NULL;
        status = parser(file, &text_ptr);
        fclose(file);

        if (status != 0)
        {
            report_message("ERREUR : Échec de l'analyse syntaxique (parsing) du fichier '%s'.\n",
                           "ERROR : Parsing failed for file '%s'.\n", filename[i]);
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
                report_message("ERREUR : Échec lors de la traduction du fichier '%s'.\n",
                               "ERROR : Translation failed for file '%s'.\n", filename[i]);
                break;
            }

            char *gen_name = duplicate_string(filename[i]);
            if (gen_name == NULL)
            {
                report_message("ERREUR SYSTEME : Échec d'allocation mémoire.\n",
                               "SYSTEM ERROR : Memory allocation failed.\n");
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
