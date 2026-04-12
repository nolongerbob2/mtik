// test.cpp — тестовый исходник для лабораторной (объявления, выражения, if/else, циклы, функции)

#include <iostream>
#include <vector>

static const int kLimit = 42;

/* многострочный
   комментарий перед функцией */
static int add(int a, int b) {
    return a + b;
}

static bool in_range(int x, int lo, int hi) {
    if (x < lo) {
        return false;
    } else if (x > hi) {
        return false;
    } else {
        return true;
    }
}

static void print_sum(int s) {
    std::cout << "sum=" << s << std::endl;  // вызов и строковый литерал с //
}

int main() {
    int n = 5;
    int sum = 0;
    int prod = 1;
    double ratio = 0.0;
    bool flag = true;

    sum = add(2, 3);
    prod = n * (n - 1);
    ratio = static_cast<double>(sum) / static_cast<double>(n);

    if ((sum > kLimit) && flag) {
        print_sum(sum);
    } else if (sum == 0 || !flag) {
        print_sum(0);
    } else {
        print_sum(sum / 2);
    }

    for (int i = 0; i < n; ++i) {
        prod += i;
        if (i % 2 == 0) {
            sum -= 1;
        }
    }

    while (n > 0) {
        --n;
        if (n == 2) {
            break;
        }
    }

    std::vector<int> v = {1, 2, 3};
    int acc = 0;
    for (int x : v) {
        acc += x;
    }

    return (in_range(acc, 0, 100) && prod >= 1 && ratio >= 0.0) ? 0 : 1;
}
