#include <iostream>
using namespace std;
int main(int argc, const char *argv[])
{
    // sizeof return only stack memory size
    int* p;
    p = new int[40];
    cout << sizeof(p) << endl;
    cout << sizeof(&(p[0])) << endl;
    return 0;
}