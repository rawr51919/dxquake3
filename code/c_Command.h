// DXQuake3 Source
// Copyright (c) 2003 Richard Geary (richard.geary@dial.pipex.com)
//
// c_Command.h: interface for the c_Command class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_C_COMMAND_H__E19D1762_7A94_4D6F_A550_3F21D18360E6__INCLUDED_)
#define AFX_C_COMMAND_H__E19D1762_7A94_4D6F_A550_3F21D18360E6__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#define MAX_ARGS 10

class c_Command  
{
public:
	c_Command();
	virtual ~c_Command();
	char *GetArg(int arg);
	int GetNumArgs() { return NumArgs; }
	char* MakeCommandString(const char *Command, int MaxLength);

private:
	int NumArgs;
	char *ArgPosition[MAX_ARGS];
	char Command[MAX_STRING_CHARS];
};

#endif // !defined(AFX_C_COMMAND_H__E19D1762_7A94_4D6F_A550_3F21D18360E6__INCLUDED_)
