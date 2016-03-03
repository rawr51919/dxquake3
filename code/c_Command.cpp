// DXQuake3 Source
// Copyright (c) 2003 Richard Geary (richard.geary@dial.pipex.com)
//
// c_Command.cpp: implementation of the c_Command class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
/*
c_Command::c_Command(char *Command)
{
	int pos=0, LastPos=0;
	char *Arg;
	
	NumArgs = -1;
	while(pos<MAX_STRING_CHARS) {	//surplus condition
		pos += DQSkipWhitespace(&Command[pos], MAX_STRING_CHARS-pos);
		if(pos==-1) break;
		++NumArgs;

		//Find the length of the next argument
		LastPos = pos;
		pos += DQSkipWord(&Command[pos], MAX_STRING_CHARS-pos);
		if(pos==-1) break;
		
		//Copy the word to a new (correctly sized) string
		Arg = (char*)DQNewVoid(char[pos-LastPos]);
		DQstrcpy(Arg, &Command[LastPos], pos-LastPos);

		//Add it to the Argument List
		ArgChain.AddUnit(Arg);
	}
}*/

c_Command::c_Command() 
{
	Command[0] = '\0';
}

c_Command::~c_Command()
{
}

//If there is another command after this (either \n or ;), return a pointer to its start position
char * c_Command::MakeCommandString(const char *_command, int MaxLength)
{
	const char *pSrc;
	char *pDest;

	Command[0] = '\0';
	pDest = Command;

	//Remove first \ 
	if(_command[0]=='\\') pSrc = _command+1;
	else				  pSrc = _command;

	//copy to member variable
	// Copies until ; ,  or \n provided the \n is not in inverted commas
	// also strips surplus whitespace
	//Returns NULL if no further commands, or the pointer to the start of the next command
	//Also marks out the positions of arguments

	BOOL isQuotes;	//is in inverted commas
	BOOL bAddSpace;
	int pos, NextCommandOffset, DestRemaining, len, posEndLine;
	
	isQuotes = FALSE;
	bAddSpace = FALSE;
	DestRemaining = MAX_STRING_CHARS;
	pos = 0;

	len = DQSkipWhitespace( pSrc, MaxLength-pos );
	pos += len;
	pSrc += len;
	if(pos>=MaxLength) return NULL;
	posEndLine = pos + DQNextLine( pSrc, MaxLength );

	ArgPosition[0] = Command;
	NumArgs = 0;

	for(; pos<MaxLength; ) {

		if(*pSrc==';' || (!isQuotes && ( *pSrc==10 || *pSrc==13 || pos>=posEndLine)) ) {
			if(*pSrc==';') ++pSrc;

			//Check there really is a command after this
			NextCommandOffset = DQSkipWhitespace( pSrc, MaxLength-pos-1 );
			pSrc += NextCommandOffset;
			if(!*pSrc) {
				//no next command
				return NULL;
			}
			return (char*)pSrc;	//return start of next command
		}

		//Add a space after a command if there's another
		if(bAddSpace) {
			*pDest = ' ';
			++pDest;
		}
		bAddSpace = TRUE;

		//Add ArgPosition
		if(NumArgs<MAX_ARGS) {
			ArgPosition[NumArgs] = pDest;
		}
/*		else {
			Error(ERR_CONSOLE, va("c_Command : Too many args (>%d)",MAX_ARGS));
			//This happens with cmd "scores"
		}*/
		++NumArgs;

		if(*pSrc=='"') {
			isQuotes = !isQuotes;
			++pSrc;
			++pos;
			if(pos>=MaxLength) return NULL;
		}

		if(isQuotes) {
			len = DQCopyUntil( pDest, pSrc, '"', DestRemaining );
			posEndLine = pos + DQNextLine( pSrc, MaxLength );
			isQuotes = FALSE;
		}
		else {
			len = DQWordstrcpy( pDest, pSrc, DestRemaining );
		}

		pSrc += len;
		pos += len;
		pDest += len;

		DestRemaining -= len;
		if(DestRemaining<=0) {
			NextCommandOffset = DQSkipWhitespace( pSrc, MaxLength-pos-1 );
			if(NextCommandOffset==0) {
				return NULL;
			}
			pSrc += NextCommandOffset;
			return (char*)pSrc;
		}

		len = DQSkipWhitespace( pSrc, MaxLength-pos );
		pos += len;
		pSrc += len;
		if(pos>=MaxLength) return NULL;

	}
	return NULL;
}


char *c_Command::GetArg(int arg) {
	if(arg==0) return Command;				//incase NumArgs = 0
	if(arg<0 || arg>=NumArgs) {
		Error(ERR_CONSOLE, "Invalid Arg Num");
		return Command;
	}
	if(arg>=MAX_ARGS) {
		//this happens with cmd scores
		int i,pos;
		pos = DQSkipWhitespace( Command, MAX_STRING_CHARS );
		for(i=0;i<arg;++i) {
			pos += DQSkipWord( &Command[pos], MAX_STRING_CHARS-pos );
			pos += DQSkipWhitespace( &Command[pos], MAX_STRING_CHARS-pos );
		}
		return &Command[pos];
	}
	
	return ArgPosition[arg];
}