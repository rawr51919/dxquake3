// DXQuake3 Source
// Copyright (c) 2003 Richard Geary (richard.geary@dial.pipex.com)
//
//
//	Useful Utilities for DQ
//

#include "stdafx.h"

//'a'-'A' = 97 - 65 = 32
#define DQLowerCase( in ) ( ( (in)>='a' && (in)<='z' ) ? (in)-32 : (in) )

//Copy pSrc to pDest up to null terminating char or MaxLength
//Return value is length of copied string, excluding null-terminator
int DQstrcpy(char *pDest, const char *pSrc, int MaxLength)
{
	int pos = 0;

	if( !pDest || !pSrc || MaxLength<0 ) {
		//Used by c_Exception, so we can't throw a c_Exception type
		throw 1;
	}

	while(*pSrc!='\0' && pos<MaxLength-1) {
		*pDest = *pSrc;
		++pos;
		++pSrc;
		++pDest;
	}
	*pDest = '\0';
	return pos;
}

//Returns length of pSrc excluding null terminting character, up to MaxLength
int DQstrlen(const char *pSrc, int MaxLength)
{
	const char *pos = pSrc;
	int len = 0;
	while(*pos!='\0' && len<MaxLength) {
		++len;
		++pos;
	}
	return len;
}

//Compares pStr1 to pStr2 up to Null terminator or Maxlength. Returns 0 if identital, else -1 (c.f. C strcmp)
int DQstrcmp(const char *pStr1, const char *pStr2, int MaxLength)
{
	int pos = 1;
	DebugCriticalAssert( pStr1 && pStr2 );

	while(*pStr1==*pStr2) {
		if(*pStr1=='\0' || pos>=MaxLength) return 0; //strings identical
		++pStr1; ++pStr2; ++pos;
	}
	if(*pStr1=='\0' || *pStr2=='\0') return 0;

	//strings different
	return -1;
}
	
//Compares (case insensitive) pStr1 to pStr2 up to Null terminator or Maxlength. Returns 0 if identital, else -1 (c.f. C strcmp)
int DQstrcmpi(const char *pStr1, const char *pStr2, int MaxLength)
{
	int pos = 0;
	DebugCriticalAssert( pStr1 && pStr2 );
	char c1, c2;

	c1 = DQLowerCase( *pStr1 );
	c2 = DQLowerCase( *pStr2 );

	while(c1==c2) {
		if(pos>=MaxLength) return -1;
		if(c1=='\0') return 0; //strings identical
		++pStr1; ++pStr2; ++pos;

		c1 = DQLowerCase( *pStr1 );
		c2 = DQLowerCase( *pStr2 );
	}

	//strings different
	return -1;
}
	
//Skips space, tab, new lines, // and /* */
//Returns position (in bytes) relative to pSrc
int DQSkipWhitespace(const char *pSrc, int MaxLength) {
	BOOL bComment = FALSE;		//Possible Comment ( // )
	int pos=0;

	DebugCriticalAssert( pSrc );

	while(pos<MaxLength && (*pSrc==' ' || *pSrc=='\t' || *pSrc=='/' || *pSrc=='*' || *pSrc==10 || *pSrc==13)) 
	{
		if(*pSrc=='*') {
		//if we have /*
			if(bComment) {
				//Skip until */
				bComment = TRUE;
				while(bComment) {
					if(pos>=MaxLength) return MaxLength;
					if(*pSrc=='*') {
						if((pos+1<MaxLength) && *(pSrc+1)=='/') {
							bComment = FALSE; 
							++pSrc; 
							++pos; 
						}
					}
					++pSrc; ++pos;
				}
			}
			else break;	// we have * on its own, which is not whitespace
		}
		if(*pSrc=='/') {
			if(bComment == TRUE) {
				//Skip to end of line
				while(*pSrc!=10 && *pSrc!=0 && pos<MaxLength) { ++pSrc; ++pos; }
				bComment = FALSE;
			}
			else bComment = TRUE;
		}
		else {
			if(bComment == TRUE) {
				//Previous / was a valid character
				return pos-1;
			}
		}
		++pSrc;
		++pos;
	}
//	if(*pSrc==10 || *pSrc==13 || *pSrc==0 || pos==MaxLength) return MaxLength;	//end of the string
	//else 
	return pos;
}

