#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "main.h"

// Charge un fichier texte ligne par ligne dans un tableau de chaînes
char **load_file_lines(const char *filepath)
{
    FILE *f = fopen(filepath, "r");
    if (!f)
    {
        fprintf(stderr, "Impossible d'ouvrir le fichier : %s\n", filepath);
        return NULL;
    }

    char **lines = malloc(sizeof(char*) * 500);
    char buffer[2048];
    int count = 0;

    while (fgets(buffer, sizeof(buffer), f))
    {
        lines[count] = strdup(buffer);
        count++;
    }
    lines[count] = NULL;

    fclose(f);
    return lines;
}

void free_file_lines(char **lines)
{
    if (!lines) return;
    for (int i = 0; lines[i] != NULL; i++)
    {
        free(lines[i]);
    }
    free(lines);
}

void execute_test_file(const char *filepath)
{
    printf("\n=== Lancement du test : %s ===\n", filepath);
    char **lines = load_file_lines(filepath);
    if (!lines) return;

    int result = trad_c((char *)filepath, &lines);
    if (result == 0)
    {
        printf("RESULTAT : Traduction réussie.\n");
    }
    else
    {
        printf("RESULTAT : Erreur interceptée avec succès (Code %d).\n", result);
    }

    free_file_lines(lines);
}

int main()
{
    execute_test_file("test_succes.l");
    execute_test_file("test_erreurs_syntaxe.l");
    execute_test_file("test_erreurs_types.l");

    return 0;
}
