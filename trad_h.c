#include "main.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>


static void trim_trailing_whitespace(char *str)
{
    size_t len = strlen(str);

    while (len > 0 && (str[len - 1] == ' ' || str[len - 1] == '\t' ||
                       str[len - 1] == '\r' || str[len - 1] == '\n'))
    {
        str[--len] = '\0';
    }
}

int trad_h(char *filename, char ***text_ptr)
{
    if (filename == NULL || text_ptr == NULL || *text_ptr == NULL)
    {
        report_message("ERREUR : Paramètres invalides fournis à trad_h.\n",
                       "ERROR : Invalid parameters provided to trad_h.\n");
        return 1;
    }

    size_t len = strlen(filename);
    char *new_name = duplicate_string(filename);
    if (new_name == NULL)
    {
        report_message("ERREUR SYSTEME : Échec d'allocation mémoire pour 'new_name'.\n",
                       "SYSTEM ERROR : Memory allocation failed for 'new_name'.\n");
        return 1;
    }
    
    if (len >= 2 && strcmp(filename + len - 2, ".H") == 0)
    {
        new_name[len - 1] = 'h';
    }

    FILE *file = fopen(new_name, "w");
    if (file == NULL)
    {
        report_message("ERREUR FICHIER : Impossible de créer ou d'ouvrir le fichier cible '%s'.\n",
                       "FILE ERROR : Cannot create or open target file '%s'.\n", new_name);
        free(new_name);
        return 1;
    }

    char *basename = strrchr(filename, '/');
    char *windows_basename = strrchr(filename, '\\');
    if (windows_basename != NULL && (basename == NULL || windows_basename > basename))
    {
        basename = windows_basename;
    }
    if (basename == NULL)
    {
        basename = filename;
    }
    else
    {
        basename++;
    }

    size_t base_len = strlen(basename);
    /* Allocation de base_len + 3 : pour le nom tronqué + "_H" + '\0' */
    char *guard_name = malloc(base_len + 3);
    if (guard_name == NULL)
    {
        report_message("ERREUR SYSTEME : Échec d'allocation mémoire pour 'guard_name'.\n",
                       "SYSTEM ERROR : Memory allocation failed for 'guard_name'.\n");
        fclose(file);
        free(new_name);
        return 1;
    }

    size_t guard_idx = 0;
    for (size_t i = 0; i < base_len; i++)
    {
        if (basename[i] == '.')
        {
            break; // S'arrête avant l'extension
        }
        if (isalnum((unsigned char)basename[i]))
        {
            guard_name[guard_idx++] = (char)toupper((unsigned char)basename[i]);
        }
        else
        {
            guard_name[guard_idx++] = '_';
        }
    }
    guard_name[guard_idx++] = '_';
    guard_name[guard_idx++] = 'H';
    guard_name[guard_idx] = '\0';

    fprintf(file, "#ifndef %s\n#define %s\n\n", guard_name, guard_name);

    char **text = *text_ptr;
    for (int i = 0; text[i] != NULL; i++)
    {
        char directive[256] = {0};
        if (safe_copy(directive, sizeof(directive), text[i]))
        {
            trim_trailing_whitespace(directive);
        }

        if (strcmp(directive, "#all") == 0)
        {
            fprintf(file, "#include <stdio.h>\n"
                          "#include <stdlib.h>\n"
                          "#include <string.h>\n"
                          "#include <stdbool.h>\n"
                          "#include <math.h>\n"
                          "#include <time.h>\n");
        }
        else if (strcmp(directive, "#linux") == 0)
        {
            fprintf(file, "#include <unistd.h>\n"
                          "#include <sys/types.h>\n"
                          "#include <sys/wait.h>\n"
                          "#include <fcntl.h>\n");
        }
        else if (strcmp(directive, "#windows") == 0)
        {
            fprintf(file, "#include <windows.h>\n");
        }
        else if (strcmp(directive, "#mac") == 0)
        {
            fprintf(file, "#include <TargetConditionals.h>\n"
                          "#include <Availability.h>\n");
        }
        else
        {
            fprintf(file, "%s", text[i]);
        }
    }

    fprintf(file, "\n#endif /* %s */\n", guard_name);

    fclose(file);
    free(guard_name);
    free(new_name);

    return 0;
}
