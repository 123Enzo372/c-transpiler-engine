#include "main.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

int flag_french = 0;
int flag_comments = 0;
int flag_trace = 0;
int flag_pretty_c = 0;
int flag_quiet = 0;
int flag_no_color = 0;
int flag_explain_generated = 0;
int flag_suggest_fix = 0;

#define TRANSPILER_VERSION "0.5.0"

static int file_exists(const char *path);

static int print_explain(const char *code)
{
    if (code == NULL || code[0] == '\0')
    {
        report_message("ERREUR : --explain nécessite un code d'erreur.\n",
                       "ERROR : --explain requires an error code.\n");
        return 1;
    }

    if (strcmp(code, "E_APPEND_TARGET") == 0)
    {
        report_message("E_APPEND_TARGET : append(...) attend une liste déjà créée par le langage.\n"
                       "Suggestion : déclare d'abord values = [] ou values = [1, 2, 3].\n",
                       "E_APPEND_TARGET : append(...) expects a list already created by the language.\n"
                       "Suggestion: declare values = [] or values = [1, 2, 3] first.\n");
    }
    else if (strcmp(code, "E_ASSIGN_TYPE") == 0)
    {
        report_message("E_ASSIGN_TYPE : une affectation change le type connu d'une variable.\n"
                       "Suggestion : utilise une nouvelle variable ou convertis explicitement côté C.\n",
                       "E_ASSIGN_TYPE : an assignment changes the known type of a variable.\n"
                       "Suggestion: use a new variable or convert explicitly in C.\n");
    }
    else if (strcmp(code, "E_CONST_ASSIGN") == 0)
    {
        report_message("E_CONST_ASSIGN : une constante ne peut pas être réassignée.\n"
                       "Suggestion : crée une nouvelle variable si la valeur doit changer.\n",
                       "E_CONST_ASSIGN : a const binding cannot be assigned again.\n"
                       "Suggestion: create a new variable if the value needs to change.\n");
    }
    else if (strcmp(code, "E_RANGE_ARGS") == 0)
    {
        report_message("E_RANGE_ARGS : range(...) accepte 1, 2 ou 3 arguments.\n"
                       "Suggestion : utilise range(stop), range(start, stop) ou range(start, stop, step).\n",
                       "E_RANGE_ARGS : range(...) accepts 1, 2, or 3 arguments.\n"
                       "Suggestion: use range(stop), range(start, stop), or range(start, stop, step).\n");
    }
    else if (strcmp(code, "E_IMPORT") == 0)
    {
        report_message("E_IMPORT : la ligne import est vide ou mal formée.\n"
                       "Suggestion : utilise import tools, import \"tools.H\" ou import <stdio.h>.\n",
                       "E_IMPORT : the import line is empty or malformed.\n"
                       "Suggestion: use import tools, import \"tools.H\", or import <stdio.h>.\n");
    }
    else if (strcmp(code, "E_HELPER_ARGS") == 0)
    {
        report_message("E_HELPER_ARGS : un helper natif a reçu des arguments invalides.\n"
                       "Suggestion : vérifie le nombre d'arguments et garde les noms de listes simples.\n",
                       "E_HELPER_ARGS : a native helper received invalid arguments.\n"
                       "Suggestion: check the argument count and keep list names simple.\n");
    }
    else if (strcmp(code, "E_BLOCK_EXPECTED") == 0)
    {
        report_message("E_BLOCK_EXPECTED : une ligne qui ouvre un bloc n'a pas de corps indenté.\n"
                       "Suggestion : ajoute au moins une instruction indentée sous cette ligne.\n",
                       "E_BLOCK_EXPECTED : a block-opening line has no indented body.\n"
                       "Suggestion: add at least one indented statement under that line.\n");
    }
    else if (strcmp(code, "E_INDENTATION") == 0)
    {
        report_message("E_INDENTATION : une ligne est indentée sans bloc ouvert juste avant.\n"
                       "Suggestion : désindente la ligne ou ajoute if/while/for/fonction avant elle.\n",
                       "E_INDENTATION : a line is indented without a block opener immediately before it.\n"
                       "Suggestion: dedent the line or add if/while/for/function before it.\n");
    }
    else if (strcmp(code, "E_RETURN_TYPE") == 0)
    {
        report_message("E_RETURN_TYPE : la valeur retournée ne correspond pas au type de la fonction.\n"
                       "Suggestion : retourne une valeur compatible ou change la signature.\n",
                       "E_RETURN_TYPE : the returned value does not match the function return type.\n"
                       "Suggestion: return a compatible value or change the signature.\n");
    }
    else if (strcmp(code, "E_PARAM_TYPE") == 0)
    {
        report_message("E_PARAM_TYPE : un paramètre de fonction n'utilise pas une forme typée simple.\n"
                       "Suggestion : écris par exemple int count ou char *name.\n",
                       "E_PARAM_TYPE : a function parameter does not use a simple typed form.\n"
                       "Suggestion: write for example int count or char *name.\n");
    }
    else
    {
        report_message("ERREUR : Code d'erreur inconnu '%s'.\n",
                       "ERROR : Unknown error code '%s'.\n", code);
        return 1;
    }

    return 0;
}

