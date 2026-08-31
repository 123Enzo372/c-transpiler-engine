#include "main.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#define MAX_INDENT_LEVELS 100
#define MAX_SYMBOLS 256
#define MAX_ALLOCATED 256

typedef struct {
    char name[64];
    char type[16];
    int scope_level;
} Symbol;

typedef struct {
    char name[64];
    int scope_level;
} AllocatedPtr;

static Symbol symbol_table[MAX_SYMBOLS];
static int symbol_count = 0;

static AllocatedPtr allocated_ptrs[MAX_ALLOCATED];
static int allocated_count = 0;

static char current_match_var[64] = {0};
static bool in_match_block = false;
static bool match_first_case = true;

static void trim_trailing_whitespace(char *str)
{
    int len = strlen(str);
    while (len > 0 && (str[len - 1] == ' ' || str[len - 1] == '\t' || str[len - 1] == '\r' || str[len - 1] == '\n'))
    {
        str[--len] = '\0';
    }
}

static void trim_leading_whitespace(char *str)
{
    char *p = str;
    while (*p == ' ' || *p == '\t') p++;
    if (p != str) memmove(str, p, strlen(p) + 1);
}

static bool append_checked(char *dest, size_t dest_size, const char *src)
{
    size_t dest_len;
    size_t src_len;

    if (dest == NULL || src == NULL || dest_size == 0)
        return false;

    dest_len = strlen(dest);
    src_len = strlen(src);
    if (src_len >= dest_size - dest_len)
        return false;

    memcpy(dest + dest_len, src, src_len + 1);
    return true;
}

static bool copy_slice(char *dest, size_t dest_size, const char *start, size_t len)
{
    if (dest == NULL || start == NULL || dest_size == 0 || len >= dest_size)
        return false;

    memcpy(dest, start, len);
    dest[len] = '\0';
    return true;
}

static bool is_valid_identifier(const char *str)
{
    if (!str || !*str) return false;
    if (!isalpha((unsigned char)*str) && *str != '_') return false;
    for (int i = 1; str[i] != '\0'; i++)
    {
        if (!isalnum((unsigned char)str[i]) && str[i] != '_') return false;
    }
    return true;
}

static bool add_symbol(const char *name, const char *type, int scope_level, int line_num)
{
    if (strlen(name) >= sizeof(symbol_table[0].name) || strlen(type) >= sizeof(symbol_table[0].type))
    {
        report_message("ERREUR [Ligne %d] : Nom de symbole ou type trop long ('%s', '%s').\n",
                       "ERROR [Line %d] : Symbol name or type is too long ('%s', '%s').\n",
                       line_num, name, type);
        return false;
    }

    for (int i = symbol_count - 1; i >= 0; i--)
    {
        if (strcmp(symbol_table[i].name, name) == 0 && symbol_table[i].scope_level == scope_level)
        {
            safe_copy(symbol_table[i].type, sizeof(symbol_table[i].type), type);
            return true;
        }
    }
    if (symbol_count >= MAX_SYMBOLS)
    {
        report_message("ERREUR [Ligne %d] : Limite maximale de la table des symboles atteinte (%d).\n",
                       "ERROR [Line %d] : Maximum symbol table capacity reached (%d).\n",
                       line_num, MAX_SYMBOLS);
        return false;
    }
    safe_copy(symbol_table[symbol_count].name, sizeof(symbol_table[symbol_count].name), name);
    safe_copy(symbol_table[symbol_count].type, sizeof(symbol_table[symbol_count].type), type);
    symbol_table[symbol_count].scope_level = scope_level;
    symbol_count++;
    return true;
}

static void pop_scope(int current_scope)
{
    int new_count = 0;
    for (int i = 0; i < symbol_count; i++)
    {
        if (symbol_table[i].scope_level <= current_scope)
        {
            symbol_table[new_count++] = symbol_table[i];
        }
    }
    symbol_count = new_count;
}

static const char *get_symbol_type(const char *name)
{
    for (int i = symbol_count - 1; i >= 0; i--)
    {
        if (strcmp(symbol_table[i].name, name) == 0)
        {
            return symbol_table[i].type;
        }
    }
    return NULL;
}

static bool add_allocated_ptr(const char *name, int scope_level, int line_num)
{
    if (strlen(name) >= sizeof(allocated_ptrs[0].name))
    {
        report_message("ERREUR [Ligne %d] : Nom de pointeur alloué trop long '%s'.\n",
                       "ERROR [Line %d] : Allocated pointer name is too long '%s'.\n",
                       line_num, name);
        return false;
    }

    for (int i = 0; i < allocated_count; i++)
    {
        if (strcmp(allocated_ptrs[i].name, name) == 0 && allocated_ptrs[i].scope_level == scope_level)
            return true;
    }
    if (allocated_count >= MAX_ALLOCATED)
    {
        report_message("ERREUR [Ligne %d] : Limite maximale de pointeurs alloués atteinte (%d).\n",
                       "ERROR [Line %d] : Maximum allocated pointers limit reached (%d).\n",
                       line_num, MAX_ALLOCATED);
        return false;
    }
    safe_copy(allocated_ptrs[allocated_count].name, sizeof(allocated_ptrs[0].name), name);
    allocated_ptrs[allocated_count].scope_level = scope_level;
    allocated_count++;
    return true;
}

