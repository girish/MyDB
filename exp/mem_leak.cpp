#include<iostream>
using namespace std;

int main(){
    // simple memory leak test
    int* x = new int[10];
    x++;
    cout << x;
    delete[] x;
}