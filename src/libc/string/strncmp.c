#include <string.h>

int strncmp(const char* a, const char* b, size_t n) {
	if (!n)
		return 0;
	while (--n && *a && (*a == *b)) {
		a++;
		b++;
	}
	return (unsigned char)*a - (unsigned char)*b;
}
