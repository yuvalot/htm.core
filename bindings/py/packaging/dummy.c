#include <stdio.h>
#include <Python.h>

int main(void)
{
    return 0;
}

#if PY_MAJOR_VERSION >= 3
void PyInit_dummy(void) {}
#else
void initdummy(void) {}
#endif

