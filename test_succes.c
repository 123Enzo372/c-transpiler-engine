#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include <math.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
int calculate()
{
    return 42;
}
int main()
{
    //  Inférence de type : entiers, flottants, expressions et retours de fonction
    int a = 10;
    int pi = 3.14;
    int ratio = a + 2.5;
    int res = calculate();
    //  Listes et interpolation de chaines
    int *numbers = malloc(3 * sizeof(int));
    numbers[0] = 1;
    numbers[1] = 2;
    numbers[2] = 3;
    int numbers_len = 3;
    printf("Inférence : a=%d, ratio=%d, res=%d\n", a, ratio, res);
    //  Bloc de processus et pipe anonyme
    int pipe_fd[2];
    if (pipe(pipe_fd) == -1)
    {
        perror("Error: anonymous pipe creation failed");
        return -1;
    }
    pid_t pid = fork();
    if (pid < 0)
    {
        perror("Error: child process initialization failed");
        return -1;
    }
    if (pid == 0)
    {
        int valeur = 100;
        printf("Enfant écrit : %d\n", valeur);
        if (numbers != NULL) { free(numbers); numbers = NULL; }
        return 0;
    }
    else
    {
        int status;
        waitpid(pid, &status, 0);

        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
        {
            return -1;
        }
        {
            printf("Parent synchrone terminé\n");
        }
        //  Match étendu avec gardes conditionnelles
        int n = 5;
        if (n > 10)
            printf("Grand nombre\n");
        else if (n > 0)
            printf("Nombre positif\n");
        else
            printf("Autre\n");
        int *items = malloc(3 * sizeof(int));
        items[0] = 10;
        items[1] = 20;
        items[2] = 30;
        int items_len = 3;
        {
            if (items_len == 0)
                printf("Liste vide\n");
            else if (items_len == 1)
            {
                int x = items[0];
                printf("Un seul element : %d\n", x);
            }
            else if (items_len > 0)
            {
                int head = items[0];
                int* tail = items + 1;
                items_len--;
                printf("Plusieurs elements\n");
            }
        }
        if (numbers != NULL) { free(numbers); numbers = NULL; }
        if (items != NULL) { free(items); items = NULL; }
        return 0;
    }
}
