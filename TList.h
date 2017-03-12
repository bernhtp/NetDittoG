/*
	Tom Bernhardt 7-2010
*/
#ifndef TLIST_H
#define TLIST_H


// TListCompare generates a prototype/header/type for a compare function used in sorted lists
#define TListCompare(funcname)														\
int										 /* ret-0(v1==v2) >0(v1>v2) <0(v1<v2)	*/	\
	funcname(																		\
		TList       const * v1          ,/* in -value1 to compare				*/	\
		TList       const * v2          )/* in -value2 to compare				*/	

#define TListSearchCompare(funcname)												\
int										 /* ret-0(v1==v2) >0(v1>v2) <0(v1<v2)	*/	\
	funcname(																		\
		TList const		  * v1          ,/* in -TList obj to compare			*/	\
		void const		  * v2          )/* in -value2 to compare				*/	


class TList
{  
	friend class TListCollection;
public:
	TList				  * left;
	TList				  * right;
	
						TList() : left(NULL), right(NULL) {}
	TList *					TreeToSortedList(TList ** p_head, TList ** p_tail);							// flattens binary tree into an in-order linear list
	TList *					ListSort(TListCompare( (* p_Compare)) );									// Sorts list by converting it to a tree and then back again
	void					ListSort(TListCompare( (* p_Compare)), TList ** p_first, TList ** p_last);	// Sorts list by converting it to a tree and then back again
	TList *					ListToTree(TListCompare((* p_Compare)) );									// Converts linear list to binary tree accoring to p_Compare
	TList *					ListSortedToTree();															// Converts ordered linear list to binary tree without compares
};


class TListCollection
{
	friend class TListCollectionEnum;
	friend class TListTreeEnum;
protected:
	TList				  * m_head,						// head of list/tree
						  * m_tail;						// tail of list
	__int32					m_count;					// count of items in collection
	__int16					m_maxdepth;					// max depth of tree
	TListCompare((* m_SortCompare));					// Compare function pointer used for sorting - compares two TList items
	TListSearchCompare((* m_SearchCompare));			// Compare function pointer used for searching - compares TList item with value to find
public:
	TListCollection() : m_head(NULL), m_tail(NULL), m_SortCompare(NULL), m_SearchCompare(NULL), m_count(0), m_maxdepth(0) { }
	TListCollection(TListCompare((* p_SortCompare)), TListSearchCompare((* p_SearchCompare))) 
        : m_head(NULL), m_tail(NULL), m_count(0), m_maxdepth(0), m_SortCompare(p_SortCompare), m_SearchCompare(p_SearchCompare) { }
	~TListCollection();

	void					SetSort(TListCompare((* p_SortCompare)),TListSearchCompare((* p_SearchCompare)));  // sets the sort order of the list/tree by setting the compare functions
	void					Insert(TList * p_ins);												// insert at end of list or sort-order position in tree
	// List-specific functions
	void					InsertTop(TList * p_ins);											// insert at top of list
	void					InsertBottom(TList * p_ins);										// insert at end of list
	void					InsertAfter(TList * p_ins, TList * p_after);						// insert after eIns
	void					InsertBefore(TList * p_ins, TList * p_before);						// insert before eIns
	void					Remove(TList const * p_rem);										// remove item
	// Tree-specific functions
	void					ToTree() { m_head = m_head->ListSortedToTree(); m_tail = NULL; }	// convert ordered list to balanced binary tree
	void					ToTree(TListCompare((* p_SortCompare)),TListSearchCompare((* p_SearchCompare))) { m_SortCompare = p_SortCompare; m_head = m_head->ListToTree(p_SortCompare); m_SearchCompare = p_SearchCompare; }
	void					ToLinear() { if (m_head) m_head->TreeToSortedList(&m_head, &m_tail); } // collapse binary tree into orderd linear list
	void					TreeInsert(TList * p_ins);											// insert item into binary tree
	void					TreeRemove(TList * p_rem);											// remove item from binary tree
	TList *					Search(void const * p_key) const;									// find and return the TList having p_key, or NULL
	void					DeleteAll();														// Deletes all TList entries in TListCollection (tree or list form)

	__int32					Count() const { return m_count; }
};


/*
   TListCollectionEnum is a 'friend' of TList used to enumerate/iterate through
   the TGroupList linked list.  It is not part of TListCollection because there
   is often the need to maintain multiple simultaneous enumerations that can't
   be allowed to 'step' on one another.
*/
class TListCollectionEnum
{
protected:
	TListCollection const	  * m_list;			// list for which enums are carried out
	TList					  * m_curr;			// next node processed by enum functions
public:
	TListCollectionEnum() : m_list(NULL), m_curr(NULL)	{ }
    TListCollectionEnum(TListCollection const * p_tlist) : m_list(p_tlist), m_curr(NULL) { }
    ~TListCollectionEnum() { EnumClose(); };

	void						SetList(TListCollection const * p_tlist)	{ m_list = p_tlist; };
	TList *						EnumOpen(TListCollection const * p_tlist)	{ m_list = p_tlist; return EnumOpen(); };
	TList *						EnumOpen() { return m_curr = m_list->m_head; };
	TList *						EnumNext() { return m_curr = m_curr->right; };
	virtual void				EnumClose() { };
};

enum TListTreeStackEntryState {Snone, Sleft, Sused, Sright};
struct TListTreeStackEntry
{
   TList					  * save;
   TListTreeStackEntryState		state;
};


const int TREE_STACKSIZE = 300;			// tree max depth - to exceed this would require a huge and very unbalanced tree
class TListTreeEnum
{
	TListTreeStackEntry   * m_stackBase,
						  * m_stackPos;
	int                     m_stackSize;

	void					Push(TList * p_item)			{ (++m_stackPos)->save = p_item; m_stackPos->state = Snone; }
	bool					Pop()							{ return --m_stackPos >= m_stackBase; }
protected:
	TList				  * m_top;				// tree top for which enums are carried out
	TList				  * m_curr;				// next node processed by enum functions
public:
	TListTreeEnum() : m_top(NULL) { }
    TListTreeEnum(TListCollection * p_tlistt, int p_stacksize = TREE_STACKSIZE) : m_top(p_tlistt->m_head), m_stackSize(p_stacksize)	
        { m_stackBase = new TListTreeStackEntry[p_stacksize]; }
    TListTreeEnum(TList * p_tlist, int p_stacksize = TREE_STACKSIZE) : m_top(p_tlist), m_stackSize(p_stacksize)	
        { m_stackBase = new TListTreeStackEntry[p_stacksize]; }
    ~TListTreeEnum()										{ EnumClose(); delete m_stackBase; }

   void						SetTree(TList * p_tlist, int p_stacksize = TREE_STACKSIZE) {  }

   TList				  * EnumOpen();						// open enumeration and return first entry
   TList				  * EnumNext();						// return next TList entry in open enumeration
   virtual void				EnumClose()						{ m_stackPos = m_stackBase - 1; }
};

#endif	// TLIST_H