//Returns the position of the next space, tab, end of line, comment, or MaxLength
int DQSkipWord(const char *pSrc, int MaxLength)
{
	BOOL bComment = FALSE;
	int pos=0;

	DebugCriticalAssert( pSrc );

	while(pos<MaxLength && *pSrc!=' ' && *pSrc!='\t' && *pSrc!=10 && *pSrc!=13 && *pSrc!=0) {
		if(*pSrc=='*' && bComment==TRUE) { 
			return pos-1; 
		}
		if(*pSrc=='/') {
			if(bComment == TRUE) {
				// return pos of start of //
				return pos-1;
			}
			else bComment = TRUE;
		}
		else {
			bComment = FALSE; //wasn't a comment line
		}
		++pSrc;
		++pos;
	}
	return pos;
}

//Strips an extention off a path
void DQStripExtention(char *pPath, int MaxLength)
{
	int pos = 0;
	
	while(pos<MaxLength && *pPath!='\0') {
		++pos; ++pPath;
	}
	//Move back to .
	while(pos>0 && *pPath!='.') {
		--pos; --pPath;
	}
	//Truncate pPath
	if(*pPath=='.') *pPath='\0';
	return;
}

//Strips a filename off a path
void DQStripFilename(char *pPath, int MaxLength)
{
	BOOL bHasFilename = FALSE;	//path has a filename in it
	int pos = 0;
	
	while(pos<MaxLength && *pPath!='\0') {
		if(*pPath=='.') bHasFilename = TRUE;
		++pos; ++pPath;
	}
	if(!bHasFilename) {
		//path has no filename already
		//Remove any / at the end of pPath
		--pPath;
		if(*pPath=='\\' || *pPath=='/') *pPath='\0';
		return;
	}

	/* Move back to / or \ */
	while(pos>0 && *pPath!='/' && *pPath!='\\') {
		--pos; --pPath;
	}
	
	//Truncate pPath
	*pPath='\0';
	return;
}

//Returns position of next line (relative to pSrc)
int DQNextLine(const char *pSrc, int MaxLength)
{
	int pos=0;
	DebugCriticalAssert( pSrc );

	while(pos<MaxLength && *pSrc!=10 && *pSrc!=13 && *pSrc!=0) {
		++pSrc;
		++pos;
	}
	if(*pSrc==0) return pos;
	return pos+1;
}

//Case insensitive Compare pSrc1 to pSrc2 until next whitespace or MaxLength, not including comments (cba)
//Return 0 if identical, else -1
int DQWordstrcmpi(const char *pStr1, const char *pStr2, int MaxLength)
{
	DebugCriticalAssert( pStr1 && pStr2 );
	int pos = 0;
	char c1, c2;

	c1 = DQLowerCase( *pStr1 );
	c2 = DQLowerCase( *pStr2 );

	while(c1==c2) {
		if(c1=='\0' || c1==' ' || c1=='\t' || pos>MaxLength) return 0; //strings identical
		++pStr1; ++pStr2; ++pos;

		c1 = DQLowerCase( *pStr1 );
		c2 = DQLowerCase( *pStr2 );
	}
	if(c1=='\0' && ( c2==' ' || c2=='\t' || c2==10 || c2==13) ) return 0;
	if(c2=='\0' && ( c1==' ' || c1=='\t' || c1==10 || c1==13) ) return 0;

	//strings different
	return -1;
}

//Compares (case insensitive) pStrShort to pStrLong up to FIRST Null terminator or Maxlength. 
//Returns 0 if pStrLong is identical to pStrShort so far, else -1
//eg. pStrShort = "blah", pStrLong = "blahman", return 0
//eg. pStrShort = "blahman", pStrLong = "blah", return -1

int DQPartialstrcmpi(const char *pStrShort, const char *pStrLong, int MaxLength)
{
	int pos = 0;
	DebugCriticalAssert( pStrShort && pStrLong );
	char c1, c2;

	c1 = DQLowerCase( *pStrShort );
	c2 = DQLowerCase( *pStrLong );

	while(c1==c2) {
		if(pos>=MaxLength) return 0;
		if(c1=='\0') return 0;
		++pStrShort; ++pStrLong; ++pos;

		c1 = DQLowerCase( *pStrShort );
		c2 = DQLowerCase( *pStrLong );
	}

	if(*pStrShort=='\0') return 0;

	//strings different
	return -1;
}
	

//Copies pSrc to pDest until whitespace or MaxLength
//Returns length of word, excluding null terminator
int DQWordstrcpy(char *pDest, const char *pSrc, int MaxLength)
{
	int pos = 0;
	DebugCriticalAssert( pSrc && pDest && MaxLength>=0 );
	
	while(*pSrc!='\0' && *pSrc!=' ' && *pSrc!='\t' && *pSrc!=10 && *pSrc!=13 && pos<MaxLength-1) {
		*pDest = *pSrc;
		++pos;
		++pSrc;
		++pDest;
	}
	*pDest = '\0';
	return pos;
}

