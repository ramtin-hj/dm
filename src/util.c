#include "util.h"
#include <string.h>

const char *base_path(const char *path)
{
        const char *p = NULL;
#ifdef _WIN32
        const char *p1 = strrchr(path, '/');
        const char *p2 = strrchr(path, '\\');

        if (p1 && p2)
                p = (p1 > p2) ? p1 : p2;
        else
                p = p1 ? p1 : p2;
#else 
        p = strrchr(path, '/');
#endif

        if (p)
                path = p + 1;

        return path;
}