static void print_help(void)
{
    if (flag_french)
    {
        printf("Usage : ./compilateur [sources] [options-gcc] [options] [-o sortie]\n\n"
               "Sources :\n"
               "  fichier.l              Traduit un fichier source en C.\n"
               "  fichier.H              Traduit un header avec garde d'inclusion.\n"
               "  fichier.c / fichier.h  Transmis directement à gcc.\n\n"
               "Commandes :\n"
               "  init [nom]        Crée un petit projet sans écraser les fichiers existants.\n"
               "  run <source.l>    Traduit, compile, puis exécute le binaire.\n"
               "  clean             Supprime les artefacts de build les plus courants.\n\n"
               "  repl              Lance un mini REPL pédagogique.\n"
               "  deps <source>     Affiche les imports locaux résolus.\n\n"
               "Options :\n"
               "  -o <nom>          Nom du binaire final.\n"
               "  -without-binary   Traduit sans lancer gcc.\n"
               "  --emit-c, -S      Traduit en C et conserve les fichiers générés sans lancer gcc.\n"
               "  -keep_c           Conserve les fichiers .c générés.\n"
               "  -keep_h           Conserve les fichiers .h générés.\n"
               "  -comments         Ajoute les lignes source en commentaires dans les .c générés.\n"
               "  --pretty-c        Ajoute une mise en forme plus lisible au C généré.\n"
               "  --trace           Affiche les étapes reconnues pendant la traduction.\n"
               "  --dump-ast        Alias de --trace pour inspecter la traduction.\n"
               "  --quiet           Masque les messages de succès.\n"
               "  --no-color        Désactive les couleurs ANSI dans les diagnostics.\n"
               "  --teach           Mode apprentissage : C lisible, commentaires et explication.\n"
               "  --explain-generated  Génère un fichier .explain.txt à côté du C.\n"
               "  --suggest-fix     Ajoute une correction concrète aux diagnostics connus.\n"
               "  --clean           Alias de la commande clean.\n"
               "  --version         Affiche la version et quitte.\n"
               "  --explain <code>  Explique un code d'erreur.\n"
               "  -rm_l             Supprime les sources .l après succès.\n"
               "  -rm_H             Supprime les sources .H après succès.\n"
               "  -french           Affiche les diagnostics en français.\n"
               "  -h, --help        Affiche cette aide.\n\n"
               "Exemples :\n"
               "  ./compilateur main.l -o app\n"
               "  ./compilateur run main.l\n"
               "  ./compilateur repl\n"
               "  ./compilateur deps main.l\n"
               "  ./compilateur main.l api.H -Wall -Wextra -o app\n"
               "  ./compilateur main.l --emit-c --pretty-c\n"
               "  ./compilateur --explain E_ASSIGN_TYPE\n");
    }
    else
    {
        printf("Usage: ./compilateur [sources] [gcc-options] [options] [-o output]\n\n"
               "Sources:\n"
               "  file.l              Translate a source file to C.\n"
               "  file.H              Translate a header with an include guard.\n"
               "  file.c / file.h     Passed directly to gcc.\n\n"
               "Commands:\n"
               "  init [name]       Create a small project without overwriting existing files.\n"
               "  run <source.l>    Translate, compile, then run the binary.\n"
               "  clean             Remove common build artifacts.\n\n"
               "  repl              Start a small teaching REPL.\n"
               "  deps <source>     Print resolved local imports.\n\n"
               "Options:\n"
               "  -o <name>         Final executable name.\n"
               "  -without-binary   Translate without running gcc.\n"
               "  --emit-c, -S      Translate to C, keep generated files, and do not run gcc.\n"
               "  -keep_c           Keep generated .c files.\n"
               "  -keep_h           Keep generated .h files.\n"
               "  -comments         Add source-line comments to generated .c files.\n"
               "  --pretty-c        Add extra readability formatting to generated C.\n"
               "  --trace           Print recognized translation steps.\n"
               "  --dump-ast        Alias for --trace when inspecting translation.\n"
               "  --quiet           Hide success messages.\n"
               "  --no-color        Disable ANSI colors in diagnostics.\n"
               "  --teach           Learning mode: readable C, comments, and explanation.\n"
               "  --explain-generated  Write a .explain.txt file next to generated C.\n"
               "  --suggest-fix     Add concrete fixes to known diagnostics.\n"
               "  --clean           Alias for the clean command.\n"
               "  --version         Print the version and exit.\n"
               "  --explain <code>  Explain an error code.\n"
               "  -rm_l             Delete .l sources after success.\n"
               "  -rm_H             Delete .H sources after success.\n"
               "  -french           Print diagnostics in French.\n"
               "  -h, --help        Show this help message.\n\n"
               "Examples:\n"
               "  ./compilateur main.l -o app\n"
               "  ./compilateur run main.l\n"
               "  ./compilateur repl\n"
               "  ./compilateur deps main.l\n"
               "  ./compilateur main.l api.H -Wall -Wextra -o app\n"
               "  ./compilateur main.l --emit-c --pretty-c\n"
               "  ./compilateur --explain E_ASSIGN_TYPE\n");
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

static int append_text(char *dest, size_t dest_size, const char *src)
{
    size_t dest_len;
    size_t src_len;

    if (dest == NULL || src == NULL || dest_size == 0)
        return 0;

    dest_len = strlen(dest);
    src_len = strlen(src);
    if (src_len >= dest_size - dest_len)
        return 0;

    memcpy(dest + dest_len, src, src_len + 1);
    return 1;
}

static int write_file_if_missing(const char *path, const char *content)
{
    FILE *file;

    if (file_exists(path))
    {
        if (!flag_quiet)
            report_message("OK : '%s' existe déjà, aucun écrasement.\n",
                           "OK : '%s' already exists, not overwritten.\n", path);
        return 1;
    }

    file = fopen(path, "w");
    if (file == NULL)
    {
        report_message("ERREUR FICHIER : Impossible de créer '%s'.\n",
                       "FILE ERROR : Cannot create '%s'.\n", path);
        return 0;
    }

    fputs(content, file);
    fclose(file);
    if (!flag_quiet)
        report_message("OK : fichier créé : %s\n",
                       "OK : created file: %s\n", path);
    return 1;
}

static int init_project(const char *project_name)
{
    char readme[1024] = {0};
    const char *main_source =
        "int main()\n"
        "    name = input(\"Name: \")\n"
        "    greeting = str_concat(\"Hello \", name)\n"
        "    print(greeting)\n"
        "    return 0\n";
    const char *gitignore =
        "# Build outputs\n"
        "*.exe\n"
        "*.out\n"
        "*.o\n"
        "*.obj\n"
        "*.gch\n"
        ".tmp/\n"
        "local_tests/\n";

    if (project_name == NULL || project_name[0] == '\0')
        project_name = "language-project";

    snprintf(readme, sizeof(readme),
             "# %s\n\n"
             "Build and run:\n\n"
             "```bash\n"
             "./compilateur run main.l\n"
             "```\n",
             project_name);

    return write_file_if_missing("main.l", main_source) &&
           write_file_if_missing(".gitignore", gitignore) &&
           write_file_if_missing("README.md", readme);
}

static int clean_project(void)
{
    const char *paths[] = {
        "compilateur", "compilateur.exe", "suite_tests", "a.out", "a.out.exe",
        "test.h.gch", NULL
    };
    int removed = 0;

    for (int i = 0; paths[i] != NULL; i++)
    {
        if (remove(paths[i]) == 0)
            removed++;
    }

    if (!flag_quiet)
        report_message("OK : %d artefact(s) supprimé(s).\n",
                       "OK : removed %d artifact(s).\n", removed);
    return 0;
}

static void derive_output_name(const char *source, char *output, size_t output_size)
{
    const char *base = source;
    const char *slash;
    const char *backslash;
    char *dot;

    if (output_size == 0)
        return;

    slash = strrchr(source, '/');
    backslash = strrchr(source, '\\');
    if (slash != NULL || backslash != NULL)
        base = slash > backslash ? slash + 1 : backslash + 1;

    safe_copy(output, output_size, base);
    dot = strrchr(output, '.');
    if (dot != NULL)
        *dot = '\0';
    if (output[0] == '\0')
        safe_copy(output, output_size, "app");
}

static int run_executable(const char *output)
{
    char executable[512] = {0};
    char executable_exe[512] = {0};
    pid_t pid;

    if (output == NULL || output[0] == '\0')
        output = "a.out";

    if (snprintf(executable, sizeof(executable), "./%s", output) >= (int)sizeof(executable) ||
        snprintf(executable_exe, sizeof(executable_exe), "./%s.exe", output) >= (int)sizeof(executable_exe))
    {
        report_message("ERREUR : Nom de binaire trop long pour run.\n",
                       "ERROR : Binary name is too long for run.\n");
        return 1;
    }

    if (!flag_quiet)
        report_message("OK : exécution : %s\n", "OK : running: %s\n", executable);

    pid = fork();
    if (pid < 0)
    {
        report_message("ERREUR SYSTEME : fork a échoué pour run.\n",
                       "SYSTEM ERROR : fork failed for run.\n");
        return 1;
    }

    if (pid == 0)
    {
        execl(executable, executable, (char *)NULL);
        execl(executable_exe, executable_exe, (char *)NULL);
        report_message("ERREUR : Impossible d'exécuter '%s'.\n",
                       "ERROR : Cannot run '%s'.\n", executable);
        exit(EXIT_FAILURE);
    }

    int status;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status))
        return WEXITSTATUS(status);
    return 1;
}

