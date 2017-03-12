/*

Queue/Tree base classes.
	TList is a base class to define a collection element.  It
	contains a left and right pointer to another TList item and
	these may be organized as a double-linked linear list or
	binary tree in the collection classes that use TList items.

	Central to its utility are member functions to convert between
	binary tree, sorted 2-way linear linked lists, and unsorted 2-way
	linked linear lists.

Functions:
    TreeToSortedList
        Converts the binary tree specified by its head (tree)
        to a two way sorted linked linear list.  This is done
        without moving any data, rather just readjusting the
        pointers so that they form the new data structure.
        This is a recursive procedure whose algorithm may
        be conceptualized by linking the current node (starting at head)
        left pointer to the rightmost node on the lefthand side.
        Also setting the right pointer to the leftmost node on
        the right side and recursively repeating this process
        down both sides of the tree, sub-tree and so forth.

    ListSort
        Takes the linear list (unsorted by key)
        and sorts it by the key(s) specified by the input comparison
        function.  The comparison function is provided with pointers
        to two values, and returns a negative, zero, or positive value
        depending upon if value1 is less than, equal to, or greater
        than value2, respectively.  The algorithm used is to convert
        the linear TList data structure to a binary tree and
        then to use ListToTree to convert this to a linear list sorted
        according to the parameter comparison routine.

    ListSortedToTree
		Converts the input sorted Tlist into a balanced binary
		tree by recursively splitting it and readjusting the pointers.
		This routine is very fast because no comparisons are required
		since the list is already in order of the key for the tree.

    ListToTree
===============================================================================
*/

#include <stdlib.h>
//#include "stdafx.h"
#include "TList.h"

// Flattens binary tree into an in-order linear list
// ***Warning: Must not pass this==NULL
TList *										 // ret-head of sorted list
	TList::TreeToSortedList(
		TList             ** p_head			,// out-leftmost branch from tree
		TList             ** p_tail			)// out-rightmost branch from tree
{
	TList				   * temp;			// catch the head/tail value at each recursion level and set it at the right time

	if ( left == NULL )
		*p_head = this;						// this is leftmost of parent node
	else
	{
		left->TreeToSortedList(p_head, &temp);	// recurse down left side to get its leftmost node for head and righmost node in temp
		left = temp;						// left = tail of sub-list
		left->right = this;
	}
	if ( right == NULL )
		*p_tail = this;						// tree is rightmost of parent node
	else
	{
		right->TreeToSortedList(&temp, p_tail);
		right = temp;						// right = head of sub-list
		right->left = this;
	}
	return *p_head;
}

// Sorts linear list according to p_Compare by using p_Compare to convert to a tree and then flattens the tree
TList *										 // ret-new head of sorted list
   TList::ListSort(
      TListCompare( (* p_Compare) )			)// in -compare function that defines order
{
	TList                   * head = NULL,
							* tail  = NULL;

	return this ? ListToTree(p_Compare)->TreeToSortedList(&head, &tail)  :  NULL;
}


// 
void
	TList::ListSort(
		TListCompare( (* p_Compare) )		,// in -compare function that defines order
		TList               ** p_head		,// out-returned head
		TList               ** p_tail		)// out-returned tail
{
	ListToTree(p_Compare)->TreeToSortedList(p_head, p_tail);
	return;
}


// Inserts the list starting at this and inserts it into a new binary tree according to p_SortCompare ordering
TList *											 // ret-new head of tree
	TList::ListToTree(
		TListCompare( (* p_Compare) )			)// in -compare function for new order
{
	TList				* treehead = NULL;
	int maxdepth = 0, depth = 0, n = 0;

	for ( TList *next, *curr = this;  curr;  curr = next )	// for each node in list, insert into binary tree
	{
		n++;
		next = curr->right;									// save next/right pointer
		curr->right = curr->left = NULL;					// clear old links
		
		depth = 0;
		TList	** prevNext = &treehead;					// start insertion at top of tree each time
		while ( *prevNext )									// find place to insert by going down in the SortCompare direction each time
		{
			depth++;
			if ( p_Compare(*prevNext, curr) < 0 )
				prevNext = &(*prevNext)->left;	
			else
				prevNext = &(*prevNext)->right;
		}
		if ( depth > maxdepth) maxdepth = depth;
		*prevNext = curr;									// at tree insertion point: insert item
	}
	return treehead;
}


