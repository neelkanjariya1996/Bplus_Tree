#include <stdio.h>

int
main ()
{
        int i = 1;

        for (i = 4; i <= 200; i = i + 5) {
                printf("Insert(%d, %d)\n", i, i);
        }

        for (i = 2; i <= 200; i = i + 5) {
                printf("Insert(%d, %d)\n", i, i);
        }
        for (i = 3; i <= 200; i = i + 5) {
                printf("Insert(%d, %d)\n", i, i);
        }
        for (i = 1; i <= 200; i = i + 5) {
                printf("Insert(%d, %d)\n", i, i);
        }
        for (i = 0; i <= 200; i = i + 5) {
                printf("Insert(%d, %d)\n", i, i);
        }

        for (i = -4; i >= -200; i = i - 5) {
                printf("Insert(%d, %d)\n", i, i);
        }
        for (i = -2; i >= -200; i = i - 5) {
                printf("Insert(%d, %d)\n", i, i);
        }
        for (i = -3; i >= -200; i = i - 5) {
                printf("Insert(%d, %d)\n", i, i);
        }
        for (i = -1; i >= -200; i = i - 5) {
                printf("Insert(%d, %d)\n", i, i);
        }
        for (i = -0; i >= -200; i = i - 5) {
                printf("Insert(%d, %d)\n", i, i);
        }

}
