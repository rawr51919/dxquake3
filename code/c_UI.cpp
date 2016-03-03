// DXQuake3 Source
// Copyright (c) 2003 Richard Geary (richard.geary@dial.pipex.com)
//
// c_UI.cpp: implementation of the c_UI class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "c_UI.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

static int QDECL Callback_UI(int command, ...);

c_UI::c_UI()
{
	CurrentCommand = NULL;
	vmMain = NULL;
	hDLL = NULL;
}

c_UI::~c_UI()
{
	Unload();

	FreeLibrary(hDLL);
	hDLL = NULL;
}

void c_UI::Load() 
{
	char Path[MAX_OSPATH];
	
	hDLL = NULL;
	vmMain = NULL;
	dllEntry = NULL;
	isConnecting = FALSE;

	//No UI if dedicated server
	if(DQConsole.IsDedicatedServer()) {
		return;
	}

	if(DQConsole.IsLocalServer()) 
		eAuthUI = eAuthServer;
	else
		eAuthUI = eAuthClient;

//	DQstrcpy( Path, "F:\\Quake III Arena\\TestMod\\uix86.dll", MAX_PATH);
//	DQstrcpy( Path, "H:\\uix86.dll", MAX_PATH);
	DQFS.GetDLLPath("uix86.dll", Path);

	if(!Path[0]) {
		CriticalError(ERR_FS, "Unable to load UI DLL (uix86.dll)");
	}
	hDLL = LoadLibrary(Path);

	if(!hDLL) {
		CriticalError(ERR_FS, "Unable to load UI DLL (uix86.dll)");
		return;
	}

	vmMain = (t_vmMain)GetProcAddress((HMODULE)hDLL, "vmMain");
	dllEntry = (t_dllEntry)GetProcAddress((HMODULE)hDLL, "dllEntry");

	//Set up callback func
	dllEntry( Callback_UI );
	
	Initialise();

	DQPrint( "UI Loaded" );
}

void c_UI::Initialise()
{
	if(!vmMain) return;

	//Initialise UI
	vmMain(UI_INIT,0,0,0,0,0,0,0,0,0,0,0,0);
	vmMain(UI_SET_ACTIVE_MENU, UIMENU_NONE, 0,0,0,0,0,0,0,0,0,0,0);
}

void c_UI::OpenMenu()
{
	if(!vmMain) return;
	if(DQConsole.IsDedicatedServer()) return;

	if(DQClientGame.isLoaded || DQClientGame.LoadMe) {
		vmMain(UI_SET_ACTIVE_MENU, UIMENU_INGAME, 0,0,0,0,0,0,0,0,0,0,0);
	}
	else {
		//Activate Main Menu
		vmMain(UI_SET_ACTIVE_MENU, UIMENU_MAIN, 0,0,0,0,0,0,0,0,0,0,0);
	}

	isConnecting = FALSE;
}

void c_UI::Connect()
{
	if(!vmMain) return;
	vmMain(UI_SET_ACTIVE_MENU, UIMENU_NONE, 0,0,0,0,0,0,0,0,0,0,0);
	vmMain(UI_REFRESH, DQTime.GetMillisecs(), 0,0,0,0,0,0,0,0,0,0,0);
	vmMain(UI_DRAW_CONNECT_SCREEN, FALSE, 0,0,0,0,0,0,0,0,0,0,0);

	isConnecting = TRUE;
}

void c_UI::Unload()
{
	if(vmMain) {
		vmMain(UI_SHUTDOWN,0,0,0,0,0,0,0,0,0,0,0,0);
	}
}

