// DXQuake3 Source
// Copyright (c) 2003 Richard Geary (richard.geary@dial.pipex.com)
//
// c_Console.cpp: implementation of the c_Console class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "c_Console.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

c_Console::c_Console():ClientCommandBuffer(MAX_COMMANDBUFFER),ServerCommandBuffer(MAX_COMMANDBUFFER)
{
	isLoaded = FALSE;
	NumCVars = 0;
	pCVarArray = (s_CVar**)DQNewVoid(s_CVar*[MAX_CVARS]);

	//Open ConsoleLog.txt to write all console text to a file
	LogFile = fopen("ConsoleLog.txt", "w+");
	if(!LogFile) {
		LogFile = fopen("c:\\ConsoleLog.txt", "w+");
		if(!LogFile) CriticalError(ERR_CONSOLE, "Unable to open C:\\ConsoleLog.txt");
	}

	LogContainsError = FALSE;
	for(int i=0;i<MAX_TABCOMMANDS;++i) TabCommands[i] = NULL;
	NumTabCommands = 0;

	ShowConsoleState = eShowConsoleNone;
	ConsoleShader = 0;
	for(i=0;i<MAX_CONSOLELINES;++i) ConsoleText[i][0] = '\0';
	ConsoleTextNextLine = 0;
	ConsoleTextCurrentInput[0] = '\0';
	CurrentInputPos = 0;
	ConsoleCaratPos = 0;
	LocalServer = FALSE;
	ShowConsoleTimer = 0;
	for(i=0;i<MAX_CONSOLEREDO;++i) ConsoleTextPreviousCommands[i][0] = '\0';
	ConsoleRedoNextPos = 0;
	ConsoleRedoSelected = 0;
	ConsoleLineDisplayOffset = 0;
	RegisterCVar( "dedicated", "0", 0, eAuthServer, &isDedicated );
	RegisterCVar( "sv_cheats", "0", CVAR_ARCHIVE, eAuthServer, &sv_cheats );

	//Set ServerInfo CVars
	RegisterCVar("mapname", "", CVAR_SERVERINFO, eAuthServer);

	OverbrightBits = 0;
	UserInfoStringSize = 0;
	LastPrintTime = 0;
}

c_Console::~c_Console()
{
	int i;

	for(i=0;i<NumCVars;++i) {
		DQDeleteArray(pCVarArray[i]->name);
		DQDeleteArray(pCVarArray[i]->string);
		DQDeleteArray(pCVarArray[i]->resetString);
		if(pCVarArray[i]->latchedString) DQDeleteArray(pCVarArray[i]->latchedString);
		DQDelete(pCVarArray[i]);
	}
	DQDeleteArray(pCVarArray);
	FlushLogfile();
	fclose(LogFile);

	for(i=0;i<NumTabCommands;++i) {
		DQDeleteArray(TabCommands[i]);
	}

	if(LogContainsError)
		MessageBox(NULL, "Note : Logfile \"ConsoleLog.txt\" contains an error", "DXQ3 Message", MB_OK | MB_SYSTEMMODAL);
}

void c_Console::Print(const char *text, BOOL bNoAutoNewline )
{
	int pos;
	int len = DQstrlen(text, MAX_STRING_CHARS);

	if(len == 0) return;

	//Forced to use Win32 functions as DQFS uses Error, which could put us in a loop
	fwrite(text, sizeof(char), len, LogFile);

	if(!bNoAutoNewline) {
		//Used by DXQ3
		pos = 0;

		if(text[len-1]!='\n')
			fwrite("\n", sizeof(char), 1, LogFile);
		do {
			
			pos += DQCopyUntil( ConsoleText[ConsoleTextNextLine++], &text[pos], '\n', MAX_CONSOLELINELENGTH );
			if(ConsoleTextNextLine>=MAX_CONSOLELINES) ConsoleTextNextLine = 0;
			ConsoleText[ConsoleTextNextLine][0] = '\0';

		}while( text[pos]!='\0' );

	}
	else {
		//Used by Q3

		DQstrcat( ConsoleText[ConsoleTextNextLine], text, MAX_CONSOLELINELENGTH );
	}

	//only go to next line if there is a \n 
	if((bNoAutoNewline && (text[len-1]==10 || text[len-1]==13)) ) {
		++ConsoleTextNextLine;
		if(ConsoleTextNextLine>=MAX_CONSOLELINES) ConsoleTextNextLine = 0;
		ConsoleText[ConsoleTextNextLine][0] = '\0';
	}

	LastPrintTime = DQTime.GetSecs();
}

void c_Console::Initialise()
{
	FILEHANDLE Q3cfg;
	char *buffer;
	int FileLength;
	
	DQPrint( va("c_FS::Q3Root = %s", DQFS.Q3Root ) );

	//Execute q3config.cfg
	Q3cfg = DQFS.OpenFile("q3config.cfg", "rb");
	if( !Q3cfg ) {
		CriticalError( ERR_CONSOLE, "Could not find q3config.cfg" );
	}
	FileLength = DQFS.GetFileLength(Q3cfg);
	buffer = (char*)DQNewVoid(char [FileLength+1]);

	DQFS.ReadFile(buffer, sizeof(char), FileLength, Q3cfg);
	buffer[FileLength] = '\0';

	DQFS.CloseFile(Q3cfg);
	
	ExecuteCommandString( buffer, FileLength, FALSE );
	ExecuteCommandsNow();

	DQDeleteArray(buffer);

	Reload();

	AddTabCommand( "set" );
	AddTabCommand( "seta" );
	AddTabCommand( "bind" );
	AddTabCommand( "unbindall" );
	AddTabCommand( "quit" );
	AddTabCommand( "vid_restart" );
	AddTabCommand( "connect" );
	AddTabCommand( "disconnect" );
	AddTabCommand( "showstats" );
	AddTabCommand( "RenderDeviceInfo" );
	AddTabCommand( "ClientConnectionInfo" );
	AddTabCommand( "ServerConnectionInfo" );
	
	RegisterUserInfoVars();

	isLoaded = TRUE;
}

void c_Console::RegisterUserInfoVars()
{
	RegisterCVar( "ip", "", CVAR_USERINFO, eAuthClient );
	RegisterCVar( "cg_predictItems", "", CVAR_USERINFO, eAuthClient );
	RegisterCVar( "name", "", CVAR_USERINFO, eAuthClient );
	RegisterCVar( "handicap", "", CVAR_USERINFO, eAuthClient );
	RegisterCVar( "team_model", "", CVAR_USERINFO, eAuthClient );
	RegisterCVar( "team_headmodel", "", CVAR_USERINFO, eAuthClient );
	RegisterCVar( "model", "", CVAR_USERINFO, eAuthClient );
	RegisterCVar( "headmodel", "", CVAR_USERINFO, eAuthClient );
	RegisterCVar( "team", "", CVAR_USERINFO, eAuthClient );
	RegisterCVar( "teamoverlay", "", CVAR_USERINFO, eAuthClient );
	RegisterCVar( "teamtask", "", CVAR_USERINFO, eAuthClient );
	RegisterCVar( "color1", "", CVAR_USERINFO, eAuthClient );
	RegisterCVar( "color2", "", CVAR_USERINFO, eAuthClient );
	RegisterCVar( "g_redteam", "", CVAR_USERINFO, eAuthClient );
	RegisterCVar( "g_blueteam", "", CVAR_USERINFO, eAuthClient );
	RegisterCVar( "skill", "", CVAR_USERINFO, eAuthClient );
	RegisterCVar( "password", "", CVAR_USERINFO, eAuthClient );
}