// Converts ordered 2-linked list into balanced binary tree in the same order
TList *										 // ret-middle of list (head of new binary tree)
	TList::ListSortedToTree()
{
	TList				  * mid = this;		// middle of list
	bool   					odd = true;		// odd flag - used to advanced every other time to find list middle

	if ( this == NULL )
		return NULL;
	for ( TList * curr = this;  curr;  curr = curr->right )		// find list middle
		if ( odd = !odd )
			mid = mid->right;
	if ( mid->left )											// split list around mid point
	{
		mid->left->right = NULL;								// right terminate new sublist
		mid->left = ListSortedToTree();							// recursive call to set left side
	}
	if ( mid->right )
	{
		mid->right->left = NULL;								// left-terminate new sublist
		mid->right = mid->right->ListSortedToTree();			// recursive call to set right side
	}
	return mid;
}


// Deletes all items in a tree via recursive iteration
static void RecursiveTreeDelete(TList * p_item)
{
	if ( !p_item )
		return;
	RecursiveTreeDelete(p_item->left);
	RecursiveTreeDelete(p_item->right);
	delete p_item;
}

// sets the sort order of the list/tree by setting the compare functions
void TListCollection::SetSort(
		TListCompare((* p_SortCompare))			,// in -the new sort compare function pointer
		TListSearchCompare((* p_SearchCompare))	)// in -the new search compare function pointer
{
	if ( m_SortCompare == p_SortCompare)
		return;		// nothing to do - if they are the same function, including both NULL, this is a no-op
	if ( m_SortCompare )
	{
		if ( p_SortCompare )			// different sorts: flatten to list in same order and then change sort order into tree
		{
			ToLinear();
			ToTree(p_SortCompare, p_SearchCompare);
		}
		else							// sort to no sort: flatten tree to list in existing order
		{
			ToLinear();
		}
	}
	else
	{
		if ( p_SortCompare )			// list to sort order: convert to tree with the sort compare function
		{
			ToTree(p_SortCompare, p_SearchCompare);	// take unsorted list and convert into binary tree according to SortCompare function
		}
		// else already handled above (existing and desired sort compare are both NULL)
	}
	m_SortCompare = p_SortCompare;
	m_SearchCompare = p_SearchCompare;
}


// Deletes all TList entries in TListCollection
void TListCollection::DeleteAll()
{
	if ( m_SortCompare )
		RecursiveTreeDelete(m_head);
	else
	{
		for ( TList * next, * item = m_head;  item;  item = next )
		{
			next = item->right;
			delete item;
		}
	}
	m_head = m_tail = NULL;
	m_count = 0;

}

TListCollection::~TListCollection()
{
	if ( m_SortCompare )
		RecursiveTreeDelete(m_head);
	else
	{
		for ( TList * next, * item = m_head;  item;  item = next )
		{
			next = item->right;
			delete item;
		}
	}
	m_head = m_tail = NULL;
}


// Inserts p_ins at the end of the list or in its appropriate position in the tree
void TListCollection::Insert(TList * p_ins)
{
	if ( m_SortCompare )
		TreeInsert(p_ins);
	else
		InsertBottom(p_ins);
}


// Inserts p_ins at the beginning of the list
void TListCollection::InsertTop(TList * p_ins)
{
	p_ins->right = m_head;
	p_ins->left  = NULL;
	if ( m_head )
		m_head->left = p_ins;
	else
		m_tail = p_ins;
	m_head = p_ins;
	m_count++;
}


// Inserts p_ins at the end of the list
void TListCollection::InsertBottom(TList * p_ins)// i/o-element to be inserted
{
	p_ins->right = NULL;
	p_ins->left  = m_tail;
	if ( m_tail )
		m_tail->right = p_ins;
	else
		m_head = p_ins;
	m_tail = p_ins;
	m_count++;
	return;
}


// Inserts p_ins in the list after p_after
void TListCollection::InsertAfter(
		TList			  * p_ins			,// i/o-item to be inserted
		TList			  * p_after			)// i/o-item insert point
{
	TList				  * next;			// element after inserted element

	if ( !p_after )
		InsertTop( p_ins );
	else
	{
		next = p_after->right;
		p_ins->right = next;
		p_ins->left = p_after;
		if ( next )
			next->left = p_ins;
		else
			m_tail = p_ins;
		p_after->right = p_ins;
		m_count++;
	}
}