static int has_suffix(const char *value, const char *suffix)
{
    size_t value_len;
    size_t suffix_len;

    if (value == NULL || suffix == NULL)
        return 0;

    value_len = strlen(value);
    suffix_len = strlen(suffix);
    return value_len >= suffix_len && strcmp(value + value_len - suffix_len, suffix) == 0;
}

static int file_exists(const char *path)
{
    FILE *file = fopen(path, "r");
    if (file == NULL)
        return 0;
    fclose(file);
    return 1;
}

static int source_already_listed(char **filename, const char *candidate)
{
    for (int i = 0; filename != NULL && filename[i] != NULL; i++)
    {
        if (strcmp(filename[i], candidate) == 0)
            return 1;
    }
    return 0;
}

static int add_source_if_exists(char ***filename, int *index_file, int *size_file,
                                const char *candidate, const char *importer)
{
    char *owned_candidate;

    if (candidate == NULL || candidate[0] == '\0' || !file_exists(candidate))
        return 1;

    if (source_already_listed(*filename, candidate))
        return 1;

    owned_candidate = duplicate_string(candidate);
    if (owned_candidate == NULL)
    {
        report_message("ERREUR SYSTEME : Échec d'allocation mémoire pour un import.\n",
                       "SYSTEM ERROR : Memory allocation failed for an import.\n");
        return 0;
    }

    if (!append_argument(filename, index_file, size_file, owned_candidate, "filename"))
    {
        free(owned_candidate);
        return 0;
    }

    if (!flag_quiet)
    {
        report_message("OK : import local détecté depuis '%s' : %s\n",
                       "OK : local import detected from '%s': %s\n", importer, candidate);
    }
    return 1;
}