BOOL c_Console::ExecuteCommand( c_Command *command, eAuthority Authority )
{
	char param1[MAX_STRING_CHARS];
	int i;

	if(DQWordstrcmpi( command->GetArg(0), "", MAX_STRING_CHARS )==0) {
		//Null string, do nothing
		return TRUE;
	}
	if(DQWordstrcmpi( command->GetArg(0), "bind", MAX_STRING_CHARS )==0) {
		//bind key "bindstring"

		DQWordstrcpy( param1, command->GetArg(1), MAX_STRING_CHARS );
		if(command->GetNumArgs()<3) 
			Print("bind key \"bindstring\"");
		else {
//			DQStripQuotes( command->GetArg(2), MAX_STRING_CHARS );
			DQInput.SetBindingString( param1, MAX_STRING_CHARS, command->GetArg(2), MAX_STRING_CHARS );
		}
		return TRUE;
	}
	if(DQWordstrcmpi( command->GetArg(0), "set", MAX_STRING_CHARS )==0) {
		//set cvarname "stringvalue"
		//set cvarname floatvalue

		DQWordstrcpy( param1, command->GetArg(1), MAX_STRING_CHARS );
		if(command->GetNumArgs()<2) Print("set cvarname \"stringvalue\"");
		else if(command->GetNumArgs()==2) {
			SetCVarString(param1, "", Authority);
		}
		else
			SetCVarString(param1, command->GetArg(2), Authority);
		return TRUE;
	}
	if(DQWordstrcmpi( command->GetArg(0), "seta", MAX_STRING_CHARS )==0) {
		//seta is similar to set, but uses CVAR_ARCHIVE

		DQWordstrcpy( param1, command->GetArg(1), MAX_STRING_CHARS );
		if(command->GetNumArgs()<2) Print("seta cvarname \"stringvalue\"");
		else if(command->GetNumArgs()==2) {
			RegisterCVar(param1, "", CVAR_ARCHIVE, Authority);
		}
		else
			RegisterCVar(param1, command->GetArg(2), CVAR_ARCHIVE, Authority);
		return TRUE;
	}
	if(DQWordstrcmpi( command->GetArg(0), "unbindall", MAX_STRING_CHARS )==0) {
		DQInput.UnbindAll();
		return TRUE;
	}
	if(DQWordstrcmpi( command->GetArg(0), "quit", MAX_STRING_CHARS )==0) {
		DQConsole.WriteConfigFile();
		PostQuitMessage(0);
		return TRUE;
	}
	if(DQWordstrcmpi( command->GetArg(0), "vid_restart", MAX_STRING_CHARS )==0) {
		DQRender.ToggleVidRestart = TRUE;
		return TRUE;
	}
	if(DQWordstrcmpi( command->GetArg(0), "localservers", MAX_STRING_CHARS )==0) {
		DQNetwork.RefreshLocalServers();
		return TRUE;
	}
	if(DQWordstrcmpi( command->GetArg(0), "ping", MAX_STRING_CHARS )==0) {
		//ignore
		return TRUE;
	}
	if(DQWordstrcmpi( command->GetArg(0), "connect", MAX_STRING_CHARS )==0) {
		if(command->GetNumArgs()<2) Print("connect <server address>");
		else {
			//connect to a server
			if(DQNetwork.isClientLoaded || DQNetwork.isServerLoaded) {
				DQNetwork.Disconnect();
				ExecuteCommandString( "wait", MAX_STRING_CHARS, FALSE );		//execute connect command in next frame, after vidrestart. Don't push to front as command will be popped straight away.
				ExecuteCommandString( command->GetArg(0), MAX_STRING_CHARS, FALSE );
			}
			else {
				DQWordstrcpy( param1, command->GetArg(1), MAX_STRING_CHARS );
				ConnectToServer(param1, MAX_STRING_CHARS);
			}
		}
		return TRUE;
	}
	if(DQWordstrcmpi( command->GetArg(0), "disconnect", MAX_STRING_CHARS )==0) {
		Disconnect();
		return TRUE;
	}
	if(DQWordstrcmpi( command->GetArg(0), "toggleconsole", MAX_STRING_CHARS )==0) {
		if(IsInConsole())	ShowConsole(eShowConsoleNone);
		else				ShowConsole(eShowConsoleHalf);
		return TRUE;
	}
	if(DQWordstrcmpi( command->GetArg(0), "showstats", MAX_STRING_CHARS )==0) {
		DQTime.PrintProfileTimes();
		return TRUE;
	}
	if(DQWordstrcmpi( command->GetArg(0), "RenderDeviceInfo", MAX_STRING_CHARS )==0) {
		DQPrint( DQRender.GetDeviceInfo() );
		return TRUE;
	}
	if(DQWordstrcmpi( command->GetArg(0), "map", MAX_STRING_CHARS )==0) {
		if(command->GetNumArgs()<2) Print( "map <mapname>" );
		else {
			if( DQClientGame.isLoaded || DQGame.isLoaded) {
				Disconnect();
				ExecuteCommandString( "wait", MAX_STRING_CHARS, FALSE );		//execute map command in next frame, after vidrestart. Don't push to front as command will be popped straight away.
				ExecuteCommandString( command->GetArg(0), MAX_STRING_CHARS, FALSE );
			}
			else {
				//Check if map exists
				DQWordstrcpy( param1, command->GetArg(1), MAX_STRING_CHARS );
				i = DQFS.OpenFile( va("maps/%s.bsp",param1), "rb" );
				if( !i ) {
					Error(ERR_FS, va("Cannot find map %s",param1) );
					return TRUE;
				}

				DQFS.CloseFile(i);

				//Start Server
				SetCVarString("mapname", param1, eAuthServer);
				DQGame.Load();
				//Connect client
				if(!IsDedicatedServer() ) {
					ConnectToServer( "localhost", 20 );
				}
			}
		}
		return TRUE;
	}
	if(DQWordstrcmpi( command->GetArg(0), "spmap", MAX_STRING_CHARS )==0) {
		DQPrint( "^1Single player not supported. Bots are not implemented in DXQuake3" );
		return TRUE;
	}
	if(DQWordstrcmpi( command->GetArg(0), "print", MAX_STRING_CHARS )==0) {
		if(command->GetNumArgs()<2) Print( "print <text>" );
		else {
			Print( command->GetArg(1) );
		}
		return TRUE;
	}
	if(DQWordstrcmpi( command->GetArg(0), "ClientConnectionInfo", MAX_STRING_CHARS )==0) {
		DQNetwork.ClientPrintConnectionStatus();
		return TRUE;
	}
	if(DQWordstrcmpi( command->GetArg(0), "ServerConnectionInfo", MAX_STRING_CHARS )==0) {
		if( command->GetNumArgs()<2 ) Print( "ServerConnectionInfo <ClientNum>" );
		else {
			DQNetwork.ServerPrintConnectionStatus( atoi(command->GetArg(1)) );
		}
		return TRUE;
	}


	DQWordstrcpy( param1, command->GetArg(0), MAX_STRING_CHARS );
	i = FindCVarFromName( param1 );
	if(i!=-1) {
		if(command->GetNumArgs()>1) {
			SetCVarString( param1, command->GetArg(1), Authority );
		}
		Print( va("\"%s\" is : \"%s\" default : \"%s\"",param1,pCVarArray[i]->string,pCVarArray[i]->resetString) );
		return TRUE;
	}

	//not found
	return FALSE;
}
/*
void c_Console::ExecuteString( const char *buffer, int MaxLength, eAuthority Authority )
{
	char word[MAX_STRING_CHARS],param1[MAX_STRING_CHARS],param2[MAX_STRING_CHARS];
	int param1Length, param2Length;
	int pos,posNextLine,pos2,i;
	char *NextCommand;

	NextCommand = NULL;		//this is used if several commands are strung together with ;

	pos = DQSkipWhitespace(buffer, MaxLength);
	posNextLine = pos + DQNextLine(&buffer[pos], MaxLength-pos);
	pos2 = pos + DQWordstrcpy(word, &buffer[pos], posNextLine+1);
	while(pos2>pos && pos<MaxLength) {
		if(word[0] == '\\') DQstrcpy( word, &word[1], MaxLength );	// get rid of preceding \ 

		param1[0] = param2[0] = '\0';

		//Skip to first param, save it to param1
		pos = pos2 + DQSkipWhitespace(&buffer[pos2], posNextLine);
		if(buffer[pos]==';') {
			//There is another command after this
			NextCommand = (char*)&buffer[pos+1];
		}
		else if(pos<posNextLine) {
			param1Length = min(posNextLine-pos,MAX_STRING_CHARS);
			if(buffer[pos]=='"') {
				param1Length = DQCopyUntil( param1, &buffer[pos+1], '"', MAX_STRING_CHARS );
				//Recalc posNextLine as we can have \n inside ""
				pos2 = pos + param1Length+1;
				posNextLine = pos2 + DQNextLine(&buffer[pos2], MaxLength-pos2);
			}
			else {
				param1Length = DQWordstrcpy(param1, &buffer[pos], param1Length+1 )+1;
				pos2 = pos + param1Length;
			}

			//copy rest of line to param2
			pos = pos2 + DQSkipWhitespace(&buffer[pos2], posNextLine);
			if(buffer[pos]==';') {
				//There is another command after this
				NextCommand = (char*)&buffer[pos+1];
			}
			else if(pos<posNextLine) {
				param2Length = min(posNextLine-pos,MAX_STRING_CHARS);
				param2Length = DQCopyUntil( param2, &buffer[pos], ';', param2Length );
				pos2 = pos + param2Length;
				DQStripQuotes(param2, param2Length+1);
			}
		}
		pos = pos2 + DQSkipWhitespace(&buffer[pos2], posNextLine);
		if(buffer[pos]==';') {
			NextCommand = (char*)&buffer[pos+1];
		}

		if(DQstrcmpi(word, "", 5)==0) {
			//Null string, do nothing
		}
		else if(DQstrcmpi(word, "bind", 5)==0) {
			//bind key "bindstring"

			if(!param1[0]) Print("bind key \"bindstring\"");
			else 
				DQInput.SetBindingString( param1, param1Length+1, param2, param2Length+1 );
		}
		//set is similar to seta
		else if(DQstrcmpi(word, "set", 5)==0) {
			//set cvarname "stringvalue"
			//set cvarname floatvalue

			if(!param1[0]) Print("set cvarname \"stringvalue\"");
			else
				RegisterCVar(param1, param2, 0, Authority);
		}
		//seta is similar to set, but uses CVAR_ARCHIVE
		else if(DQstrcmpi(word, "seta", 5)==0) {
			//seta cvarname "stringvalue"
			//seta cvarname floatvalue

			if(!param1[0] ) Print("seta cvarname \"stringvalue\"");
			else
				RegisterCVar(param1, param2, CVAR_ARCHIVE, Authority);
		}
		else if(DQstrcmpi(word, "unbindall", 10)==0) {
			DQInput.UnbindAll();
		}
		else if(DQstrcmpi(word, "quit", 10)==0) {
			PostQuitMessage(0);
		}
		else if(DQstrcmpi(word, "vidrestart", 20)==0) {
			DQRender.ToggleVidRestart = TRUE;
		}
		else if(DQstrcmpi(word, "localservers", 20)==0) {
			DQNetwork.RefreshLocalServers();
		}
		else if(DQstrcmpi(word, "ping", 20)==0) {
			//ignore
		}
		else if(DQstrcmpi(word, "connect", 20)==0) {
			if(!param1[0]) Print("connect <server address>");
			else {
				//connect to a server
				Disconnect();
				ConnectToServer(param1, param1Length);
			}
		}
		else if(DQstrcmpi(word, "disconnect", 20)==0) {
			Disconnect();
		}
		else if(DQstrcmpi(word, "toggleconsole", 20)==0) {
			if(IsInConsole())	ShowConsole(eShowConsoleNone);
			else				ShowConsole(eShowConsoleHalf);
		}
		else if(DQstrcmpi(word, "wait", 20)==0) {
			//TODO
		}
		else if(DQstrcmpi(word, "map", 20)==0) {
			if(!param1[0]) Print( "map <mapname>" );
			else {
				//Start Server
				RegisterCVar("mapname", param1, CVAR_SERVERINFO, eAuthServer);
				DQGame.Load();
				//Connect client
				ConnectToServer( "localhost", 20 );
			}
		}
		else if(DQstrcmpi(word, "print", 20)==0) {
			if(!param1[0]) Print( "print <text>" );
			else {
				Print( param1 );
			}
		}
		else {
			i = FindCVarFromName( word );
			if(i==-1) {
				Error(ERR_CONSOLE, va("Unknown command or cvar : %s",word) );
			}
			else {
				if(param1[0]) {
					SetCVarString( word, param1, Authority );
				}
				Print( va("\"%s\" is : \"%s\" default : \"%s\"",word,pCVarArray[i]->string,pCVarArray[i]->resetString) );
			}
		}
		pos = posNextLine;
		posNextLine = pos + DQNextLine(&buffer[pos], MaxLength-pos);
		pos2 = pos + DQWordstrcpy(word, &buffer[pos], posNextLine+1);
		//Execute any further commands nested in this string
		if(NextCommand) ExecuteString( NextCommand, MaxLength, Authority );
	}
}*/

