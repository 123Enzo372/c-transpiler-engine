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
    bool is_const;
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

static bool scope_returned[MAX_INDENT_LEVELS] = {false};
static const char *diagnostic_filename = NULL;
static int diagnostic_line_num = 0;
static char diagnostic_source_line[2048] = {0};
static char pending_foreach_decl[256] = {0};
static char emitted_includes[128][512];
static int emitted_include_count = 0;

static bool transform_expression(const char *input, char *output, size_t output_size, int line_num);
static const char *infer_expression_type(const char *val);
static bool add_symbol_ex(const char *name, const char *type, int scope_level, int line_num, bool is_const);

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

static void set_diagnostic_source(const char *filename, int line_num, const char *line)
{
    diagnostic_filename = filename;
    diagnostic_line_num = line_num;
    if (line != NULL)
        safe_copy(diagnostic_source_line, sizeof(diagnostic_source_line), line);
    else
        diagnostic_source_line[0] = '\0';
}

static int diagnostic_column_for(const char *needle)
{
    const char *line = diagnostic_source_line;
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

static void report_source_hint(int line_num, const char *needle,
                               const char *fr_suggestion, const char *en_suggestion)
{
    const char *suggestion = flag_french ? fr_suggestion : en_suggestion;
    const char *source = diagnostic_source_line;
    bool has_source = (line_num == diagnostic_line_num && source[0] != '\0');
    int column = has_source ? diagnostic_column_for(needle) : 1;

    if (line_num <= 0)
        line_num = diagnostic_line_num;

    fprintf(stderr, "  --> %s:%d\n", diagnostic_filename != NULL ? diagnostic_filename : "<source>", line_num);
    if (has_source)
    {
        fprintf(stderr, "   |\n");
        fprintf(stderr, "%4d | %s\n", line_num, source);
        fprintf(stderr, "   | ");
        for (int i = 1; i < column; i++)
            fputc(source[i - 1] == '\t' ? '\t' : ' ', stderr);
        fprintf(stderr, "^\n");
    }
    if (suggestion != NULL && suggestion[0] != '\0')
    {
        fprintf(stderr, flag_french ? "Suggestion : %s\n" : "Suggestion: %s\n", suggestion);
    }
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

static bool add_symbol_ex(const char *name, const char *type, int scope_level, int line_num, bool is_const)
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
            symbol_table[i].is_const = symbol_table[i].is_const || is_const;
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
    symbol_table[symbol_count].is_const = is_const;
    symbol_count++;
    return true;
}

static bool add_symbol(const char *name, const char *type, int scope_level, int line_num)
{
    return add_symbol_ex(name, type, scope_level, line_num, false);
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

static bool is_const_symbol(const char *name)
{
    for (int i = symbol_count - 1; i >= 0; i--)
    {
        if (strcmp(symbol_table[i].name, name) == 0)
            return symbol_table[i].is_const;
    }
    return false;
}

static void trace_translation(int line_num, const char *kind, const char *detail)
{
    if (!flag_trace || flag_quiet)
        return;

    fprintf(stderr, "trace %s:%d: %s",
            diagnostic_filename != NULL ? diagnostic_filename : "<source>",
            line_num, kind);
    if (detail != NULL && detail[0] != '\0')
        fprintf(stderr, " %s", detail);
    fprintf(stderr, "\n");
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

static void generate_frees_except(FILE *file, int indent_level, const char *skip_name)
{
    char emitted[MAX_ALLOCATED][64] = {{0}};
    int emitted_count = 0;

    for (int i = allocated_count - 1; i >= 0; i--)
    {
        bool already_emitted = false;

        if (skip_name != NULL && strcmp(allocated_ptrs[i].name, skip_name) == 0)
            continue;

        for (int j = 0; j < emitted_count; j++)
        {
            if (strcmp(emitted[j], allocated_ptrs[i].name) == 0)
            {
                already_emitted = true;
                break;
            }
        }
        if (already_emitted)
            continue;

        safe_copy(emitted[emitted_count], sizeof(emitted[0]), allocated_ptrs[i].name);
        emitted_count++;

        for (int k = 0; k < indent_level; k++) fprintf(file, "    ");
        fprintf(file, "if (%s != NULL) { free(%s); %s = NULL; }\n",
                allocated_ptrs[i].name, allocated_ptrs[i].name, allocated_ptrs[i].name);
    }
}

static void generate_frees(FILE *file, int indent_level)
{
    generate_frees_except(file, indent_level, NULL);
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

static void discard_scope_allocations(int scope_level)
{
    int new_count = 0;

    for (int i = 0; i < allocated_count; i++)
    {
        if (allocated_ptrs[i].scope_level != scope_level)
        {
            allocated_ptrs[new_count++] = allocated_ptrs[i];
        }
    }
    allocated_count = new_count;
}

static bool is_float_like_type(const char *type)
{
    return type != NULL && (strcmp(type, "float") == 0 || strcmp(type, "double") == 0);
}

static bool expression_has_boolean_operator(const char *expr)
{
    bool in_double_quotes = false;
    bool in_single_quotes = false;

    for (const char *p = expr; *p != '\0'; p++)
    {
        if (*p == '"' && !in_single_quotes)
            in_double_quotes = !in_double_quotes;
        else if (*p == '\'' && !in_double_quotes)
            in_single_quotes = !in_single_quotes;

        if (in_double_quotes || in_single_quotes)
            continue;

        if ((p[0] == '=' && p[1] == '=') ||
            (p[0] == '!' && p[1] == '=') ||
            (p[0] == '<' && p[1] == '=') ||
            (p[0] == '>' && p[1] == '=') ||
            (p[0] == '&' && p[1] == '&') ||
            (p[0] == '|' && p[1] == '|'))
            return true;

        if ((*p == '<' || *p == '>') && p[1] != *p)
            return true;

        if (*p == '!' && p[1] != '=')
            return true;
    }

    return false;
}

static void scan_expression_numeric_type(const char *expr, bool *has_float, bool *has_double)
{
    bool in_double_quotes = false;
    bool in_single_quotes = false;

    for (const char *p = expr; *p != '\0';)
    {
        if (*p == '"' && !in_single_quotes)
        {
            in_double_quotes = !in_double_quotes;
            p++;
            continue;
        }
        if (*p == '\'' && !in_double_quotes)
        {
            in_single_quotes = !in_single_quotes;
            p++;
            continue;
        }
        if (in_double_quotes || in_single_quotes)
        {
            p++;
            continue;
        }

        if (isalpha((unsigned char)*p) || *p == '_')
        {
            char ident[64] = {0};
            size_t ident_len = 0;

            while ((isalnum((unsigned char)*p) || *p == '_') && ident_len < sizeof(ident) - 1)
            {
                ident[ident_len++] = *p;
                p++;
            }
            ident[ident_len] = '\0';

            const char *type = get_symbol_type(ident);
            if (is_float_like_type(type))
            {
                if (strcmp(type, "double") == 0)
                    *has_double = true;
                else
                    *has_float = true;
            }

            const char *lookahead = p;
            while (*lookahead == ' ' || *lookahead == '\t')
                lookahead++;
            if (*lookahead == '(')
            {
                if (type != NULL && strcmp(type, "double") == 0)
                    *has_double = true;
                else if (type != NULL && strcmp(type, "float") == 0)
                    *has_float = true;
            }
            continue;
        }

        if (isdigit((unsigned char)*p) ||
            ((*p == '+' || *p == '-') && isdigit((unsigned char)p[1])))
        {
            bool literal_has_decimal = false;
            bool literal_has_float_suffix = false;

            if (*p == '+' || *p == '-')
                p++;
            while (isdigit((unsigned char)*p))
                p++;
            if (*p == '.')
            {
                literal_has_decimal = true;
                p++;
                while (isdigit((unsigned char)*p))
                    p++;
            }
            if (*p == 'e' || *p == 'E')
            {
                literal_has_decimal = true;
                p++;
                if (*p == '+' || *p == '-')
                    p++;
                while (isdigit((unsigned char)*p))
                    p++;
            }
            if (*p == 'f' || *p == 'F')
            {
                literal_has_float_suffix = true;
                p++;
            }

            if (literal_has_float_suffix)
                *has_float = true;
            else if (literal_has_decimal)
                *has_double = true;
            continue;
        }

        p++;
    }
}

static const char *infer_expression_type(const char *val)
{
    bool has_float = false;
    bool has_double = false;

    if (val[0] == '[' && strchr(val, '+') == NULL) return "list";
    if (strcmp(val, "true") == 0 || strcmp(val, "false") == 0) return "bool";
    if (val[0] == '\'') return "char";
    if (val[0] == '"') return "char*";
    if (strncmp(val, "nl_str_len(", 11) == 0) return "int";
    if (strncmp(val, "nl_str_eq(", 10) == 0) return "bool";
    if (strncmp(val, "nl_contains_", 12) == 0) return "bool";
    if (strncmp(val, "str_copy(", 9) == 0 || strncmp(val, "str_concat(", 11) == 0) return "char*";
    if (expression_has_boolean_operator(val)) return "bool";

    scan_expression_numeric_type(val, &has_float, &has_double);
    if (has_double) return "double";
    if (has_float) return "float";

    char func_name[64] = {0};
    if (sscanf(val, "%63[a-zA-Z0-9_](", func_name) == 1)
    {
        const char *ret_type = get_symbol_type(func_name);
        if (ret_type != NULL) return ret_type;
        return "int";
    }

    return "int";
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

static bool is_list_type(const char *type)
{
    return type != NULL &&
           (strcmp(type, "int*") == 0 ||
            strcmp(type, "float*") == 0 ||
            strcmp(type, "char**") == 0 ||
            strcmp(type, "bool*") == 0);
}

static const char *runtime_suffix_for_elem(const char *elem_type)
{
    if (strcmp(elem_type, "int") == 0) return "int";
    if (strcmp(elem_type, "float") == 0) return "float";
    if (strcmp(elem_type, "char*") == 0) return "str";
    if (strcmp(elem_type, "bool") == 0) return "bool";
    return NULL;
}

static bool can_assign_type(const char *target_type, const char *value_type)
{
    if (target_type == NULL || value_type == NULL)
        return true;
    if (strcmp(target_type, value_type) == 0)
        return true;
    if (strcmp(target_type, "double") == 0 &&
        (strcmp(value_type, "float") == 0 || strcmp(value_type, "int") == 0))
        return true;
    if (strcmp(target_type, "float") == 0 && strcmp(value_type, "int") == 0)
        return true;
    return false;
}

static bool check_assignment_type(const char *var, const char *target_type,
                                  const char *value_expr, int line_num)
{
    const char *value_type = infer_expression_type(value_expr);

    if (strcmp(value_type, "list") != 0 && can_assign_type(target_type, value_type))
        return true;

    report_message("ERREUR E_ASSIGN_TYPE [Ligne %d] : Type incompatible pour '%s' (%s <- %s).\n",
                   "ERROR E_ASSIGN_TYPE [Line %d] : Incompatible type for '%s' (%s <- %s).\n",
                   line_num, var, target_type, value_type);
    report_source_hint(line_num, var,
                       "Utilise une nouvelle variable, ou assure-toi que la valeur a le même type.",
                       "Use a new variable, or make sure the value has the same type.");
    return false;
}

static bool emit_include_once(FILE *file, const char *include_line)
{
    for (int i = 0; i < emitted_include_count; i++)
    {
        if (strcmp(emitted_includes[i], include_line) == 0)
            return true;
    }

    if (emitted_include_count < (int)(sizeof(emitted_includes) / sizeof(emitted_includes[0])))
        safe_copy(emitted_includes[emitted_include_count++], sizeof(emitted_includes[0]), include_line);

    fprintf(file, "%s\n", include_line);
    return true;
}

static bool add_list_len_symbol(const char *list_name, int scope_level, int line_num)
{
    char len_name[80] = {0};

    if (snprintf(len_name, sizeof(len_name), "%s_len", list_name) >= (int)sizeof(len_name))
    {
        report_message("ERREUR [Ligne %d] : Nom de longueur de liste trop long pour '%s'.\n",
                       "ERROR [Line %d] : List length name is too long for '%s'.\n",
                       line_num, list_name);
        return false;
    }

    return add_symbol(len_name, "int", scope_level, line_num);
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
        if (strcmp(type, "double") == 0) return "%f";
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
        if (strcmp(type, "double") == 0) return "%f";
        if (strcmp(type, "char*") == 0) return "%s";
        if (strcmp(type, "char") == 0) return "%c";
        if (strcmp(type, "bool") == 0) return "%d";
    }
    if (is_valid_identifier(var_name) && type == NULL)
    {
        report_message("AVERTISSEMENT [Ligne %d] : Variable '%s' utilisée sans déclaration explicite prioritaire.\n",
                       "WARNING [Line %d] : Variable '%s' used without prior explicit declaration.\n",
                       line_num, var_name);
        return "%s";
    }

    const char *expr_type = infer_expression_type(var_name);
    if (strcmp(expr_type, "int") == 0) return "%d";
    if (strcmp(expr_type, "float") == 0) return "%f";
    if (strcmp(expr_type, "double") == 0) return "%f";
    if (strcmp(expr_type, "char*") == 0) return "%s";
    if (strcmp(expr_type, "char") == 0) return "%c";
    if (strcmp(expr_type, "bool") == 0) return "%d";

    if (type == NULL)
    {
        report_message("AVERTISSEMENT [Ligne %d] : Format print par défaut utilisé pour '%s'.\n",
                       "WARNING [Line %d] : Default print format used for '%s'.\n",
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

static bool is_identifier_char(char c)
{
    return isalnum((unsigned char)c) || c == '_';
}

static bool starts_with_word(const char *str, const char *word)
{
    size_t len = strlen(word);

    return strncmp(str, word, len) == 0 && !is_identifier_char(str[len]);
}

static int parse_arguments(const char *args_src, char args[][256], int max_args, int line_num)
{
    int count = 0;
    int depth = 0;
    bool in_double_quotes = false;
    bool in_single_quotes = false;
    char current[256] = {0};
    size_t current_len = 0;

    for (const char *p = args_src; ; p++)
    {
        char c = *p;
        bool end = (c == '\0');

        if (!end && c == '"' && !in_single_quotes)
            in_double_quotes = !in_double_quotes;
        else if (!end && c == '\'' && !in_double_quotes)
            in_single_quotes = !in_single_quotes;
        else if (!in_double_quotes && !in_single_quotes)
        {
            if (c == '(' || c == '[')
                depth++;
            else if (c == ')' || c == ']')
            {
                if (depth <= 0)
                {
                    report_message("ERREUR [Ligne %d] : Parenthèse ou crochet fermant inattendu dans les arguments.\n",
                                   "ERROR [Line %d] : Unexpected closing parenthesis or bracket in arguments.\n",
                                   line_num);
                    report_source_hint(line_num, NULL,
                                       "Vérifie que chaque '(' ou '[' ouvert est fermé dans le bon ordre.",
                                       "Check that every opened '(' or '[' is closed in the right order.");
                    return -1;
                }
                depth--;
            }
        }

        if (end || (c == ',' && depth == 0 && !in_double_quotes && !in_single_quotes))
        {
            if (count >= max_args)
            {
                report_message("ERREUR [Ligne %d] : Trop d'arguments.\n",
                               "ERROR [Line %d] : Too many arguments.\n", line_num);
                report_source_hint(line_num, NULL,
                                   "Retire les arguments en trop ou vérifie les virgules dans l'appel.",
                                   "Remove the extra arguments or check the commas in the call.");
                return -1;
            }

            current[current_len] = '\0';
            trim_trailing_whitespace(current);
            trim_leading_whitespace(current);
            if (!safe_copy(args[count], 256, current))
            {
                report_message("ERREUR [Ligne %d] : Argument trop long.\n",
                               "ERROR [Line %d] : Argument is too long.\n", line_num);
                report_source_hint(line_num, current,
                                   "Raccourcis l'argument ou décompose l'expression en plusieurs variables.",
                                   "Shorten the argument or split the expression into intermediate variables.");
                return -1;
            }
            count++;
            current_len = 0;
            current[0] = '\0';

            if (end)
                break;
            continue;
        }

        if (current_len >= sizeof(current) - 1)
        {
            report_message("ERREUR [Ligne %d] : Argument trop long.\n",
                           "ERROR [Line %d] : Argument is too long.\n", line_num);
            report_source_hint(line_num, NULL,
                               "Raccourcis l'argument ou décompose l'expression en plusieurs variables.",
                               "Shorten the argument or split the expression into intermediate variables.");
            return -1;
        }
        current[current_len++] = c;
    }

    if (in_double_quotes || in_single_quotes || depth != 0)
    {
        report_message("ERREUR [Ligne %d] : Arguments mal formés.\n",
                       "ERROR [Line %d] : Malformed arguments.\n", line_num);
        report_source_hint(line_num, NULL,
                           "Vérifie les guillemets, parenthèses, crochets et virgules de cet appel.",
                           "Check quotes, parentheses, brackets, and commas in this call.");
        return -1;
    }

    if (count == 1 && args[0][0] == '\0')
        return 0;

    return count;
}

static int parse_call_arguments(const char *line, const char *name, char args[][256], int max_args, int line_num)
{
    size_t name_len = strlen(name);
    const char *start;
    const char *end;
    char inner[1024] = {0};

    if (strncmp(line, name, name_len) != 0 || line[name_len] != '(')
        return -2;

    start = line + name_len + 1;
    end = strrchr(start, ')');
    if (end == NULL)
    {
        report_message("ERREUR [Ligne %d] : Appel '%s' mal formé.\n",
                       "ERROR [Line %d] : Malformed '%s' call.\n", line_num, name);
        report_source_hint(line_num, name,
                           "Ajoute la parenthèse fermante de l'appel.",
                           "Add the closing parenthesis for this call.");
        return -1;
    }

    if (!copy_slice(inner, sizeof(inner), start, (size_t)(end - start)))
    {
        report_message("ERREUR [Ligne %d] : Appel '%s' trop long.\n",
                       "ERROR [Line %d] : '%s' call is too long.\n", line_num, name);
        report_source_hint(line_num, name,
                           "Décompose cet appel en variables intermédiaires plus courtes.",
                           "Split this call into shorter intermediate variables.");
        return -1;
    }

    char trailing[64] = {0};
    if (!safe_copy(trailing, sizeof(trailing), end + 1))
    {
        report_message("ERREUR [Ligne %d] : Appel '%s' mal formé.\n",
                       "ERROR [Line %d] : Malformed '%s' call.\n", line_num, name);
        report_source_hint(line_num, name,
                           "Retire le texte après la parenthèse fermante ou termine l'appel par ';'.",
                           "Remove text after the closing parenthesis or end the call with ';'.");
        return -1;
    }
    trim_trailing_whitespace(trailing);
    trim_leading_whitespace(trailing);
    if (strcmp(trailing, "") != 0 && strcmp(trailing, ";") != 0)
    {
        report_message("ERREUR [Ligne %d] : Texte inattendu après '%s(...)'.\n",
                       "ERROR [Line %d] : Unexpected text after '%s(...)'.\n", line_num, name);
        report_source_hint(line_num, name,
                           "Un appel helper doit occuper toute l'expression ou toute la ligne.",
                           "A helper call must occupy the whole expression or the whole line.");
        return -1;
    }

    return parse_arguments(inner, args, max_args, line_num);
}

static bool transform_logical_operators(const char *input, char *output, size_t output_size)
{
    bool in_double_quotes = false;
    bool in_single_quotes = false;

    output[0] = '\0';
    for (const char *p = input; *p != '\0';)
    {
        if (*p == '"' && !in_single_quotes)
            in_double_quotes = !in_double_quotes;
        else if (*p == '\'' && !in_double_quotes)
            in_single_quotes = !in_single_quotes;

        if (!in_double_quotes && !in_single_quotes &&
            (p == input || !is_identifier_char(*(p - 1))))
        {
            if (starts_with_word(p, "and"))
            {
                if (!append_checked(output, output_size, "&&"))
                    return false;
                p += 3;
                continue;
            }
            if (starts_with_word(p, "or"))
            {
                if (!append_checked(output, output_size, "||"))
                    return false;
                p += 2;
                continue;
            }
            if (starts_with_word(p, "not"))
            {
                if (!append_checked(output, output_size, "!"))
                    return false;
                p += 3;
                continue;
            }
        }

        char tmp[2] = {*p, '\0'};
        if (!append_checked(output, output_size, tmp))
            return false;
        p++;
    }

    return true;
}

static bool transform_len_calls(const char *input, char *output, size_t output_size, int line_num)
{
    bool in_double_quotes = false;
    bool in_single_quotes = false;

    output[0] = '\0';
    for (const char *p = input; *p != '\0';)
    {
        if (*p == '"' && !in_single_quotes)
            in_double_quotes = !in_double_quotes;
        else if (*p == '\'' && !in_double_quotes)
            in_single_quotes = !in_single_quotes;

        if (!in_double_quotes && !in_single_quotes &&
            strncmp(p, "len(", 4) == 0 &&
            (p == input || !is_identifier_char(*(p - 1))))
        {
            const char *arg_start = p + 4;
            const char *arg_end = strchr(arg_start, ')');
            char list_name[64] = {0};

            if (arg_end == NULL)
            {
                report_message("ERREUR [Ligne %d] : Appel len(...) mal formé.\n",
                               "ERROR [Line %d] : Malformed len(...) call.\n", line_num);
                report_source_hint(line_num, "len(",
                                   "Écris len(nom_de_liste) avec une parenthèse fermante.",
                                   "Write len(list_name) with a closing parenthesis.");
                return false;
            }
            if (!copy_slice(list_name, sizeof(list_name), arg_start, (size_t)(arg_end - arg_start)))
            {
                report_message("ERREUR [Ligne %d] : Argument len(...) trop long.\n",
                               "ERROR [Line %d] : len(...) argument is too long.\n", line_num);
                report_source_hint(line_num, "len(",
                                   "Utilise un nom de liste simple, pas une expression longue.",
                                   "Use a simple list name, not a long expression.");
                return false;
            }
            trim_trailing_whitespace(list_name);
            trim_leading_whitespace(list_name);
            if (!is_valid_identifier(list_name))
            {
                report_message("ERREUR [Ligne %d] : len(...) attend un nom de liste simple.\n",
                               "ERROR [Line %d] : len(...) expects a simple list name.\n", line_num);
                report_source_hint(line_num, "len(",
                                   "Utilise len(numbers), pas len(numbers[0]) ni len(a + b).",
                                   "Use len(numbers), not len(numbers[0]) or len(a + b).");
                return false;
            }
            const char *list_type = get_symbol_type(list_name);
            if (list_type == NULL ||
                (strcmp(list_type, "int*") != 0 &&
                 strcmp(list_type, "float*") != 0 &&
                 strcmp(list_type, "char**") != 0 &&
                 strcmp(list_type, "bool*") != 0))
            {
                report_message("ERREUR [Ligne %d] : len(...) attend une liste déjà déclarée.\n",
                               "ERROR [Line %d] : len(...) expects an already declared list.\n", line_num);
                report_source_hint(line_num, list_name,
                                   "Déclare d'abord la liste avec une affectation comme numbers = [1, 2, 3].",
                                   "Declare the list first with an assignment such as numbers = [1, 2, 3].");
                return false;
            }
            if (!append_checked(output, output_size, list_name) ||
                !append_checked(output, output_size, "_len"))
            {
                report_message("ERREUR [Ligne %d] : Expression trop longue après len(...).\n",
                               "ERROR [Line %d] : Expression is too long after len(...).\n", line_num);
                report_source_hint(line_num, "len(",
                                   "Décompose cette expression en plusieurs lignes plus courtes.",
                                   "Split this expression across shorter intermediate lines.");
                return false;
            }
            p = arg_end + 1;
            continue;
        }

        char tmp[2] = {*p, '\0'};
        if (!append_checked(output, output_size, tmp))
            return false;
        p++;
    }

    return true;
}

static const char *find_call_close(const char *arg_start)
{
    int depth = 1;
    bool in_double_quotes = false;
    bool in_single_quotes = false;

    for (const char *p = arg_start; *p != '\0'; p++)
    {
        if (*p == '"' && !in_single_quotes)
            in_double_quotes = !in_double_quotes;
        else if (*p == '\'' && !in_double_quotes)
            in_single_quotes = !in_single_quotes;
        else if (!in_double_quotes && !in_single_quotes)
        {
            if (*p == '(')
                depth++;
            else if (*p == ')')
            {
                depth--;
                if (depth == 0)
                    return p;
            }
        }
    }

    return NULL;
}

static bool transform_native_expr_calls(const char *input, char *output, size_t output_size, int line_num)
{
    bool in_double_quotes = false;
    bool in_single_quotes = false;

    output[0] = '\0';
    for (const char *p = input; *p != '\0';)
    {
        if (*p == '"' && !in_single_quotes)
            in_double_quotes = !in_double_quotes;
        else if (*p == '\'' && !in_double_quotes)
            in_single_quotes = !in_single_quotes;

        if (!in_double_quotes && !in_single_quotes &&
            (p == input || !is_identifier_char(*(p - 1))))
        {
            const char *name = NULL;
            int name_len = 0;

            if (strncmp(p, "str_len(", 8) == 0)
            {
                name = "str_len";
                name_len = 7;
            }
            else if (strncmp(p, "str_eq(", 7) == 0)
            {
                name = "str_eq";
                name_len = 6;
            }
            else if (strncmp(p, "contains(", 9) == 0)
            {
                name = "contains";
                name_len = 8;
            }

            if (name != NULL)
            {
                const char *arg_start = p + name_len + 1;
                const char *arg_end = find_call_close(arg_start);
                char raw_args[512] = {0};
                char args[2][256] = {{0}};
                int arg_count;

                if (arg_end == NULL || !copy_slice(raw_args, sizeof(raw_args), arg_start, (size_t)(arg_end - arg_start)))
                {
                    report_message("ERREUR E_HELPER_ARGS [Ligne %d] : Appel %s(...) mal formé.\n",
                                   "ERROR E_HELPER_ARGS [Line %d] : Malformed %s(...) call.\n",
                                   line_num, name);
                    report_source_hint(line_num, name,
                                       "Vérifie les parenthèses et limite la taille des arguments.",
                                       "Check parentheses and keep arguments short.");
                    return false;
                }

                arg_count = parse_arguments(raw_args, args, 2, line_num);
                if (arg_count < 0)
                    return false;

                if (strcmp(name, "str_len") == 0)
                {
                    char arg[512] = {0};
                    if (arg_count != 1 || !transform_expression(args[0], arg, sizeof(arg), line_num))
                    {
                        report_message("ERREUR E_HELPER_ARGS [Ligne %d] : str_len(...) attend un argument.\n",
                                       "ERROR E_HELPER_ARGS [Line %d] : str_len(...) expects one argument.\n",
                                       line_num);
                        report_source_hint(line_num, "str_len",
                                           "Utilise str_len(texte).",
                                           "Use str_len(text).");
                        return false;
                    }
                    if (!append_checked(output, output_size, "nl_str_len(") ||
                        !append_checked(output, output_size, arg) ||
                        !append_checked(output, output_size, ")"))
                        return false;
                }
                else if (strcmp(name, "str_eq") == 0)
                {
                    char left[512] = {0};
                    char right[512] = {0};
                    if (arg_count != 2 ||
                        !transform_expression(args[0], left, sizeof(left), line_num) ||
                        !transform_expression(args[1], right, sizeof(right), line_num))
                    {
                        report_message("ERREUR E_HELPER_ARGS [Ligne %d] : str_eq(...) attend deux arguments.\n",
                                       "ERROR E_HELPER_ARGS [Line %d] : str_eq(...) expects two arguments.\n",
                                       line_num);
                        report_source_hint(line_num, "str_eq",
                                           "Utilise str_eq(gauche, droite).",
                                           "Use str_eq(left, right).");
                        return false;
                    }
                    if (!append_checked(output, output_size, "nl_str_eq(") ||
                        !append_checked(output, output_size, left) ||
                        !append_checked(output, output_size, ", ") ||
                        !append_checked(output, output_size, right) ||
                        !append_checked(output, output_size, ")"))
                        return false;
                }
                else
                {
                    char value[512] = {0};
                    const char *list_type;
                    const char *elem_type;
                    const char *suffix;

                    if (arg_count != 2 || !is_valid_identifier(args[0]))
                    {
                        report_message("ERREUR E_HELPER_ARGS [Ligne %d] : contains(...) attend une liste et une valeur.\n",
                                       "ERROR E_HELPER_ARGS [Line %d] : contains(...) expects a list and a value.\n",
                                       line_num);
                        report_source_hint(line_num, "contains",
                                           "Utilise contains(values, value) avec un nom de liste simple.",
                                           "Use contains(values, value) with a simple list name.");
                        return false;
                    }

                    list_type = get_symbol_type(args[0]);
                    if (!is_list_type(list_type))
                    {
                        report_message("ERREUR E_APPEND_TARGET [Ligne %d] : contains(...) attend une liste déjà déclarée.\n",
                                       "ERROR E_APPEND_TARGET [Line %d] : contains(...) expects an already declared list.\n",
                                       line_num);
                        report_source_hint(line_num, args[0],
                                           "Déclare d'abord la liste avec values = [1, 2, 3].",
                                           "Declare the list first with values = [1, 2, 3].");
                        return false;
                    }

                    if (!transform_expression(args[1], value, sizeof(value), line_num))
                        return false;

                    elem_type = get_elem_type(args[0]);
                    suffix = runtime_suffix_for_elem(elem_type);
                    if (suffix == NULL || !check_assignment_type(args[0], elem_type, value, line_num))
                        return false;

                    if (!append_checked(output, output_size, "nl_contains_") ||
                        !append_checked(output, output_size, suffix) ||
                        !append_checked(output, output_size, "(") ||
                        !append_checked(output, output_size, args[0]) ||
                        !append_checked(output, output_size, ", ") ||
                        !append_checked(output, output_size, args[0]) ||
                        !append_checked(output, output_size, "_len, ") ||
                        !append_checked(output, output_size, value) ||
                        !append_checked(output, output_size, ")"))
                        return false;
                }

                p = arg_end + 1;
                continue;
            }
        }

        char tmp[2] = {*p, '\0'};
        if (!append_checked(output, output_size, tmp))
            return false;
        p++;
    }

    return true;
}

static bool transform_expression(const char *input, char *output, size_t output_size, int line_num)
{
    char len_buf[2048] = {0};
    char helper_buf[2048] = {0};

    if (!transform_len_calls(input, len_buf, sizeof(len_buf), line_num))
        return false;
    if (!transform_native_expr_calls(len_buf, helper_buf, sizeof(helper_buf), line_num))
        return false;
    if (!transform_logical_operators(helper_buf, output, output_size))
    {
        report_message("ERREUR [Ligne %d] : Expression trop longue.\n",
                       "ERROR [Line %d] : Expression is too long.\n", line_num);
        report_source_hint(line_num, NULL,
                           "Réduis l'expression ou stocke une partie du calcul dans une variable.",
                           "Shorten the expression or store part of the calculation in a variable.");
        return false;
    }
    return true;
}

static bool parse_import_line(const char *line, char *out_include, size_t out_size, int line_num)
{
    char target[256] = {0};
    char include_path[256] = {0};
    size_t len;

    if (strncmp(line, "import ", 7) != 0)
        return false;

    if (!safe_copy(target, sizeof(target), line + 7))
    {
        report_message("ERREUR [Ligne %d] : Import trop long.\n",
                       "ERROR [Line %d] : Import target is too long.\n", line_num);
        report_source_hint(line_num, "import",
                           "Utilise un nom de fichier d'import plus court.",
                           "Use a shorter import file name.");
        return false;
    }
    trim_trailing_whitespace(target);
    trim_leading_whitespace(target);

    len = strlen(target);
    if (len == 0)
    {
        report_message("ERREUR [Ligne %d] : Import vide.\n",
                       "ERROR [Line %d] : Empty import.\n", line_num);
        report_source_hint(line_num, "import",
                           "Écris par exemple import tools, import \"tools.H\" ou import <stdio.h>.",
                           "Write for example import tools, import \"tools.H\", or import <stdio.h>.");
        return false;
    }
    if (len >= 2 && target[0] == '"' && target[len - 1] == '"')
    {
        memmove(target, target + 1, len - 2);
        target[len - 2] = '\0';
    }

    len = strlen(target);
    if (len == 0)
    {
        report_message("ERREUR [Ligne %d] : Import vide.\n",
                       "ERROR [Line %d] : Empty import.\n", line_num);
        report_source_hint(line_num, "import",
                           "Écris par exemple import tools, import \"tools.H\" ou import <stdio.h>.",
                           "Write for example import tools, import \"tools.H\", or import <stdio.h>.");
        return false;
    }
    if (target[0] == '<')
    {
        if (len < 2 || target[len - 1] != '>')
        {
            report_message("ERREUR [Ligne %d] : Import système mal formé.\n",
                           "ERROR [Line %d] : Malformed system import.\n", line_num);
            report_source_hint(line_num, "import",
                               "Un import système doit avoir la forme import <nom.h>.",
                               "A system import must use the form import <name.h>.");
            return false;
        }
    }
    else if (len > 2 && strcmp(target + len - 2, ".H") == 0)
        target[len - 1] = 'h';
    else if (strchr(target, '.') == NULL)
    {
        if (snprintf(include_path, sizeof(include_path), "%s.h", target) >= (int)sizeof(include_path))
        {
            report_message("ERREUR [Ligne %d] : Import trop long.\n",
                           "ERROR [Line %d] : Import target is too long.\n", line_num);
            report_source_hint(line_num, "import",
                               "Utilise un nom d'import plus court ou écris le #include C directement.",
                               "Use a shorter import name or write the C #include directly.");
            return false;
        }
        safe_copy(target, sizeof(target), include_path);
    }

    if (target[0] == '<')
    {
        if (snprintf(out_include, out_size, "#include %s", target) >= (int)out_size)
            return false;
    }
    else if (snprintf(out_include, out_size, "#include \"%s\"", target) >= (int)out_size)
        return false;

    return true;
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

static void write_source_comment(FILE *file, int indent_level, int line_num, const char *source)
{
    print_indent(file, indent_level);
    fprintf(file, "/* source line %d: ", line_num);
    for (const char *p = source; *p != '\0'; p++)
    {
        if (*p == '*' && *(p + 1) == '/')
        {
            fprintf(file, "* /");
            p++;
        }
        else if (isprint((unsigned char)*p) || *p == '\t')
        {
            fputc(*p, file);
        }
        else
        {
            fputc(' ', file);
        }
    }
    fprintf(file, " */\n");
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
        report_source_hint(line_num, "=",
                           "Raccourcis le nom ou la valeur, ou coupe l'expression en plusieurs affectations.",
                           "Shorten the name or value, or split the expression into multiple assignments.");
        return -1;
    }

    if (!copy_slice(var, var_size, line, var_len) || !safe_copy(val, val_size, eq + 1))
    {
        report_message("ERREUR [Ligne %d] : Impossible de copier l'affectation.\n",
                       "ERROR [Line %d] : Could not copy assignment.\n", line_num);
        report_source_hint(line_num, "=",
                           "Vérifie que l'affectation ressemble à nom = valeur.",
                           "Check that the assignment looks like name = value.");
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
        report_source_hint(line_num, "[",
                           "Écris une liste avec des crochets fermés, par exemple values = [1, 2, 3].",
                           "Write a list with closed brackets, for example values = [1, 2, 3].");
        return NULL;
    }

    if (!copy_slice(buf, sizeof(buf), start + 1, (size_t)(end - start - 1)))
    {
        report_message("ERREUR [Ligne %d] : Liste trop longue.\n",
                       "ERROR [Line %d] : List literal is too long.\n", line_num);
        report_source_hint(line_num, "[",
                           "Réduis la liste ou construis-la avec append(...) sur plusieurs lignes.",
                           "Shorten the list or build it with append(...) across multiple lines.");
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
            report_source_hint(line_num, token,
                               "Stocke cet élément dans une variable avant de l'ajouter à la liste.",
                               "Store this element in a variable before adding it to the list.");
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
                report_source_hint(line_num, "[",
                                   "Utilise une liste plus courte ou ajoute des éléments progressivement avec append(...).",
                                   "Use a shorter list or add elements progressively with append(...).");
                return NULL;
            }
            if (!safe_copy(elements[*count_out], 64, elem))
            {
                report_message("ERREUR [Ligne %d] : Élément de liste trop long '%s'.\n",
                               "ERROR [Line %d] : List element is too long '%s'.\n",
                               line_num, elem);
                report_source_hint(line_num, elem,
                                   "Raccourcis cet élément ou stocke-le dans une variable.",
                                   "Shorten this element or store it in a variable.");
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
                report_source_hint(line_num, elem,
                                   "Les listes doivent être homogènes : garde un seul type d'élément.",
                                   "Lists must be homogeneous: keep only one element type.");
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
                        report_source_hint(line_num, "{",
                                           "Raccourcis l'expression entre accolades ou utilise une variable intermédiaire.",
                                           "Shorten the expression inside braces or use an intermediate variable.");
                        return false;
                    }
                    var_name[v_idx++] = *p++;
                }

                if (*p != '}')
                {
                    report_message("ERREUR [Ligne %d] : Interpolation print non fermée.\n",
                                   "ERROR [Line %d] : Unclosed print interpolation.\n",
                                   line_num);
                    report_source_hint(line_num, "{",
                                       "Ajoute le '}' manquant dans la chaîne print.",
                                       "Add the missing '}' in the print string.");
                    return false;
                }
                if (v_idx == 0)
                {
                    report_message("ERREUR [Ligne %d] : Interpolation print vide.\n",
                                   "ERROR [Line %d] : Empty print interpolation.\n",
                                   line_num);
                    report_source_hint(line_num, "{}",
                                       "Place un nom ou une expression entre accolades, par exemple {value}.",
                                       "Put a name or expression inside braces, for example {value}.");
                    return false;
                }
                p++;

                char transformed_var[256] = {0};
                if (!transform_expression(var_name, transformed_var, sizeof(transformed_var), line_num))
                    return false;

                const char *specifier = get_format_specifier(transformed_var, line_num);
                if (!append_checked(format_str, sizeof(format_str), specifier))
                {
                    report_message("ERREUR [Ligne %d] : Format print trop long.\n",
                                   "ERROR [Line %d] : Print format is too long.\n", line_num);
                    report_source_hint(line_num, "print",
                                       "Raccourcis la chaîne ou découpe l'affichage en plusieurs print(...).",
                                       "Shorten the string or split the output across multiple print(...) calls.");
                    return false;
                }
                f_out = format_str + strlen(format_str);

                if (strlen(args) > 0 && !append_checked(args, sizeof(args), ", "))
                {
                    report_message("ERREUR [Ligne %d] : Liste d'arguments print trop longue.\n",
                                   "ERROR [Line %d] : Print argument list is too long.\n", line_num);
                    report_source_hint(line_num, "print",
                                       "Réduis le nombre d'interpolations ou utilise plusieurs print(...).",
                                       "Reduce the number of interpolations or use multiple print(...) calls.");
                    return false;
                }
                if (!append_checked(args, sizeof(args), transformed_var))
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
                report_source_hint(line_num, "print",
                                   "Raccourcis cette instruction print ou découpe-la.",
                                   "Shorten this print statement or split it.");
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
                report_source_hint(line_num, "print",
                                   "Stocke l'expression dans une variable avant de l'afficher.",
                                   "Store the expression in a variable before printing it.");
                return false;
            }
            trim_trailing_whitespace(expr);
            char transformed_expr[512] = {0};
            if (!transform_expression(expr, transformed_expr, sizeof(transformed_expr), line_num))
                return false;
            const char *spec = get_format_specifier(transformed_expr, line_num);
            if (snprintf(out_buf, out_size, "printf(\"%s\\n\", %s);", spec, transformed_expr) >= (int)out_size)
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

static bool is_allocated_ptr_name(const char *name)
{
    for (int i = 0; i < allocated_count; i++)
    {
        if (strcmp(allocated_ptrs[i].name, name) == 0)
            return true;
    }
    return false;
}

static void remove_allocated_ptr_name(const char *name)
{
    int new_count = 0;

    for (int i = 0; i < allocated_count; i++)
    {
        if (strcmp(allocated_ptrs[i].name, name) != 0)
            allocated_ptrs[new_count++] = allocated_ptrs[i];
    }
    allocated_count = new_count;
}

static void write_allocation_failure_return(FILE *file, int indent_level, const char *ptr_name)
{
    print_indent(file, indent_level);
    fprintf(file, "if (%s == NULL)\n", ptr_name);
    print_indent(file, indent_level);
    fprintf(file, "{\n");
    generate_frees_except(file, indent_level + 1, ptr_name);
    print_indent(file, indent_level + 1);
    fprintf(file, "return -1;\n");
    print_indent(file, indent_level);
    fprintf(file, "}\n");
}

static bool write_input_assignment(FILE *file, int indent_level, const char *var, const char *val, int line_num)
{
    char args[1][256] = {{0}};
    int arg_count = parse_call_arguments(val, "input", args, 1, line_num);
    const char *existing_type;

    if (arg_count == -2)
        return false;
    if (arg_count < 0)
        return false;
    if (arg_count > 1)
    {
        report_message("ERREUR [Ligne %d] : input(...) accepte au plus un argument.\n",
                       "ERROR [Line %d] : input(...) accepts at most one argument.\n", line_num);
        report_source_hint(line_num, "input",
                           "Utilise input() ou input(\"message: \").",
                           "Use input() or input(\"message: \").");
        return false;
    }

    existing_type = get_symbol_type(var);
    if (existing_type != NULL && strcmp(existing_type, "char*") != 0)
    {
        report_message("ERREUR [Ligne %d] : input(...) doit être assigné à une chaîne.\n",
                       "ERROR [Line %d] : input(...) must be assigned to a string.\n", line_num);
        report_source_hint(line_num, var,
                           "Assigne input(...) à une nouvelle variable chaîne, ou réutilise une variable char*.",
                           "Assign input(...) to a new string variable, or reuse a char* variable.");
        return false;
    }

    if (existing_type == NULL)
    {
        char prompt[512] = {0};
        const char *prompt_expr = "NULL";
        if (arg_count == 1 && args[0][0] != '\0')
        {
            if (!transform_expression(args[0], prompt, sizeof(prompt), line_num))
                return false;
            prompt_expr = prompt;
        }

        print_indent(file, indent_level);
        fprintf(file, "char *%s = nl_input(%s);\n", var, prompt_expr);
        if (!add_symbol(var, "char*", indent_level, line_num)) return false;
        if (!add_allocated_ptr(var, indent_level, line_num)) return false;
        write_allocation_failure_return(file, indent_level, var);
    }
    else
    {
        char prompt[512] = {0};
        char tmp_name[64] = {0};
        const char *prompt_expr = "NULL";
        if (!transform_expression(args[0], prompt, sizeof(prompt), line_num))
        {
            if (arg_count == 1 && args[0][0] != '\0')
                return false;
        }
        if (arg_count == 1 && args[0][0] != '\0')
            prompt_expr = prompt;
        snprintf(tmp_name, sizeof(tmp_name), "__input_tmp_%d", line_num);

        print_indent(file, indent_level);
        fprintf(file, "char *%s = nl_input(%s);\n", tmp_name, prompt_expr);
        write_allocation_failure_return(file, indent_level, tmp_name);
        if (is_allocated_ptr_name(var))
        {
            print_indent(file, indent_level);
            fprintf(file, "if (%s != NULL) { free(%s); %s = NULL; }\n", var, var, var);
        }
        print_indent(file, indent_level);
        fprintf(file, "%s = %s;\n", var, tmp_name);
        if (!add_allocated_ptr(var, indent_level, line_num)) return false;
    }

    return true;
}

static bool write_str_concat_assignment(FILE *file, int indent_level, const char *var, const char *val, int line_num)
{
    char args[2][256] = {{0}};
    char left[512] = {0};
    char right[512] = {0};
    int arg_count = parse_call_arguments(val, "str_concat", args, 2, line_num);
    const char *existing_type;

    if (arg_count == -2)
        return false;
    if (arg_count < 0)
        return false;
    if (arg_count != 2)
    {
        report_message("ERREUR [Ligne %d] : str_concat(...) attend deux arguments.\n",
                       "ERROR [Line %d] : str_concat(...) expects two arguments.\n", line_num);
        report_source_hint(line_num, "str_concat",
                           "Utilise str_concat(gauche, droite), par exemple str_concat(\"Hello \", name).",
                           "Use str_concat(left, right), for example str_concat(\"Hello \", name).");
        return false;
    }
    if (!transform_expression(args[0], left, sizeof(left), line_num) ||
        !transform_expression(args[1], right, sizeof(right), line_num))
        return false;

    existing_type = get_symbol_type(var);
    if (existing_type != NULL && strcmp(existing_type, "char*") != 0)
    {
        report_message("ERREUR [Ligne %d] : str_concat(...) doit être assigné à une chaîne.\n",
                       "ERROR [Line %d] : str_concat(...) must be assigned to a string.\n", line_num);
        report_source_hint(line_num, var,
                           "Assigne str_concat(...) à une nouvelle variable chaîne, ou à une variable char* existante.",
                           "Assign str_concat(...) to a new string variable, or to an existing char* variable.");
        return false;
    }

    if (existing_type == NULL)
    {
        print_indent(file, indent_level);
        fprintf(file, "char *%s = nl_str_concat(%s, %s);\n", var, left, right);
        if (!add_symbol(var, "char*", indent_level, line_num)) return false;
        if (!add_allocated_ptr(var, indent_level, line_num)) return false;
        write_allocation_failure_return(file, indent_level, var);
    }
    else
    {
        char tmp_name[64] = {0};
        snprintf(tmp_name, sizeof(tmp_name), "__str_concat_tmp_%d", line_num);
        print_indent(file, indent_level);
        fprintf(file, "char *%s = nl_str_concat(%s, %s);\n", tmp_name, left, right);
        write_allocation_failure_return(file, indent_level, tmp_name);
        if (is_allocated_ptr_name(var))
        {
            print_indent(file, indent_level);
            fprintf(file, "if (%s != NULL) { free(%s); %s = NULL; }\n", var, var, var);
        }
        print_indent(file, indent_level);
        fprintf(file, "%s = %s;\n", var, tmp_name);
        if (!add_allocated_ptr(var, indent_level, line_num)) return false;
    }

    return true;
}

static bool write_str_copy_assignment(FILE *file, int indent_level, const char *var, const char *val, int line_num)
{
    char args[1][256] = {{0}};
    char value[512] = {0};
    int arg_count = parse_call_arguments(val, "str_copy", args, 1, line_num);
    const char *existing_type;

    if (arg_count < 0)
        return false;
    if (arg_count != 1)
    {
        report_message("ERREUR E_HELPER_ARGS [Ligne %d] : str_copy(...) attend un argument.\n",
                       "ERROR E_HELPER_ARGS [Line %d] : str_copy(...) expects one argument.\n",
                       line_num);
        report_source_hint(line_num, "str_copy",
                           "Utilise str_copy(texte) pour créer une chaîne modifiable.",
                           "Use str_copy(text) to create a mutable string.");
        return false;
    }
    if (!transform_expression(args[0], value, sizeof(value), line_num))
        return false;

    existing_type = get_symbol_type(var);
    if (existing_type != NULL && strcmp(existing_type, "char*") != 0)
    {
        report_message("ERREUR E_ASSIGN_TYPE [Ligne %d] : str_copy(...) doit être assigné à une chaîne.\n",
                       "ERROR E_ASSIGN_TYPE [Line %d] : str_copy(...) must be assigned to a string.\n",
                       line_num);
        report_source_hint(line_num, var,
                           "Assigne str_copy(...) à une nouvelle variable chaîne, ou à une variable char* existante.",
                           "Assign str_copy(...) to a new string variable, or to an existing char* variable.");
        return false;
    }

    if (existing_type == NULL)
    {
        print_indent(file, indent_level);
        fprintf(file, "char *%s = nl_str_copy(%s);\n", var, value);
        if (!add_symbol(var, "char*", indent_level, line_num)) return false;
        if (!add_allocated_ptr(var, indent_level, line_num)) return false;
        write_allocation_failure_return(file, indent_level, var);
    }
    else
    {
        char tmp_name[64] = {0};
        snprintf(tmp_name, sizeof(tmp_name), "__str_copy_tmp_%d", line_num);
        print_indent(file, indent_level);
        fprintf(file, "char *%s = nl_str_copy(%s);\n", tmp_name, value);
        write_allocation_failure_return(file, indent_level, tmp_name);
        if (is_allocated_ptr_name(var))
        {
            print_indent(file, indent_level);
            fprintf(file, "if (%s != NULL) { free(%s); %s = NULL; }\n", var, var, var);
        }
        print_indent(file, indent_level);
        fprintf(file, "%s = %s;\n", var, tmp_name);
        if (!add_allocated_ptr(var, indent_level, line_num)) return false;
    }

    return true;
}

static bool write_append_call(FILE *file, int indent_level, const char *line, int line_num)
{
    char args[2][256] = {{0}};
    char value[512] = {0};
    const char *list_type;
    const char *elem_type;
    const char *suffix;
    int arg_count = parse_call_arguments(line, "append", args, 2, line_num);

    if (arg_count == -2)
        return false;
    if (arg_count < 0)
        return false;
    if (arg_count != 2)
    {
        report_message("ERREUR [Ligne %d] : append(...) attend deux arguments.\n",
                       "ERROR [Line %d] : append(...) expects two arguments.\n", line_num);
        report_source_hint(line_num, "append",
                           "Utilise append(nom_de_liste, valeur).",
                           "Use append(list_name, value).");
        return false;
    }

    if (!is_valid_identifier(args[0]))
    {
        report_message("ERREUR [Ligne %d] : append(...) attend un nom de liste simple.\n",
                       "ERROR [Line %d] : append(...) expects a simple list name.\n", line_num);
        report_source_hint(line_num, "append",
                           "Le premier argument doit être le nom de la liste, par exemple append(values, 4).",
                           "The first argument must be the list name, for example append(values, 4).");
        return false;
    }

    list_type = get_symbol_type(args[0]);
    if (!is_list_type(list_type))
    {
        report_message("ERREUR E_APPEND_TARGET [Ligne %d] : append(...) attend une liste déjà déclarée.\n",
                       "ERROR E_APPEND_TARGET [Line %d] : append(...) expects an already declared list.\n", line_num);
        report_source_hint(line_num, args[0],
                           "Crée d'abord la liste avec values = [] ou values = [1, 2, 3].",
                           "Create the list first with values = [] or values = [1, 2, 3].");
        return false;
    }

    if (!transform_expression(args[1], value, sizeof(value), line_num))
        return false;

    elem_type = get_elem_type(args[0]);
    suffix = runtime_suffix_for_elem(elem_type);
    if (suffix == NULL || !check_assignment_type(args[0], elem_type, value, line_num))
        return false;

    print_indent(file, indent_level);
    fprintf(file, "if (!nl_append_%s(&%s, &%s_len, %s))\n", suffix, args[0], args[0], value);
    print_indent(file, indent_level);
    fprintf(file, "{\n");
    generate_frees(file, indent_level + 1);
    print_indent(file, indent_level + 1);
    fprintf(file, "return -1;\n");
    print_indent(file, indent_level);
    fprintf(file, "}\n");

    return true;
}

static bool write_pop_call(FILE *file, int indent_level, const char *line, int line_num)
{
    char args[1][256] = {{0}};
    const char *list_type;
    int arg_count = parse_call_arguments(line, "pop", args, 1, line_num);

    if (arg_count < 0)
        return false;
    if (arg_count != 1 || !is_valid_identifier(args[0]))
    {
        report_message("ERREUR E_HELPER_ARGS [Ligne %d] : pop(...) attend un nom de liste.\n",
                       "ERROR E_HELPER_ARGS [Line %d] : pop(...) expects a list name.\n",
                       line_num);
        report_source_hint(line_num, "pop",
                           "Utilise pop(values) avec une liste déjà déclarée.",
                           "Use pop(values) with an already declared list.");
        return false;
    }

    list_type = get_symbol_type(args[0]);
    if (!is_list_type(list_type))
    {
        report_message("ERREUR E_APPEND_TARGET [Ligne %d] : pop(...) attend une liste déjà déclarée.\n",
                       "ERROR E_APPEND_TARGET [Line %d] : pop(...) expects an already declared list.\n",
                       line_num);
        report_source_hint(line_num, args[0],
                           "Déclare d'abord la liste avec values = [1, 2, 3].",
                           "Declare the list first with values = [1, 2, 3].");
        return false;
    }

    print_indent(file, indent_level);
    fprintf(file, "if (%s_len > 0) { %s_len--; }\n", args[0], args[0]);
    return true;
}

static bool write_insert_call(FILE *file, int indent_level, const char *line, int line_num)
{
    char args[3][256] = {{0}};
    char index_expr[512] = {0};
    char value[512] = {0};
    const char *list_type;
    const char *elem_type;
    const char *suffix;
    int arg_count = parse_call_arguments(line, "insert", args, 3, line_num);

    if (arg_count < 0)
        return false;
    if (arg_count != 3 || !is_valid_identifier(args[0]))
    {
        report_message("ERREUR E_HELPER_ARGS [Ligne %d] : insert(...) attend une liste, un index et une valeur.\n",
                       "ERROR E_HELPER_ARGS [Line %d] : insert(...) expects a list, an index, and a value.\n",
                       line_num);
        report_source_hint(line_num, "insert",
                           "Utilise insert(values, index, value) avec un nom de liste simple.",
                           "Use insert(values, index, value) with a simple list name.");
        return false;
    }

    list_type = get_symbol_type(args[0]);
    if (!is_list_type(list_type))
    {
        report_message("ERREUR E_APPEND_TARGET [Ligne %d] : insert(...) attend une liste déjà déclarée.\n",
                       "ERROR E_APPEND_TARGET [Line %d] : insert(...) expects an already declared list.\n",
                       line_num);
        report_source_hint(line_num, args[0],
                           "Déclare d'abord la liste avec values = [1, 2, 3].",
                           "Declare the list first with values = [1, 2, 3].");
        return false;
    }

    if (!transform_expression(args[1], index_expr, sizeof(index_expr), line_num) ||
        !transform_expression(args[2], value, sizeof(value), line_num))
        return false;

    elem_type = get_elem_type(args[0]);
    suffix = runtime_suffix_for_elem(elem_type);
    if (suffix == NULL || !check_assignment_type(args[0], elem_type, value, line_num))
        return false;

    print_indent(file, indent_level);
    fprintf(file, "if (!nl_insert_%s(&%s, &%s_len, %s, %s))\n",
            suffix, args[0], args[0], index_expr, value);
    print_indent(file, indent_level);
    fprintf(file, "{\n");
    generate_frees(file, indent_level + 1);
    print_indent(file, indent_level + 1);
    fprintf(file, "return -1;\n");
    print_indent(file, indent_level);
    fprintf(file, "}\n");

    return true;
}

static bool parse_range_loop(const char *line, char *var_name, size_t var_size,
                             char *start, size_t start_size, char *stop, size_t stop_size,
                             char *step, size_t step_size, int line_num)
{
    const char *range_marker = " in range(";
    const char *range_start = strstr(line, range_marker);
    const char *args_start;
    const char *args_end;
    char raw_args[1024] = {0};
    char args[3][256] = {{0}};
    int arg_count;

    if (strncmp(line, "for ", 4) != 0 || range_start == NULL)
        return false;

    if (!copy_slice(var_name, var_size, line + 4, (size_t)(range_start - (line + 4))))
    {
        report_message("ERREUR [Ligne %d] : Nom de variable de boucle trop long.\n",
                       "ERROR [Line %d] : Loop variable name is too long.\n", line_num);
        report_source_hint(line_num, "for",
                           "Utilise un nom de variable de boucle plus court.",
                           "Use a shorter loop variable name.");
        return false;
    }
    trim_trailing_whitespace(var_name);
    trim_leading_whitespace(var_name);
    if (!is_valid_identifier(var_name))
    {
        report_message("ERREUR [Ligne %d] : Nom de variable de boucle invalide '%s'.\n",
                       "ERROR [Line %d] : Invalid loop variable name '%s'.\n", line_num, var_name);
        report_source_hint(line_num, var_name,
                           "Un identifiant doit commencer par une lettre ou '_' et ne contenir que lettres, chiffres ou '_'.",
                           "An identifier must start with a letter or '_' and contain only letters, digits, or '_'.");
        return false;
    }

    args_start = range_start + strlen(range_marker);
    args_end = strrchr(args_start, ')');
    if (args_end == NULL)
    {
        report_message("ERREUR [Ligne %d] : Syntaxe de range(...) invalide.\n",
                       "ERROR [Line %d] : Invalid range(...) syntax.\n", line_num);
        report_source_hint(line_num, "range",
                           "Utilise for i in range(stop):, range(start, stop): ou range(start, stop, step):.",
                           "Use for i in range(stop):, range(start, stop):, or range(start, stop, step):.");
        return false;
    }
    char trailing[64] = {0};
    if (!safe_copy(trailing, sizeof(trailing), args_end + 1))
    {
        report_message("ERREUR [Ligne %d] : Syntaxe de range(...) invalide.\n",
                       "ERROR [Line %d] : Invalid range(...) syntax.\n", line_num);
        report_source_hint(line_num, "range",
                           "Termine la boucle par ':' après range(...).",
                           "End the loop with ':' after range(...).");
        return false;
    }
    trim_trailing_whitespace(trailing);
    trim_leading_whitespace(trailing);
    if (strcmp(trailing, "") != 0 && strcmp(trailing, ":") != 0)
    {
        report_message("ERREUR [Ligne %d] : Texte inattendu après range(...).\n",
                       "ERROR [Line %d] : Unexpected text after range(...).\n", line_num);
        report_source_hint(line_num, "range",
                           "Après range(...), garde seulement ':' ou la fin de ligne.",
                           "After range(...), keep only ':' or the end of the line.");
        return false;
    }
    if (!copy_slice(raw_args, sizeof(raw_args), args_start, (size_t)(args_end - args_start)))
    {
        report_message("ERREUR [Ligne %d] : Arguments de range(...) trop longs.\n",
                       "ERROR [Line %d] : range(...) arguments are too long.\n", line_num);
        report_source_hint(line_num, "range",
                           "Stocke les bornes de range dans des variables aux noms courts.",
                           "Store range bounds in short-named variables.");
        return false;
    }

    arg_count = parse_arguments(raw_args, args, 3, line_num);
    if (arg_count < 0)
        return false;
    if (arg_count < 1 || arg_count > 3)
    {
        report_message("ERREUR [Ligne %d] : range(...) attend 1, 2 ou 3 arguments.\n",
                       "ERROR [Line %d] : range(...) expects 1, 2, or 3 arguments.\n", line_num);
        report_source_hint(line_num, "range",
                           "Les formes valides sont range(stop), range(start, stop) et range(start, stop, step).",
                           "Valid forms are range(stop), range(start, stop), and range(start, stop, step).");
        return false;
    }

    if (arg_count == 1)
    {
        safe_copy(start, start_size, "0");
        if (!transform_expression(args[0], stop, stop_size, line_num))
            return false;
        safe_copy(step, step_size, "1");
    }
    else if (arg_count == 2)
    {
        if (!transform_expression(args[0], start, start_size, line_num) ||
            !transform_expression(args[1], stop, stop_size, line_num))
            return false;
        safe_copy(step, step_size, "1");
    }
    else
    {
        if (!transform_expression(args[0], start, start_size, line_num) ||
            !transform_expression(args[1], stop, stop_size, line_num) ||
            !transform_expression(args[2], step, step_size, line_num))
            return false;
    }

    return true;
}

static bool parse_foreach_loop(const char *line, char *item_name, size_t item_size,
                               char *list_name, size_t list_size, int line_num)
{
    const char *marker = " in ";
    const char *list_start;
    char trailing[128] = {0};
    size_t len;

    if (strncmp(line, "for ", 4) != 0)
        return false;

    list_start = strstr(line + 4, marker);
    if (list_start == NULL || strstr(line, " in range(") != NULL)
        return false;

    if (!copy_slice(item_name, item_size, line + 4, (size_t)(list_start - (line + 4))))
    {
        report_message("ERREUR [Ligne %d] : Nom de variable foreach trop long.\n",
                       "ERROR [Line %d] : foreach variable name is too long.\n", line_num);
        report_source_hint(line_num, "for",
                           "Utilise un nom de variable plus court.",
                           "Use a shorter variable name.");
        return false;
    }
    trim_trailing_whitespace(item_name);
    trim_leading_whitespace(item_name);

    if (!is_valid_identifier(item_name))
    {
        report_message("ERREUR [Ligne %d] : Variable foreach invalide '%s'.\n",
                       "ERROR [Line %d] : Invalid foreach variable '%s'.\n",
                       line_num, item_name);
        report_source_hint(line_num, item_name,
                           "Un identifiant doit commencer par une lettre ou '_' et ne contenir que lettres, chiffres ou '_'.",
                           "An identifier must start with a letter or '_' and contain only letters, digits, or '_'.");
        return false;
    }

    if (!safe_copy(trailing, sizeof(trailing), list_start + strlen(marker)))
    {
        report_message("ERREUR [Ligne %d] : Nom de liste foreach trop long.\n",
                       "ERROR [Line %d] : foreach list name is too long.\n", line_num);
        report_source_hint(line_num, "in",
                           "Utilise un nom de liste simple et plus court.",
                           "Use a simple, shorter list name.");
        return false;
    }
    trim_trailing_whitespace(trailing);
    trim_leading_whitespace(trailing);
    len = strlen(trailing);
    if (len > 0 && trailing[len - 1] == ':')
    {
        trailing[len - 1] = '\0';
        trim_trailing_whitespace(trailing);
    }

    if (!is_valid_identifier(trailing))
    {
        report_message("ERREUR [Ligne %d] : foreach attend un nom de liste simple.\n",
                       "ERROR [Line %d] : foreach expects a simple list name.\n", line_num);
        report_source_hint(line_num, "in",
                           "Utilise for item in values: avec une liste déjà déclarée.",
                           "Use for item in values: with an already declared list.");
        return false;
    }

    if (!is_list_type(get_symbol_type(trailing)))
    {
        report_message("ERREUR E_APPEND_TARGET [Ligne %d] : foreach attend une liste déjà déclarée.\n",
                       "ERROR E_APPEND_TARGET [Line %d] : foreach expects an already declared list.\n", line_num);
        report_source_hint(line_num, trailing,
                           "Déclare la liste avant la boucle, par exemple values = [1, 2, 3].",
                           "Declare the list before the loop, for example values = [1, 2, 3].");
        return false;
    }

    return safe_copy(list_name, list_size, trailing);
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
    memset(scope_returned, 0, sizeof(scope_returned));
    pending_foreach_decl[0] = '\0';
    emitted_include_count = 0;
    set_diagnostic_source(filename, 0, NULL);

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

    emit_include_once(file, "#include <stdio.h>");
    emit_include_once(file, "#include <stdlib.h>");
    emit_include_once(file, "#include <stdbool.h>");
    emit_include_once(file, "#include <string.h>");
    emit_include_once(file, "#include \"runtime.h\"");
    fprintf(file, "\n");

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
            report_source_hint(line_number + 1, NULL,
                               "Raccourcis cette ligne ou découpe-la en plusieurs instructions.",
                               "Shorten this line or split it into multiple statements.");
            goto error_cleanup;
        }

        if (strchr(text[i], '\n') != NULL)
        {
            line_number++;
            trim_trailing_whitespace(line_buf);
            char source_comment_line[2048] = {0};
            if (!safe_copy(source_comment_line, sizeof(source_comment_line), line_buf))
            {
                report_message("ERREUR [Ligne %d] : Ligne source trop longue.\n",
                               "ERROR [Line %d] : Source line is too long.\n", line_number);
                report_source_hint(line_number, NULL,
                                   "Raccourcis cette ligne ou découpe-la en plusieurs instructions.",
                                   "Shorten this line or split it into multiple statements.");
                goto error_cleanup;
            }
            set_diagnostic_source(filename, line_number, source_comment_line);
            normalize_booleans(line_buf);

            int current_indent = get_indentation_level(line_buf);

            char *line = line_buf;
            while (*line == ' ' || *line == '\t') line++;

            size_t line_len = strlen(line);

            if (line_len > 0)
            {
                if (pending_foreach_decl[0] != '\0' && current_indent <= indent_stack[indent_top])
                {
                    report_message("ERREUR [Ligne %d] : foreach attend un bloc indenté.\n",
                                   "ERROR [Line %d] : foreach expects an indented block.\n",
                                   line_number);
                    report_source_hint(line_number, "for",
                                       "Indente au moins une instruction sous la boucle foreach.",
                                       "Indent at least one statement under the foreach loop.");
                    goto error_cleanup;
                }

                while (indent_top > 0 && current_indent < indent_stack[indent_top])
                {
                    if (scope_returned[indent_top])
                        discard_scope_allocations(indent_top);
                    else
                        generate_scope_frees(file, indent_top);
                    scope_returned[indent_top] = false;
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
                        scope_returned[indent_top] = false;
                        if (pending_foreach_decl[0] != '\0')
                        {
                            print_indent(file, indent_top);
                            fprintf(file, "%s\n", pending_foreach_decl);
                            pending_foreach_decl[0] = '\0';
                        }
                    }
                    else
                    {
                        report_message("ERREUR [Ligne %d] : Dépassement du niveau d'indentation maximal (%d).\n",
                                       "ERROR [Line %d] : Maximum indentation level exceeded (%d).\n",
                                       line_number, MAX_INDENT_LEVELS);
                        report_source_hint(line_number, NULL,
                                           "Réduis l'imbrication ou extrais une partie du code dans une fonction.",
                                           "Reduce nesting or extract part of the code into a function.");
                        fclose(file);
                        remove(new_name);
                        free(new_name);
                        return 1;
                    }
                }

                if (flag_comments)
                {
                    char *comment_line = source_comment_line;
                    while (*comment_line == ' ' || *comment_line == '\t') comment_line++;
                    write_source_comment(file, indent_top, line_number, comment_line);
                }

                if (strcmp(line, "#all") == 0)
                {
                    emit_include_once(file, "#include <math.h>");
                    emit_include_once(file, "#include <time.h>");
                    trace_translation(line_number, "directive", "#all");
                }
                else if (strcmp(line, "#linux") == 0)
                {
                    emit_include_once(file, "#include <unistd.h>");
                    emit_include_once(file, "#include <sys/types.h>");
                    emit_include_once(file, "#include <sys/wait.h>");
                    trace_translation(line_number, "directive", "#linux");
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
                else if (strncmp(line, "import ", 7) == 0)
                {
                    char include_line[512] = {0};
                    if (!parse_import_line(line, include_line, sizeof(include_line), line_number))
                        goto error_cleanup;
                    emit_include_once(file, include_line);
                    trace_translation(line_number, "import", include_line);
                }
                else if (strncmp(line, "#include", 8) == 0 || strncmp(line, "#define", 7) == 0 || strncmp(line, "#ifdef", 6) == 0 || strncmp(line, "#ifndef", 7) == 0 || strncmp(line, "#endif", 6) == 0)
                {
                    if (strncmp(line, "#include", 8) == 0)
                        emit_include_once(file, line);
                    else
                        fprintf(file, "%s\n", line);
                }
                else if (line[0] == '#')
                {
                    print_indent(file, indent_top);
                    fprintf(file, "// %s\n", line + 1);
                }
                else if (strncmp(line, "int main()", 10) == 0)
                {
                    if (flag_pretty_c)
                        fprintf(file, "\n");
                    if (!add_symbol("main", "int", indent_top, line_number)) goto error_cleanup;
                    print_indent(file, indent_top);
                    fprintf(file, "int main()\n");
                    trace_translation(line_number, "function", "main");
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
                            report_source_hint(line_number, "match",
                                               "Utilise la forme match value with, puis des branches indentées commençant par |.",
                                               "Use the form match value with, followed by indented branches starting with |.");
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
                            report_source_hint(line_number, "|",
                                               "Utilise une branche comme | 1 -> print(\"one\") ou | _ -> print(\"other\").",
                                               "Use a branch such as | 1 -> print(\"one\") or | _ -> print(\"other\").");
                            goto error_cleanup;
                        }
                        trim_trailing_whitespace(val_pat);
                        trim_trailing_whitespace(action);

                        char var_pat[64] = {0};
                        char guard_cond[128] = {0};
                        bool has_guard = false;

                        if (strstr(val_pat, " if ") != NULL)
                        {
                            char transformed_guard[256] = {0};
                            has_guard = true;
                            sscanf(val_pat, "%63s if %127[^\n]", var_pat, guard_cond);
                            trim_trailing_whitespace(var_pat);
                            trim_trailing_whitespace(guard_cond);
                            if (!transform_expression(guard_cond, transformed_guard, sizeof(transformed_guard), line_number))
                                goto error_cleanup;
                            safe_copy(guard_cond, sizeof(guard_cond), transformed_guard);
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
                            fprintf(file, "int %s_len = %s_len - 1;\n", tail_var, current_match_var);
                            if (!add_list_len_symbol(tail_var, indent_top + 1, line_number)) goto error_cleanup;

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
                    else if (strncmp(line, "append(", 7) == 0)
                    {
                        if (!write_append_call(file, indent_top, line, line_number)) goto error_cleanup;
                        trace_translation(line_number, "helper", "append");
                    }
                    else if (strncmp(line, "insert(", 7) == 0)
                    {
                        if (!write_insert_call(file, indent_top, line, line_number)) goto error_cleanup;
                        trace_translation(line_number, "helper", "insert");
                    }
                    else if (strncmp(line, "pop(", 4) == 0)
                    {
                        if (!write_pop_call(file, indent_top, line, line_number)) goto error_cleanup;
                        trace_translation(line_number, "helper", "pop");
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
                            if (!add_list_len_symbol(tail_var, indent_top, line_number)) goto error_cleanup;
                        }
                        else
                        {
                            report_message("ERREUR [Ligne %d] : Décomposition de liste '::' mal formée.\n",
                                           "ERROR [Line %d] : Malformed list decomposition '::'.\n",
                                           line_number);
                            report_source_hint(line_number, "::",
                                               "Utilise la forme head::tail = values avec une liste déjà déclarée.",
                                               "Use the form head::tail = values with an already declared list.");
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

                        if (strncmp(line, "//", 2) == 0 || strncmp(line, "/*", 2) == 0 || strncmp(line, "*", 1) == 0 || strncmp(line, "*/", 2) == 0)
                        {
                            print_indent(file, indent_top);
                            fprintf(file, "%s\n", line);
                        }
                        else if (strncmp(line, "const ", 6) == 0)
                        {
                            char const_var[64] = {0};
                            char const_val[256] = {0};
                            int const_status = parse_assignment(line + 6, const_var, sizeof(const_var), const_val, sizeof(const_val), line_number);

                            if (const_status < 0)
                                goto error_cleanup;
                            if (!const_status || !is_valid_identifier(const_var))
                            {
                                report_message("ERREUR [Ligne %d] : Déclaration const mal formée.\n",
                                               "ERROR [Line %d] : Malformed const declaration.\n", line_number);
                                report_source_hint(line_number, "const",
                                                   "Utilise const name = value.",
                                                   "Use const name = value.");
                                goto error_cleanup;
                            }
                            if (get_symbol_type(const_var) != NULL)
                            {
                                report_message("ERREUR E_CONST_ASSIGN [Ligne %d] : La constante '%s' existe déjà.\n",
                                               "ERROR E_CONST_ASSIGN [Line %d] : Const '%s' already exists.\n",
                                               line_number, const_var);
                                report_source_hint(line_number, const_var,
                                                   "Choisis un autre nom pour cette constante.",
                                                   "Choose another name for this const binding.");
                                goto error_cleanup;
                            }

                            char transformed_val[1024] = {0};
                            if (!transform_expression(const_val, transformed_val, sizeof(transformed_val), line_number)) goto error_cleanup;
                            const char *inferred_type = infer_expression_type(transformed_val);
                            if (strcmp(inferred_type, "list") == 0)
                            {
                                report_message("ERREUR [Ligne %d] : const ne supporte pas encore les listes.\n",
                                               "ERROR [Line %d] : const does not support lists yet.\n", line_number);
                                report_source_hint(line_number, "const",
                                                   "Déclare la liste normalement, puis évite de la modifier.",
                                                   "Declare the list normally, then avoid mutating it.");
                                goto error_cleanup;
                            }
                            if (!add_symbol_ex(const_var, inferred_type, indent_top, line_number, true)) goto error_cleanup;
                            print_indent(file, indent_top);
                            fprintf(file, "const %s %s = %s;\n", inferred_type, const_var, transformed_val);
                            trace_translation(line_number, "const", const_var);
                        }
                        else if (assignment_status && !is_c_declaration_lhs(var))
                        {
                            if (!is_valid_identifier(var))
                            {
                                report_message("ERREUR [Ligne %d] : Identifiant de variable invalide '%s'.\n",
                                               "ERROR [Line %d] : Invalid variable identifier '%s'.\n",
                                               line_number, var);
                                report_source_hint(line_number, var,
                                                   "Un nom de variable doit commencer par une lettre ou '_' et ne contenir que lettres, chiffres ou '_'.",
                                                   "A variable name must start with a letter or '_' and contain only letters, digits, or '_'.");
                                goto error_cleanup;
                            }

                            const char *existing_type = get_symbol_type(var);

                            if (existing_type != NULL && is_const_symbol(var))
                            {
                                report_message("ERREUR E_CONST_ASSIGN [Ligne %d] : Impossible de réassigner la constante '%s'.\n",
                                               "ERROR E_CONST_ASSIGN [Line %d] : Cannot assign to const '%s'.\n",
                                               line_number, var);
                                report_source_hint(line_number, var,
                                                   "Crée une nouvelle variable si la valeur doit changer.",
                                                   "Create a new variable if the value needs to change.");
                                goto error_cleanup;
                            }

                            if (strncmp(val, "input(", 6) == 0)
                            {
                                if (!write_input_assignment(file, indent_top, var, val, line_number)) goto error_cleanup;
                                trace_translation(line_number, "assignment", "input");
                            }
                            else if (strncmp(val, "str_concat(", 11) == 0)
                            {
                                if (!write_str_concat_assignment(file, indent_top, var, val, line_number)) goto error_cleanup;
                                trace_translation(line_number, "assignment", "str_concat");
                            }
                            else if (strncmp(val, "str_copy(", 9) == 0)
                            {
                                if (!write_str_copy_assignment(file, indent_top, var, val, line_number)) goto error_cleanup;
                                trace_translation(line_number, "assignment", "str_copy");
                            }
                            else if (existing_type != NULL)
                            {
                                char transformed_val[1024] = {0};
                                if (!transform_expression(val, transformed_val, sizeof(transformed_val), line_number)) goto error_cleanup;
                                if (!check_assignment_type(var, existing_type, transformed_val, line_number)) goto error_cleanup;
                                if (strcmp(existing_type, "char*") == 0 && is_allocated_ptr_name(var))
                                {
                                    print_indent(file, indent_top);
                                    fprintf(file, "if (%s != NULL) { free(%s); %s = NULL; }\n", var, var, var);
                                    remove_allocated_ptr_name(var);
                                }
                                print_indent(file, indent_top);
                                fprintf(file, "%s = %s;\n", var, transformed_val);
                                trace_translation(line_number, "assignment", var);
                            }
                            else
                            {
                                char transformed_val[1024] = {0};
                                if (!transform_expression(val, transformed_val, sizeof(transformed_val), line_number)) goto error_cleanup;
                                const char *inferred_type = infer_expression_type(transformed_val);

                                if (strcmp(inferred_type, "list") == 0)
                                {
                                    char elems[128][64];
                                    int count = 0;
                                    const char *elem_type = parse_and_validate_list_literal(transformed_val, elems, &count, line_number);

                                    if (elem_type == NULL) goto error_cleanup;

                                    char list_type[32] = {0};
                                    snprintf(list_type, sizeof(list_type), "%s*", elem_type);
                                    int alloc_count = count > 0 ? count : 1;

                                    print_indent(file, indent_top);
                                    fprintf(file, "%s *%s = malloc(%d * sizeof(%s));\n", elem_type, var, alloc_count, elem_type);
                                    if (!add_symbol(var, list_type, indent_top, line_number)) goto error_cleanup;
                                    if (!add_allocated_ptr(var, indent_top, line_number)) goto error_cleanup;
                                    write_allocation_failure_return(file, indent_top, var);

                                    for (int k = 0; k < count; k++)
                                    {
                                        print_indent(file, indent_top);
                                        fprintf(file, "%s[%d] = %s;\n", var, k, elems[k]);
                                    }

                                    print_indent(file, indent_top);
                                    fprintf(file, "int %s_len = %d;\n", var, count);
                                    if (!add_list_len_symbol(var, indent_top, line_number)) goto error_cleanup;
                                    trace_translation(line_number, "list", var);
                                }
                                else if (strcmp(inferred_type, "char*") == 0)
                                {
                                    if (!add_symbol(var, "char*", indent_top, line_number)) goto error_cleanup;
                                    print_indent(file, indent_top);
                                    fprintf(file, "char *%s = %s;\n", var, transformed_val);
                                    trace_translation(line_number, "assignment", var);
                                }
                                else
                                {
                                    if (!add_symbol(var, inferred_type, indent_top, line_number)) goto error_cleanup;
                                    print_indent(file, indent_top);
                                    fprintf(file, "%s %s = %s;\n", inferred_type, var, transformed_val);
                                    trace_translation(line_number, "assignment", var);
                                }
                            }
                        }
                        else if (strcmp(line, "return") == 0 || strncmp(line, "return ", 7) == 0)
                        {
                            char return_stmt[1024] = {0};
                            generate_frees(file, indent_top);
                            scope_returned[indent_top] = true;
                            print_indent(file, indent_top);
                            if (strcmp(line, "return") == 0)
                            {
                                fprintf(file, "return;\n");
                            }
                            else
                            {
                                char return_expr[1024] = {0};
                                if (!safe_copy(return_expr, sizeof(return_expr), line + 6))
                                {
                                    report_message("ERREUR [Ligne %d] : Expression return trop longue.\n",
                                                   "ERROR [Line %d] : return expression is too long.\n",
                                                   line_number);
                                    report_source_hint(line_number, "return",
                                                       "Stocke le résultat dans une variable avant le return.",
                                                       "Store the result in a variable before returning it.");
                                    goto error_cleanup;
                                }
                                trim_trailing_whitespace(return_expr);
                                trim_leading_whitespace(return_expr);
                                if (!transform_expression(return_expr, return_stmt, sizeof(return_stmt), line_number)) goto error_cleanup;
                                fprintf(file, "return %s;\n", return_stmt);
                            }
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
                                report_source_hint(line_number, "if",
                                                   "Stocke une partie de la condition dans une variable booléenne.",
                                                   "Store part of the condition in a boolean variable.");
                                goto error_cleanup;
                            }
                            size_t c_len = strlen(cond);
                            if (c_len > 0 && cond[c_len - 1] == ':') cond[c_len - 1] = '\0';

                            trim_trailing_whitespace(cond);
                            char transformed_cond[1024] = {0};
                            if (!transform_expression(cond, transformed_cond, sizeof(transformed_cond), line_number)) goto error_cleanup;
                            print_indent(file, indent_top);
                            fprintf(file, "if (%s)\n", transformed_cond);
                        }
                        else if (strncmp(line, "elif ", 5) == 0)
                        {
                            char cond[1024] = {0};
                            if (!safe_copy(cond, sizeof(cond), line + 5))
                            {
                                report_message("ERREUR [Ligne %d] : Condition elif trop longue.\n",
                                               "ERROR [Line %d] : elif condition is too long.\n",
                                               line_number);
                                report_source_hint(line_number, "elif",
                                                   "Stocke une partie de la condition dans une variable booléenne.",
                                                   "Store part of the condition in a boolean variable.");
                                goto error_cleanup;
                            }
                            size_t c_len = strlen(cond);
                            if (c_len > 0 && cond[c_len - 1] == ':') cond[c_len - 1] = '\0';

                            trim_trailing_whitespace(cond);
                            char transformed_cond[1024] = {0};
                            if (!transform_expression(cond, transformed_cond, sizeof(transformed_cond), line_number)) goto error_cleanup;
                            print_indent(file, indent_top);
                            fprintf(file, "else if (%s)\n", transformed_cond);
                        }
                        else if (strncmp(line, "while ", 6) == 0)
                        {
                            char cond[1024] = {0};
                            if (!safe_copy(cond, sizeof(cond), line + 6))
                            {
                                report_message("ERREUR [Ligne %d] : Condition while trop longue.\n",
                                               "ERROR [Line %d] : while condition is too long.\n",
                                               line_number);
                                report_source_hint(line_number, "while",
                                                   "Stocke une partie de la condition dans une variable booléenne.",
                                                   "Store part of the condition in a boolean variable.");
                                goto error_cleanup;
                            }
                            size_t c_len = strlen(cond);
                            if (c_len > 0 && cond[c_len - 1] == ':') cond[c_len - 1] = '\0';

                            trim_trailing_whitespace(cond);
                            char transformed_cond[1024] = {0};
                            if (!transform_expression(cond, transformed_cond, sizeof(transformed_cond), line_number)) goto error_cleanup;
                            print_indent(file, indent_top);
                            fprintf(file, "while (%s)\n", transformed_cond);
                        }
                        else if (strncmp(line, "for ", 4) == 0 && strstr(line, " in range(") != NULL)
                        {
                            char var_name[64] = {0};
                            char range_start[256] = {0};
                            char range_stop[256] = {0};
                            char range_step[256] = {0};

                            if (!parse_range_loop(line, var_name, sizeof(var_name),
                                                  range_start, sizeof(range_start),
                                                  range_stop, sizeof(range_stop),
                                                  range_step, sizeof(range_step),
                                                  line_number))
                                goto error_cleanup;
                            if (!add_symbol(var_name, "int", indent_top, line_number)) goto error_cleanup;
                            print_indent(file, indent_top);
                            if (strcmp(range_step, "1") == 0)
                            {
                                fprintf(file, "for (int %s = %s; %s < %s; %s++)\n",
                                        var_name, range_start, var_name, range_stop, var_name);
                            }
                            else
                            {
                                fprintf(file, "for (int %s = %s; (%s) != 0 && ((%s) > 0 ? %s < %s : %s > %s); %s += (%s))\n",
                                        var_name, range_start, range_step, range_step,
                                        var_name, range_stop, var_name, range_stop,
                                        var_name, range_step);
                            }
                            trace_translation(line_number, "loop", "range");
                        }
                        else if (strncmp(line, "for ", 4) == 0 && strstr(line, " in ") != NULL)
                        {
                            char item_name[64] = {0};
                            char list_name[64] = {0};
                            char index_name[96] = {0};
                            const char *elem_type;

                            if (!parse_foreach_loop(line, item_name, sizeof(item_name),
                                                    list_name, sizeof(list_name), line_number))
                                goto error_cleanup;

                            elem_type = get_elem_type(list_name);
                            snprintf(index_name, sizeof(index_name), "__%s_index_%d", item_name, line_number);
                            print_indent(file, indent_top);
                            fprintf(file, "for (int %s = 0; %s < %s_len; %s++)\n",
                                    index_name, index_name, list_name, index_name);
                            snprintf(pending_foreach_decl, sizeof(pending_foreach_decl),
                                     "%s %s = %s[%s];", elem_type, item_name, list_name, index_name);
                            if (!add_symbol(item_name, elem_type, indent_top + 1, line_number)) goto error_cleanup;
                            trace_translation(line_number, "loop", "foreach");
                        }
                        else if (strcmp(line, "else") == 0 || strcmp(line, "else:") == 0)
                        {
                            print_indent(file, indent_top);
                            fprintf(file, "else\n");
                        }
                        else if (strcmp(line, "break") == 0 || strcmp(line, "continue") == 0)
                        {
                            print_indent(file, indent_top);
                            fprintf(file, "%s;\n", line);
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
                                char symbol_type[32] = {0};
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
                            if (function_header)
                            {
                                if (flag_pretty_c && indent_top == 0)
                                    fprintf(file, "\n");
                                if (is_valid_identifier(var_exp) && !add_symbol(var_exp, type_exp, indent_top, line_number))
                                    goto error_cleanup;
                                trace_translation(line_number, "function", var_exp);
                            }
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

    if (pending_foreach_decl[0] != '\0')
    {
        report_message("ERREUR : foreach attend un bloc indenté avant la fin du fichier.\n",
                       "ERROR : foreach expects an indented block before end of file.\n");
        goto error_cleanup;
    }

    while (indent_top > 0)
    {
        if (scope_returned[indent_top])
            discard_scope_allocations(indent_top);
        else
            generate_scope_frees(file, indent_top);
        scope_returned[indent_top] = false;
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
