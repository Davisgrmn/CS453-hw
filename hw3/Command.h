#ifndef COMMAND_H
#define COMMAND_H

#include "Tree.h"

typedef void *Command;

extern Command newCommand(T_command tree);
extern int isBuiltinCommand(Command command);
/* Execute in the calling process. Only foreground builtins use child == 0. */
extern int execCommand(Command command, int *eof, int child);
extern void freeCommand(Command command);
extern void freestateCommand(void);

#endif