void c_Console::Reload()
{
	for(int i=0;i<NumCVars;++i) {
		if(pCVarArray[i]->flags & CVAR_LATCH) {
			DQstrcpy( pCVarArray[i]->string, pCVarArray[i]->latchedString, MAX_CVAR_VALUE_STRING );
		}
	}

	OverbrightBits = GetCVarInteger( "r_overBrightBits", eAuthClient );
	if(!DQRender.IsFullscreen()) OverbrightBits = 0;

	UpdateCVar( &isDedicated );
	UpdateCVar( &sv_cheats );
}

//The value supplied is set as the default value
void c_Console::RegisterCVar( const char *var_name, const char *value, int flags, eAuthority Authority, vmCvar_t *vmCVar )
{
	if(NumCVars>=MAX_CVARS) {
		Error(ERR_CVAR, "Too many CVars");
		return;
	}

	int i;
	s_CVar *CVar;

	i = FindCVarFromName(var_name);
	if( i != -1 ) {	//if cvar already exists
		CVar = pCVarArray[i];
		if(Authority>=CVar->Authority) {
			CVar->flags |= flags;
			DQstrcpy(CVar->resetString, value, MAX_CVAR_VALUE_STRING);
			CVar->Authority = Authority;
			CVar->modified = TRUE;
			if(flags & CVAR_LATCH) {
				DQstrcpy(CVar->latchedString, value, MAX_CVAR_VALUE_STRING);
			}
			else CVar->latchedString[0] = '\0';
		}
	}
	else {

		CVar = DQNew(s_CVar);
		pCVarArray[NumCVars] = CVar;

		//Alloc memory for strings in CVar
		CVar->name = (char*) DQNewVoid( char[DQstrlen(var_name, MAX_CVAR_NAME)+1] );
		CVar->string = (char*)DQNewVoid(char[MAX_CVAR_VALUE_STRING]);
		CVar->resetString = (char*)DQNewVoid(char[MAX_CVAR_VALUE_STRING]);
		CVar->latchedString = (char*)DQNewVoid(char[MAX_CVAR_VALUE_STRING]);

		//Copy parameters into CVar
		DQstrcpy(CVar->name, var_name, MAX_CVAR_NAME);
		DQstrcpy(CVar->string, value, MAX_CVAR_VALUE_STRING);
		DQstrcpy(CVar->resetString, value, MAX_CVAR_VALUE_STRING);
		CVar->flags = flags;
		CVar->Authority = Authority;

		CVar->integer = atoi(CVar->string);
		CVar->value = atof(CVar->string);
		if(flags & CVAR_LATCH) {
			DQstrcpy(CVar->latchedString, value, MAX_CVAR_VALUE_STRING);
		}
		else CVar->latchedString[0] = '\0';
		CVar->modificationCount = 0;
		CVar->modified = TRUE;
		i = NumCVars;
		++NumCVars;
	}

	//If given, fill the vmCVar struct
	if(vmCVar) {
		vmCVar->handle = i+1;				//Handle is 1+ArrayIndex
		vmCVar->integer = CVar->integer;
		vmCVar->value = CVar->value;
		vmCVar->modificationCount = CVar->modificationCount;
		DQstrcpy(vmCVar->string, CVar->string, MAX_CVAR_VALUE_STRING);
	}
}

