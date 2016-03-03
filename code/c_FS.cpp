// DXQuake3 Source
// Copyright (c) 2003 Richard Geary (richard.geary@dial.pipex.com)
//
// c_FS.cpp: implementation of the c_FS class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

c_FS::c_FS()
{
	NumFileHandles = 0;

//#ifndef _DEBUG
#if 1
	int len;

	GetCurrentDirectory( MAX_OSPATH, Q3Root );
	len = DQstrlen( Q3Root, MAX_OSPATH );
	if( len==0 ) CriticalError( ERR_FS, "Unable to find Quake 3 Root path" );
	
	//Remove ending \ if present
	if( Q3Root[len-1]=='\\' ) {
		Q3Root[len-1]='\0';
	}
#else
	//Hack to get DXQ3 to work from Dev path
	DQstrcpy(Q3Root, "H:\\Microsoft Visual Studio", MAX_OSPATH);
//	DQstrcpy(Q3Root, "F:\\Q3ADemo", MAX_OSPATH);
//	DQstrcpy(Q3Root, "F:\\Quake III Arena", MAX_OSPATH);
	DQstrcpy((char*)SubDirChain.AddUnit(), "mysource\\DXQuake3", MAX_QPATH);
//	DQstrcpy((char*)SubDirChain.AddUnit(), "q3 data", MAX_QPATH);
#endif

	DQstrcpy(SubDirChain.AddUnit()->Path, "baseq3", MAX_QPATH);
	DQstrcpy(SubDirChain.AddUnit()->Path, "demoq3", MAX_QPATH);
	DQstrcpy(SubDirChain.AddUnit()->Path, ".", MAX_QPATH);
}
//TODO: Alternate constructor, taking fs_game for copy to SubDir

c_FS::~c_FS()
{
	for(int i=0;i<NumFileHandles;++i) {
		pFileHandleArray[i].CloseFile();
	}
}

void c_FS::FindPK3s()
{
	c_PK3 *PK3;
	const int bufsize = 10000;
	int i, pos, number;
	char buffer[bufsize];
	char PAK0Path[MAX_OSPATH];

	//update SubDirChain with fs_game
	DQConsole.GetCVarString( "fs_game", buffer, bufsize, eAuthClient );
	if( buffer[0] ) {
		DQCopyUntil(SubDirChain.AddUnitToFront()->Path, buffer, ' ', MAX_QPATH);
	}

	PK3Chain.DestroyChain();

	//Add PK0 first
	FindFile( "pak0.pk3", PAK0Path );
	if(!PAK0Path[0]) {
		CriticalError( ERR_FS, "Unable to find pak0.pk3" );
	}

	//Search for file
	number = GetFileList( "", "pk3", buffer, bufsize, TRUE );
	pos = 0;
	for( i=0; i<number; ++i ) {
		if( DQstrcmpi( PAK0Path, &buffer[pos], MAX_OSPATH)==0 ) {
			PK3 = PK3Chain.AddUnitToFront();
		}
		else {
			PK3 = PK3Chain.AddUnit();
		}
		pos += DQstrcpy( PK3->FullPath, &buffer[pos], MAX_OSPATH ) + 1;
		DQPrint( va("c_FS::AddPK3 : %s",PK3->FullPath) );
		
		if( !PK3->OpenPK3() ) {
			//corrupt PK3 - remove from chain
			PK3Chain.RemoveNode( PK3 );
		}

	}
}

