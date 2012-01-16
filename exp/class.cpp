#include<iostream>
using namespace std;
class Wtf {
public:
    int x;
    int y;
    Wtf(){
        cout << "This sucks!!";
    }
    Wtf(int x, int y){
        cout << "Really";
        this->x = x;
        this->y = y;
    }
};
int main(){
    /*
    Wtf c, d(5, 6);
    cout << d.x << endl;
    cout << d.y << endl;
    cout << c.x << endl;
    cout << c.y << endl;*/

    Wtf* c = new Wtf(5,6);
}