static void directory_of(const char *path, char *out_dir, size_t out_size)
{
    const char *slash;
    const char *backslash;
    const char *sep;

    if (out_size == 0)
        return;

    out_dir[0] = '\0';
    if (path == NULL)
        return;

    slash = strrchr(path, '/');
    backslash = strrchr(path, '\\');
    sep = slash > backslash ? slash : backslash;
    if (sep == NULL)
    {
        safe_copy(out_dir, out_size, ".");
        return;
    }

    if ((size_t)(sep - path) >= out_size)
    {
        safe_copy(out_dir, out_size, ".");
        return;
    }

    memcpy(out_dir, path, (size_t)(sep - path));
    out_dir[sep - path] = '\0';
}

static int join_path(char *out_path, size_t out_size, const char *dir, const char *name)
{
    if (dir == NULL || name == NULL || out_path == NULL || out_size == 0)
        return 0;

    if (strcmp(dir, ".") == 0)
        return snprintf(out_path, out_size, "%s", name) < (int)out_size;

    return snprintf(out_path, out_size, "%s/%s", dir, name) < (int)out_size;
}

static void trim_text(char *text)
{
    char *start = text;
    size_t len;

    while (*start == ' ' || *start == '\t')
        start++;
    if (start != text)
        memmove(text, start, strlen(start) + 1);

    len = strlen(text);
    while (len > 0 && isspace((unsigned char)text[len - 1]))
        text[--len] = '\0';
}