//Searches for and opens Filename
//Returns NULL if failed
FILEHANDLE c_FS::OpenFile(const char *Filename, const char *Mode)
{
	char Path[MAX_OSPATH];
	s_ChainNode<c_PK3> *PK3Node;
	s_File *File;

	if(NumFileHandles>=MAX_FILEHANDLE) {
		CriticalError(ERR_FS, "Too many open files");
	}

	FindFile(Filename, Path);
	if(Mode[0] == 'w') {
		BOOL ret;
		HANDLE x;
		WIN32_FIND_DATA DirectorySearch;
		char tempFilename[MAX_QPATH];
		int len;

		//Make sure directory exists
		//Root+SubDir
		DQstrcpy( Path, va("%s\\%s",Q3Root,SubDirChain.GetFirstNode()->current->Path), MAX_OSPATH);
		x = FindFirstFile( Path, &DirectorySearch );
		FindClose(x);
		if(x==INVALID_HANDLE_VALUE) {
			ret = CreateDirectory( Path, NULL );
			if(!ret) {
#if _DEBUG
				Error( ERR_FS, va("Could not create directory %s",Path));
#else
				DQPrint( va("Could not create directory %s",Path) );		//no need to give an error in release version
#endif
				return 0;
			}
		}

		//Root+SubDir+Q3Path
		DQstrcpy( tempFilename, Filename, MAX_QPATH );
		DQStripFilename( tempFilename, MAX_QPATH );
		if( tempFilename[0] ) {
			DQstrcpy( Path, va("%s\\%s",Path,tempFilename), MAX_OSPATH );

			len = DQstrlen(Path,MAX_QPATH);
			if(Path[len-1]=='\\' || Path[len-1]=='/') Path[len-1]='\0';

			x = FindFirstFile( Path, &DirectorySearch );
			FindClose(x);
			if(x==INVALID_HANDLE_VALUE) {
				ret = CreateDirectory( Path, NULL );
				if(!ret) {
#if _DEBUG
					Error( ERR_FS, va("Could not create directory %s",Path));
#else
					DQPrint( va("Could not create directory %s",Path) );		//no need to give an error in release version
#endif
					return 0;
				}
			}
		}

		//Now open (or create) the file
		DQstrcpy( Path, va("%s\\%s\\%s",Q3Root,SubDirChain.GetFirstNode()->current->Path, Filename), MAX_OSPATH);
	}

	if(Path[0]) {
		return OpenFileWithFullPath(Path, Mode);
	}

	//Look in PK3 files
	PK3Node = PK3Chain.GetFirstNode();
	while(PK3Node) {
		if( PK3Node->current->FindFile( Filename ) ) {
			File = &pFileHandleArray[NumFileHandles];
			++NumFileHandles;

			File->Filesize = PK3Node->current->OpenCurrentFile();
			
			File->Data = (BYTE*)DQNewVoid( BYTE[File->Filesize] );
			PK3Node->current->ReadCurrentFile( File->Data, File->Filesize );
			
			PK3Node->current->CloseCurrentFile();
			return NumFileHandles;
		}

		PK3Node = PK3Node->next;
	}

#if _DEBUG
	DQPrint( va("c_FS::OpenFile - Could not find file %s\n",Filename));
#endif

	return NULL;
}

//Opens FullPath file
FILEHANDLE c_FS::OpenFileWithFullPath(const char *FullFilename, const char *Mode)
{
	FILE *filehandle = NULL;

	Assert(FullFilename[0]!='\0');

	filehandle = fopen(FullFilename, Mode);

	if(!filehandle) {
		return 0;
	}

	pFileHandleArray[NumFileHandles].File = filehandle;
	++NumFileHandles;

	return NumFileHandles;
}

//Returns the full path of the file if found. FullPath must be a pointer to a char[MAX_OSPATH] array
void c_FS::FindFile(const char *Filename, char *FullPath) {
	FILE *filehandle = NULL;
	s_ChainNode<s_SubDir> *SubDirNode = SubDirChain.GetFirstNode();

	if(!Filename || !FullPath) return;

	//Search for file
	while(!filehandle && SubDirNode) {
		Com_sprintf(FullPath, MAX_OSPATH, "%s\\%s\\%s",Q3Root,SubDirNode->current->Path, Filename);
		filehandle = fopen( FullPath, "rb" );
		SubDirNode = SubDirNode->next;
	}
	if(!filehandle) {
		FullPath[0] = '\0';
		return;
	}
	//else
	fclose(filehandle);
	return;
}


void c_FS::CloseFile(FILEHANDLE Handle)
{
	if(Handle<1 || Handle>NumFileHandles) {
		Error(ERR_FS, va("FS::CloseFile - Invalid Handle %d. Max Handle %d",Handle, NumFileHandles));
		return;
	}

	pFileHandleArray[Handle-1].CloseFile();

	if(Handle == NumFileHandles && NumFileHandles>0) --NumFileHandles;
	return;
}

