#include <string.h>

char* strncpy(char* restrict dst, const char* restrict src, size_t n) {
	char* ret = dst;
	while (n && (*dst++ = *src++))
		n--;
	while (n--)
		*dst++ = '\0';
	return ret;
}
