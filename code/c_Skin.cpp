// DXQuake3 Source
// Copyright (c) 2003 Richard Geary (richard.geary@dial.pipex.com)
//
// c_Skin.cpp: implementation of the c_Skin class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

c_Skin::c_Skin()
{
	SkinFilename[0] = '\0';
}

c_Skin::~c_Skin()
{

}

void c_Skin::LoadSkin(const char *Filename)
{
	char word[MAX_STRING_CHARS];
	FILEHANDLE Handle;
	int pos,pos2,posNextLine,FileLength,i;
	char *buffer;
	s_SkinInfo *SkinInfo;
	BOOL bFound;

	//Copy to member var
	DQstrcpy(SkinFilename, Filename, MAX_QPATH);

	//Q3UI passes an invalid Skin filename - fix it
	bFound = FALSE;
	for(i=0;i<MAX_STRING_CHARS && SkinFilename[i]!='\0';++i) {
		if(SkinFilename[i]=='"') 
			bFound = TRUE;
		if(bFound) SkinFilename[i]=SkinFilename[i+1];
	}


	Handle = DQFS.OpenFile(SkinFilename, "rb");
	if(!Handle) {
		Error(ERR_RENDER, va("Could not open skin %s",SkinFilename) );
		return;
	}

	FileLength = DQFS.GetFileLength(Handle);
	buffer = (char*)DQNewVoid(char [FileLength+1]);

	DQFS.ReadFile(buffer, sizeof(char), FileLength, Handle);
	buffer[FileLength] = '\0';
	DQFS.CloseFile(Handle);

	pos = pos2 = posNextLine = 0;
	while(1) {
		pos = pos2 + DQSkipWhitespace(&buffer[pos2], FileLength);
		posNextLine = pos + DQNextLine(&buffer[pos], MAX_STRING_CHARS);
		pos2 = pos + DQCopyUntil(word, &buffer[pos], ',', MAX_STRING_CHARS);
		if(pos2<=pos) break;
		if( (word[0] == 't' || word[0]=='T') 
			&&(word[1]=='a' || word[1]=='A')
			&&(word[2]=='g' || word[2]=='G') )
					continue;
		SkinInfo = SkinChain.AddUnit();
		DQstrcpy(SkinInfo->SurfaceName, word, pos2-pos);
		pos = pos2 + DQSkipWhitespace(&buffer[pos2], FileLength);
		pos2 = pos + DQWordstrcpy(word, &buffer[pos], MAX_STRING_CHARS);
		DQstrcpy(SkinInfo->ShaderName, word, pos2-pos+1);

		//TODO : LightingDiffuse
		SkinInfo->ShaderHandle = DQRender.RegisterShader(SkinInfo->ShaderName);
	}

	DQDeleteArray(buffer);
}

int c_Skin::GetShaderFromSurfaceName(const char *SurfaceName)
{
	s_ChainNode<s_SkinInfo> *SkinNode = SkinChain.GetFirstNode();

	while(SkinNode) {
		if(DQstrcmpi(SurfaceName, SkinNode->current->SurfaceName, MAX_QPATH)==0)
			return SkinNode->current->ShaderHandle;
		SkinNode = SkinNode->next;
	}
	return 0;
}