void c_FS::ReadFile(void *Buffer, int size, int number, FILEHANDLE Handle)
{
	int CurrentPos;

	if(number<=0) return;
	if(Handle<1 || Handle>NumFileHandles) {
		Error(ERR_FS, va("c_FS::ReadFile - Invalid Handle %d. Max Handle %d",Handle, NumFileHandles));
		return;
	}
	if(!Buffer || size<1) {
		Error(ERR_FS, "c_FS::ReadFile - Invalid Buffer");
		return;
	}

	if(pFileHandleArray[Handle-1].File) {
		if(fread(Buffer, size, number, pFileHandleArray[Handle-1].File) == 0) {
			Error(ERR_FS, "c_FS::ReadFile - Read zero bytes");
		}
		return;
	}
	
	if(pFileHandleArray[Handle-1].Data) {
		size *= number;
		CurrentPos = pFileHandleArray[Handle-1].CurrentPos;
		size = min( size, pFileHandleArray[Handle-1].Filesize - CurrentPos );
		memcpy( Buffer, &pFileHandleArray[Handle-1].Data[CurrentPos], size );

		pFileHandleArray[Handle-1].CurrentPos += size;
		return;
	}

	Error(ERR_FS, va("FS::ReadFile - Invalid Handle %d. Max Handle %d",Handle, NumFileHandles));
}

void c_FS::WriteFile(void *Buffer, size_t size, int number, FILEHANDLE Handle)
{
	if(number<=0) return;
	if(Handle<1 || Handle>NumFileHandles || pFileHandleArray[Handle-1].File==NULL) {
		Error(ERR_FS, va("FS::WriteFile - Invalid Handle %d. Max Handle %d",Handle, NumFileHandles));
		return;
	}
	if(!Buffer) {
		Error(ERR_FS, "c_FS::WriteFile : Invalid Buffer");
		return;
	}
	
	if(fwrite(Buffer, size, number, pFileHandleArray[Handle-1].File) == 0) {
		Error(ERR_FS, "c_FS::WriteFile - Wrote zero bytes");
	}
}

void c_FS::Seek(long offset, int origin, FILEHANDLE Handle)
{
	int fseekOrigin;

	if(Handle<1 || Handle>NumFileHandles) {
		Error(ERR_FS, va("FS::Seek - Invalid Handle %d. Max Handle %d",Handle, NumFileHandles));
		return;
	}

	if( pFileHandleArray[Handle-1].File ) {
		if(origin==(int)FS_SEEK_CUR) fseekOrigin = SEEK_CUR;
		else if(origin==(int)FS_SEEK_END) fseekOrigin = SEEK_END;
		else if(origin==(int)FS_SEEK_SET) fseekOrigin = SEEK_SET;
		else return;
		
		fseek(pFileHandleArray[Handle-1].File, offset, fseekOrigin);
	}
	else if( pFileHandleArray[Handle-1].Data ) {
		switch(origin) {
		case FS_SEEK_CUR: pFileHandleArray[Handle-1].CurrentPos += offset; break;
		case FS_SEEK_END: pFileHandleArray[Handle-1].CurrentPos = pFileHandleArray[Handle-1].Filesize - offset; break;
		case FS_SEEK_SET: pFileHandleArray[Handle-1].CurrentPos = offset; break;
		};
		pFileHandleArray[Handle-1].CurrentPos = min( pFileHandleArray[Handle-1].CurrentPos, pFileHandleArray[Handle-1].Filesize );
	}
}

int c_FS::GetFileLength(FILEHANDLE Handle)
{
	int currentPos,lastPos;

	if(Handle<1 || Handle>NumFileHandles) {
		Error(ERR_FS, va("FS::GetFileLength - Invalid Handle %d. Max Handle %d",Handle, NumFileHandles));
		return 0;
	}

	if( pFileHandleArray[Handle-1].File ) {
		currentPos = ftell(pFileHandleArray[Handle-1].File);
		fseek(pFileHandleArray[Handle-1].File, 0, SEEK_END);
		lastPos = ftell(pFileHandleArray[Handle-1].File);
		fseek(pFileHandleArray[Handle-1].File, currentPos, SEEK_SET);
		return lastPos;
	}
	if( pFileHandleArray[Handle-1].Data ) {
		return pFileHandleArray[Handle-1].Filesize;
	}

	return 0;
}

int c_FS::GetPosition(FILEHANDLE Handle)
{
	if(Handle<1 || Handle>NumFileHandles) {
		Error(ERR_FS, va("FS::GetPosition - Invalid Handle %d. Max Handle %d",Handle, NumFileHandles));
		return 0;
	}

	if( pFileHandleArray[Handle-1].File ) {
		return ftell(pFileHandleArray[Handle-1].File);
	}
	if( pFileHandleArray[Handle-1].Data ) {
		return pFileHandleArray[Handle-1].CurrentPos;
	}

	return 0;
}