static void generate_frees(FILE *file, int indent_level)
{
    for (int i = 0; i < allocated_count; i++)
    {
        for (int k = 0; k < indent_level; k++) fprintf(file, "    ");
        fprintf(file, "if (%s != NULL) { free(%s); %s = NULL; }\n",
                allocated_ptrs[i].name, allocated_ptrs[i].name, allocated_ptrs[i].name);
    }
}

static void generate_scope_frees(FILE *file, int scope_level)
{
    int new_count = 0;

    for (int i = 0; i < allocated_count; i++)
    {
        if (allocated_ptrs[i].scope_level == scope_level)
        {
            for (int k = 0; k < scope_level; k++) fprintf(file, "    ");
            fprintf(file, "if (%s != NULL) { free(%s); %s = NULL; }\n",
                    allocated_ptrs[i].name, allocated_ptrs[i].name, allocated_ptrs[i].name);
        }
        else
        {
            allocated_ptrs[new_count++] = allocated_ptrs[i];
        }
    }
    allocated_count = new_count;
}

static const char *infer_expression_type(const char *val)
{
    if (val[0] == '[' && strchr(val, '+') == NULL) return "list";
    if (strcmp(val, "true") == 0 || strcmp(val, "false") == 0) return "bool";
    if (val[0] == '\'') return "char";
    if (val[0] == '"') return "char*";

    char func_name[64] = {0};
    if (sscanf(val, "%63[a-zA-Z0-9_](", func_name) == 1)
    {
        const char *ret_type = get_symbol_type(func_name);
        if (ret_type != NULL) return ret_type;
        return "int";
    }

    bool has_float = false;
    if (strchr(val, '.') != NULL) has_float = true;

    char val_copy[256];
    strncpy(val_copy, val, sizeof(val_copy) - 1);
    val_copy[sizeof(val_copy) - 1] = '\0';
    char *token = strtok(val_copy, " +-*/()%");
    while (token != NULL)
    {
        const char *t_type = get_symbol_type(token);
        if (t_type != NULL && (strcmp(t_type, "float") == 0 || strcmp(t_type, "double") == 0))
        {
            has_float = true;
            break;
        }
        token = strtok(NULL, " +-*/()%");
    }

    return has_float ? "float" : "int";
}

static const char *get_elem_type(const char *list_name)
{
    const char *type = get_symbol_type(list_name);
    if (type != NULL)
    {
        if (strcmp(type, "int*") == 0) return "int";
        if (strcmp(type, "float*") == 0) return "float";
        if (strcmp(type, "char**") == 0) return "char*";
        if (strcmp(type, "bool*") == 0) return "bool";
    }
    return "int";
}

static const char *get_format_specifier(const char *var_name, int line_num)
{
    char list_name[64] = {0};
    char index_expr[64] = {0};
    const char *type = get_symbol_type(var_name);
    if (type != NULL)
    {
        if (strcmp(type, "int") == 0) return "%d";
        if (strcmp(type, "float") == 0) return "%f";
        if (strcmp(type, "char*") == 0) return "%s";
        if (strcmp(type, "char") == 0) return "%c";
        if (strcmp(type, "bool") == 0) return "%d";
    }
    if (sscanf(var_name, "%63[^[][%63[^]]]", list_name, index_expr) == 2)
    {
        (void)index_expr;
        type = get_elem_type(list_name);
        if (strcmp(type, "int") == 0) return "%d";
        if (strcmp(type, "float") == 0) return "%f";
        if (strcmp(type, "char*") == 0) return "%s";
        if (strcmp(type, "char") == 0) return "%c";
        if (strcmp(type, "bool") == 0) return "%d";
    }
    if (strstr(var_name, "+") || strstr(var_name, "-") || strstr(var_name, "*") || strstr(var_name, "/"))
    {
        return "%d";
    }
    if (is_valid_identifier(var_name) && type == NULL)
    {
        report_message("AVERTISSEMENT [Ligne %d] : Variable '%s' utilisée sans déclaration explicite prioritaire.\n",
                       "WARNING [Line %d] : Variable '%s' used without prior explicit declaration.\n",
                       line_num, var_name);
    }
    return "%s";
}

static bool is_c_type(const char *word)
{
    const char *types[] = {"int", "float", "double", "char", "void", "bool", "long", "short", "unsigned", "pid_t", NULL};
    for (int i = 0; types[i] != NULL; i++)
    {
        if (strcmp(word, types[i]) == 0) return true;
    }
    return false;
}

static bool is_c_declaration_lhs(const char *lhs)
{
    char type_name[16] = {0};
    char variable_name[64] = {0};

    return sscanf(lhs, "%15s %63s", type_name, variable_name) == 2 && is_c_type(type_name);
}

static int get_indentation_level(const char *line)
{
    int count = 0;
    while (*line != '\0')
    {
        if (*line == ' ') count++;
        else if (*line == '\t') count += 4;
        else break;
        line++;
    }
    return count;
}

static void print_indent(FILE *file, int indent_level)
{
    for (int i = 0; i < indent_level; i++)
    {
        fprintf(file, "    ");
    }
}

static void normalize_booleans(char *str)
{
    char buffer[2048] = {0};
    char *ptr = str;
    char *out = buffer;

    while (*ptr != '\0')
    {
        if (strncmp(ptr, "True", 4) == 0 && !isalnum((unsigned char)ptr[4]) && ptr[4] != '_')
        {
            strcpy(out, "true");
            out += 4;
            ptr += 4;
        }
        else if (strncmp(ptr, "False", 5) == 0 && !isalnum((unsigned char)ptr[5]) && ptr[5] != '_')
        {
            strcpy(out, "false");
            out += 5;
            ptr += 5;
        }
        else
        {
            *out++ = *ptr++;
        }
    }
    *out = '\0';
    strcpy(str, buffer);
}

