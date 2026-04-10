#include <string.h>

char* strrchr(const char* str, int c) {
	const char* last = NULL;
	do {
		if (*str == (char)c)
			last = str;
	} while (*str++);
	return (char*)last;
}
