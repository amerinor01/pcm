#include <stdio.h>

int foo(int x) {
    if (x > 0) {
        for (int i = 0; i < x; i++) {
            printf("Loop iteration %d\n", i);
        }
        return x * 2;
    } else {
        return -1;
    }
}

int main() {
    int result = foo(42);
    printf("Result: %d\n", result);
    return 0;
}
