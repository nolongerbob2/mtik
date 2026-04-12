#include <stdio.h>
#define THRESHOLD 10
static int add(int a, int b) {
return a + b;
}
static int max(int x, int y) {
if (x > y) {
return x;
} else {
return y;
}
}
int main(void) {
int i;
int sum = 0;
int product = 1;
float ratio = 0.0f;
int n = 5;
int flag = 1;
sum = add(2, 3);
product = n * (n - 1);
ratio = (float)sum / (float)n;
if ((sum > THRESHOLD) && (flag != 0)) {
printf("sum is large\n");
} else if (sum == 0 || flag == 0) {
printf("edge\n");
} else {
printf("ok: %d\n", sum);
}
for (i = 0; i < n; i++) {
product += i;
if (i % 2 == 0) {
sum -= 1;
}
}
while (n > 0) {
n--;
if (n == 2) {
break;
}
}
printf("max=%d product=%d ratio=%f\n", max(sum, product), product, ratio);
return 0;
}
