#include<ctime>
#include<iostream>
using namespace std;

int main(){
    time_t start;
    int a[10];
    a[-1] = 1111111111;
    cout << a[-1];
    start = time(NULL);
    cout << start << endl;
    return 0;
}