static int extract_local_import_target(const char *line, char *target, size_t target_size)
{
    size_t len;

    if (strncmp(line, "import ", 7) != 0)
        return 0;

    if (!safe_copy(target, target_size, line + 7))
        return -1;
    trim_text(target);
    len = strlen(target);
    if (len == 0 || target[0] == '<')
        return 0;

    if (len >= 2 && target[0] == '"' && target[len - 1] == '"')
    {
        memmove(target, target + 1, len - 2);
        target[len - 2] = '\0';
        trim_text(target);
    }

    if (target[0] == '\0' || target[0] == '<')
        return 0;
    return 1;
}

static int add_import_candidates(char ***filename, int *index_file, int *size_file,
                                 const char *importer, const char *target)
{
    char dir[512] = {0};
    char candidate[1024] = {0};
    char stem[512] = {0};

    directory_of(importer, dir, sizeof(dir));

    if (has_suffix(target, ".l") || has_suffix(target, ".H"))
    {
        if (!join_path(candidate, sizeof(candidate), dir, target))
            return 0;
        return add_source_if_exists(filename, index_file, size_file, candidate, importer);
    }

    if (has_suffix(target, ".h"))
    {
        if (!safe_copy(stem, sizeof(stem), target))
            return 0;
        stem[strlen(stem) - 1] = 'H';
        if (!join_path(candidate, sizeof(candidate), dir, stem))
            return 0;
        return add_source_if_exists(filename, index_file, size_file, candidate, importer);
    }

    if (strchr(target, '.') == NULL)
    {
        if (snprintf(stem, sizeof(stem), "%s.H", target) >= (int)sizeof(stem))
            return 0;
        if (!join_path(candidate, sizeof(candidate), dir, stem))
            return 0;
        if (!add_source_if_exists(filename, index_file, size_file, candidate, importer))
            return 0;

        if (snprintf(stem, sizeof(stem), "%s.l", target) >= (int)sizeof(stem))
            return 0;
        if (!join_path(candidate, sizeof(candidate), dir, stem))
            return 0;
        if (!add_source_if_exists(filename, index_file, size_file, candidate, importer))
            return 0;
    }

    return 1;
}

