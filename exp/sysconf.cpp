#include <iostream>
using namespace std;
int main(int argc, const char *argv[])
{
    int pagesize = sysconf(_SC_PAGESIZE);
    long long clockspeed = sysconf(_SC_CLK_TCK);
    cout << pagesize << "  " << clockspeed << '\n';
    return 0;
}