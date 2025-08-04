#ifndef COMMANDS_H
#define COMMANDS_H

#include "shell.h"

// Command function prototypes
void cmd_help(int argc, char args[][MAX_ARG_LENGTH]);
void cmd_hello(int argc, char args[][MAX_ARG_LENGTH]);
void cmd_clear(int argc, char args[][MAX_ARG_LENGTH]);
void cmd_echo(int argc, char args[][MAX_ARG_LENGTH]);
void cmd_history(int argc, char args[][MAX_ARG_LENGTH]);
void cmd_uptime(int argc, char args[][MAX_ARG_LENGTH]);
void cmd_exit(int argc, char args[][MAX_ARG_LENGTH]);

// Command table - defined in commands.c
extern const shell_command_t commands[];

#endif // COMMANDS_H