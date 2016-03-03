// DXQuake3 Source
// Copyright (c) 2003 Richard Geary (richard.geary@dial.pipex.com)
//
//
//	c_Chain definition
//

#ifndef __C_CHAIN
#define __C_CHAIN

template <class unit>
struct s_ChainNode {
	s_ChainNode<unit> *next;
	s_ChainNode<unit> *prev;
	unit	*current;
};

template <class unit>
class c_Chain {
public:
	c_Chain();
	virtual ~c_Chain();
	unit* AddUnit(unit* AddThis = NULL); //If AddThis == NULL, create new unit
	unit* AddUnitToFront(unit* AddThis = NULL); //If AddThis == NULL, create new unit
	s_ChainNode<unit> *GetFirstNode() { return FirstNode; }
	s_ChainNode<unit> *GetLastNode() { return LastNode; }
	virtual void DestroyChain();
	virtual void RemoveNode(unit *RemoveThis);
	virtual void RemoveNode2(s_ChainNode<unit> *RemoveThisNode);
	virtual void RemoveLastNode();
	int GetQuantity();

protected:
	s_ChainNode<unit> *FirstNode,*LastNode;
};

//Implementation

template <class unit>
unit *c_Chain<unit>::AddUnit(unit* AddThis) {
	s_ChainNode<unit> *NewNode;
	unit *NewUnit;
	
	if(!AddThis) NewUnit = DQNew(unit);
	else NewUnit = AddThis;

	NewNode = DQNew(s_ChainNode<unit>);
	
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
unit *c_Chain<unit>::AddUnitToFront(unit* AddThis) {
	s_ChainNode<unit> *NewNode;
	unit *NewUnit;
	
	if(!AddThis) NewUnit = DQNew(unit);
	else NewUnit = AddThis;

	NewNode = DQNew(s_ChainNode<unit>);
	
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

template <class unit>
c_Chain<unit>::c_Chain() {
	FirstNode=NULL;
	LastNode = NULL;
}

template <class unit>
c_Chain<unit>::~c_Chain<unit>() 
{
	DestroyChain();
}

//node contents ARE deleted
template <class unit>
void c_Chain<unit>::DestroyChain()
{
	s_ChainNode<unit> *node,*nextnode;

	node = FirstNode;
	while(node) {
		nextnode = node->next;
		DQDelete(node->current);
		DQDelete(node);
		node = nextnode;
	};
	FirstNode = LastNode = NULL;
}

template <class unit>
void c_Chain<unit>::RemoveNode(unit *RemoveThis)
{
	s_ChainNode<unit> *node;

	node = FirstNode;
	while(node) {
		if(node->current != RemoveThis) {
			node = node->next;
			continue;
		}
		//else node->current == RemoveThis
		RemoveNode2(node);
		return;
	};
	Error(ERR_MEM, "Could not find node to remove");
}

template <class unit>
void c_Chain<unit>::RemoveNode2(s_ChainNode<unit> *RemoveThisNode)
{
	if(RemoveThisNode == FirstNode) {
		FirstNode = RemoveThisNode->next;
	}
	if(RemoveThisNode == LastNode) {
		LastNode = RemoveThisNode->prev;
	}
	if(RemoveThisNode->next) RemoveThisNode->next->prev = RemoveThisNode->prev;
	if(RemoveThisNode->prev) RemoveThisNode->prev->next = RemoveThisNode->next;
	DQDelete(RemoveThisNode->current);
	DQDelete(RemoveThisNode);
}

template <class unit>
void c_Chain<unit>::RemoveLastNode()
{
	if(LastNode) RemoveNode2(LastNode);
}

template <class unit>
int c_Chain<unit>::GetQuantity() {
	s_ChainNode<unit> *node;
	int num;

	num = 0;
	node = FirstNode;
	while(node) {
		++num;
		node = node->next;
	};
	return num;	
}		

//***********************************

//c_ChainNoDestroy does not delete the contents of the chain on exit, only the chain itself
template <class unit>
class c_ChainNoDestroy : public c_Chain<unit>
{
public:
	c_ChainNoDestroy():c_Chain<unit>() {}
	virtual ~c_ChainNoDestroy() {}
	virtual void DestroyChain();
	virtual void RemoveNode(unit *RemoveThis);
};


//node contents NOT deleted.
template <class unit>
void c_ChainNoDestroy<unit>::DestroyChain()
{
	s_ChainNode<unit> *node,*nextnode;

	node = FirstNode;
	while(node) {
		nextnode = node->next;
		DQDelete(node);
		node = nextnode;
	};
	FirstNode = LastNode = NULL;
}

template <class unit>
void c_ChainNoDestroy<unit>::RemoveNode(unit *RemoveThis)
{
	s_ChainNode<unit> *node;

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
		DQDelete(node);
		return;
	};
	Error(ERR_MEM, "Could not find node to remove");
}

//*****************************************************

//Custom c_Chain for c_Memory
/*
template <class unit>
class c_ChainUseMalloc {
public:
	c_ChainUseMalloc();
	virtual ~c_ChainUseMalloc();
	s_ChainNode<unit> *AddUnitGetNode(unit* AddThis = NULL); //Same as AddUnit, but returns the Node which the unit is attached to
	s_ChainNode<unit> *GetFirstNode() { return FirstNode; }
	virtual void DestroyChain();
	virtual void NodeRemoveNode(s_ChainNode<unit> *RemoveNode);

protected:
	s_ChainNode<unit> *FirstNode,*LastNode;
};

//Almost the same as AddUnit
template <class unit>
s_ChainNode<unit> *c_ChainUseMalloc<unit>::AddUnitGetNode(unit* AddThis)
{
	s_ChainNode<unit> *NewNode;
	unit *NewUnit;
	
	if(!AddThis) NewUnit = (unit*)malloc(sizeof(unit));
	else NewUnit = AddThis;

	NewNode = (s_ChainNode<unit> *)malloc(sizeof(s_ChainNode<unit>));
	
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

	return NewNode;
}

template <class unit>
c_ChainUseMalloc<unit>::c_ChainUseMalloc() {
	FirstNode = NULL;
	LastNode  = NULL;
}

template <class unit>
c_ChainUseMalloc<unit>::~c_ChainUseMalloc<unit>() 
{
	DestroyChain();
}

//node contents ARE deleted
template <class unit>
void c_ChainUseMalloc<unit>::DestroyChain()
{
	s_ChainNode<unit> *node,*nextnode;

	node = FirstNode;
	while(node) {
		nextnode = node->next;
		free(node->current);
		free(node);
		node = nextnode;
	};
}


template <class unit>
void c_ChainUseMalloc<unit>::NodeRemoveNode(s_ChainNode<unit> *RemoveNode)
{
	if(RemoveNode == FirstNode) {
		FirstNode = RemoveNode->next;
	}
	if(RemoveNode == LastNode) {
		LastNode = RemoveNode->prev;
	}
	if(RemoveNode->next) RemoveNode->next->prev = RemoveNode->prev;
	if(RemoveNode->prev) RemoveNode->prev->next = RemoveNode->next;
	free(RemoveNode->current);
	free(RemoveNode);
	return;
}
*/
#endif //__C_CHAIN