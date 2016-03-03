// DXQuake3 Source
// Copyright (c) 2003 Richard Geary (richard.geary@dial.pipex.com)
//
// c_Memory.h: interface for the c_Memory class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_C_MEMORY_H__F828B648_5F3D_4420_B3FB_7729E6A97C49__INCLUDED_)
#define AFX_C_MEMORY_H__F828B648_5F3D_4420_B3FB_7729E6A97C49__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

struct s_MemoryContainer {
	int MagicNumber;
	s_MemoryContainer *next,*prev;
	char TypeName[50];
	char FileName[20];
	int Size;
	int Line;
};

//Has to be a global instance for the global operator-new override to work
class c_Memory
{
public:
	c_Memory();
	virtual ~c_Memory();
	void * Register( void *ItemPtr, int Line, char *File, char *TypeName );	//Takes a pointer to a new container, registers it, and returns the pointer to the new object
	void UnRegister( void *ContainerPtr );
	int  GetRemaining();
	BOOL ValidateMemory();


	int NumAllocated, MaxNumAllocated, SizeAllocated, MaxSizeAllocated;
	int NumDQNewCalls;
private:
	s_MemoryContainer *FirstNode, *LastNode;	//Don't use c_Chain as it is inefficient
};

//Global Variable
extern c_Memory DQMemory;

//WARNING : DQNew screws up if you create an array of classes, ie. DQNewVoid( c_MyClass[num] )
#if _DEBUG
	#define DQNew(type) (type*)DQMemory.Register(new type, __LINE__, __FILE__, #type )
	#define DQNewVoid(type) DQMemory.Register(new type, __LINE__, __FILE__, #type )
	#define DQDelete( ptr ) { DQMemory.UnRegister( ptr ); delete ptr; ptr = NULL; }
	#define DQDeleteArray( ptr ) { DQMemory.UnRegister( ptr ); delete[] ptr; ptr = NULL; }
#else
	#define DQNew(type) new type
	#define DQNewVoid(type) new type
	#define DQDelete(ptr) { delete ptr; ptr = NULL; }
	#define DQDeleteArray(ptr) { delete[] ptr; ptr = NULL; }
#endif

#endif // !defined(AFX_C_MEMORY_H__F828B648_5F3D_4420_B3FB_7729E6A97C49__INCLUDED_)
