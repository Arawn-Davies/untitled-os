#include <string.h>

char* strstr(const char* haystack, const char* needle) {
	if (!*needle)
		return (char*)haystack;
	size_t nlen = strlen(needle);
	for (; *haystack; haystack++) {
		if (strncmp(haystack, needle, nlen) == 0)
			return (char*)haystack;
	}
	return NULL;
}
