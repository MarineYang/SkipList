#pragma once
#include <iostream>

const unsigned int	MAXLEVEL = 6;
using namespace std;

class Node
{
public:
	int m_key;
	Node** m_forward;

public:
	Node(int inputKey, int inputLevel);
	~Node();
};

class SkipList
{
public:
	Node* header;
	int m_key;

public:
	SkipList() {
		header = new Node(MAXLEVEL, m_key);
	}
	~SkipList() {
		delete header;
	}

	void display();
	bool contains(int &);
	void insert_Node(int &);
	void delete_Node(int &);



};