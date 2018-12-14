#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void do_work(void) {
    puts("do_work enter");

    char *p = new char[666];
    p = new char[666];
    puts("do_work return");
}

int main(void) {
    do_work();

    write(1, "exiting", 7);

    return 0;
}