void c_Console::UpdateCVar(vmCvar_t *vmCVar ) {
	if(!vmCVar) {
		Error(ERR_CVAR, "UpdateCVar was passed NULL"); 
		return;
	}
	if(!vmCVar->handle) { 
//		DQPrint("UpdateCVar : Null Handle given\n"); 
		return; 
	}
	//Get the CVar from the handle
	int index = vmCVar->handle-1;
	if(index<0 || index>NumCVars) {
		Error(ERR_MISC, "Invalid CVar Handle\n");
		return;
	}
	vmCVar->integer = pCVarArray[index]->integer;
	vmCVar->value = pCVarArray[index]->value;
	vmCVar->modificationCount = pCVarArray[index]->modificationCount;
	DQstrcpy(vmCVar->string, pCVarArray[index]->string, MAX_CVAR_VALUE_STRING);
}

int c_Console::FindCVarFromName(const char *Name)
{
	static cachedCVars[10] = { 0,0,0,0,0,0,0,0,0,0 };
	static int cacheNum = 0;
	int i,j;

	if(NumCVars>0) {
		//check cache first
		for(i=0;i<10;++i) {
			j = cachedCVars[i];
			if(!DQstrcmpi(Name, pCVarArray[j]->name, MAX_CVAR_VALUE_STRING)) {
				//update cache
				cachedCVars[cacheNum++] = j;
				if(cacheNum>=10) cacheNum = 0;
				return j;
			}
		}
	}

	for(i=0;i<NumCVars;++i) {
		if(!DQstrcmpi(Name, pCVarArray[i]->name, MAX_CVAR_VALUE_STRING)) {
			//update cache
			cachedCVars[cacheNum++] = i;
			if(cacheNum>=10) cacheNum = 0;
			return i;
		}
	}
	return -1;
}

void c_Console::GetCVar( const char *name, vmCvar_t *vmCVar )
{
	int index = FindCVarFromName( name );
	vmCVar->handle = 0;
	if(index==-1) return;
	vmCVar->handle = index+1;
	UpdateCVar( vmCVar );
}

void c_Console::SetCVarString( const char *name, const char *value, eAuthority Authority )
{
	int index = FindCVarFromName(name);
	if(index==-1) {
		RegisterCVar(name, value, NULL, Authority);
		index = NumCVars-1;
	}
	SetCVarString( index, value, Authority );
}

void c_Console::SetCVarString( const int index, const char *value, eAuthority Authority )
{
	if(DQstrcmpi(pCVarArray[index]->string, value, MAX_STRING_CHARS)==0) return;

	//Check flags
	if(pCVarArray[index]->flags&CVAR_ROM) {
		if(Authority<pCVarArray[index]->Authority) {
			Error(ERR_CVAR, va("Cannot modify Read-Only cvar %s - insufficient authority (%d)",pCVarArray[index]->name, Authority));
			return;
		}
	}
	if((pCVarArray[index]->flags & CVAR_INIT) && isLoaded) {
		Error(ERR_CVAR, "SetCVar : This CVar can only be set from the command line");
		return;
	}
	if(pCVarArray[index]->flags & CVAR_LATCH) {
		DQstrcpy(pCVarArray[index]->latchedString, value, MAX_CVAR_VALUE_STRING);
		return;
	}
/*	if(pCVarArray[index]->flags & CVAR_ROM && Authority<pCVarArray[index]->Authority) {
		Error(ERR_CVAR, va("Cannot modify Read-Only cvar %s - insufficient authority (%d)",pCVarArray[index]->name, Authority));
		return;
	}
*/	if((pCVarArray[index]->flags & CVAR_CHEAT) && !sv_cheats.integer) {
		Error(ERR_CVAR, va("%s cannot be changed without enabling sv_cheats",pCVarArray[index]->name));
		return;
	}

	DQstrcpy(pCVarArray[index]->string, value, MAX_CVAR_VALUE_STRING);

	pCVarArray[index]->integer = atoi(value);
	pCVarArray[index]->value = atof(value);
	++pCVarArray[index]->modificationCount;
	pCVarArray[index]->modified = TRUE;
}

