// DXQuake3 Source
// Copyright (c) 2003 Richard Geary (richard.geary@dial.pipex.com)
//
// c_UI.h: interface for the c_UI class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_C_UI_H__B9E68D29_C1BD_4B68_9918_047DA0873BC0__INCLUDED_)
#define AFX_C_UI_H__B9E68D29_C1BD_4B68_9918_047DA0873BC0__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

typedef int (QDECL  *t_syscall)( int arg,... );
typedef int (QDECL *t_vmMain)( int command, int arg0, int arg1, int arg2, int arg3, int arg4, int arg5, int arg6, int arg7, int arg8, int arg9, int arg10, int arg11  );
typedef void (QDECL *t_dllEntry)( t_syscall );

int PASSFLOAT( float x );

class c_UI : public c_Singleton<c_UI>
{
public:
	c_UI();
	virtual ~c_UI();
	void Load();
	void Unload();
	void Update();
	void Initialise();
	void Connect();		//draw connecting screen
	void OpenMenu();	//draw Main Menu or In Game Menu
	BOOL ExecuteCommand( c_Command *command );

	eAuthority eAuthUI;
	c_Command *CurrentCommand;
	BOOL	isConnecting;
private:
	HINSTANCE hDLL;
	t_vmMain vmMain;
	t_dllEntry dllEntry;
};

#define DQUI c_UI::GetSingleton()

#endif // !defined(AFX_C_UI_H__B9E68D29_C1BD_4B68_9918_047DA0873BC0__INCLUDED_)
