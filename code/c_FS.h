// DXQuake3 Source
// Copyright (c) 2003 Richard Geary (richard.geary@dial.pipex.com)
//
// c_FS.h: interface for the c_FS class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_C_FS_H__1F113785_1A1A_435C_A442_637554ED55F1__INCLUDED_)
#define AFX_C_FS_H__1F113785_1A1A_435C_A442_637554ED55F1__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

typedef int FILEHANDLE;
#define MAX_FILEHANDLE	10000

struct s_SubDir {
	char Path[MAX_QPATH];
};

class c_PK3 {
private:
	struct s_path {
		char Filename[MAX_QPATH];
		int pos_in_central_dir;
	};
public:
	c_PK3() { ZipFile = NULL; NumFiles = 0; FileList = NULL; }
	
	inline BOOL OpenPK3() { 
		unz_global_info GlobalInfo;
		unz_file_info FileInfo;
		int i,err;

		ZipFile = unzOpen( FullPath );

		//Get FileList
		err = unzGetGlobalInfo( ZipFile, &GlobalInfo );
		if( err != UNZ_OK) {
			Error( ERR_FS, va( "Corrupt PK3 : %s",FullPath ) ); return FALSE;
		}

		NumFiles = GlobalInfo.number_entry;
		DQDeleteArray( FileList );
		FileList = (s_path*)DQNewVoid( s_path[NumFiles] );

		for( i=0; i<GlobalInfo.number_entry; ++i ) {
			err = unzGetCurrentFileInfo( ZipFile, &FileInfo, FileList[i].Filename, MAX_QPATH, NULL, 0, NULL, 0 );
			FileList[i].pos_in_central_dir = unzGetPosInCentralDir( ZipFile );

			if( err != UNZ_OK) {
				Error( ERR_FS, va( "Corrupt PK3 : %s",FullPath ) ); return FALSE;
			}
			unzGoToNextFile( ZipFile );
			if( err != UNZ_OK) {
				Error( ERR_FS, va( "Corrupt PK3 : %s",FullPath ) ); return FALSE;
			}
		}

		return TRUE;

	}

	~c_PK3() {
		unzCloseCurrentFile( ZipFile );
		DQDeleteArray( FileList );
	}

	//Returns TRUE if file exists in PK3
	inline BOOL FindFile( const char *Filename )
	{
		int i;
		for( i=0; i<NumFiles; ++i ) {
			if( DQFilenamestrcmpi( Filename, FileList[i].Filename, MAX_QPATH )==0 ) {
				unzGoToFileNum( ZipFile, i, FileList[i].pos_in_central_dir);
				return TRUE;
			}
		}
		return FALSE;
	}

	inline unsigned int OpenCurrentFile()
	{
		unz_file_info FileInfo;

		if(     ( unzOpenCurrentFile( ZipFile ) != UNZ_OK )
			|| ( unzGetCurrentFileInfo( ZipFile, &FileInfo, NULL, 0, NULL, 0, NULL, 0 ) ) )
		{
			Error( ERR_FS, "Unable to open current file in PK3" );
			return 0;
		}
		
		return FileInfo.uncompressed_size;
	}

	inline void ReadCurrentFile( BYTE *buffer, unsigned int bufsize )
	{
		if( unzReadCurrentFile( ZipFile, buffer, bufsize ) < 0 ) 
			Error( ERR_FS, va("Unable to read current file in PK3 %s",FullPath) );
	}

	inline void CloseCurrentFile() 
	{
		unzCloseCurrentFile( ZipFile );
	}

	//Called by c_FS::GetFileList
	//Get directory listing of path
	inline int GetFileList( char *path, char *extention, char **pBuffer, int &sizeleft )
	{
		int len,i,number;
		int ExtLen, PathLen;
		char *buffer;
		int CheckExt;

		buffer = *pBuffer;
		PathLen = DQstrlen( path, MAX_QPATH ); 
		ExtLen = DQstrlen( extention, MAX_QPATH );
		number = 0;

		for( i=0; i<NumFiles; ++i ) {
			if( DQPartialstrcmpi( path, FileList[i].Filename, MAX_QPATH )==0 ) {
				//Path matches, check this is not in a subdir and check extention

				if(FileList[i].Filename[PathLen+1] == '\0') continue;

				CheckExt = -1;
				for( len=0; len<sizeleft; ++len ) {
					buffer[len] = FileList[i].Filename[PathLen+1+len];
					
					if(buffer[len] == '/' || buffer[len] == '\\' ) {
						if( FileList[i].Filename[PathLen+len+2]!='\0' ) break; //this is a file in a subdir
					}

					if(CheckExt>=0) {
						if( extention[CheckExt] != buffer[len] ) break;
						++CheckExt;
					}

					if(buffer[len] == '.' && ExtLen>0) CheckExt = 0;

					if(buffer[len] == '\0') {
						if(CheckExt>0 || ExtLen==0) {
							//valid file/folder
							++len;
							buffer += len;
							sizeleft -= len;
							++number;
						}
						break;
					}
				}
			}
		}

		*pBuffer = buffer;
		return number;
	}


	char FullPath[MAX_OSPATH];

private:
	unzFile ZipFile;
	s_path *FileList;
	int NumFiles;
};

//Either open the file and set File, or uncompress the whole file into Data. File's data can be forced into Data using GetPointerToData()
struct s_File {
	FILE *File;
	BYTE *Data;
	unsigned int CurrentPos, Filesize;

	s_File():File(NULL),Data(NULL),CurrentPos(0),Filesize(0) {}

	inline void CloseFile() {
		if(File) fclose(File);
		File = NULL;
		DQDeleteArray( Data );
		CurrentPos = Filesize = 0;
	}
};

class c_FS : public c_Singleton<c_FS>
{
public:
	c_FS();
	virtual ~c_FS();
	FILEHANDLE OpenFile(const char *Filename, const char *Mode);
	void CloseFile(FILEHANDLE Handle);
	int  GetFileLength(FILEHANDLE Handle);
	int  GetFileList(char *path, char *extention, char *buffer, int bufsize, BOOL bReturnFullPath);
	int  GetPosition(FILEHANDLE Handle);
	void ReadFile(void *Buffer, int size, int number, FILEHANDLE handle);
	void WriteFile(void *Buffer, size_t size, int number, FILEHANDLE handle);
	void Seek(long offset, int origin, FILEHANDLE handle);
	void FindPK3s();
	void *GetPointerToData(int Handle);

	inline void c_FS::Initialise() { FindPK3s(); }
	void c_FS::GetDLLPath(const char *DLLFilename, char *FullPath);

	char Q3Root[MAX_OSPATH];
private:
	FILEHANDLE c_FS::OpenFileWithFullPath(const char *FullFilename, const char *Mode);
	void FindFile(const char *Filename, char *FullPath);

	c_Chain<s_SubDir> SubDirChain;
	c_Chain<c_PK3> PK3Chain;
	s_File pFileHandleArray[MAX_FILEHANDLE];
	int NumFileHandles;

};

#define DQFS c_FS::GetSingleton()

#endif // !defined(AFX_C_FS_H__1F113785_1A1A_435C_A442_637554ED55F1__INCLUDED_)