//		This fills buffer with a NULL delimited list of all the files in the directory specified 
//		by path ending in extension. The number of files listed is returned.

// eg. path = "models/crash"
//     extention = "md3"
int c_FS::GetFileList(char *path, char *extention, char *buffer, int bufsize, BOOL bReturnFullPath)
{
	HANDLE Handle;
	int number, sizeleft,len;
	WIN32_FIND_DATA FindFile;
	char PathAndExtention[MAX_PATH];
	char FullPath[MAX_OSPATH];
	s_ChainNode<s_SubDir> *SubDirNode;
	s_ChainNode<c_PK3> *PK3Node;

	char *temp;

	Assert( path && extention && buffer && bufsize>0 );

	//Strip unrequired prefixes
	char Extention2[MAX_QPATH];
	if( extention[0] == '\\' || extention[0] == '/' || extention[0]=='.' ) {
		DQstrcpy( Extention2, &extention[1], MAX_QPATH);
	}
	else {
		DQstrcpy( Extention2, extention, MAX_QPATH);
	}

	//Concatenate path and extention
	DQstrcpy(PathAndExtention, path, MAX_PATH);
	DQStripFilename(PathAndExtention, MAX_PATH);

	if( PathAndExtention[0]!='\0' ) {
		DQstrcat(PathAndExtention, "\\*.", MAX_PATH);
	}
	else {
		DQstrcat(PathAndExtention, "*.", MAX_PATH);
	}

	if( Extention2[0] ) {
		DQstrcat(PathAndExtention, Extention2, MAX_PATH);
	}
	else {
		DQstrcat(PathAndExtention, "*", MAX_PATH);
	}

	len = DQstrlen(PathAndExtention, MAX_PATH);
	
	number = 0;
	sizeleft = bufsize;

	SubDirNode = SubDirChain.GetFirstNode();
	while(SubDirNode) {
		Com_sprintf(FullPath, MAX_OSPATH, "%s\\%s\\%s",Q3Root,SubDirNode->current->Path,PathAndExtention);
		Handle = NULL;
		Handle = FindFirstFile(FullPath, &FindFile);
		if(Handle == INVALID_HANDLE_VALUE) {
			SubDirNode = SubDirNode->next;
			continue;
		}
		do {
			if(bReturnFullPath) {
				temp = va("%s\\%s\\%s",Q3Root,SubDirNode->current->Path,FindFile.cFileName);
				DQstrcpy(buffer, temp, sizeleft);
				len = DQstrlen(temp, MAX_OSPATH)+1;
			}
			else {
				DQstrcpy(buffer, FindFile.cFileName, sizeleft);
				len = DQstrlen(FindFile.cFileName, MAX_PATH)+1;
			}
			buffer += len;
			sizeleft -= len;
			++number;
			if(sizeleft<=0) return number;
		}while(FindNextFile(Handle, &FindFile));
		FindClose(Handle);
		
		SubDirNode = SubDirNode->next;
	}

	for( PK3Node = PK3Chain.GetFirstNode(); PK3Node; PK3Node = PK3Node->next )
	{
		number += PK3Node->current->GetFileList( path, Extention2, &buffer, sizeleft );
	}

	return number;
}

void *c_FS::GetPointerToData(int Handle)
{
	s_File *File;

	if(Handle<1 || Handle>NumFileHandles) {
		Error(ERR_FS, va("FS::GetPointerToData - Invalid Handle %d. Max Handle %d",Handle, NumFileHandles));
		return 0;
	}

	if( pFileHandleArray[Handle-1].File ) {
		//Read and Copy to Data member
		File = &pFileHandleArray[Handle-1];

		File->Filesize = GetFileLength( Handle );
		File->Data = (BYTE*)DQNewVoid( BYTE[File->Filesize] );
		ReadFile( File->Data, 1, File->Filesize, Handle );
		
		fclose( File->File );
		File->File = NULL;
		File->CurrentPos = 0;
	}
	else if( !pFileHandleArray[Handle-1].Data ) {
		CriticalError(ERR_FS, va("FS::GetPointerToData - Invalid Handle %d. Max Handle %d",Handle, NumFileHandles));
	}

	return pFileHandleArray[Handle-1].Data;
}

void c_FS::GetDLLPath(const char *DLLFilename, char *FullPath)
{
	FindFile( DLLFilename, FullPath );
}
