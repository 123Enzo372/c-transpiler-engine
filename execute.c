#include "main.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>


int execute(char **filename, char *output, char **options)
{
    if (filename == NULL || filename[0] == NULL)
    {
        if (flag_french)
            fprintf(stderr, "ERREUR : Liste de fichiers sources vide ou non initialisée pour la compilation C.\n");
        else
            fprintf(stderr, "ERROR : Source file list is empty or uninitialized for C compilation.\n");
        return 1;
    }
    
    pid_t pid = fork();
    if (pid < 0)
    {
        if (flag_french)
            fprintf(stderr, "ERREUR SYSTEME : L'initialisation du processus enfant (fork) a échoué.\n");
        else
            fprintf(stderr, "SYSTEM ERROR : Child process initialization (fork) failed.\n");
        return -1;
    }

    if (pid == 0)
    {
        int nb_options = 0;
        while (options != NULL && options[nb_options] != NULL)
        {
            nb_options++;
        }

        int nb_files = 0;
        while (filename[nb_files] != NULL)
        {
            nb_files++;
        }
        
        int total_arg = 1 + nb_options + nb_files + 1;
        if (output != NULL)
        {
            total_arg += 2;
        }

        char **commande = malloc(sizeof(char *) * total_arg);
        if (commande == NULL)
        {
            if (flag_french)
                fprintf(stderr, "ERREUR SYSTEME : Échec d'allocation mémoire pour 'commande'.\n");
            else
                fprintf(stderr, "SYSTEM ERROR : Memory allocation failed for 'commande'.\n");
            exit(EXIT_FAILURE);
        }
        
        int index = 0;
        commande[index++] = "gcc";

        for (int i = 0; i < nb_options; i++)
        {
            commande[index++] = options[i];
        }

        for (int i = 0; i < nb_files; i++)
        {
            commande[index++] = filename[i];
        }

        if (output != NULL)
        {
            commande[index++] = "-o";
            commande[index++] = output;
        }

        commande[index++] = NULL;

        execvp("gcc", commande);

        if (flag_french)
            fprintf(stderr, "ERREUR SYSTEME : L'exécution du compilateur 'gcc' via execvp a échoué.\n");
        else
            fprintf(stderr, "SYSTEM ERROR : Execution of 'gcc' compiler via execvp failed.\n");
        free(commande);
        exit(EXIT_FAILURE);
    }
    else 
    {
        int status;
        waitpid(pid, &status, 0);
        
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
        {
            return 0;
        }
        else 
        {
            if (flag_french)
                fprintf(stderr, "ERREUR : La compilation C intermédiaire avec GCC a échoué.\n");
            else
                fprintf(stderr, "ERROR : Intermediate C compilation with GCC failed.\n");
            return -1;
        }
    }
}