//Appends pSrcAppend to pDest, up to pSrcAppend[MaxLength] or Null-terminator
//Returns length of pSrcAppend
int DQstrcat(char *pDest, const char *pSrcAppend, int MaxLength)
{
	int pos = 0, SrcLength = 0;
	while(*pDest!='\0') {
		if(pos>=MaxLength) {
			return MaxLength;
		}
		++pDest;
		++pos;
	}
	while(*pSrcAppend!='\0') {
		if(pos>=MaxLength-1) {
			return MaxLength;
		}
		*pDest = *pSrcAppend;
		++pDest;
		++pSrcAppend;
		++pos;
		++SrcLength;
	}
	*pDest = '\0';
	return SrcLength;
}

void DQStripPath(char *pFullPath, int MaxLength)
{
	int pos = 0;
	char *pPath = pFullPath;
	
	while(pos<MaxLength && *pPath!='\0') {
		++pos; ++pPath;
	}
	while(pos>0 && *pPath!='/' && *pPath!='\\') {
		--pPath; --pos;
	}
	if(pos==0) return;
	++pPath; ++pos;
	while(pos<MaxLength && *pPath!='\0') {
		*pFullPath = *pPath;
		++pPath;
		++pFullPath;
		++pos;
	}
	*pFullPath='\0';
}

//Copy pSrc to pDest until *pSrc==marker or '\0' or MaxLength reached
//returns num chars copied +1, or the pos of the null-terminator
int DQCopyUntil(char *pDest, const char *pSrc, char marker, int MaxLength)
{
	int pos = 0;
	while(pos<MaxLength-1 && *pSrc!=marker && *pSrc!='\0') {
		*pDest=*pSrc; ++pDest; ++pSrc; ++pos;
	}
	*pDest = '\0';

	if(*pSrc=='\0') return pos;
	return pos+1;		//return the char after marker
}

//remove "'s
void DQStripQuotes(char *pString, int MaxLength)
{
	int i;
	if(*pString=='"') {
		for(i=0;i<MaxLength && *(pString+1)!='"';++i,++pString) 
			*pString = *(pString+1);
		*pString='\0';
	}
}

//Truncate pStr1 when it varies (case insensitive) from pCompareString
void DQstrTruncate(char *pStr1, const char *pCompareString, int MaxLength)
{
	int pos = 0;
	DebugCriticalAssert( pStr1 && pCompareString );
	char c1,c2;

	c1 = DQLowerCase( *pStr1 );
	c2 = DQLowerCase( *pCompareString );

	while(c1==c2) {
		if(pos>=MaxLength) return;
		if(c1=='\0') return;
		++pStr1; ++pCompareString; ++pos;

		c1 = DQLowerCase( *pStr1 );
		c2 = DQLowerCase( *pCompareString );
	}

	//Truncate
	*pStr1 = '\0';
}

void DQWCharToChar(WCHAR *pSrc, int SrcLength, char *pDest, int DestLength)
{
	WideCharToMultiByte( CP_ACP, NULL, pSrc, SrcLength, pDest, DestLength, NULL, NULL );
}

//same as strcmpi, but insensitive to \ and /
int DQFilenamestrcmpi(const char *pStr1, const char *pStr2, int MaxLength)
{
	int pos = 0;
	DebugCriticalAssert( pStr1 && pStr2 );
	char c1, c2;

	c1 = DQLowerCase( *pStr1 );
	c2 = DQLowerCase( *pStr2 );
	if(c1=='\\') c1='/';
	if(c2=='\\') c2='/';

	while(c1==c2) {
		if(pos>=MaxLength) return -1;
		if(c1=='\0') return 0; //strings identical
		++pStr1; ++pStr2; ++pos;

		c1 = DQLowerCase( *pStr1 );
		c2 = DQLowerCase( *pStr2 );
		if(c1=='\\') c1='/';
		if(c2=='\\') c2='/';
	}

	//strings different
	return -1;
}


void DQStripGfxExtention(char *pSrc, int MaxLength)
{
	int len;

	if(!pSrc || MaxLength<4) return;
	
	len = DQstrlen( pSrc, MAX_QPATH );
	if(len>4) {
		if( (DQstrcmpi(&pSrc[len-4], ".tga", MAX_QPATH)==0) || (DQstrcmpi(&pSrc[len-4], ".jpg", MAX_QPATH)==0) ) {
			pSrc[len-4] = '\0';
		}
	}
}