static int scan_local_imports(char ***filename, int *index_file, int *size_file, int source_index)
{
    FILE *file;
    char line[1024];
    const char *source = (*filename)[source_index];

    if (!has_suffix(source, ".l") && !has_suffix(source, ".H"))
        return 1;

    file = fopen(source, "r");
    if (file == NULL)
        return 1;

    while (fgets(line, sizeof(line), file) != NULL)
    {
        char *trimmed = line;
        char target[512] = {0};
        int import_status;

        while (*trimmed == ' ' || *trimmed == '\t')
            trimmed++;

        import_status = extract_local_import_target(trimmed, target, sizeof(target));
        if (import_status < 0)
        {
            fclose(file);
            report_message("ERREUR E_IMPORT : Import trop long dans '%s'.\n",
                           "ERROR E_IMPORT : Import is too long in '%s'.\n", source);
            return 0;
        }
        if (import_status > 0 &&
            !add_import_candidates(filename, index_file, size_file, source, target))
        {
            fclose(file);
            report_message("ERREUR E_IMPORT : Impossible de résoudre l'import '%s' depuis '%s'.\n",
                           "ERROR E_IMPORT : Cannot resolve import '%s' from '%s'.\n", target, source);
            return 0;
        }
    }

    fclose(file);
    return 1;
}

static int print_dependency_graph(const char *source)
{
    int size_file = 10;
    int index_file = 0;
    char **filename = calloc(size_file, sizeof(char *));
    char *owned_source;

    if (source == NULL || source[0] == '\0')
    {
        report_message("ERREUR : deps nécessite un fichier source.\n",
                       "ERROR : deps requires a source file.\n");
        return 1;
    }

    if (filename == NULL)
    {
        report_message("ERREUR SYSTEME : Échec d'allocation mémoire pour deps.\n",
                       "SYSTEM ERROR : Memory allocation failed for deps.\n");
        return 1;
    }

    owned_source = duplicate_string(source);
    if (owned_source == NULL ||
        !append_argument(&filename, &index_file, &size_file, owned_source, "filename"))
    {
        free(owned_source);
        free(filename);
        return 1;
    }

    int old_quiet = flag_quiet;
    flag_quiet = 1;
    for (int i = 0; i < index_file; i++)
    {
        if (!scan_local_imports(&filename, &index_file, &size_file, i))
        {
            flag_quiet = old_quiet;
            for (int j = 0; filename[j] != NULL; j++)
                free(filename[j]);
            free(filename);
            return 1;
        }
    }
    flag_quiet = old_quiet;

    printf("Dependency graph for %s\n", source);
    for (int i = 0; filename[i] != NULL; i++)
    {
        printf("%s%s\n", i == 0 ? "root: " : "dep:  ", filename[i]);
    }

    for (int i = 0; filename[i] != NULL; i++)
        free(filename[i]);
    free(filename);
    return 0;
}

static int run_compiler_for_repl(const char *compiler_path)
{
    char command[1024] = {0};
    const char *compiler = file_exists("./compilateur.exe") ? "./compilateur.exe" : compiler_path;
    int status;

    if (compiler == NULL || compiler[0] == '\0')
        compiler = "./compilateur";

    if (snprintf(command, sizeof(command), "%s .tmp/repl.l -o .tmp/repl_app --quiet", compiler) >=
        (int)sizeof(command))
    {
        report_message("ERREUR : Commande REPL trop longue.\n",
                       "ERROR : REPL command is too long.\n");
        return 1;
    }

    status = system(command);
    if (status == -1)
        return 1;
    if (WIFEXITED(status))
        return WEXITSTATUS(status);
    return status == 0 ? 0 : 1;
}

