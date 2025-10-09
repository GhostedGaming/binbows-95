#ifndef CMD_PARSER_H
#define CMD_PARSER_H

#include <shell.h>

void parse_args(const char *command, char args[][MAX_ARG_LENGTH], int *argc);
void parse_command(void);

#endif