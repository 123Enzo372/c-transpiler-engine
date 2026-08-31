#ifndef MAIN_H
#define MAIN_H

#include <stdio.h>

extern int flag_french;

int parser(FILE *file, char ***text_ptr);
int trad_h(char *filename, char ***text_ptr);
int trad_c(char *filename, char ***text_ptr);
int execute(char **filename, char *output, char **options);

void remove_created_files(char **created_filenames, char **new_option);
void remove_H_files(char **filename);
void remove_l_files(char **filename);

#endif /* MAIN_H */

