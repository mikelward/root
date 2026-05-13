#ifndef PATH_H
#define PATH_H

#define PATHENVSEP ":"
#define DIRSEP '/'

char *get_command_path(const char *command, const char *pathenv);
int is_absolute_path(const char *path);
int is_qualified_path(const char *path);
int is_unqualified_path(const char *path);
void pathenv_each(const char *pathenv, void (*func)(const char *pathentry));

#endif
/* vim: set ts=4 sw=4 tw=0 et:*/