static int parse_assignment(const char *line, char *var, size_t var_size, char *val, size_t val_size, int line_num)
{
    bool in_quotes = false;
    const char *eq = NULL;
    size_t var_len;
    size_t val_len;

    for (const char *p = line; *p != '\0'; p++)
    {
        if (*p == '"')
        {
            in_quotes = !in_quotes;
        }
        else if (*p == '=' && !in_quotes)
        {
            eq = p;
            break;
        }
    }

    if (!eq) return 0;

    if ((eq > line && *(eq - 1) == '!') || *(eq + 1) == '=' || (eq > line && *(eq - 1) == '=') ||
        (eq > line && *(eq - 1) == '<') || (eq > line && *(eq - 1) == '>'))
    {
        return 0;
    }

    char first_word[16] = {0};
    if (sscanf(line, "%15s", first_word) == 1 && is_c_type(first_word))
    {
        return 0;
    }

    var_len = (size_t)(eq - line);
    val_len = strlen(eq + 1);
    if (var_len >= var_size || val_len >= val_size)
    {
        report_message("ERREUR [Ligne %d] : Affectation trop longue.\n",
                       "ERROR [Line %d] : Assignment is too long.\n", line_num);
        return -1;
    }

    if (!copy_slice(var, var_size, line, var_len) || !safe_copy(val, val_size, eq + 1))
    {
        report_message("ERREUR [Ligne %d] : Impossible de copier l'affectation.\n",
                       "ERROR [Line %d] : Could not copy assignment.\n", line_num);
        return -1;
    }

    trim_trailing_whitespace(var);
    trim_leading_whitespace(var);
    trim_trailing_whitespace(val);
    trim_leading_whitespace(val);

    if (strlen(val) > 0 && val[strlen(val) - 1] == ';')
    {
        val[strlen(val) - 1] = '\0';
        trim_trailing_whitespace(val);
    }

    return (strlen(var) > 0 && strlen(val) > 0) ? 1 : 0;
}

static const char *parse_and_validate_list_literal(const char *literal, char elements[128][64], int *count_out, int line_num)
{
    *count_out = 0;
    char buf[512] = {0};

    const char *start = strchr(literal, '[');
    const char *end = strrchr(literal, ']');

    if (!start || !end || end <= start)
    {
        report_message("ERREUR [Ligne %d] : Syntaxe de liste invalide '%s'.\n",
                       "ERROR [Line %d] : Invalid list syntax '%s'.\n", line_num, literal);
        return NULL;
    }

    if (!copy_slice(buf, sizeof(buf), start + 1, (size_t)(end - start - 1)))
    {
        report_message("ERREUR [Ligne %d] : Liste trop longue.\n",
                       "ERROR [Line %d] : List literal is too long.\n", line_num);
        return NULL;
    }

    if (strlen(buf) == 0) return "int";

    char *token = strtok(buf, ",");
    static char detected_type[16] = {0};
    detected_type[0] = '\0';

    while (token != NULL)
    {
        while (*token == ' ' || *token == '\t') token++;
        char elem[64] = {0};
        if (!safe_copy(elem, sizeof(elem), token))
        {
            report_message("ERREUR [Ligne %d] : Élément de liste trop long '%s'.\n",
                           "ERROR [Line %d] : List element is too long '%s'.\n",
                           line_num, token);
            return NULL;
        }
        trim_trailing_whitespace(elem);

        if (strlen(elem) > 0)
        {
            if (*count_out >= 128)
            {
                report_message("ERREUR [Ligne %d] : Trop d'éléments dans la liste (maximum %d).\n",
                               "ERROR [Line %d] : Too many elements in list (maximum %d).\n",
                               line_num, 128);
                return NULL;
            }
            if (!safe_copy(elements[*count_out], 64, elem))
            {
                report_message("ERREUR [Ligne %d] : Élément de liste trop long '%s'.\n",
                               "ERROR [Line %d] : List element is too long '%s'.\n",
                               line_num, elem);
                return NULL;
            }
            (*count_out)++;

            const char *elem_type = "int";
            if (elem[0] == '"') elem_type = "char*";
            else if (strcmp(elem, "true") == 0 || strcmp(elem, "false") == 0) elem_type = "bool";
            else if (strchr(elem, '.') != NULL) elem_type = "float";

            if (detected_type[0] == '\0')
            {
                safe_copy(detected_type, sizeof(detected_type), elem_type);
            }
            else if (strcmp(detected_type, elem_type) != 0)
            {
                report_message("ERREUR [Ligne %d] : Incohérence de types dans le tableau (%s vs %s).\n",
                               "ERROR [Line %d] : Type mismatch in array (%s vs %s).\n",
                               line_num, detected_type, elem_type);
                return NULL;
            }
        }
        token = strtok(NULL, ",");
    }

    return detected_type;
}

