#include "SkipList.h"

Node::Node(int inputKey, int inputLevel)
{
	this->m_key = inputKey;
	m_forward = new Node*[inputLevel + 1];

	memset(m_forward, 0, sizeof(Node*)*(inputLevel + 1));
}

Node::~Node()
{
	delete [] m_forward;
}

void SkipList::display()
{
}

bool SkipList::contains(int &)
{
	return false;
}

void SkipList::insert_Node(int &value)
{
	Node *x = header;
	Node *update[MAXLEVEL + 1];
	memset(update, 0, sizeof(Node*) * (MAXLEVEL + 1));
	for (int i = MAXLEVEL; i >= 0; i--)
	{
		while (x->m_forward[i] != NULL && x->m_forward[i]->m_key < value)
		{
			x = x->m_forward[i];
		}
		update[i] = x;
	}
	x = x->m_forward[0];
	if (x == NULL || x->value != value)
	{
		int lvl = random_level();
		if (lvl > MAXLEVEL)
		{
			for (int i = MAXLEVEL + 1; i <= lvl; i++)
			{
				update[i] = header;
			}
			level = lvl;
		}
		x = new snode(lvl, value);
		for (int i = 0; i <= lvl; i++)
		{
			x->forw[i] = update[i]->forw[i];
			update[i]->forw[i] = x;
		}
	}

}

void SkipList::delete_Node(int &)
{
}