void c_Console::SetCVarValue( const char *name, float value, eAuthority Authority )
{
	char string[MAX_CVAR_VALUE_STRING];
	sprintf(string, "%f",value);
	SetCVarString( name, string, Authority );
}

int c_Console::GetCVarInteger( const char *name, eAuthority Authority )
{
	int index = FindCVarFromName(name);
	if(index==-1) {
		RegisterCVar(name, "", NULL, Authority);
//		Error(ERR_CVAR, va("GetCVarInteger : CVar %s not found",name));
		return 0;
	}
	//else
	return pCVarArray[index]->integer;
}

float c_Console::GetCVarFloat( const char *name, eAuthority Authority )
{
	int index = FindCVarFromName(name);
	if(index==-1) {
		RegisterCVar(name, "", NULL, Authority);
//		Error(ERR_CVAR, va("GetCVarFloat : CVar %s not found",name));
		return 0;
	}
	//else
	return pCVarArray[index]->value;
}


void c_Console::GetCVarString( const char *name, char *buffer, int MaxLength, eAuthority Authority)
{
	int index = FindCVarFromName(name);

	if(!buffer) {
		Error(ERR_CVAR, "GetCVarString : buffer pointer is NULL");
		return;
	}

	if(index==-1) {
		//Error(ERR_CVAR, va("GetCVarString : CVar %s not found",name));
		RegisterCVar(name, "", NULL, Authority);
		buffer[0] = '\0';
		return;
	}

	DQstrcpy(buffer, pCVarArray[index]->string, MaxLength);
}

void c_Console::ResetCVar( const char *name, eAuthority Authority )
{
	int index = FindCVarFromName(name);
	if(index==-1) {
		Error(ERR_CVAR, va("ResetCVar : CVar %s not found",name));
		return;
	}
	if(pCVarArray[index]->flags & CVAR_ROM && Authority<pCVarArray[index]->Authority) {
		Error(ERR_CVAR, va("Cannot modify Read-Only cvar %s - insufficient authority (%d)",pCVarArray[index]->name, Authority));
		return;
	}

	DQstrcpy(pCVarArray[index]->string, pCVarArray[index]->resetString, MAX_CVAR_VALUE_STRING);

	pCVarArray[index]->integer = atoi(pCVarArray[index]->string);
	pCVarArray[index]->value = atof(pCVarArray[index]->string);
	pCVarArray[index]->modificationCount = 0;
	pCVarArray[index]->modified = TRUE;
}

//Return the ServerInfo string containing all the fundamental server cvars (denoted by CVAR_SERVERINFO)
void c_Console::GetServerInfo(char *buffer, int bufsize)
{
	buffer[0]='\0';

	for(int i=0;i<NumCVars;++i) {
		if(pCVarArray[i]->flags&CVAR_SERVERINFO) {
			bufsize -= DQstrcat(buffer, va("\\%s",pCVarArray[i]->name),bufsize);
			bufsize -= DQstrcat(buffer, va("\\%s",pCVarArray[i]->string),bufsize);
		}
		if(bufsize<=0) {
			Error(ERR_CVAR, "ServerInfo string too big");
			return;
		}
	}
}

void c_Console::AddTabCommand(const char *command)
{
	int i,len;

	if(NumTabCommands>=MAX_TABCOMMANDS) {
		Error(ERR_CONSOLE, "Too many Tab-Completion Commands");
		return;
	}
	//check to see if TabCommand is already added
	for(i=0;i<NumTabCommands;++i) {
		if(DQstrcmpi( command, TabCommands[i], MAX_CONSOLELINELENGTH )==0)
			return;
	}

	len = DQstrlen(command, MAX_STRING_CHARS);

	TabCommands[NumTabCommands] = (char*)DQNewVoid( char[len+1] );
	DQstrcpy(TabCommands[NumTabCommands], command, MAX_CONSOLELINELENGTH);
	++NumTabCommands;
}

//Tab complete the current line
void c_Console::TabComplete()
{
	char InputWord[MAX_CONSOLELINELENGTH];
	char FoundWord[MAX_CONSOLELINELENGTH];
	char *pStrCmp;
	int i;
	BOOL bMultipleWords;

	if(CurrentInputPos==0) return;

	if(ConsoleTextCurrentInput[0]=='\\') {
		DQWordstrcpy( InputWord, &ConsoleTextCurrentInput[1], MAX_CONSOLELINELENGTH );
	}
	else {
		DQWordstrcpy( InputWord, ConsoleTextCurrentInput, MAX_CONSOLELINELENGTH );
	}
	FoundWord[0] = '\0';
	bMultipleWords = FALSE;

	//Iterate through TabCommands then CVars
	for(i=0;i<NumTabCommands+NumCVars;++i) {

		if(i<NumTabCommands) pStrCmp = TabCommands[i];
		else pStrCmp = pCVarArray[i-NumTabCommands]->name;

		if(DQPartialstrcmpi( InputWord, pStrCmp, MAX_CONSOLELINELENGTH )==0) {
			if(FoundWord[0]) {

				//Multiple possibles found
				if(!bMultipleWords) {				//if this is the 2nd word
					bMultipleWords = TRUE;
					Print( va("]%s",ConsoleTextCurrentInput) );
					Print( va("    %s",FoundWord) );
				}
				//Use FoundWord to create the largest matching word fragment possible
				DQstrTruncate( FoundWord, pStrCmp, MAX_CONSOLELINELENGTH );

				Print( va("    %s",pStrCmp) );
			}
			else {		//1st word found
				DQstrcpy( FoundWord, pStrCmp, MAX_CONSOLELINELENGTH );
			}
		}
	}
	if(FoundWord) {
		DQstrcpy( &ConsoleTextCurrentInput[1], FoundWord, MAX_CONSOLELINELENGTH );
		ConsoleTextCurrentInput[0] = '\\';
		ConsoleCaratPos = CurrentInputPos = DQstrlen( ConsoleTextCurrentInput, MAX_CONSOLELINELENGTH );
	}
}

void c_Console::Update()
{
	if( UpdateUserInfo() && DQNetwork.IsClientConnected()) {
		if(! DQNetwork.ClientSendUserInfo(UserInfoString, UserInfoStringSize) ) {
			//disconnected from server
			Disconnect();
		}
	}

	ExecuteCommandsNow();

	RenderConsole();
}