static int run_repl(const char *compiler_path)
{
    char line[512];
    char statements[8192] = {0};

    mkdir(".tmp", 0777);

    printf("Teaching REPL. Commands: :run, :show, :reset, :quit\n");
    while (1)
    {
        printf("l> ");
        fflush(stdout);
        if (fgets(line, sizeof(line), stdin) == NULL)
            break;

        trim_text(line);
        if (strcmp(line, ":quit") == 0 || strcmp(line, ":q") == 0)
            break;
        if (strcmp(line, ":reset") == 0)
        {
            statements[0] = '\0';
            printf("reset\n");
            continue;
        }
        if (strcmp(line, ":show") == 0)
        {
            printf("int main()\n%s    return 0\n", statements);
            continue;
        }
        if (strcmp(line, ":run") == 0)
        {
            FILE *file = fopen(".tmp/repl.l", "w");
            if (file == NULL)
            {
                report_message("ERREUR FICHIER : Impossible d'écrire .tmp/repl.l.\n",
                               "FILE ERROR : Cannot write .tmp/repl.l.\n");
                return 1;
            }
            fprintf(file, "int main()\n%s    return 0\n", statements);
            fclose(file);

            if (run_compiler_for_repl(compiler_path) == 0)
                run_executable(".tmp/repl_app");
            continue;
        }

        if (!append_text(statements, sizeof(statements), "    ") ||
            !append_text(statements, sizeof(statements), line) ||
            !append_text(statements, sizeof(statements), "\n"))
        {
            report_message("ERREUR : Trop de lignes dans cette session REPL.\n",
                           "ERROR : Too many lines in this REPL session.\n");
            return 1;
        }
    }

    return 0;
}

static int write_generated_explanation(const char *generated_file)
{
    FILE *in;
    FILE *out;
    char explanation_path[1024] = {0};
    char line[2048];
    int source_block = 0;

    if (generated_file == NULL || !has_suffix(generated_file, ".c"))
        return 0;

    if (snprintf(explanation_path, sizeof(explanation_path), "%s.explain.txt", generated_file) >=
        (int)sizeof(explanation_path))
    {
        report_message("ERREUR : Nom de fichier d'explication trop long.\n",
                       "ERROR : Generated explanation filename is too long.\n");
        return 1;
    }

    in = fopen(generated_file, "r");
    if (in == NULL)
        return 0;

    out = fopen(explanation_path, "w");
    if (out == NULL)
    {
        fclose(in);
        report_message("ERREUR FICHIER : Impossible de créer '%s'.\n",
                       "FILE ERROR : Cannot create '%s'.\n", explanation_path);
        return 1;
    }

    fprintf(out, "Generated C explanation for %s\n\n", generated_file);
    while (fgets(line, sizeof(line), in) != NULL)
    {
        char *source = strstr(line, "/* source line ");
        if (source != NULL)
        {
            char *colon = strchr(source, ':');
            char *end = strstr(source, " */");
            if (colon != NULL && end != NULL)
            {
                *end = '\0';
                fprintf(out, "\nSource:%s\nGenerated C:\n", colon + 1);
                source_block = 1;
            }
            continue;
        }

        if (source_block && line[0] != '\n')
            fprintf(out, "  %s", line);
    }

    fclose(in);
    fclose(out);

    if (!flag_quiet)
        report_message("OK : explication générée : %s\n",
                       "OK : generated explanation: %s\n", explanation_path);
    return 0;
}

