// DXQuake3 Source
// Copyright (c) 2003 Richard Geary (richard.geary@dial.pipex.com)
//
//	c_Queue definition
//

#ifndef __C_QUEUE
#define __C_QUEUE

template <class unit>
class c_Queue {
public:
	c_Queue(int Size);
	virtual ~c_Queue();
	unit* Push(); //Push onto back of queue
	unit* PushToFront();
	unit *GetFirstUnit();
	unit *GetLastUnit();
	void Pop();				//Pop from back of queue
	void PopFromFront();

protected:
	unit *Array;
	int TotalSize, FrontUnitPos, BackUnitPos;
	BOOL isEmpty;
};

//Implementation

template <class unit>
c_Queue<unit>::c_Queue(int Size) {
	CriticalAssert( Size > 1 );
	Array			= (unit*)DQNewVoid( unit[Size] );
	TotalSize		= Size;
	FrontUnitPos	= 0;
	BackUnitPos		= 0;
	isEmpty			= TRUE;
}

template <class unit>
c_Queue<unit>::~c_Queue<unit>() 
{
	DQDeleteArray( Array );
}

template <class unit>
unit *c_Queue<unit>::Push() {
	//Check that there is space
	CriticalAssert( (BackUnitPos+1)%TotalSize != FrontUnitPos );

	if(isEmpty) {
		isEmpty = FALSE;
		return &Array[BackUnitPos];
	}

	++BackUnitPos;
	BackUnitPos = BackUnitPos%TotalSize;
	return &Array[BackUnitPos];
}

template <class unit>
unit* c_Queue<unit>::PushToFront()
{
	if(isEmpty) {
		isEmpty = FALSE;
		return &Array[BackUnitPos];
	}

	--FrontUnitPos;
	if(FrontUnitPos<0) FrontUnitPos = TotalSize-1;
	CriticalAssert( BackUnitPos != FrontUnitPos );

	return &Array[FrontUnitPos];
}

template <class unit>
unit *c_Queue<unit>::GetLastUnit()
{
	if(isEmpty) return NULL;
	return &Array[BackUnitPos];
}

template <class unit>
unit *c_Queue<unit>::GetFirstUnit()
{
	if(isEmpty) return NULL;
	return &Array[FrontUnitPos];
}

template <class unit>
void c_Queue<unit>::Pop()
{
	if(isEmpty) return;

	//Check if this is the last unit
	if(FrontUnitPos == BackUnitPos) {
		isEmpty = TRUE;
		FrontUnitPos = 0;
		BackUnitPos = 0;
		return;
	}

	--BackUnitPos;
	if(BackUnitPos<0) BackUnitPos = TotalSize-1;
}

template <class unit>
void c_Queue<unit>::PopFromFront()
{
	if(isEmpty) return;

	//Check if this is the last unit
	if(FrontUnitPos == BackUnitPos) {
		isEmpty = TRUE;
		FrontUnitPos = 0;
		BackUnitPos = 0;
		return;
	}

	++FrontUnitPos;
	FrontUnitPos = FrontUnitPos%TotalSize;
}

//***********************************
/*
template <class unit>
struct s_QueueNode {
	s_QueueNode<unit> *next;
	s_QueueNode<unit> *prev;
	unit	*current;
};

template <class unit>
class c_Queue {
public:
	c_Queue(int HeapSize);
	virtual ~c_Queue();
	unit* Push(unit* AddThis = NULL); //If AddThis == NULL, create new unit
	unit* PushToFront(unit* AddThis = NULL);
	s_QueueNode<unit> *GetFirstNode() { return FirstNode; }
	unit *GetFirstUnit();
	unit *GetLastUnit();
	void Pop();
	void PopFromFront();
	virtual void DestroyQueue();
	virtual void RemoveNode(unit *RemoveThis);

protected:
	s_QueueNode<unit> *FirstNode,*LastNode;
};

//Implementation

template <class unit>
unit *c_Queue<unit>::Push(unit* AddThis) {
	s_QueueNode<unit> *NewNode;
	unit *NewUnit;
	
	if(!AddThis) NewUnit = DQNew( unit );
	else NewUnit = AddThis;

	NewNode = DQNew( s_QueueNode<unit> );
	
	NewNode->current = NewUnit;
	NewNode->next = NewNode->prev = NULL;

	if(!FirstNode) {
		FirstNode = LastNode = NewNode;
	}
	else {
		LastNode->next = NewNode;
		NewNode->prev = LastNode;
		LastNode = NewNode;
	}

	return NewUnit;
}		

template <class unit>
unit *c_Queue<unit>::GetLastUnit()
{
	if(!LastNode) return NULL;
	return LastNode->current;
}

template <class unit>
unit *c_Queue<unit>::GetFirstUnit()
{
	if(!FirstNode) return NULL;
	return FirstNode->current;
}

template <class unit>
void c_Queue<unit>::Pop()
{
	s_QueueNode<unit> *tempnode;
	if(!LastNode) return;

	DQDelete(LastNode->current);
	tempnode = LastNode->prev;
	DQDelete(LastNode);
	LastNode = tempnode;
	if(LastNode)
		LastNode->next = NULL;
	else FirstNode = NULL;
}

template <class unit>
void c_Queue<unit>::PopFromFront()
{
	s_QueueNode<unit> *tempnode;
	if(!FirstNode) return;

	DQDelete(FirstNode->current);
	tempnode = FirstNode->next;
	DQDelete(FirstNode);
	FirstNode = tempnode;
	if(FirstNode)
		FirstNode->prev = NULL;
	else LastNode = NULL;
}

template <class unit>
c_Queue<unit>::c_Queue() {
	FirstNode = NULL;
	LastNode  = NULL;
}

template <class unit>
c_Queue<unit>::~c_Queue<unit>() 
{
	DestroyQueue();
}

//node contents ARE deleted
template <class unit>
void c_Queue<unit>::DestroyQueue()
{
	s_QueueNode<unit> *node,*nextnode;

	node = FirstNode;
	while(node) {
		nextnode = node->next;
		DQDelete (node->current);
		DQDelete (node);
		node = nextnode;
	};
}

template <class unit>
void c_Queue<unit>::RemoveNode(unit *RemoveThis)
{
	s_QueueNode<unit> *node;

	node = FirstNode;
	while(node) {
		if(node->current != RemoveThis) {
			node = node->next;
			continue;
		}
		//else node->current == RemoveThis
		if(node == FirstNode) {
			FirstNode = node->next;
		}
		if(node == LastNode) {
			LastNode = node->prev;
		}
		if(node->next) node->next->prev = node->prev;
		if(node->prev) node->prev->next = node->next;
		DQDelete(node->current);
		DQDelete(node);
		return;
	};
	Error(ERR_MEM, "Could not find node to remove");
}

template <class unit>
unit* c_Queue<unit>::PushToFront(unit* AddThis)
{
	s_QueueNode<unit> *NewNode;
	unit *NewUnit;
	
	if(!AddThis) NewUnit = DQNew( unit );
	else NewUnit = AddThis;

	NewNode = DQNew( s_QueueNode<unit> );
	
	NewNode->current = NewUnit;
	NewNode->next = NewNode->prev = NULL;

	if(!FirstNode) {
		FirstNode = LastNode = NewNode;
	}
	else {
		FirstNode->prev = NewNode;
		NewNode->next = FirstNode;
		FirstNode = NewNode;
	}

	return NewUnit;
}
*/

//***********************************

#endif //__C_QUEUE