static bool transform_print(const char *line, char *out_buf, size_t out_size, int line_num)
{
    char format_str[1024] = {0};
    char args[512] = {0};
    
    const char *start_quote = strchr(line, '"');
    const char *end_quote = strrchr(line, '"');

    if (start_quote != NULL && end_quote != NULL && end_quote > start_quote)
    {
        const char *p = start_quote + 1;
        char *f_out = format_str;

        while (p < end_quote)
        {
            if (*p == '{')
            {
                p++;
                char var_name[128] = {0};
                int v_idx = 0;

                while (p < end_quote && *p != '}')
                {
                    if (v_idx >= (int)sizeof(var_name) - 1)
                    {
                        report_message("ERREUR [Ligne %d] : Expression interpolée trop longue.\n",
                                       "ERROR [Line %d] : Interpolated expression is too long.\n",
                                       line_num);
                        return false;
                    }
                    var_name[v_idx++] = *p++;
                }

                if (*p != '}')
                {
                    report_message("ERREUR [Ligne %d] : Interpolation print non fermée.\n",
                                   "ERROR [Line %d] : Unclosed print interpolation.\n",
                                   line_num);
                    return false;
                }
                if (v_idx == 0)
                {
                    report_message("ERREUR [Ligne %d] : Interpolation print vide.\n",
                                   "ERROR [Line %d] : Empty print interpolation.\n",
                                   line_num);
                    return false;
                }
                p++;

                const char *specifier = get_format_specifier(var_name, line_num);
                if (!append_checked(format_str, sizeof(format_str), specifier))
                {
                    report_message("ERREUR [Ligne %d] : Format print trop long.\n",
                                   "ERROR [Line %d] : Print format is too long.\n", line_num);
                    return false;
                }
                f_out = format_str + strlen(format_str);

                if (strlen(args) > 0 && !append_checked(args, sizeof(args), ", "))
                {
                    report_message("ERREUR [Ligne %d] : Liste d'arguments print trop longue.\n",
                                   "ERROR [Line %d] : Print argument list is too long.\n", line_num);
                    return false;
                }
                if (!append_checked(args, sizeof(args), var_name))
                {
                    report_message("ERREUR [Ligne %d] : Liste d'arguments print trop longue.\n",
                                   "ERROR [Line %d] : Print argument list is too long.\n", line_num);
                    return false;
                }
            }
            else
            {
                if ((size_t)(f_out - format_str) >= sizeof(format_str) - 1)
                {
                    report_message("ERREUR [Ligne %d] : Format print trop long.\n",
                                   "ERROR [Line %d] : Print format is too long.\n", line_num);
                    return false;
                }
                *f_out++ = *p++;
                *f_out = '\0';
            }
        }
        *f_out = '\0';

        if (strlen(args) > 0)
        {
            if (snprintf(out_buf, out_size, "printf(\"%s\\n\", %s);", format_str, args) >= (int)out_size)
            {
                report_message("ERREUR [Ligne %d] : Instruction print générée trop longue.\n",
                               "ERROR [Line %d] : Generated print statement is too long.\n", line_num);
                return false;
            }
        }
        else
        {
            if (snprintf(out_buf, out_size, "printf(\"%s\\n\");", format_str) >= (int)out_size)
            {
                report_message("ERREUR [Ligne %d] : Instruction print générée trop longue.\n",
                               "ERROR [Line %d] : Generated print statement is too long.\n", line_num);
                return false;
            }
        }
    }
    else
    {
        const char *arg_start = strchr(line, '(');
        const char *arg_end = strrchr(line, ')');
        if (arg_start && arg_end && arg_end > arg_start)
        {
            char expr[256] = {0};
            if (!copy_slice(expr, sizeof(expr), arg_start + 1, (size_t)(arg_end - arg_start - 1)))
            {
                report_message("ERREUR [Ligne %d] : Expression print trop longue.\n",
                               "ERROR [Line %d] : Print expression is too long.\n", line_num);
                return false;
            }
            trim_trailing_whitespace(expr);
            const char *spec = get_format_specifier(expr, line_num);
            if (snprintf(out_buf, out_size, "printf(\"%s\\n\", %s);", spec, expr) >= (int)out_size)
            {
                report_message("ERREUR [Ligne %d] : Instruction print générée trop longue.\n",
                               "ERROR [Line %d] : Generated print statement is too long.\n", line_num);
                return false;
            }
        }
        else
        {
            if (snprintf(out_buf, out_size, "printf(%s);", line + 6) >= (int)out_size)
            {
                report_message("ERREUR [Ligne %d] : Instruction print générée trop longue.\n",
                               "ERROR [Line %d] : Generated print statement is too long.\n", line_num);
                return false;
            }
        }
    }
    return true;
}

static bool write_action(FILE *file, int indent_level, const char *action, int line_num)
{
    if (strlen(action) == 0) return true;
    print_indent(file, indent_level);
    if (strncmp(action, "print(", 6) == 0)
    {
        char print_buf[2048] = {0};
        if (!transform_print(action, print_buf, sizeof(print_buf), line_num))
            return false;
        fprintf(file, "%s\n", print_buf);
    }
    else
    {
        size_t len = strlen(action);
        if (len > 0 && action[len - 1] != ';')
        {
            fprintf(file, "%s;\n", action);
        }
        else
        {
            fprintf(file, "%s\n", action);
        }
    }
    return true;
}

