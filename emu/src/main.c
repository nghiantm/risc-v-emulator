#include <stdio.h>

enum { EXIT_ERROR = 3 };

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    fputs("usage: emu [--max-insts N] [--trace] [--fault NAME[:key=val,...]]... program.elf\n", stderr);
    return EXIT_ERROR;
}
