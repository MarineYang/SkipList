// DoubleLinkedList.cpp : 이 파일에는 'main' 함수가 포함됩니다. 거기서 프로그램 실행이 시작되고 종료됩니다.
//

#include <iostream>
#include "SkipList.h"

using namespace std;

int main()
{
	SkipList ss;
	int choice, n;
	while (1)
	{
		cout << endl << "-----------------------" << endl;
		cout << endl << "Operations on Skip list" << endl;
		cout << endl << "-----------------------" << endl;
		cout << "1.Insert Element" << endl;
		cout << "2.Delete Element" << endl;
		cout << "3.Search Element" << endl;
		cout << "4.Display List " << endl;
		cout << "5.Exit " << endl;
		cout << "Enter your choice : ";
		cin >> choice;
		switch (choice)
		{
		case 1:
			cout << "Enter the element to be inserted: ";
			cin >> n;
			ss.insert_Node(n);
			if (ss.contains(n))
				cout << "Element Inserted" << endl;
			break;
		case 2:
			cout << "Enter the element to be deleted: ";
			cin >> n;
			if (!ss.contains(n))
			{
				cout << "Element not found" << endl;
				break;
			}
			ss.delete_Node(n);
			if (!ss.contains(n))
				cout << "Element Deleted" << endl;
			break;
		case 3:
			cout << "Enter the element to be searched: ";
			cin >> n;
			if (ss.contains(n))
				cout << "Element " << n << " is in the list" << endl;
			else
				cout << "Element not found" << endl;
		case 4:
			cout << "The List is: ";
			ss.display();
			break;
		case 5:
			exit(1);
			break;
		default:
			cout << "Wrong Choice" << endl;
		}
	}
	return 0;
}
