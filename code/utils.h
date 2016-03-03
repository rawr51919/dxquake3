// DXQuake3 Source
// Copyright (c) 2003 Richard Geary (richard.geary@dial.pipex.com)
//
//
//	Useful Utilities for DQ
//

#ifndef __UTILS_H
#define __UTILS_H

int DQstrcpy(char *pDest, const char *pSrc, int MaxLength);
int DQstrlen(const char *pSrc, int MaxLength);
int DQstrcmp(const char *pStr1, const char *pStr2, int MaxLength);
int DQstrcmpi(const char *pStr1, const char *pStr2, int MaxLength);
int DQstrcat(char *pDest, const char *pSrcAppend, int MaxLength);

int DQSkipWhitespace(const char *pSrc, int MaxLength);
int DQSkipWord(const char *pSrc, int MaxLength);
int DQNextLine(const char *pSrc, int MaxLength);

void DQStripExtention(char *pPath, int MaxLength);
void DQStripFilename(char *pPath, int MaxLength);
void DQStripPath(char *pFullPath, int MaxLength);
void DQStripGfxExtention(char *pSrc, int MaxLength);
void DQStripQuotes(char *pString, int MaxLength);

int DQWordstrcmpi(const char *pSrc1, const char *pSrc2, int MaxLength);
int DQWordstrcpy(char *pDest, const char *pSrc, int MaxLength);
int DQCopyUntil(char *pDest, const char *pSrc, char marker, int MaxLength);

int DQPartialstrcmpi(const char *pStr1, const char *pStr2, int MaxLength);
void DQstrTruncate(char *pStr1, const char *pCompareString, int MaxLength);
void DQWCharToChar(WCHAR *pSrc, int SrcLength, char *pDest, int DestLength);
int DQFilenamestrcmpi(const char *pStr1, const char *pStr2, int MaxLength);

#define ALIGN16
//#define ALIGN16 __declspec(align(16))
typedef ALIGN16 D3DXMATRIX MATRIX;

#endif //__UTILS_H