int main(int argc, char *argv[]) 
{
    /* Première passe pour détecter le flag -french préventivement */
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-french") == 0)
        {
            flag_french = 1;
        }
        else if (strcmp(argv[i], "-comments") == 0)
        {
            flag_comments = 1;
        }
        else if (strcmp(argv[i], "--trace") == 0 || strcmp(argv[i], "--dump-ast") == 0)
        {
            flag_trace = 1;
        }
        else if (strcmp(argv[i], "--pretty-c") == 0)
        {
            flag_pretty_c = 1;
        }
        else if (strcmp(argv[i], "--quiet") == 0)
        {
            flag_quiet = 1;
        }
        else if (strcmp(argv[i], "--no-color") == 0)
        {
            flag_no_color = 1;
        }
        else if (strcmp(argv[i], "--explain-generated") == 0)
        {
            flag_explain_generated = 1;
            flag_comments = 1;
        }
        else if (strcmp(argv[i], "--suggest-fix") == 0)
        {
            flag_suggest_fix = 1;
        }
        else if (strcmp(argv[i], "--teach") == 0)
        {
            flag_explain_generated = 1;
            flag_comments = 1;
            flag_pretty_c = 1;
            flag_suggest_fix = 1;
        }
    }

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0)
        {
            print_help();
            return 0;
        }
        if (strcmp(argv[i], "--version") == 0)
        {
            printf("compilateur %s\n", TRANSPILER_VERSION);
            return 0;
        }
        if (strcmp(argv[i], "--explain") == 0)
        {
            return print_explain(i + 1 < argc ? argv[i + 1] : NULL);
        }
    }

    if (argc >= 2 && strcmp(argv[1], "init") == 0)
    {
        return init_project(argc >= 3 ? argv[2] : NULL) ? 0 : 1;
    }
    if (argc >= 2 && (strcmp(argv[1], "clean") == 0 || strcmp(argv[1], "--clean") == 0))
    {
        return clean_project();
    }
    if (argc >= 2 && strcmp(argv[1], "deps") == 0)
    {
        return print_dependency_graph(argc >= 3 ? argv[2] : NULL);
    }
    if (argc >= 2 && strcmp(argv[1], "repl") == 0)
    {
        return run_repl(argv[0]);
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
    int run_mode = 0;
    char default_output[512] = {0};

    for (int i = 1; i < argc; i++)
    {
        size_t len = strlen(argv[i]);
        
        if (strcmp(argv[i], "run") == 0)
        {
            run_mode = 1;
        }
        else if (len >= 2 && (strcmp(argv[i] + len - 2, ".l") == 0 || strcmp(argv[i] + len - 2, ".H") == 0))
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
                     strcmp(argv[i], "-without-binary") == 0 || strcmp(argv[i], "-french") == 0 ||
                     strcmp(argv[i], "-comments") == 0 || strcmp(argv[i], "--emit-c") == 0 ||
                     strcmp(argv[i], "-S") == 0 || strcmp(argv[i], "--trace") == 0 ||
                     strcmp(argv[i], "--dump-ast") == 0 || strcmp(argv[i], "--pretty-c") == 0 ||
                     strcmp(argv[i], "--quiet") == 0 || strcmp(argv[i], "--no-color") == 0 ||
                     strcmp(argv[i], "--clean") == 0 || strcmp(argv[i], "--teach") == 0 ||
                     strcmp(argv[i], "--explain-generated") == 0 || strcmp(argv[i], "--suggest-fix") == 0)
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

    if (run_mode && output == NULL)
    {
        derive_output_name(filename[0], default_output, sizeof(default_output));
        output = default_output;
    }

    for (int i = 0; i < index_file; i++)
    {
        if (!scan_local_imports(&filename, &index_file, &size_file, i))
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
                {
                    gen_name[len - 1] = 'c';
                }
                else if (strcmp(filename[i] + len - 2, ".H") == 0)
                {
                    char *header_name = malloc(len - 2 + strlen(".generated.h") + 1);
                    if (header_name == NULL)
                    {
                        free(gen_name);
                        report_message("ERREUR SYSTEME : Échec d'allocation mémoire.\n",
                                       "SYSTEM ERROR : Memory allocation failed.\n");
                        status = 1;
                        break;
                    }
                    snprintf(header_name, len - 2 + strlen(".generated.h") + 1,
                             "%.*s.generated.h", (int)(len - 2), filename[i]);
                    free(gen_name);
                    gen_name = header_name;
                }
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
        if (strcmp(new_option[i], "--emit-c") == 0 || strcmp(new_option[i], "-S") == 0)
        {
            without_binary = 1;
            break;
        }
        if (strcmp(new_option[i], "--teach") == 0)
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

    if (flag_explain_generated)
    {
        for (int i = 0; created_filenames[i] != NULL; i++)
        {
            if (write_generated_explanation(created_filenames[i]) != 0)
            {
                res = 1;
                break;
            }
        }
    }

    if (res == 0)
        remove_created_files(created_filenames, new_option);

    if (!flag_quiet && res == 0)
    {
        report_message("OK : %d fichier(s) traduit(s).\n",
                       "OK : translated %d file(s).\n", created_count);
        if (without_binary)
        {
            report_message("OK : fichiers C/header générés conservés.\n",
                           "OK : generated C/header files were kept.\n");
        }
        else
        {
            report_message("OK : binaire généré : %s\n",
                           "OK : generated binary: %s\n", output != NULL ? output : "a.out");
        }
    }

    if (res == 0 && run_mode && !without_binary)
    {
        res = run_executable(output);
    }

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
