// DXQuake3 Source
// Copyright (c) 2003 Richard Geary (richard.geary@dial.pipex.com)
//
// c_Console.h: interface for the c_Console class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_C_CONSOLE_H__5E808094_889D_44C3_83E1_434B3ADE9EE1__INCLUDED_)
#define AFX_C_CONSOLE_H__5E808094_889D_44C3_83E1_434B3ADE9EE1__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

//
// Console Command Handler Interface
// Reference : FlipCode Tutorial : Console Variables And Commands - Gaz Iqbal
//			   http://www.flipcode.com/tutorials/tut_console.shtml
//
struct I_CommandHandler {
	virtual void HandleCommand( int CommandID, c_Chain<char*> argChain );
};

#define MAX_CVARS		65535
#define MAX_CVAR_NAME	256
#define MAX_TABCOMMANDS 1024
#define MAX_COMMANDBUFFER 1000
#define MAX_CONSOLELINES		1000
#define MAX_CONSOLELINELENGTH	120
#define MAX_CONSOLEREDO	50

enum eAuthority { eAuthClient, eAuthServer };

//Similar to Q3 cvar_s
struct s_CVar {
	char		*name;
	char		*string;
	char		*resetString;		// cvar_restart will reset to this value
	char		*latchedString;		// for CVAR_LATCH vars
	int			flags;
	BOOL		modified;			// set each time the cvar is changed
	int			modificationCount;	// incremented each time the cvar is changed
	float		value;				// atof( string )
	int			integer;			// atoi( string )
	eAuthority	Authority;
};

enum eShowConsole { eShowConsoleNone, eShowConsoleHalf, eShowConsoleFull };

class c_Console : public c_Singleton<c_Console>
{
public:
	c_Console();
	virtual ~c_Console();
	void Initialise();
	void Update();
	void RenderConsole();
	inline void FlushLogfile() { if(LogFile) fflush(LogFile); }
	void WriteConfigFile();
	void ProcessCommandLine( char *lpCmdLine );

	void RegisterCommand();
	void RegisterCVar( const char *name, const char *value, int flags, eAuthority Authority, vmCvar_t *vmCVar = NULL );
	void UpdateCVar( vmCvar_t *vmCVar );
	void GetCVar( const char *name, vmCvar_t *vmCVar );
	int FindCVarFromName(const char *Name);
	void SetCVarString( const char *name, const char *value, eAuthority Authority );
	void SetCVarString( const int index, const char *value, eAuthority Authority );
	void SetCVarValue( const char *name, float value, eAuthority Authority );
	int GetCVarInteger( const char *name, eAuthority Authority );
	float GetCVarFloat( const char *name, eAuthority Authority );
	void GetCVarString( const char *name, char *buffer, int MaxLength, eAuthority Authority);	//Copies string into buffer
	void ResetCVar( const char *name, eAuthority Authority );
	void Print(const char *text, BOOL bNoAutoNewline = FALSE );
	void AddTabCommand(const char *command);
	void TabComplete();
	void Reload();

	void ExecuteCommandString( const char *string, int MaxLength, BOOL PushToFront );
	BOOL c_Console::ExecuteCommand( c_Command *command, eAuthority Authority );
	void AddServerConsoleCommand( const char *string );

	void GetServerInfo(char *buffer, int bufsize);
	BOOL UpdateUserInfo();

	BOOL isReady() { return isLoaded; }
	BOOL IsLocalServer() { return LocalServer; }
	void ProcessError( c_Exception &ErrorMessage, int Line, char *File );
	void RunLocalServer( BOOL RunLocalServer );
	void ConnectToServer(char *ServerName, int MaxLength);
	void Disconnect();

	char UserInfoString[MAX_INFO_STRING];
	int  UserInfoStringSize;

	void ShowConsole( eShowConsole State );
	void AddInputChar( char ch );
	void InputKey( int KeyPressed );
	float ShowConsoleTimer;
	BOOL IsInConsole() { return (ShowConsoleState==eShowConsoleNone)?FALSE:TRUE; }
	BOOL isWindowActive;
	BOOL IsDedicatedServer() { UpdateCVar( &isDedicated ); return isDedicated.integer; }

	int OverbrightBits;
	int ConsoleShader;

private:
	void c_Console::RegisterUserInfoVars();
	void c_Console::ExecuteCommandsNow();

	c_Queue<c_Command> ClientCommandBuffer, ServerCommandBuffer;

	BOOL isLoaded;
	int NumCVars;
	s_CVar **pCVarArray;
	FILE *LogFile;
	BOOL LogContainsError;
	char *TabCommands[MAX_TABCOMMANDS];
	int NumTabCommands;
	vmCvar_t isDedicated, sv_cheats;

	//Console Rendering
	eShowConsole ShowConsoleState;
	char ConsoleText[MAX_CONSOLELINES][MAX_CONSOLELINELENGTH];
	int	ConsoleTextNextLine;
	char ConsoleTextCurrentInput[MAX_CONSOLELINELENGTH];
	char ConsoleTextPreviousCommands[MAX_CONSOLEREDO][MAX_CONSOLELINELENGTH];
	int ConsoleRedoNextPos;
	int ConsoleRedoSelected;
	int ConsoleLineDisplayOffset;
	int CurrentInputPos;
	int ConsoleCaratPos;
	BOOL ConsoleCaratTimerReset;
	BOOL LocalServer;
	float LastPrintTime;
};

#define DQConsole c_Console::GetSingleton()
#define DQPrint(x) DQConsole.Print(x)

#endif // !defined(AFX_C_CONSOLE_H__5E808094_889D_44C3_83E1_434B3ADE9EE1__INCLUDED_)