// Inserts p_ins in the list before p_before
void TListCollection::InsertBefore(
		TList                 * p_ins			,// i/o-element to be inserted
		TList                 * p_before		)// i/o-element insert point
{
	TList					  * prev;			// element before inserted element

	if ( !p_before )
		InsertBottom( p_ins );
	else
	{
		prev = p_before->left;
		p_ins->right = p_before;
		p_ins->left  = prev;
		if ( prev )
			prev->right = p_ins;
		else
			m_head = p_ins;
		p_before->left = p_ins;
		m_count++;
	}
}

// Remove p_del from the list
void TListCollection::Remove(TList const * p_del)	// i/o-new node to remove from list but not delete
{
	if ( p_del->left )
		p_del->left->right = p_del->right;
	else
		m_head = p_del->right;

	if ( p_del->right )
		p_del->right->left = p_del->left;
	else
		m_tail = p_del->left;
	m_count--;
}

// Inserts p_ins into the binary tree ordered by m_SortCompare
void TListCollection::TreeInsert(TList * p_ins)			// i/o-node to insert into binary tree
{
	TList				 ** prevNext = &m_head;
	__int16					depth = 0;

	while ( *prevNext )
	{
		depth++;
		int cmp = m_SortCompare(*prevNext, p_ins);
		if ( cmp <= 0 )
			prevNext = &(*prevNext)->left;
		else
			prevNext = &(*prevNext)->right;
	}
	*prevNext = p_ins;
	p_ins->left = p_ins->right = NULL;
	m_count++;
	if ( depth > m_maxdepth )
		m_maxdepth = depth;
}


// Removes p_rem from a tree and reorganizes it around the removal
void TListCollection::TreeRemove(TList * p_rem)
{
	TList				  * repLeft,
						  * temp;

	for ( TList **prevNext = &m_head;  *prevNext; )
	{
		int cmp = m_SortCompare( p_rem, *prevNext );
		if ( cmp < 0 )
			prevNext = &(*prevNext)->left;
		else if ( cmp > 0 )
			prevNext = &(*prevNext)->right;
		else								// found a matching value
		{	
			if ( *prevNext == p_rem )		// is it the same address?
			{
				if ( (*prevNext)->right )
				{
					TList * rep = repLeft = (*prevNext)->right;
					for ( temp = rep->left;  temp;  temp = temp->left )
						repLeft = temp;
					repLeft->left = (*prevNext)->left;
					temp = *prevNext;
					*prevNext = rep;
				}
				else
				{
					temp = *prevNext;
					*prevNext = (*prevNext)->left;		// easy case
				}
				temp->left = temp->right = NULL;		// break removed node's links to existing tree
				break;
			}
		}
	}
	m_count--;
}

// Find and return the TList item having p_key, or NULL - 
// NOTE: m_SearchCompare must be set properly and the tree ordered by m_SortCompare
TList * TListCollection::Search(void const * p_key) const
{
	for ( TList * curr = m_head;  curr;  )
	{
		int cmp = m_SearchCompare(curr, p_key);
		if ( cmp < 0 )
			curr = curr->left;	
		else if ( cmp > 0 )
			curr = curr->right;
		else
			return curr;		// found it!  return it.
	}
	return NULL;
}

//=================================================

TList *	TListTreeEnum::EnumOpen()
{
	m_stackPos = m_stackBase - 1;
	if ( !m_top )
		return NULL;

	Push(m_top);
	return EnumNext();
}


TList * TListTreeEnum::EnumNext()
{
	for ( ;; )
	{
		switch ( m_stackPos->state )
		{
			case Snone:                       // we've done nothing here
				m_stackPos->state = Sleft;
				if ( m_stackPos->save->left )
					Push(m_stackPos->save->left);
				break;
			case Sleft:                       // we've gone left and are back
				m_stackPos->state = Sused;
				return m_stackPos->save;
			case Sused:                       // we've used the node
				m_stackPos->state = Sright;
				if ( m_stackPos->save->right )
					Push(m_stackPos->save->right);// process right side of branch
				break;
			case Sright:                      // we've gone right and are back
				if ( !Pop() )
					return NULL;
				break;
			default:                          // bad error
				return NULL;
		}
	}

	return NULL;   // can't get here
}
