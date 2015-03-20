#include <stdio.h>
#include <stdlib.h>

#if defined(__linux__) || !defined(__NetBSD__)
# error compiler wrapper fail
#endif

int main (int argc, char *argv[])
{
    char *world = getenv ("WORLD");
    if (world)
        printf ("Hello, %s!\n", world);
    else
        printf ("Hello, world!\n");
    printf ("Sleeping 5s...\n");
    sleep (5);
    printf ("Goodbye, world!\n");
    return 0;
}
