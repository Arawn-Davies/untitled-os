#ifndef _KERNEL_VICS_H
#define _KERNEL_VICS_H

/*
 * vics.h — VICS interactive text editor for Makar.
 *
 * VICS (Visual Interactive Character Shell) is a simple full-screen text
 * editor that matches the Medli VICS editor in key bindings and file format.
 *
 * Key bindings:
 *   Arrow keys  — navigate (up/down/left/right)
 *   Printable   — insert character at cursor
 *   Backspace   — delete character before cursor; join lines at column 0
 *   Enter       — split line at cursor (insert newline)
 *   Tab         — insert 4 spaces
 *   Ctrl+S      — save file
 *   Ctrl+Q      — quit (press twice to discard unsaved changes)
 *
 * Layout (VGA 80×25):
 *   Rows 0–23  : editable text (24 visible lines)
 *   Row  24    : status bar
 */

/*
 * vics_edit – open path for editing (creates an empty buffer if the file
 * does not exist or cannot be read).  Blocks until the user quits.
 *
 * path: absolute VFS path, e.g. "/hd/notes.txt".  Pass NULL or "" to open
 * an unnamed new buffer (save will fail — user will be warned).
 */
void vics_edit(const char *path);

#endif /* _KERNEL_VICS_H */
