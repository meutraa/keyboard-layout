#include <stdio.h>

int main()
{
    int j;
    int i[] = {0, 1, 2, 3};
    printf("%d\n", i[j = 2]);
    printf("%d\n", j);
}
