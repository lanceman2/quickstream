
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <stddef.h>

struct Foo {

    int x;
};


struct Bar {

    int x;
    char y;
    struct Foo foo;
};



void printx(struct Foo *f) {

     struct Bar *b = 0;

     uint32_t diff = (uint32_t) ((uintptr_t) (((void *) &b->foo) -
             (uintptr_t) ((void *) b)));

     b = (struct Bar *) (((void *)f) - diff);

     printf("diff Address=%" PRIu32 "\n", diff);

     printf("x=%d\n", b->x);

     // Now I get it:
     b = ((void *) f) - offsetof(struct Bar, foo);

     printf("x=%d\n", b->x);
}



int main(void) {


    struct Bar b;
    b.x = 1234;
    b.foo.x = 45;

    printx(&b.foo);


    return 0;
};
