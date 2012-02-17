#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
int main(int argc, const char *argv[])
{
    int f;
    char a[10];
    f = open("f", O_WRONLY | O_APPEND);
    write(f, a, 10);
    return 0;
}