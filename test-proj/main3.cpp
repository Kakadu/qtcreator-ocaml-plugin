void fib_helper22(int n, int a, int b) {
    if (n<=2)
        return 1;
    else
        return fib_helper22(n-1, b, a+b);
}
void fib(int n) {
    fib_helper22(n,1,1);
}


class A {

} /* */
