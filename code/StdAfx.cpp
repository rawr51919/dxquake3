// DXQuake3 Source
// Copyright (c) 2003 Richard Geary (richard.geary@dial.pipex.com)
//

#include "stdafx.h"

HRESULT DXError_hResult;

#ifdef _DEBUG
	int d3dcheckNum=0;
	char d3dcheckList[6000][200];
#endif

void WindowsErrorMessage(e_ErrorType ErrorType, const char *ErrorMessage, int Line, char *File)
{
	char text[1000],errtype[100];

	switch (ErrorType) {
	case ERR_ASSERT:
		sprintf_s(errtype, "Assert Error"); break;
	case ERR_MEM:
		sprintf_s(errtype, "Memory Error"); break;
	default:
		errtype[0] = '\0';
	};

	sprintf_s(text, "Error : %s\n\n%s\n\nline : %d\nfile : %s",errtype,ErrorMessage,Line,File);
	MessageBox(NULL, text, "DQ Error", MB_OK | MB_SYSTEMMODAL);
	return;
}

/*
inline void* operator new( size_t size ) 
{
	void *ptr = malloc(size);
	if(!ptr) CriticalError(ERR_MEM, "Unable to allocate Memory");

	++NumAllocated;
	return ptr;
}
inline void operator delete( void* ptr )
{
	if(!ptr) return;
	if(NumAllocated==0) Error(ERR_MEM, "Unable to delete memory");
	free(ptr);
	
	--NumAllocated;
}

void __cdecl OnExit(void) {
	if(DQMemory.NumAllocated!=0) {
		char Text[1000];
		sprintf(Text, "Memory Still Allocated : %d items",DQMemory.NumAllocated);
		MessageBox(NULL, Text, "Memory Leak", MB_OK | MB_ICONHAND | MB_TASKMODAL | MB_TOPMOST);
	}
}
*/