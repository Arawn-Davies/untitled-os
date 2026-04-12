/*
 * shell_priv.h -- internal declarations shared between shell source files.
 *
 * Not part of the public kernel API; include only from shell*.c files.
 */

#ifndef SHELL_PRIV_H
#define SHELL_PRIV_H

#include <stddef.h>

#define SHELL_MAX_INPUT  256
#define SHELL_MAX_ARGS   8
#define SHELL_PROMPT     "makar-sh> "

#define BUILD_DATE __DATE__
#define BUILD_TIME __TIME__
#define COPYRIGHT  "Copyright (c) 2026 Arawn Davies"

/* shell.c – REPL core */
void shell_readline(char *buf, size_t max);

/* shell_help.c – help & version commands */
void cmd_help(void);
void cmd_version(void);

/* shell_cmds.c – all other built-in commands */
void cmd_clear(void);
void cmd_echo(int argc, char **argv);
void cmd_meminfo(void);
void cmd_uptime(void);
void cmd_tasks(void);
void cmd_lsdisks(void);
void cmd_lspart(int argc, char **argv);
void cmd_mkpart(int argc, char **argv);
void cmd_readsector(int argc, char **argv);
void cmd_setmode(int argc, char **argv);
void cmd_shutdown(void);
void cmd_reboot(void);

/* FAT32 commands */
void cmd_mount(int argc, char **argv);
void cmd_umount(void);
void cmd_ls(int argc, char **argv);
void cmd_cat(int argc, char **argv);
void cmd_cd(int argc, char **argv);
void cmd_mkdir(int argc, char **argv);
void cmd_mkfs(int argc, char **argv);

#endif /* SHELL_PRIV_H */