int trad_c(char *filename, char ***text_ptr)
{
    if (filename == NULL || text_ptr == NULL || *text_ptr == NULL)
    {
        report_message("ERREUR CRITIQUE : Paramètres d'entrée invalides transmis à trad_c.\n",
                       "CRITICAL ERROR : Invalid input parameters provided to trad_c.\n");
        return 1;
    }

    symbol_count = 0;
    allocated_count = 0;
    in_match_block = false;
    current_match_var[0] = '\0';

    char *new_name = duplicate_string(filename);
    if (new_name == NULL)
    {
        report_message("ERREUR SYSTEME : Échec d'allocation mémoire pour 'new_name'.\n",
                       "SYSTEM ERROR : Memory allocation failed for 'new_name'.\n");
        return 1;
    }

    char *dot = strrchr(new_name, '.');
    if (dot != NULL) *dot = '\0';

    char *c_filename = malloc(strlen(new_name) + 3);
    if (c_filename == NULL)
    {
        report_message("ERREUR SYSTEME : Échec d'allocation mémoire pour 'c_filename'.\n",
                       "SYSTEM ERROR : Memory allocation failed for 'c_filename'.\n");
        free(new_name);
        return 1;
    }
    snprintf(c_filename, strlen(new_name) + 3, "%s.c", new_name);
    free(new_name);
    new_name = c_filename;

    FILE *file = fopen(new_name, "w");
    if (file == NULL)
    {
        report_message("ERREUR FICHIER : Impossible de créer le fichier de sortie '%s'.\n",
                       "FILE ERROR : Cannot create output file '%s'.\n", new_name);
        free(new_name);
        return 1;
    }

    fprintf(file, "#include <stdio.h>\n#include <stdlib.h>\n#include <stdbool.h>\n#include <string.h>\n\n");

    char **text = *text_ptr;
    char line_buf[2048] = {0};

    int indent_stack[MAX_INDENT_LEVELS];
    int indent_top = 0;
    indent_stack[0] = 0;
    int line_number = 0;

    for (int i = 0; text[i] != NULL; i++)
    {
        if (!append_checked(line_buf, sizeof(line_buf), text[i]))
        {
            report_message("ERREUR [Ligne %d] : Ligne source trop longue.\n",
                           "ERROR [Line %d] : Source line is too long.\n", line_number + 1);
            goto error_cleanup;
        }

        if (strchr(text[i], '\n') != NULL)
        {
            line_number++;
            trim_trailing_whitespace(line_buf);
            normalize_booleans(line_buf);

            int current_indent = get_indentation_level(line_buf);

            char *line = line_buf;
            while (*line == ' ' || *line == '\t') line++;

            size_t line_len = strlen(line);

            if (line_len > 0)
            {
                while (indent_top > 0 && current_indent < indent_stack[indent_top])
                {
                    generate_scope_frees(file, indent_top);
                    indent_top--;
                    pop_scope(indent_top);
                    print_indent(file, indent_top);
                    fprintf(file, "}\n");
                }

                if (current_indent > indent_stack[indent_top])
                {
                    if (indent_top < MAX_INDENT_LEVELS - 1)
                    {
                        print_indent(file, indent_top);
                        fprintf(file, "{\n");
                        indent_top++;
                        indent_stack[indent_top] = current_indent;
                    }
                    else
                    {
                        report_message("ERREUR [Ligne %d] : Dépassement du niveau d'indentation maximal (%d).\n",
                                       "ERROR [Line %d] : Maximum indentation level exceeded (%d).\n",
                                       line_number, MAX_INDENT_LEVELS);
                        fclose(file);
                        remove(new_name);
                        free(new_name);
                        return 1;
                    }
                }

                if (strcmp(line, "#all") == 0)
                {
                    fprintf(file, "#include <math.h>\n#include <time.h>\n");
                }
                else if (strcmp(line, "#linux") == 0)
                {
                    fprintf(file, "#include <unistd.h>\n#include <sys/types.h>\n#include <sys/wait.h>\n");
                }
                else if (strcmp(line, "#pipe") == 0)
                {
                    print_indent(file, indent_top);
                    fprintf(file, "int pipe_fd[2];\n");
                    print_indent(file, indent_top);
                    fprintf(file, "if (pipe(pipe_fd) == -1)\n");
                    print_indent(file, indent_top);
                    fprintf(file, "{\n");
                    print_indent(file, indent_top + 1);
                    fprintf(file, "perror(\"%s\");\n",
                            flag_french ? "Erreur : création du tube anonyme échouée" :
                                          "Error: anonymous pipe creation failed");
                    print_indent(file, indent_top + 1);
                    fprintf(file, "return -1;\n");
                    print_indent(file, indent_top);
                    fprintf(file, "}\n");
                }
                else if (strcmp(line, "#process") == 0)
                {
                    print_indent(file, indent_top);
                    fprintf(file, "pid_t pid = fork();\n");
                    print_indent(file, indent_top);
                    fprintf(file, "if (pid < 0)\n");
                    print_indent(file, indent_top);
                    fprintf(file, "{\n");
                    print_indent(file, indent_top + 1);
                    fprintf(file, "perror(\"%s\");\n",
                            flag_french ? "Erreur : l'initialisation du processus enfant a échoué" :
                                          "Error: child process initialization failed");
                    print_indent(file, indent_top + 1);
                    fprintf(file, "return -1;\n");
                    print_indent(file, indent_top);
                    fprintf(file, "}\n");
                    if (!add_symbol("pid", "pid_t", indent_top, line_number)) goto error_cleanup;
                }
                else if (strcmp(line, "#enfant") == 0)
                {
                    print_indent(file, indent_top);
                    fprintf(file, "if (pid == 0)\n");
                }
                else if (strcmp(line, "#parent basic") == 0)
                {
                    print_indent(file, indent_top);
                    fprintf(file, "else\n");
                    
                    if (indent_top < MAX_INDENT_LEVELS - 1)
                    {
                        print_indent(file, indent_top);
                        fprintf(file, "{\n");
                        indent_top++;
                        indent_stack[indent_top] = current_indent;
                    }

                    print_indent(file, indent_top);
                    fprintf(file, "int status;\n");
                    print_indent(file, indent_top);
                    fprintf(file, "waitpid(pid, &status, 0);\n\n");
                    
                    print_indent(file, indent_top);
                    fprintf(file, "if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)\n");
                    print_indent(file, indent_top);
                    fprintf(file, "{\n");
                    print_indent(file, indent_top + 1);
                    fprintf(file, "return -1;\n");
                    print_indent(file, indent_top);
                    fprintf(file, "}\n");
                }
                else if (strcmp(line, "#parent") == 0)
                {
                    print_indent(file, indent_top);
                    fprintf(file, "else\n");
                }
                else if (strncmp(line, "#include", 8) == 0 || strncmp(line, "#define", 7) == 0 || strncmp(line, "#ifdef", 6) == 0 || strncmp(line, "#ifndef", 7) == 0 || strncmp(line, "#endif", 6) == 0)
                {
                    fprintf(file, "%s\n", line);
                }
                else if (line[0] == '#')
                {
                    print_indent(file, indent_top);
                    fprintf(file, "// %s\n", line + 1);
                }
                else if (strncmp(line, "int main()", 10) == 0)
                {
                    print_indent(file, indent_top);
                    fprintf(file, "int main()\n");
                }
                else
                {
                    if (strncmp(line, "match ", 6) == 0 && strstr(line, " with") != NULL)
                    {
                        if (sscanf(line, "match %63s with", current_match_var) != 1)
                        {
                            report_message("ERREUR [Ligne %d] : Syntaxe 'match <var> with' incorrecte.\n",
                                           "ERROR [Line %d] : Incorrect 'match <var> with' syntax.\n",
                                           line_number);
                            goto error_cleanup;
                        }
                        in_match_block = true;
                        match_first_case = true;
                    }
                    else if (in_match_block && line[0] == '|')
                    {
                        char val_pat[128] = {0};
                        char action[512] = {0};

                        if (sscanf(line, "| %127[^-]-> %511[^\n]", val_pat, action) != 2)
                        {
                            report_message("ERREUR [Ligne %d] : Branche de 'match' mal formée.\n",
                                           "ERROR [Line %d] : Malformed 'match' branch.\n",
                                           line_number);
                            goto error_cleanup;
                        }
                        trim_trailing_whitespace(val_pat);
                        trim_trailing_whitespace(action);

                        char var_pat[64] = {0};
                        char guard_cond[128] = {0};
                        bool has_guard = false;

                        if (strstr(val_pat, " if ") != NULL)
                        {
                            has_guard = true;
                            sscanf(val_pat, "%63s if %127[^\n]", var_pat, guard_cond);
                            trim_trailing_whitespace(var_pat);
                            trim_trailing_whitespace(guard_cond);
                        }

                        print_indent(file, indent_top);

                        if (has_guard)
                        {
                            fprintf(file, "%s (%s)\n", match_first_case ? "if" : "else if", guard_cond);
                            match_first_case = false;

                            if (is_valid_identifier(var_pat) && strcmp(var_pat, "_") != 0)
                            {
                                print_indent(file, indent_top);
                                fprintf(file, "{\n");
                                const char *var_type = get_symbol_type(current_match_var);
                                if (!var_type) var_type = "int";

                                print_indent(file, indent_top + 1);
                                fprintf(file, "%s %s = %s;\n", var_type, var_pat, current_match_var);
                                if (!add_symbol(var_pat, var_type, indent_top + 1, line_number)) goto error_cleanup;

                                if (!write_action(file, indent_top + 1, action, line_number)) goto error_cleanup;

                                print_indent(file, indent_top);
                                fprintf(file, "}\n");
                            }
                            else
                            {
                                if (!write_action(file, indent_top + 1, action, line_number)) goto error_cleanup;
                            }
                        }
                        else if (strcmp(val_pat, "[]") == 0)
                        {
                            fprintf(file, "%s (%s_len == 0)\n", match_first_case ? "if" : "else if", current_match_var);
                            match_first_case = false;
                            if (!write_action(file, indent_top + 1, action, line_number)) goto error_cleanup;
                        }
                        else if (strstr(val_pat, "::") != NULL)
                        {
                            char head_var[64] = {0};
                            char tail_var[64] = {0};
                            sscanf(val_pat, "%63[^:]::%63s", head_var, tail_var);

                            fprintf(file, "%s (%s_len > 0)\n", match_first_case ? "if" : "else if", current_match_var);
                            match_first_case = false;

                            print_indent(file, indent_top);
                            fprintf(file, "{\n");
                            
                            const char *elem_type = get_elem_type(current_match_var);
                            print_indent(file, indent_top + 1);
                            fprintf(file, "%s %s = %s[0];\n", elem_type, head_var, current_match_var);
                            if (!add_symbol(head_var, elem_type, indent_top + 1, line_number)) goto error_cleanup;

                            print_indent(file, indent_top + 1);
                            fprintf(file, "%s* %s = %s + 1;\n", elem_type, tail_var, current_match_var);
                            char list_type[32] = {0};
                            snprintf(list_type, sizeof(list_type), "%s*", elem_type);
                            if (!add_symbol(tail_var, list_type, indent_top + 1, line_number)) goto error_cleanup;

                            print_indent(file, indent_top + 1);
                            fprintf(file, "%s_len--;\n", current_match_var);

                            if (!write_action(file, indent_top + 1, action, line_number)) goto error_cleanup;
                            
                            print_indent(file, indent_top);
                            fprintf(file, "}\n");
                        }
                        else if (val_pat[0] == '[' && val_pat[strlen(val_pat) - 1] == ']')
                        {
                            char elem_var[64] = {0};
                            sscanf(val_pat, "[%63[^]]]", elem_var);

                            fprintf(file, "%s (%s_len == 1)\n", match_first_case ? "if" : "else if", current_match_var);
                            match_first_case = false;

                            print_indent(file, indent_top);
                            fprintf(file, "{\n");

                            const char *elem_type = get_elem_type(current_match_var);
                            print_indent(file, indent_top + 1);
                            fprintf(file, "%s %s = %s[0];\n", elem_type, elem_var, current_match_var);
                            if (!add_symbol(elem_var, elem_type, indent_top + 1, line_number)) goto error_cleanup;

                            if (!write_action(file, indent_top + 1, action, line_number)) goto error_cleanup;

                            print_indent(file, indent_top);
                            fprintf(file, "}\n");
                        }
                        else if (strcmp(val_pat, "_") == 0)
                        {
                            fprintf(file, "else\n");
                            if (!write_action(file, indent_top + 1, action, line_number)) goto error_cleanup;
                        }
                        else
                        {
                            fprintf(file, "%s (%s == %s)\n", match_first_case ? "if" : "else if", current_match_var, val_pat);
                            match_first_case = false;
                            if (!write_action(file, indent_top + 1, action, line_number)) goto error_cleanup;
                        }
                    }
                    else if (strstr(line, "::") != NULL)
                    {
                        if (line[0] != '|') in_match_block = false;

                        char head_var[64] = {0};
                        char tail_var[64] = {0};
                        char source_list[64] = {0};

                        if (sscanf(line, "%63[^:]::%63s = %63[^\n;]", head_var, tail_var, source_list) == 3)
                        {
                            trim_trailing_whitespace(head_var);
                            trim_trailing_whitespace(tail_var);
                            trim_trailing_whitespace(source_list);

                            const char *elem_type = get_elem_type(source_list);

                            print_indent(file, indent_top);
                            fprintf(file, "%s %s = %s[0];\n", elem_type, head_var, source_list);
                            if (!add_symbol(head_var, elem_type, indent_top, line_number)) goto error_cleanup;

                            print_indent(file, indent_top);
                            fprintf(file, "%s* %s = %s + 1;\n", elem_type, tail_var, source_list);
                            char list_type[32] = {0};
                            snprintf(list_type, sizeof(list_type), "%s*", elem_type);
                            if (!add_symbol(tail_var, list_type, indent_top, line_number)) goto error_cleanup;

                            print_indent(file, indent_top);
                            fprintf(file, "int %s_len = %s_len - 1;\n", tail_var, source_list);
                        }
                        else
                        {
                            report_message("ERREUR [Ligne %d] : Décomposition de liste '::' mal formée.\n",
                                           "ERROR [Line %d] : Malformed list decomposition '::'.\n",
                                           line_number);
                            goto error_cleanup;
                        }
                    }
                    else
                    {
                        if (line[0] != '|') in_match_block = false;

                        char var[64] = {0}, val[256] = {0};
                        int assignment_status = parse_assignment(line, var, sizeof(var), val, sizeof(val), line_number);

                        if (assignment_status < 0)
                        {
                            goto error_cleanup;
                        }

                        if (assignment_status && !is_c_declaration_lhs(var))
                        {
                            if (!is_valid_identifier(var))
                            {
                                report_message("ERREUR [Ligne %d] : Identifiant de variable invalide '%s'.\n",
                                               "ERROR [Line %d] : Invalid variable identifier '%s'.\n",
                                               line_number, var);
                                goto error_cleanup;
                            }

                            print_indent(file, indent_top);
                            const char *existing_type = get_symbol_type(var);

                            if (existing_type != NULL)
                            {
                                fprintf(file, "%s = %s;\n", var, val);
                            }
                            else
                            {
                                const char *inferred_type = infer_expression_type(val);

                                if (strcmp(inferred_type, "list") == 0)
                                {
                                    char elems[128][64];
                                    int count = 0;
                                    const char *elem_type = parse_and_validate_list_literal(val, elems, &count, line_number);

                                    if (elem_type == NULL) goto error_cleanup;

                                    char list_type[32] = {0};
                                    snprintf(list_type, sizeof(list_type), "%s*", elem_type);

                                    fprintf(file, "%s *%s = malloc(%d * sizeof(%s));\n", elem_type, var, count, elem_type);
                                    if (!add_symbol(var, list_type, indent_top, line_number)) goto error_cleanup;
                                    if (!add_allocated_ptr(var, indent_top, line_number)) goto error_cleanup;

                                    for (int k = 0; k < count; k++)
                                    {
                                        print_indent(file, indent_top);
                                        fprintf(file, "%s[%d] = %s;\n", var, k, elems[k]);
                                    }

                                    print_indent(file, indent_top);
                                    fprintf(file, "int %s_len = %d;\n", var, count);
                                }
                                else if (strcmp(inferred_type, "char*") == 0)
                                {
                                    if (!add_symbol(var, "char*", indent_top, line_number)) goto error_cleanup;
                                    fprintf(file, "char *%s = %s;\n", var, val);
                                }
                                else
                                {
                                    if (!add_symbol(var, inferred_type, indent_top, line_number)) goto error_cleanup;
                                    fprintf(file, "%s %s = %s;\n", inferred_type, var, val);
                                }
                            }
                        }
                        else if (strncmp(line, "return", 6) == 0)
                        {
                            generate_frees(file, indent_top);
                            print_indent(file, indent_top);
                            fprintf(file, "%s;\n", line);
                        }
                        else if (strncmp(line, "print(", 6) == 0)
                        {
                            char print_formatted[2048] = {0};
                            if (!transform_print(line, print_formatted, sizeof(print_formatted), line_number)) goto error_cleanup;
                            print_indent(file, indent_top);
                            fprintf(file, "%s\n", print_formatted);
                        }
                        else if (strncmp(line, "if ", 3) == 0)
                        {
                            char cond[1024] = {0};
                            if (!safe_copy(cond, sizeof(cond), line + 3))
                            {
                                report_message("ERREUR [Ligne %d] : Condition if trop longue.\n",
                                               "ERROR [Line %d] : if condition is too long.\n",
                                               line_number);
                                goto error_cleanup;
                            }
                            size_t c_len = strlen(cond);
                            if (c_len > 0 && cond[c_len - 1] == ':') cond[c_len - 1] = '\0';

                            trim_trailing_whitespace(cond);
                            print_indent(file, indent_top);
                            fprintf(file, "if (%s)\n", cond);
                        }
                        else if (strncmp(line, "while ", 6) == 0)
                        {
                            char cond[1024] = {0};
                            if (!safe_copy(cond, sizeof(cond), line + 6))
                            {
                                report_message("ERREUR [Ligne %d] : Condition while trop longue.\n",
                                               "ERROR [Line %d] : while condition is too long.\n",
                                               line_number);
                                goto error_cleanup;
                            }
                            size_t c_len = strlen(cond);
                            if (c_len > 0 && cond[c_len - 1] == ':') cond[c_len - 1] = '\0';

                            trim_trailing_whitespace(cond);
                            print_indent(file, indent_top);
                            fprintf(file, "while (%s)\n", cond);
                        }
                        else if (strncmp(line, "for ", 4) == 0 && strstr(line, " in range(") != NULL)
                        {
                            char var_name[64] = {0};
                            char range_val[256] = {0};

                            if (sscanf(line, "for %63s in range(%255[^)])", var_name, range_val) != 2)
                            {
                                report_message("ERREUR [Ligne %d] : Syntaxe de boucle 'for ... in range()' invalide.\n",
                                               "ERROR [Line %d] : Invalid 'for ... in range()' loop syntax.\n",
                                               line_number);
                                goto error_cleanup;
                            }
                            if (!add_symbol(var_name, "int", indent_top, line_number)) goto error_cleanup;
                            print_indent(file, indent_top);
                            fprintf(file, "for (int %s = 0; %s < %s; %s++)\n", var_name, var_name, range_val, var_name);
                        }
                        else if (strncmp(line, "else:", 5) == 0 || strncmp(line, "else", 4) == 0)
                        {
                            print_indent(file, indent_top);
                            fprintf(file, "else\n");
                        }
                        else
                        {
                            char type_exp[16] = {0}, var_exp[64] = {0};
                            if (sscanf(line, "%15s %63[^ (](%*[^)])", type_exp, var_exp) == 2 && is_c_type(type_exp))
                            {
                                if (!add_symbol(var_exp, type_exp, indent_top, line_number)) goto error_cleanup;
                            }
                            else if (sscanf(line, "%15s %63s", type_exp, var_exp) == 2 && is_c_type(type_exp))
                            {
                                bool pointer_decl = false;
                                while (var_exp[0] == '*')
                                {
                                    pointer_decl = true;
                                    memmove(var_exp, var_exp + 1, strlen(var_exp));
                                }
                                char *suffix = strpbrk(var_exp, "=;[(");
                                if (suffix != NULL) *suffix = '\0';
                                char symbol_type[16] = {0};
                                if (pointer_decl)
                                    snprintf(symbol_type, sizeof(symbol_type), "%s*", type_exp);
                                else
                                    safe_copy(symbol_type, sizeof(symbol_type), type_exp);
                                if (is_valid_identifier(var_exp) && !add_symbol(var_exp, symbol_type, indent_top, line_number)) goto error_cleanup;
                            }

                            print_indent(file, indent_top);
                            size_t l_len = strlen(line);
                            bool function_header = is_c_type(type_exp) && strchr(line, '(') != NULL &&
                                                   l_len > 0 && line[l_len - 1] == ')';
                            if (l_len > 0 && line[l_len - 1] != ';' && !function_header && line[l_len - 1] != '{' && line[l_len - 1] != '}')
                            {
                                fprintf(file, "%s;\n", line);
                            }
                            else
                            {
                                fprintf(file, "%s\n", line);
                            }
                        }
                    }
                }
            }

            line_buf[0] = '\0';
        }
    }

    while (indent_top > 0)
    {
        generate_scope_frees(file, indent_top);
        indent_top--;
        pop_scope(indent_top);
        print_indent(file, indent_top);
        fprintf(file, "}\n");
    }

    fclose(file);
    free(new_name);

    return 0;

error_cleanup:
    fclose(file);
    remove(new_name);
    free(new_name);
    return 1;
}
