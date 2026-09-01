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

static void trim_leading_whitespace(char *str)
{
    char *start = str;

    while (*start == ' ' || *start == '\t')
        start++;

    if (start != str)
        memmove(str, start, strlen(start) + 1);
}

static const char *header_diagnostic_filename = NULL;
static int header_diagnostic_line_num = 0;
static char header_diagnostic_source_line[512] = {0};

static void set_header_diagnostic_source(const char *filename, int line_num, const char *line)
{
    header_diagnostic_filename = filename;
    header_diagnostic_line_num = line_num;
    if (line != NULL)
        safe_copy(header_diagnostic_source_line, sizeof(header_diagnostic_source_line), line);
    else
        header_diagnostic_source_line[0] = '\0';
}

static int header_diagnostic_column_for(const char *needle)
{
    const char *line = header_diagnostic_source_line;
    const char *found = NULL;

    if (needle != NULL && needle[0] != '\0')
        found = strstr(line, needle);
    if (found != NULL)
        return (int)(found - line) + 1;

    int column = 1;
    while (*line == ' ' || *line == '\t')
    {
        column++;
        line++;
    }
    return column;
}

static void report_header_hint(const char *needle, const char *fr_suggestion, const char *en_suggestion)
{
    const char *suggestion = flag_french ? fr_suggestion : en_suggestion;
    int column = header_diagnostic_column_for(needle);

    fprintf(stderr, "  --> %s:%d\n",
            header_diagnostic_filename != NULL ? header_diagnostic_filename : "<header>",
            header_diagnostic_line_num);
    if (header_diagnostic_source_line[0] != '\0')
    {
        fprintf(stderr, "   |\n");
        fprintf(stderr, "%4d | %s\n", header_diagnostic_line_num, header_diagnostic_source_line);
        fprintf(stderr, "   | ");
        for (int i = 1; i < column; i++)
            fputc(header_diagnostic_source_line[i - 1] == '\t' ? '\t' : ' ', stderr);
        fprintf(stderr, "^\n");
    }
    if (suggestion != NULL && suggestion[0] != '\0')
    {
        fprintf(stderr, flag_french ? "Suggestion : %s\n" : "Suggestion: %s\n", suggestion);
    }
}

static int parse_header_import_line(const char *line, char *out_include, size_t out_size,
                                    const char *filename, int line_num)
{
    char target[256] = {0};
    char include_path[256] = {0};
    size_t len;

    if (strncmp(line, "import ", 7) != 0)
        return 0;

    if (!safe_copy(target, sizeof(target), line + 7))
    {
        report_message("ERREUR HEADER [%s:%d] : Import trop long.\n",
                       "HEADER ERROR [%s:%d] : Import target is too long.\n", filename, line_num);
        report_header_hint("import",
                           "Utilise un nom de fichier d'import plus court.",
                           "Use a shorter import file name.");
        return -1;
    }
    trim_trailing_whitespace(target);
    trim_leading_whitespace(target);

    len = strlen(target);
    if (len == 0)
    {
        report_message("ERREUR HEADER [%s:%d] : Import vide.\n",
                       "HEADER ERROR [%s:%d] : Empty import.\n", filename, line_num);
        report_header_hint("import",
                           "Écris par exemple import tools, import \"tools.H\" ou import <stddef.h>.",
                           "Write for example import tools, import \"tools.H\", or import <stddef.h>.");
        return -1;
    }

    if (len >= 2 && target[0] == '"' && target[len - 1] == '"')
    {
        memmove(target, target + 1, len - 2);
        target[len - 2] = '\0';
    }

    len = strlen(target);
    if (len == 0)
    {
        report_message("ERREUR HEADER [%s:%d] : Import vide.\n",
                       "HEADER ERROR [%s:%d] : Empty import.\n", filename, line_num);
        report_header_hint("import",
                           "Écris par exemple import tools, import \"tools.H\" ou import <stddef.h>.",
                           "Write for example import tools, import \"tools.H\", or import <stddef.h>.");
        return -1;
    }

    if (target[0] == '<')
    {
        if (len < 2 || target[len - 1] != '>')
        {
            report_message("ERREUR HEADER [%s:%d] : Import système mal formé.\n",
                           "HEADER ERROR [%s:%d] : Malformed system import.\n", filename, line_num);
            report_header_hint("import",
                               "Un import système doit avoir la forme import <nom.h>.",
                               "A system import must use the form import <name.h>.");
            return -1;
        }
    }
    else if (len > 2 && strcmp(target + len - 2, ".H") == 0)
    {
        target[len - 1] = 'h';
    }
    else if (strchr(target, '.') == NULL)
    {
        if (snprintf(include_path, sizeof(include_path), "%s.h", target) >= (int)sizeof(include_path))
        {
            report_message("ERREUR HEADER [%s:%d] : Import trop long.\n",
                           "HEADER ERROR [%s:%d] : Import target is too long.\n", filename, line_num);
            report_header_hint("import",
                               "Utilise un nom d'import plus court ou écris le #include C directement.",
                               "Use a shorter import name or write the C #include directly.");
            return -1;
        }
        safe_copy(target, sizeof(target), include_path);
    }

    if (target[0] == '<')
    {
        if (snprintf(out_include, out_size, "#include %s", target) >= (int)out_size)
        {
            report_header_hint("import",
                               "Raccourcis cet import système.",
                               "Shorten this system import.");
            return -1;
        }
    }
    else if (snprintf(out_include, out_size, "#include \"%s\"", target) >= (int)out_size)
    {
        report_header_hint("import",
                           "Raccourcis cet import local.",
                           "Shorten this local import.");
        return -1;
    }

    return 1;
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
    set_header_diagnostic_source(filename, 0, NULL);
    for (int i = 0; text[i] != NULL; i++)
    {
        char directive[256] = {0};
        int line_num = i + 1;
        if (!safe_copy(directive, sizeof(directive), text[i]))
        {
            set_header_diagnostic_source(filename, line_num, NULL);
            report_message("ERREUR HEADER [%s:%d] : Ligne source trop longue.\n",
                           "HEADER ERROR [%s:%d] : Source line is too long.\n", filename, line_num);
            report_header_hint(NULL,
                               "Raccourcis cette ligne de header.",
                               "Shorten this header line.");
            fclose(file);
            free(guard_name);
            free(new_name);
            return 1;
        }

        trim_trailing_whitespace(directive);
        set_header_diagnostic_source(filename, line_num, directive);

        char include_line[512] = {0};
        int import_status = parse_header_import_line(directive, include_line, sizeof(include_line), filename, line_num);
        if (import_status < 0)
        {
            fclose(file);
            free(guard_name);
            free(new_name);
            return 1;
        }

        if (import_status == 1)
        {
            fprintf(file, "%s\n", include_line);
        }
        else if (strcmp(directive, "#all") == 0)
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