void c_Console::ExecuteCommandsNow()
{
	c_Command *command;
	BOOL isProcessed;

	//Execute waiting commands
	/*Sequence :
		CLIENT CONSOLE
		Engine Commands
		CVars
		UI Console Command
		if( DQClientGame.isLoaded ) ClientGame Console Command
		* if( DQGame.isLoaded ) Server Console Commmand
		* if( isClient ) Send to Server

		SERVER CONSOLE
		if( DQGame.isLoaded ) Server Console Commmand

	  If the string is not recognised, the Game DLL sends an error message
	*/

	//CLIENT CONSOLE
	command = ClientCommandBuffer.GetFirstUnit();
	while(command) {
		isProcessed = FALSE;

		//The command 'wait' pauses all command processing until the next frame
		// so we have to process it here
		if(DQWordstrcmpi( command->GetArg(0), "wait", MAX_STRING_CHARS )==0) {
			ClientCommandBuffer.PopFromFront();
			break;
		}

		//Engine commands and cvars
		isProcessed = ExecuteCommand( command, (IsDedicatedServer())?eAuthServer:eAuthClient );

		//UI Console Command
		if( !isProcessed ) isProcessed = DQUI.ExecuteCommand( command );

		//Client Console Command
		if(DQClientGame.isLoaded) {
			if( !isProcessed ) isProcessed = DQClientGame.ExecuteCommand( command );
		}

		//Server Console Command if we're running the Game DLL
		if(DQGame.isLoaded) {
			if( !isProcessed ) isProcessed = DQGame.ExecuteConsoleCommand( command );
		}

		//Server Client-Command
		if( DQClientGame.isLoaded ) {
			//Send to server to process
			if( !isProcessed ) {
				if( !DQNetwork.ClientSendCommandToServer( command->GetArg(0) ) ) {
					//disconnected from server
					Disconnect();
				}
			}
		}
		else {
			//Failed
			if( !isProcessed ) Print( va("CVar or Command \"%s\" not found",command->GetArg(0) ) );
		}

		ClientCommandBuffer.PopFromFront();
		command = ClientCommandBuffer.GetFirstUnit();
	};

	//SERVER CONSOLE
	if(!DQGame.isLoaded) return;
	command = ServerCommandBuffer.GetFirstUnit();
	while(command) {

		if( !DQGame.ExecuteConsoleCommand( command ) ) {
			//Failed
			Print( va("CVar or Command \"%s\" not found",command->GetArg(0) ) );
		}
		ServerCommandBuffer.PopFromFront();
		command = ServerCommandBuffer.GetFirstUnit();
	};
}

//Actually just adds it to the command queue
void c_Console::ExecuteCommandString( const char *string, int MaxLength, BOOL PushToFront )
{
	const int MAX_CONCATENATED_COMMANDS = 100;
	c_Command	*command;
	c_Command	ReverseOrderCommands[MAX_CONCATENATED_COMMANDS];
	int			i,numCommands;
	char		*NextCommand;
	
	numCommands = 0;
	NextCommand = (char*)string;

	if(PushToFront) {
		for(; numCommands<MAX_CONCATENATED_COMMANDS && NextCommand; ++numCommands ) {
			NextCommand = ReverseOrderCommands[numCommands].MakeCommandString( NextCommand, MaxLength );
			++numCommands;
		}
		
		//Push the commands to the front of the buffer
		for(i=0;i<numCommands;++i) {
			memcpy( ClientCommandBuffer.PushToFront(), &ReverseOrderCommands[numCommands-i], sizeof(c_Command) );
		}
	}
	else {
		while(NextCommand) {
			command = ClientCommandBuffer.Push();
			NextCommand = command->MakeCommandString( NextCommand, MaxLength );
		}
	}
}

void c_Console::AddServerConsoleCommand( const char *string )
{
	c_Command *command;
	const char *NextCommand;

	NextCommand = string;
	while(NextCommand) {
		command = ServerCommandBuffer.Push();
		NextCommand = command->MakeCommandString( NextCommand, MAX_STRING_CHARS );
	}
}

//Called once per frame, this updates the UserInfo string for this client
//Returns true if modified from last call
BOOL c_Console::UpdateUserInfo()
{
	int &pos = UserInfoStringSize;
	BOOL Modified = FALSE;

	pos = 0;

	for(int i=0;i<NumCVars;++i) {
		if(pCVarArray[i]->flags&CVAR_USERINFO) {
			if(pCVarArray[i]->modified) {
				pCVarArray[i]->modified = FALSE;
				Modified = TRUE;
			}
			pos += DQstrcpy(&UserInfoString[pos], va("\\%s",pCVarArray[i]->name),MAX_INFO_STRING-pos);
			pos += DQstrcpy(&UserInfoString[pos], va("\\%s",pCVarArray[i]->string),MAX_INFO_STRING-pos);
		}
		if(pos>=MAX_INFO_STRING) {
			Error(ERR_CVAR, "UserInfo string too big");
			return TRUE;
		}
	}

	return Modified;
}