void c_UI::Update() {
	int i, NumKeyBuffer;
	c_Input::s_KeyBuffer KeyBuffer[MAX_KEYBUFFER];

	if(!vmMain) return;

	if(DQClientGame.isLoaded) isConnecting = FALSE;

	vmMain(UI_MOUSE_EVENT, DQInput.GetMouse_dx(), DQInput.GetMouse_dy(), 0,0,0,0,0,0,0,0,0,0);
	
	NumKeyBuffer = DQInput.GetKeyBuffer( KeyBuffer );
	for(i=0;i<NumKeyBuffer;++i) {
 		vmMain(UI_KEY_EVENT, KeyBuffer[i].key, KeyBuffer[i].isDown, 0,0,0,0,0,0,0,0,0,0);
	}

	if(!DQClientGame.isLoaded && !DQClientGame.LoadMe && !isConnecting) {
		if(!vmMain(UI_IS_FULLSCREEN, 0,0,0,0,0,0,0,0,0,0,0,0))	//if menu not visible
			DQUI.OpenMenu();	//Goto Main Menu
	}

	if(isConnecting) {
		if(DQNetwork.IsClientConnected()) {
			//Draw overlay
			vmMain(UI_DRAW_CONNECT_SCREEN, TRUE, 0,0,0,0,0,0,0,0,0,0,0);
		}
		else {
			vmMain(UI_DRAW_CONNECT_SCREEN, FALSE, 0,0,0,0,0,0,0,0,0,0,0);
		}
	}

	vmMain(UI_REFRESH, DQTime.GetMillisecs(), 0,0,0,0,0,0,0,0,0,0,0);
}

BOOL c_UI::ExecuteCommand( c_Command *command )
{
	if(!vmMain) return FALSE;

	CurrentCommand = command;
	return vmMain(UI_CONSOLE_COMMAND, DQTime.GetMillisecs(), 0,0,0,0,0,0,0,0,0,0,0);
}

//*****************************************************************************
int PASSFLOAT( float x ) {
	float	floatTemp;
	floatTemp = x;
	return *(int *)&floatTemp;
}

