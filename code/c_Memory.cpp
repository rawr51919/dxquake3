// DXQuake3 Source
// Copyright (c) 2003 Richard Geary (richard.geary@dial.pipex.com)
//
// c_Memory.cpp: implementation of the c_Memory class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"

//Create global var
c_Memory DQMemory;

c_Memory::c_Memory()
{
	FirstNode = LastNode = NULL;
	NumAllocated = 0;
	SizeAllocated = 0;
	MaxSizeAllocated = 0;
	MaxNumAllocated = 0;
	NumDQNewCalls = 0;
}

c_Memory::~c_Memory()
{
#if _DEBUG
	FILE *file;
	char Text[1001];
	int len;
	s_MemoryContainer *MemPtr = FirstNode;

	file = fopen("Memory Info.txt", "w+");
	if(!file) {
 		file = fopen("C:\\Memory Info.txt", "w+");
		if(!file) {
			Error(ERR_FS, "Unable to open \"Memory Info.txt\" for writing");
			return;
		}
	}

	Com_sprintf(Text, 1000, "Max Num Allocated : %d\nMax Allocated Size : %d\nMemory Leaks :\n",MaxNumAllocated,MaxSizeAllocated);
	fwrite(Text, DQstrlen(Text, 1000), 1, file);

	if(NumAllocated==0) { fclose(file); return; }
	
	sprintf(Text, "Memory Still Allocated : %d items",NumAllocated);
	MessageBox(NULL, Text, "Memory Leak", MB_OK | MB_ICONHAND | MB_TASKMODAL | MB_TOPMOST);
	
	while(MemPtr) {
		if(MemPtr->MagicNumber!=0x33515844) Error(ERR_MEM, "Not a DQMemory item!");
		Com_sprintf(Text, 1000, "%s,Line %d,%s\n",
			MemPtr->FileName,
			MemPtr->Line,
			MemPtr->TypeName);
		len = DQstrlen(Text, 1000);
		fwrite(Text, len, 1, file);
		--NumAllocated;
		MemPtr = MemPtr->next;
	}
	if(NumAllocated>0) {
		Com_sprintf(Text, 1000, "NumAllocated counts %d unaccounted items", NumAllocated);
		fwrite(Text, DQstrlen(Text, 1000), 1, file);
	}
	fclose(file);
#endif
}

//Takes a pointer to a new item, calculates the container pointer, registers it, and returns the pointer to the new item
void * c_Memory::Register( void *ItemPtr, int Line, char *File, char *TypeName )
{
	char Filename[MAX_OSPATH];
	s_MemoryContainer *ContainerPtr = (s_MemoryContainer *)((char*)ItemPtr-sizeof(s_MemoryContainer));
	ContainerPtr->MagicNumber = 0x33515844;
	ContainerPtr->next = NULL;
	ContainerPtr->prev = LastNode;
	if(LastNode) LastNode->next = ContainerPtr;
	LastNode = ContainerPtr;
	if(!FirstNode) {
		FirstNode = LastNode = ContainerPtr;
	}
	ContainerPtr->Line = Line;
	DQstrcpy(Filename, File, MAX_OSPATH);
	DQStripPath(Filename, MAX_OSPATH);
	DQstrcpy(ContainerPtr->FileName, Filename, 20);
	DQstrcpy(ContainerPtr->TypeName, TypeName, 50);

	++NumDQNewCalls;

	return ItemPtr;
}

void c_Memory::UnRegister( void *ItemPtr )
{
	if( !ItemPtr ) return;
	s_MemoryContainer *ContainerPtr = (s_MemoryContainer *)((char*)ItemPtr-sizeof(s_MemoryContainer));
	if(ContainerPtr->MagicNumber != 0x33515844) {
		Error(ERR_MEM, "DQDelete used on pointer to a non DQNew item");
		return;
	}
	ContainerPtr->MagicNumber = 0x12fe34dc;	//Set it back to the regular value
	if(ContainerPtr == FirstNode) {
		FirstNode = ContainerPtr->next;
	}
	if(ContainerPtr == LastNode) {
		LastNode = ContainerPtr->prev;
	}
	if(ContainerPtr->next) ContainerPtr->next->prev = ContainerPtr->prev;
	if(ContainerPtr->prev) ContainerPtr->prev->next = ContainerPtr->next;
}

int c_Memory::GetRemaining()
{
	MEMORYSTATUS mem;
	GlobalMemoryStatus(&mem);
	return mem.dwAvailVirtual;
}

BOOL c_Memory::ValidateMemory()
{
	s_MemoryContainer *m;

	for(m=FirstNode; m!=LastNode; m=m->next) {
		if(m->MagicNumber != 0x33515844) {
			Error(ERR_MEM, va("ValidateMemory : Heap is corrupted - item %s Line %d",m->TypeName,m->Line) );
			m = m->prev;
			if(m) {
				DQPrint( va("after %s Line %d",m->TypeName,m->Line ) );
				m = m->prev;
				if(m) {
					DQPrint( va("after %s Line %d",m->TypeName,m->Line ) );
					m = m->prev;
					if(m) {
						DQPrint( va("after %s Line %d",m->TypeName,m->Line ) );
					}
				}
			}
			return FALSE;
		}
	}

	return TRUE;
}

#if _DEBUG

inline void* operator new( size_t size ) {
	void *ptr = malloc(size+sizeof(s_MemoryContainer));
	if(!ptr) CriticalError(ERR_MEM, "Unable to allocate Memory");

	s_MemoryContainer *ContainerPtr = (s_MemoryContainer*)ptr;

	ContainerPtr->MagicNumber = 0x12fe34dc;
	ContainerPtr->Size = size;
	DQstrcpy(ContainerPtr->TypeName, "DQMemory Not Used", 50);
	ptr = (void*)( ((char*)ContainerPtr) + sizeof(s_MemoryContainer) );

	++DQMemory.NumAllocated;
	DQMemory.MaxNumAllocated = (DQMemory.NumAllocated>DQMemory.MaxNumAllocated)?DQMemory.NumAllocated:DQMemory.MaxNumAllocated;
	DQMemory.SizeAllocated += size;
	DQMemory.MaxSizeAllocated = (DQMemory.SizeAllocated>DQMemory.MaxSizeAllocated)?DQMemory.SizeAllocated:DQMemory.MaxSizeAllocated;

	return ptr;
}

inline void operator delete( void* ptr )
{
	if(!ptr) return;
	if(DQMemory.NumAllocated==0) Error(ERR_MEM, "Unable to delete memory");

	s_MemoryContainer *ContainerPtr;

	ContainerPtr = (s_MemoryContainer*)((char*)ptr-sizeof(s_MemoryContainer));

	if(ContainerPtr->MagicNumber == 0x33515844) {
		Error(ERR_MEM, "Use DQDelete for items created with DQNew");
	}
	else if(ContainerPtr->MagicNumber != 0x12fe34dc) {
		Error(ERR_MEM, "Memory Corrupted!");
		return;
	}

	DQMemory.SizeAllocated -= ContainerPtr->Size;

	free(ContainerPtr);
	
	--DQMemory.NumAllocated;
}
#endif