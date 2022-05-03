#include <iostream>
using namespace std;

enum test
{
	qwe,
	qwe1
};

enum test2
{
	qwe3,
	qwe4
};

int main()
{
	cout << qwe << "\n";
	cout << qwe3 << "\n";
	if (qwe == qwe3)
		cout << "equal\n";
	else
		cout << "false\n";
}