void c_Console::RenderConsole()
{
	float ConsoleYHeight;
	int i, TextYPos, Line;
	char ConsoleInputChar[2];
	char MoreBelowMarker[] = "v  v  v  v  v  v  v  v  v  v  v  v  v  v";
	float CurrentTime, Timer;
	static float CaratTimer = 0;
	static float CaratTimerOffset = 0;
	char Carat[2] = { 21, 0 };
	int &vidHeight = DQRender.GLconfig.vidHeight;
	int &vidWidth = DQRender.GLconfig.vidWidth;

	CurrentTime = DQTime.GetSecs();
	Timer = (CurrentTime - ShowConsoleTimer)*5.0f;		//slide in 0.2sec

	//Do this now, otherwise Q3's "white" shader will be registered with a ShaderFlag_ConsoleShader
	if(!DQRender.whiteimageShader) {
		DQRender.whiteimageShader = DQRender.RegisterShader( "white" , ShaderFlag_ConsoleShader);
	}

	if(Timer>=1.0f) {
		Timer = 1.0f;
		if(ShowConsoleState == eShowConsoleNone) {

			//Draw the last Print line at the top of the screen for 5 secs

			//Don't draw if talking
			if( !(DQInput.GetKeyCatcher() & KEYCATCH_MESSAGE) ) {
				if( CurrentTime - LastPrintTime < 5.0f ) {
					DQRender.SetSpriteColour( NULL );
					Line = max( 0, ConsoleTextNextLine - 1 );
					DQRender.DrawString( 0, 5, SMALLCHAR_WIDTH, SMALLCHAR_HEIGHT, ConsoleText[Line], MAX_CONSOLELINELENGTH );
				}
			}
			return;
		}

		//Console is fully out, so force key catcher (UI sometimes grabs it)
		DQInput.SetKeyCatcher( DQInput.GetKeyCatcher() | KEYCATCH_CONSOLE );
	}

	switch(ShowConsoleState) {
	case eShowConsoleNone:
		ConsoleYHeight = (1.0f-Timer)*vidHeight/2;
		break;
	case eShowConsoleHalf:
		ConsoleYHeight = Timer*vidHeight/2;
		break;
	case eShowConsoleFull:
		ConsoleYHeight = vidHeight;		//no slide
		break;
	default:
		return;
	};

	DQRender.SetSpriteColour( NULL );		//defaults to white

	//Render the console as a sprite, with identity View and Projection matricies
	DQRender.DrawSprite( 0, 0, vidWidth, ConsoleYHeight, 0,0, 1,1, ConsoleShader );

	//Draw a red line under the console
	float rgba[4];
	rgba[0] = 1.0f; rgba[1] = 0.0f; rgba[2] = 0.0f; rgba[3] = 1.0f; 
	DQRender.SetSpriteColour( rgba );

	DQRender.DrawSprite( 0, ConsoleYHeight, DQRender.GLconfig.vidWidth, DQRender.GLconfig.vidHeight/160, 0, 0, 0, 0, DQRender.whiteimageShader );

	//Add "More Below" markers (v v v v v v v)
	if(ConsoleLineDisplayOffset>0) {
		rgba[0] = 0.9f; rgba[1] = 0.9f; rgba[2] = 0.01f; rgba[3] = 1.0f; //Yellow
		DQRender.SetSpriteColour( rgba );
		DQRender.DrawString( SMALLCHAR_WIDTH*4, ConsoleYHeight - SMALLCHAR_HEIGHT-1, SMALLCHAR_WIDTH, SMALLCHAR_HEIGHT, MoreBelowMarker, 100);
	}

	//Draw Current Console Input
	//*********************
	DQRender.SetSpriteColour( NULL );
	TextYPos = ConsoleYHeight - 2*SMALLCHAR_HEIGHT;

	//Draw Current Input Text
	ConsoleInputChar[0] = ']';
	ConsoleInputChar[1] = '\0';
	DQRender.DrawString( 0, TextYPos, SMALLCHAR_WIDTH, SMALLCHAR_HEIGHT, ConsoleInputChar, 2 );
	DQRender.DrawString( SMALLCHAR_WIDTH, TextYPos, SMALLCHAR_WIDTH, SMALLCHAR_HEIGHT, ConsoleTextCurrentInput, MAX_CONSOLELINELENGTH );

	//Add flashing cursor
	CaratTimer = DQTime.GetSecs();
	if(ConsoleCaratTimerReset) {
		CaratTimerOffset = CaratTimer;
		ConsoleCaratTimerReset = FALSE;
	}
	if(! ((int)(CaratTimer-CaratTimerOffset) % 2) ) {
		DQRender.DrawString( (ConsoleCaratPos+1)*SMALLCHAR_WIDTH-1, TextYPos, SMALLCHAR_WIDTH, SMALLCHAR_HEIGHT, Carat, 2 );
	}

	//Draw Text
	//*********************
	Line = ConsoleTextNextLine-1-ConsoleLineDisplayOffset;
	TextYPos -= SMALLCHAR_HEIGHT;
	while(TextYPos>0) {
		if(Line<0) Line = MAX_CONSOLELINES-1;
		if(Line==ConsoleTextNextLine) break;			//Don't repeat text

		//Stop if there's no more text
		if(!ConsoleText[Line][0]) break;

		DQRender.DrawString( 0, TextYPos, SMALLCHAR_WIDTH, SMALLCHAR_HEIGHT, ConsoleText[Line], MAX_CONSOLELINELENGTH );
		DQRender.SetSpriteColour( NULL );

		TextYPos -= SMALLCHAR_HEIGHT;
		--Line;
	};

	//Add Signature to bottom right of console
	char Signature[100];
	DQstrcpy( Signature, "DXQuake3 v1.03 by R.Geary. Quake3 (c) ID Software", 100 );
	i = DQstrlen( Signature, 100 )+1;
	TextYPos = ConsoleYHeight - SMALLCHAR_HEIGHT - 5;

	rgba[0] = 1.0f; rgba[1] = 0.0f; rgba[2] = 0.0f; rgba[3] = 1.0f; 
	DQRender.SetSpriteColour( rgba );
	DQRender.DrawString( DQRender.GLconfig.vidWidth - SMALLCHAR_WIDTH*i, TextYPos, SMALLCHAR_WIDTH, SMALLCHAR_HEIGHT, Signature, MAX_CONSOLELINELENGTH );

	DQRender.SetSpriteColour( NULL );
}

//Run from WinProc on WM_CHAR message
void c_Console::AddInputChar( char ch )
{
	ConsoleCaratTimerReset = TRUE;		//always show the carat after a keypress
	int x;

	if(ch == 27 /*escape*/) return;
	if(ch == 8 /*backspace*/) {
		if(ConsoleCaratPos==0) return;
		--CurrentInputPos;
		--ConsoleCaratPos;
		DQstrcpy( &ConsoleTextCurrentInput[ConsoleCaratPos], &ConsoleTextCurrentInput[ConsoleCaratPos+1], MAX_CONSOLELINELENGTH-ConsoleCaratPos-1 );
		return;
	}
	if(ch == 13 /*Enter*/) {
		Print( va("]%s",ConsoleTextCurrentInput ) );
		ExecuteCommandString( ConsoleTextCurrentInput, MAX_CONSOLELINELENGTH, FALSE );
		
		x = DQSkipWhitespace( ConsoleTextCurrentInput, MAX_CONSOLELINELENGTH );
		if(ConsoleTextCurrentInput[x]!='\0') {
			DQstrcpy( ConsoleTextPreviousCommands[ConsoleRedoNextPos], ConsoleTextCurrentInput, MAX_CONSOLELINELENGTH );
			++ConsoleRedoNextPos;
			if(ConsoleRedoNextPos>=MAX_CONSOLEREDO) ConsoleRedoNextPos = 0;
		}

		ConsoleTextCurrentInput[0] = '\0';
		ConsoleCaratPos = 0;
		CurrentInputPos = 0;
		ConsoleRedoSelected = ConsoleRedoNextPos;
		ConsoleLineDisplayOffset = 0;
		return;
	}
	if(ch == '\t') {
		//Tab Completion
		TabComplete();
		return;
	}

	if(CurrentInputPos>=MAX_CONSOLELINELENGTH-2) return;

	for(x=CurrentInputPos; x>ConsoleCaratPos; --x) {
		ConsoleTextCurrentInput[x] = ConsoleTextCurrentInput[x-1];
	}
	++CurrentInputPos;
	ConsoleTextCurrentInput[CurrentInputPos] = '\0';
	ConsoleTextCurrentInput[ConsoleCaratPos] = ch;
	++ConsoleCaratPos;
}