//This function is a switchboard for function calls from the UI DLL
int QDECL Callback_UI(int command, ...)
{
	vmCvar_t *vmCvar;
	char *name;
	char *value;
	int flags, iTemp1, iTemp2, iTemp3;
	char *buffer, *buffer2;
	int bufsize;
	int length;
	char *path, *extention;
	int *pInt;
	int FileHandle;
	fsMode_t FileMode;
	float fTemp1, fTemp2, fTemp3, fTemp4;
	float *rgba, *pFloat;
	glconfig_t *pGLconfig;
	float s1,t1,s2,t2;
	int hShader;
	refEntity_t *refEntity;
	refdef_t *refdef;
	uiClientState_t *ClientState;
	orientation_t *tag;
	c_Model *Model;
	c_Command *pCommand;
	eAuthority eAuthUI = DQUI.eAuthUI;

	va_list ArgList;
	va_start(ArgList, command);

	switch(command) {
	case UI_ERROR:
		Error(ERR_UI, va_arg(ArgList, const char *)); break;
	case UI_PRINT:
		DQPrint(va_arg(ArgList, const char *)); break;
	case UI_MILLISECONDS:
		va_end(ArgList);
		return DQTime.GetMillisecs();

	//
	// CVar and Console commands
	//

	case UI_CVAR_REGISTER:
		vmCvar = va_arg(ArgList, vmCvar_t *);
		name = va_arg(ArgList, char *);
		value = va_arg(ArgList, char *);
		flags = va_arg(ArgList, int);
		DQConsole.RegisterCVar(name, value, flags, eAuthUI, vmCvar);
		break;
	case UI_CVAR_UPDATE:
		vmCvar = va_arg(ArgList, vmCvar_t *);
		DQConsole.UpdateCVar(vmCvar);
		break;
	case UI_CVAR_SET:
		name = va_arg(ArgList, char *);
		value = va_arg(ArgList, char *);
		DQConsole.SetCVarString(name,value, eAuthUI);
		break;
	case UI_CVAR_VARIABLEVALUE:
		name = va_arg(ArgList, char *);
		va_end(ArgList);
		return PASSFLOAT(DQConsole.GetCVarFloat(name, eAuthServer));
	case UI_CVAR_VARIABLESTRINGBUFFER:
		name = va_arg(ArgList, char *);
		buffer = va_arg(ArgList, char *);
		bufsize = va_arg(ArgList, int);
		DQConsole.GetCVarString(name, buffer, bufsize, eAuthUI);
		break;
	case UI_CVAR_SETVALUE:
		name = va_arg(ArgList, char *);
		fTemp1 = *((float*)&va_arg(ArgList, int));
		DQConsole.SetCVarValue(name, fTemp1, eAuthUI);
		break;
	case UI_CVAR_RESET:
		name = va_arg(ArgList, char *);
		DQConsole.ResetCVar(name, eAuthUI);
		break;
	case UI_CVAR_CREATE:
	case UI_CVAR_INFOSTRINGBUFFER:
		CriticalError(ERR_UI, va("trap %d not implemented",command));
		break;
	case UI_ARGC:
		//int trap_Argc( void )
		va_end(ArgList);
		pCommand = DQUI.CurrentCommand;
		if(!pCommand) {
			return 0;
		}
		return pCommand->GetNumArgs();
	case UI_ARGV:
		//void trap_Argv( int n, char *buffer, int bufferLength )
		pCommand = DQUI.CurrentCommand;
		iTemp1 = va_arg(ArgList, int);
		buffer = va_arg(ArgList, char*);
		bufsize = va_arg(ArgList, int);
		if(!pCommand || iTemp1>=pCommand->GetNumArgs()) {
			buffer[0]='\0'; 
			break;
		}
		buffer2 = pCommand->GetArg(iTemp1);
		DQWordstrcpy(buffer, buffer2, bufsize);
		break;
	case UI_CMD_EXECUTETEXT:
		//void trap_Cmd_ExecuteText( int exec_when, const char *text )
		iTemp1 = va_arg(ArgList, int);
		buffer = va_arg(ArgList, char*);
		switch(iTemp1) {
		case EXEC_NOW:
			//Drop Through
		case EXEC_INSERT:
			DQConsole.ExecuteCommandString( buffer, MAX_STRING_CHARS, TRUE );
			break;
		case EXEC_APPEND:
			DQConsole.ExecuteCommandString( buffer, MAX_STRING_CHARS, FALSE );
			break;
		default:
			Error(ERR_UI, "Invalid Execute-When");
		}
		break;

	//
	// File System Commands
	//

	case UI_FS_FOPENFILE:						//Check that binary read is right?
		buffer = va_arg(ArgList, char*);
		pInt = va_arg(ArgList, int*);
		FileMode = va_arg(ArgList, fsMode_t);
		switch(FileMode) {
		case FS_READ:
			*pInt = DQFS.OpenFile(buffer, "rb"); break;
		case FS_WRITE:
			*pInt = DQFS.OpenFile(buffer, "wb"); break;
		case FS_APPEND:
			*pInt = DQFS.OpenFile(buffer, "ab"); break;
		case FS_APPEND_SYNC:
			*pInt = DQFS.OpenFile(buffer, "a+b"); break;
		};
		va_end(ArgList);
		return (*pInt)?DQFS.GetFileLength(*pInt):0;
		break;
	case UI_FS_READ:
		buffer = va_arg(ArgList, char*);
		length = va_arg(ArgList, int);
		FileHandle = va_arg(ArgList, int);

		DQFS.ReadFile(buffer, length, 1, FileHandle);
		break;
	case UI_FS_WRITE:
		buffer = va_arg(ArgList, char*);
		length = va_arg(ArgList, int);
		FileHandle = va_arg(ArgList, int);

		DQFS.WriteFile(buffer, length, 1, FileHandle);
		break;
	case UI_FS_FCLOSEFILE:
		FileHandle = va_arg(ArgList, int);
		DQFS.CloseFile(FileHandle);
		break;
	case UI_FS_GETFILELIST:
//		This fills buffer with a NULL delimited list of all the files in the directory specified 
//		by path ending in extension. The number of files listed is returned.
		path = va_arg(ArgList, char*);
		extention = va_arg(ArgList, char*);
		buffer = va_arg(ArgList, char*);
		bufsize = va_arg(ArgList, int);
		va_end(ArgList);
		return DQFS.GetFileList(path, extention, buffer, bufsize, FALSE);
	case UI_FS_SEEK:
		FileHandle = va_arg(ArgList, int);
		iTemp1 = va_arg(ArgList, int);
		iTemp2 = va_arg(ArgList, int);
		DQFS.Seek(iTemp1, iTemp2, FileHandle);	//GUESS at return value
		va_end(ArgList);
		return iTemp2;

	//
	// Renderer Commands
	//

	case UI_GETGLCONFIG:
		//void trap_GetGlconfig( glconfig_t *glconfig )
		pGLconfig = va_arg(ArgList, glconfig_t*);
		memcpy(pGLconfig, &DQRender.GLconfig, sizeof(glconfig_t) );
		break;
	case UI_R_REGISTERMODEL:
		buffer = va_arg(ArgList, char *);
		va_end(ArgList);
		return DQRender.RegisterModel(buffer);
	case UI_R_REGISTERSKIN:
		//qhandle_t trap_R_RegisterSkin( const char *name )
		buffer = va_arg(ArgList, char *);
		va_end(ArgList);
		return DQRender.RegisterSkin(buffer);
	case UI_R_REGISTERSHADERNOMIP:
		buffer = va_arg(ArgList, char *);
		va_end(ArgList);
		if( !buffer[0] ) return 0;
		return DQRender.RegisterShader(buffer, ShaderFlag_NoMipmaps);
	case UI_R_DRAWSTRETCHPIC:
		//void trap_R_DrawStretchPic( float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader )
		//Create a rectangle size w,h at pos x,y with tex coords s1,t1 and s2,t2 at opposite corners
		//Paint it with the shader specified. NB. Don't draw until UI_R_RENDERSCENE
		fTemp1 = *((float*)&va_arg(ArgList, int));
		fTemp2 = *((float*)&va_arg(ArgList, int));
		fTemp3 = *((float*)&va_arg(ArgList, int));
		fTemp4 = *((float*)&va_arg(ArgList, int));
		s1 = *((float*)&va_arg(ArgList, int));
		t1 = *((float*)&va_arg(ArgList, int));
		s2 = *((float*)&va_arg(ArgList, int));
		t2 = *((float*)&va_arg(ArgList, int));
		hShader = va_arg(ArgList, int);
		DQRender.DrawSprite(fTemp1,fTemp2,fTemp3, fTemp4,s1,t1,s2,t2,hShader);
		break;
	case UI_R_RENDERSCENE:
		//void trap_R_RenderScene( const refdef_t *fd )
		//Sets the Render information. The argument refdef provides camera information
		refdef = va_arg(ArgList, refdef_t *);
		DQCamera.SetWithRefdef(refdef);
		break;
	case UI_R_CLEARSCENE:
		//void trap_R_ClearScene( void )
		DQRender.bClearScreen = TRUE;
		break;
	case UI_R_ADDREFENTITYTOSCENE:
		//void trap_R_AddRefEntityToScene( const refEntity_t *re )
		refEntity = va_arg(ArgList, refEntity_t *);
		DQRender.DrawRefEntity(refEntity);
		break;
	case UI_R_SETCOLOR:
		//void trap_R_SetColor( const float *rgba )
		rgba = va_arg(ArgList, float *);
		DQRender.SetSpriteColour(rgba);
		break;
	case UI_R_ADDLIGHTTOSCENE:
		//void trap_R_AddLightToScene( const vec3_t org, float intensity, float r, float g, float b )

		//Disabled due to using Additive lighting in game
		pFloat = va_arg(ArgList, float *);
		fTemp1 = va_arg(ArgList, float);
		fTemp2 = va_arg(ArgList, float);
		fTemp3 = va_arg(ArgList, float);
		fTemp4 = va_arg(ArgList, float);
//		DQRender.AddLight( D3DXVECTOR3( pFloat[0], pFloat[1], pFloat[2]), fTemp1, D3DXCOLOR( fTemp2, fTemp3, fTemp4, 1.0f ) );
		break;

	case UI_CM_LERPTAG:
	//int trap_CM_LerpTag( orientation_t *tag, clipHandle_t mod, int startFrame, int endFrame, float frac, const char *tagName )
	//Linearly interpolates "tagname" (belonging to model handle "mod") between startFrame and endFrame by amount frac, returning it through *tag
	//return value : TRUE if tag exists, FALSE if not
		tag = va_arg(ArgList, orientation_t *);
		iTemp1 = va_arg(ArgList, int);
		iTemp2 = va_arg(ArgList, int);
		iTemp3 = va_arg(ArgList, int);
		fTemp1 = va_arg(ArgList, float);
		buffer = va_arg(ArgList, char *);
		va_end(ArgList);
		Model = DQRender.GetModel(iTemp1);
		if(Model) {
			return Model->LerpTag( tag, iTemp2, iTemp3, fTemp1, buffer);
		}
		return 0;

		//Sound

	case UI_S_REGISTERSOUND:
		//sfxHandle_t	trap_S_RegisterSound( const char *sample, qboolean compressed )
		buffer = va_arg(ArgList, char *);
		iTemp1 = va_arg(ArgList, int);
		va_end(ArgList);
		return DQSound.RegisterSound(buffer, iTemp1);
	case UI_S_STARTLOCALSOUND:
		//void trap_S_StartLocalSound( sfxHandle_t sfx, int channelNum )
		iTemp1 = va_arg(ArgList, int);
		iTemp2 = va_arg(ArgList, int);
		DQSound.Play2DSound( iTemp1, iTemp2 );
		break;

	//
	// Keyboard Input
	//

	case UI_KEY_KEYNUMTOSTRINGBUF:
		//void trap_Key_KeynumToStringBuf( int keynum, char *buf, int buflen )
		//Fills buf with the string (English) representation of a key keynum
		iTemp1 = va_arg(ArgList, int);
		buffer = va_arg(ArgList, char*);
		bufsize = va_arg(ArgList, int);
		DQInput.GetKeynumString( iTemp1, buffer, bufsize );
		break;
	case UI_KEY_ISDOWN:
		//qboolean trap_Key_IsDown( int keynum )
		iTemp1 = va_arg(ArgList, int);
		va_end(ArgList);
		return DQInput.KeyIsDown(iTemp1);
	case UI_KEY_GETCATCHER:
		va_end(ArgList);
		return DQInput.GetKeyCatcher();
	case UI_KEY_SETCATCHER:
		//void trap_Key_SetCatcher( int catcher )
		iTemp1 = va_arg(ArgList, int);
		DQInput.SetKeyCatcher(iTemp1);
		break;
	case UI_KEY_GETBINDINGBUF:
		//void trap_Key_GetBindingBuf( int keynum, char *buf, int buflen )
		//Fills buf with the command bound to key keynum
		iTemp1 = va_arg(ArgList, int);
		buffer = va_arg(ArgList, char*);
		bufsize = va_arg(ArgList, int);
		DQInput.GetBindingString( iTemp1, buffer, bufsize );
		break;
	case UI_KEY_SETBINDING:
		//void trap_Key_GetBindingBuf( int keynum, char *buf, int buflen )
		//Sets the binding to Keynum
		iTemp1 = va_arg(ArgList, int);
		buffer = va_arg(ArgList, char*);
		DQInput.SetBindingString( iTemp1, buffer, MAX_STRING_CHARS);
		break;
	case UI_KEY_GETOVERSTRIKEMODE:
		//OverstrikeMode = IsOverwriteEnabled
		return DQInput.isOverwriteEnabled;
	case UI_KEY_SETOVERSTRIKEMODE:
		iTemp1 = va_arg(ArgList, int);
		DQInput.isOverwriteEnabled = iTemp1;
		break;
	case UI_KEY_CLEARSTATES:
		//Sets all the KeyStatus states to false
		DQInput.ClearKeyStatus();
		break;

	//
	// Miscellaneous
	//
	case UI_GET_CDKEY:
		buffer = va_arg(ArgList, char*);
		bufsize = va_arg(ArgList, int);
		DQstrcpy(buffer, "", bufsize);
		break;
	case UI_SET_CDKEY:
		break;
	case UI_VERIFY_CDKEY:
		//qboolean trap_VerifyCDKey( const char *key, const char *chksum)
		va_end(ArgList);
		return TRUE;
	case UI_MEMORY_REMAINING:
		//int trap_MemoryRemaining( void )
		va_end(ArgList);
		return DQMemory.GetRemaining();
	case UI_S_STOPBACKGROUNDTRACK:
	case UI_S_STARTBACKGROUNDTRACK:
	case UI_CIN_SETEXTENTS:
	case UI_GETCLIPBOARDDATA:
	case UI_PC_ADD_GLOBAL_DEFINE:
		//int trap_PC_AddGlobalDefine( char *define ) 
	case UI_PC_LOAD_SOURCE:
		//int trap_PC_LoadSource( const char *filename )
	case UI_PC_FREE_SOURCE:
		//int trap_PC_FreeSource( int handle ) 
	case UI_PC_READ_TOKEN:
		//int trap_PC_ReadToken( int handle, pc_token_t *pc_token )
	case UI_PC_SOURCE_FILE_AND_LINE:
		//int trap_PC_SourceFileAndLine( int handle, char *filename, int *line )
	case UI_REAL_TIME:
		//int trap_RealTime(qtime_t *qtime) 
	case UI_CIN_PLAYCINEMATIC:
		//int trap_CIN_PlayCinematic( const char *arg0, int xpos, int ypos, int width, int height, int bits) 
	case UI_CIN_STOPCINEMATIC:
	case UI_CIN_RUNCINEMATIC:
	case UI_CIN_DRAWCINEMATIC:
	case UI_SET_PBCLSTATUS:
		Error(ERR_UI, "Unknown UI Command");
		break;
		
		//
		// Network
		//

	case UI_GETCLIENTSTATE:
		//void trap_GetClientState( uiClientState_t *state )
		ClientState = va_arg(ArgList, uiClientState_t*);
		DQNetwork.ClientGetUIState(ClientState);
		break;
	case UI_GETCONFIGSTRING:
		//int trap_GetConfigString( int index, char* buff, int buffsize )
		iTemp1 = va_arg(ArgList, int);
		buffer = va_arg(ArgList, char *);
		bufsize = va_arg(ArgList, int);
		va_end(ArgList);
		return DQNetwork.ClientGetConfigString( iTemp1, buffer, bufsize );

	case UI_LAN_GETPING:
		//void trap_LAN_GetPing( int n, char *buf, int buflen, int *pingtime )
		//Returns buffer[0]=0 if server n doesn't exist
		iTemp1 = va_arg(ArgList, int);
		buffer = va_arg(ArgList, char *);
		bufsize = va_arg(ArgList, int);
		pInt = va_arg(ArgList, int *);
		*pInt = DQNetwork.GetPing(iTemp1, buffer, bufsize);
		break;
	case UI_LAN_GETPINGINFO:
		//void trap_LAN_GetPingInfo( int n, char *buf, int buflen )
		//Returns the ServerInfo string for Ping n
		iTemp1 = va_arg(ArgList, int);
		buffer = va_arg(ArgList, char *);
		bufsize = va_arg(ArgList, int);
		DQNetwork.GetLANServerInfoString( iTemp1, buffer, bufsize );
		break;
	case UI_LAN_CLEARPING:
		//void trap_LAN_ClearPing( int n )
		iTemp1 = va_arg(ArgList, int);
		//unused
		break;
	case UI_LAN_GETSERVERCOUNT:
		//int	trap_LAN_GetServerCount( int ServerType )
		iTemp1 = va_arg(ArgList, int);	//ignore (only deal with LAN games)
		va_end(ArgList);
		return DQNetwork.GetServerCount();
	case UI_LAN_GETSERVERADDRESSSTRING:
		//void trap_LAN_GetServerAddressString( int ServerType, int n, char *buf, int buflen )
		iTemp1 = va_arg(ArgList, int);	//ignore
		iTemp2 = va_arg(ArgList, int);
		buffer = va_arg(ArgList, char*);
		bufsize = va_arg(ArgList, int);
		DQNetwork.GetServerAddressString(iTemp2, buffer, bufsize);
		break;
	case UI_LAN_GETPINGQUEUECOUNT:
		//int trap_LAN_GetPingQueueCount( void )
		iTemp1 = va_arg(ArgList, int);
		va_end(ArgList);
		return DQNetwork.GetServerCount();

	case UI_R_REGISTERFONT:
	case UI_R_ADDPOLYTOSCENE:
	case UI_R_MODELBOUNDS:
	case UI_UPDATESCREEN:
	case UI_R_REMAP_SHADER:

	case UI_LAN_GETSERVERINFO:
	case UI_LAN_GETSERVERPING:
		//int trap_LAN_GetServerPing( int source, int n )
	case UI_LAN_SERVERSTATUS:
		//int trap_LAN_ServerStatus( const char *serverAddress, char *serverStatus, int maxLen )
	case UI_LAN_SAVECACHEDSERVERS:
	case UI_LAN_LOADCACHEDSERVERS:
	case UI_LAN_RESETPINGS:
	case UI_LAN_MARKSERVERVISIBLE:
	case UI_LAN_SERVERISVISIBLE:
		//int trap_LAN_ServerIsVisible( int source, int n)
	case UI_LAN_UPDATEVISIBLEPINGS:
		//qboolean trap_LAN_UpdateVisiblePings( int source ) 
	case UI_LAN_ADDSERVER:
		//int trap_LAN_AddServer(int source, const char *name, const char *addr)
	case UI_LAN_REMOVESERVER:
	case UI_LAN_COMPARESERVERS:
		//int trap_LAN_CompareServers( int source, int sortKey, int sortDir, int s1, int s2 )
		Error(ERR_UI, "Unknown UI Command");
		break;
	default:
		Error(ERR_UI, "Unknown UI Command");
	};

	va_end(ArgList);
	return 0;
}