#include <iostream>

using namespace std;


int time;

string test(int value1,int value2){
    cout << "test function : " << time++ << endl;
    if(value1+value2 != 0){
        return"value1 + value2 = "
         + to_string(value1+value2);
    } else {
        return "it is zero";
    }
}

int main() {
    // input
    /*
    int x;
    cout << "Enter a number: ";
    cin >> x;
    cout << x;
    */

    // ค่าคงที่
    const string NAME_PIE = "PP";

    string name = "PP";
    float pie = 3.14;
    int a = 10 , b = 20;
    a = 20;
    b += 10;
    a++;
    b = b % 3;
    if (a > b ){
        cout << b << " hi\n";
    } else if (a == b) {
        cout << "a == b\n";
    }

    // display massage
    std::cout << "Hello, JWT!\n" ;

    std::cout << "pangpone\n";
    cout << "pangpone999";

    int N = 0;
    while (N < 10)
    {
        cout << N << endl;
        ++N;
    }

    for (int i = 0; i < N; i++)
    {
        cout << i << endl;
    }
    
    int num[] = {1,2,3,4};
    num[0] = 10;

    cout << endl << sizeof(int) << " is size of int\n";
    cout << sizeof(num)/4;
    
    for (int i = 0; i < sizeof(num)/4; i++)
    {
        cout << endl << "index in array:" << i+1
        <<" => " << num[i] << endl;
    }
    
    test(0,0);
    test(5,5);
    test(1,2);

    return 0;
}