void c_Console::InputKey( int KeyPressed )
{
	ConsoleCaratTimerReset = TRUE;
	int x;

	switch(KeyPressed) {
	case K_LEFTARROW:
		if(ConsoleCaratPos>0) --ConsoleCaratPos;
		return;
	case K_RIGHTARROW:
		if(ConsoleCaratPos<CurrentInputPos) ++ConsoleCaratPos;
		return;
	case K_HOME:
		ConsoleCaratPos = 0;
		return;
	case K_END:
		ConsoleCaratPos = CurrentInputPos;
		return;

	case K_UPARROW:
		//Previous Command
		x = ConsoleRedoSelected-1;
		if(x<0) x = MAX_CONSOLEREDO-1;
		if(ConsoleTextPreviousCommands[x][0]!='\0') {
			DQstrcpy( ConsoleTextCurrentInput, ConsoleTextPreviousCommands[x], MAX_CONSOLELINELENGTH );
			ConsoleCaratPos = CurrentInputPos = DQstrlen( ConsoleTextCurrentInput, MAX_CONSOLELINELENGTH );
			ConsoleRedoSelected = x;
		}
		return;
	case K_DOWNARROW:
		//Next Command
		if(ConsoleTextPreviousCommands[ConsoleRedoSelected][0]=='\0') return;	//do nothing if no further previous commands
		
		x = ConsoleRedoSelected+1;
		if(x>=MAX_CONSOLEREDO) x = 0;

		DQstrcpy( ConsoleTextCurrentInput, ConsoleTextPreviousCommands[x], MAX_CONSOLELINELENGTH );
		ConsoleCaratPos = CurrentInputPos = DQstrlen( ConsoleTextCurrentInput, MAX_CONSOLELINELENGTH );
		ConsoleRedoSelected = x;
		return;

	case K_DEL:
		if(ConsoleCaratPos==CurrentInputPos || CurrentInputPos==0) return;
		--CurrentInputPos;
		DQstrcpy( &ConsoleTextCurrentInput[ConsoleCaratPos], &ConsoleTextCurrentInput[ConsoleCaratPos+1], MAX_CONSOLELINELENGTH-ConsoleCaratPos-1 );
		return;

	case K_PGUP:
		if(ConsoleLineDisplayOffset>=MAX_CONSOLELINES || ConsoleLineDisplayOffset>=ConsoleTextNextLine) return;
		++ConsoleLineDisplayOffset;
		return;
	case K_PGDN:
		--ConsoleLineDisplayOffset;
		if(ConsoleLineDisplayOffset<0) ConsoleLineDisplayOffset = 0;
		return;

	default:
		//WM_CHAR messages intercepted instead
		break;
	};
}

void c_Console::ShowConsole( eShowConsole State )
{
	if(IsDedicatedServer()) {
		State = eShowConsoleFull;	//Force full console for Dedicated Server
	}

	if(State==ShowConsoleState) return;	//don't reset slide timer

	if(ShowConsoleState!=eShowConsoleFull && State!=eShowConsoleFull) {
		ShowConsoleTimer = DQTime.GetSecs();
	}
	else {
		ShowConsoleTimer = 0;	//no slide in/out
	}

	ShowConsoleState = State;
	if(State==eShowConsoleNone) DQInput.SetKeyCatcher( DQInput.GetKeyCatcher() & ~KEYCATCH_CONSOLE );
	else						DQInput.SetKeyCatcher( DQInput.GetKeyCatcher() | KEYCATCH_CONSOLE );

	if(!DQRender.IsFullscreen()) {
		if(State==eShowConsoleNone) {
			DQInput.SetCaptureMouse( TRUE );
		}
		else {
			DQInput.SetCaptureMouse( FALSE );
		}
	}
}

void c_Console::RunLocalServer( BOOL RunLocalServer )
{
	LocalServer = RunLocalServer;
	SetCVarString("sv_running", (LocalServer)?"1":"0", eAuthServer);	
}

void c_Console::Disconnect()
{
	//Stop UI displaying connection screen
	DQUI.isConnecting = FALSE;

	//Disconnect any network connection (and send Disconnect Client message to server)
	DQNetwork.Disconnect();

	//Disconnect from current game
	DQClientGame.Unload();

	//Close any running server
	DQGame.Unload();

	//Reload UI (so we can clear the RenderStack)
	DQUI.Unload();

	DQRender.ToggleVidRestart = TRUE;

	DQRenderStack.ResetState();

	DQUI.Load();
}

void c_Console::ConnectToServer(char *ServerName, int MaxLength)
{
	if(IsDedicatedServer()) return;

	ShowConsole( eShowConsoleNone );

	//Set hostname cvar
	SetCVarString( "sv_hostname", ServerName, eAuthServer );

	//Display Connection Screen
	DQUI.Connect();

	if(DQstrcmpi(ServerName, "localhost", MaxLength)==0) {
		//connect to local server
		RunLocalServer( TRUE );
	}
	else {
		//connect to remote server
		RunLocalServer( FALSE );
	}
	
	//Connect client to server
	DQNetwork.ConnectToServer(ServerName);
}

void c_Console::WriteConfigFile()
{
	int Q3cfg, i;
	char *pText, buffer[MAX_STRING_CHARS], buffer2[MAX_STRING_CHARS];

	Q3cfg = DQFS.OpenFile("q3config.cfg", "w+b");
	if( !Q3cfg ) {
		DQPrint( "Could not write to q3config.cfg" );
		return;
	}

	pText = va("// Generated by DXQuake3 \n"), 
	DQFS.WriteFile( pText, 1, DQstrlen(pText, MAX_STRING_CHARS), Q3cfg );

	pText = va("unbindall\n"), 
	DQFS.WriteFile( pText, 1, DQstrlen(pText, MAX_STRING_CHARS), Q3cfg );

	//write bind strings
	for(i=0;i<256;++i) {
		DQInput.GetBindingString( i, buffer, MAX_STRING_CHARS );
		if(!buffer[0]) continue;
		DQInput.GetKeynumString( i, buffer2, MAX_STRING_CHARS );
		//For compatibility with Quake3, make a to z in lower case, all else in upper case
		if(i>=97 && i<=122) strlwr( buffer2 );
		else strupr( buffer2 );
		pText = va("bind %s \"%s\"\n",buffer2,buffer);
		DQFS.WriteFile( pText, 1, DQstrlen(pText, MAX_STRING_CHARS), Q3cfg );
	}

	//write archived cvars
	for(i=0; i<NumCVars; ++i ) {
		if( pCVarArray[i]->flags & CVAR_ARCHIVE ) {
			pText = va( "seta %s \"%s\"\n",pCVarArray[i]->name,pCVarArray[i]->string );
			DQFS.WriteFile( pText, 1, DQstrlen(pText, MAX_STRING_CHARS), Q3cfg );
		}
	}

	DQFS.CloseFile( Q3cfg );
}

void c_Console::ProcessCommandLine( char *lpCmdLine )
{
	int pos;
	char CommandString[MAX_STRING_CHARS];

	if( !lpCmdLine || !lpCmdLine[0] ) return;
	
	pos = 0;
	do {
		pos += DQSkipWhitespace( &lpCmdLine[pos], MAX_STRING_CHARS );
		if( !lpCmdLine[pos] ) break;

		if(lpCmdLine[pos]=='+') {
			++pos;

			//console command
			pos += DQCopyUntil( CommandString, &lpCmdLine[pos], '+', MAX_STRING_CHARS );
			if( pos>0 && lpCmdLine[pos-1]=='+' ) --pos;
			ExecuteCommandString( CommandString, MAX_STRING_CHARS, FALSE );
		}
	}while(lpCmdLine[pos]);

	ExecuteCommandsNow();
	Reload();		//Load Latched vars like dedicated
}