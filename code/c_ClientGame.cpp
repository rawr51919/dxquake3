// DXQuake3 Source
// Copyright (c) 2003 Richard Geary (richard.geary@dial.pipex.com)
//
// c_ClientGame.cpp: implementation of the c_ClientGame class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "c_ClientGame.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

int QDECL Callback_ClientGame(int command, ...);

c_ClientGame::c_ClientGame()
{
	hDLL = NULL;
	vmMain = NULL;
	dllEntry = NULL;
	isLoaded = FALSE;
	LoadMe = FALSE;
	ClientNum = 0;
	LoadHint_SnapshotNum = 0;

	InitializeCriticalSection( &cs_vmMain );
	ClientLoadedEvent = CreateEvent( NULL, FALSE, FALSE, NULL );

	DQConsole.RegisterCVar( "cg_NetworkInfo", "0", 0, eAuthServer, &cg_NetworkInfo );
}

c_ClientGame::~c_ClientGame()
{
	Unload();
	DQNetwork.Disconnect();
	DeleteCriticalSection( &cs_vmMain );
	CloseHandle( ClientLoadedEvent );
}

void c_ClientGame::Load() 
{
	char Path[MAX_OSPATH];

	if(isLoaded) return;
	LoadMe = TRUE;			//fixes Main Menu in-game on vid_restart bug

	DQRenderStack.SaveState();

	DQPrint( "Loading ClientGame..." );
	
//	DQstrcpy( Path, "F:\\Quake III Arena\\TestMod\\cgamex86.dll", MAX_PATH);
//	DQstrcpy( Path, "H:\\cgamex86.dll", MAX_PATH);
	DQFS.GetDLLPath("cgamex86.dll", Path);
	if(!Path[0]) {
		CriticalError(ERR_FS, "Unable to load ClientGame DLL (cgamex86.dll)");
	}
	hDLL = LoadLibrary(Path);

	if(!hDLL) {
		CriticalError(ERR_FS, "Unable to load ClientGame DLL (cgamex86.dll)");
		return;
	}

	EnterCriticalSection( &cs_vmMain );
	{
		vmMain = (t_vmMain)GetProcAddress((HMODULE)hDLL, "vmMain");
		dllEntry = (t_dllEntry)GetProcAddress((HMODULE)hDLL, "dllEntry");

		//Set up callback func
		dllEntry( Callback_ClientGame );
		//Initialise CGame

		ZeroMemory( &UserCmd, sizeof(usercmd_t));
		CurrentWeapon = 0;
		ViewAngles[0] = ViewAngles[1] = ViewAngles[2] = 0.0f;
		ZoomSensitivity = 1.0f;
		CurrentCommand = NULL;

		//int serverMessageNum, int serverCommandSequence, int clientNum
		vmMain(CG_INIT,LoadHint_SnapshotNum,0,ClientNum, 0,0,0,0,0,0,0,0,0);
	}
	LeaveCriticalSection( &cs_vmMain );

	SetEvent( ClientLoadedEvent );

	DQPrint( "ClientGame Loaded." );

	LoadMe = FALSE;
	isLoaded = TRUE;
}


void c_ClientGame::Unload()
{
	if(!isLoaded) return;

	DQPrint( "Shutting Down ClientGame..." );

	EnterCriticalSection( &cs_vmMain );
	{
		if(vmMain) {
			vmMain(CG_SHUTDOWN,0,0,0,0,0,0,0,0,0,0,0,0);
			vmMain = NULL;
		}
	}
	LeaveCriticalSection( &cs_vmMain );

	if(hDLL) {
		FreeLibrary(hDLL);
		hDLL = NULL;
	}

	ResetEvent( ClientLoadedEvent );

	isLoaded = FALSE;
}

void c_ClientGame::Update() {
	int ServerTime;

	if(!vmMain || !isLoaded || !DQNetwork.isClientLoaded) return;

	DQConsole.UpdateCVar( &cg_NetworkInfo );

	ServerTime = DQNetwork.ClientUpdateServerTime();

	CreateUserCmdMessage(ServerTime);

	//Send UserCmd messages to server
	if( !DQNetwork.ClientSendUserCmd(&UserCmd) ) {
		//disconnected from server
		DQConsole.Disconnect();
		return;
	}

	EnterCriticalSection( &cs_vmMain );
	{
		vmMain(CG_DRAW_ACTIVE_FRAME, ServerTime, 0,0,0,0,0,0,0,0,0,0,0);
	}
	LeaveCriticalSection( &cs_vmMain );

	DQNetwork.ClientCleanServerCommandChain();
}

void c_ClientGame::UpdateLoadingScreen()
{
	EnterCriticalSection( &cs_vmMain );
	{
//		vmMain(CG_DRAW_ACTIVE_FRAME, DQTime.GetMillisecs(), 0,0,0,0,0,0,0,0,0,0,0);
		vmMain(CG_DRAW_ACTIVE_FRAME, 0, 0,0,0,0,0,0,0,0,0,0,0);
	}
	LeaveCriticalSection( &cs_vmMain );
	DQUI.Update();
}

BOOL c_ClientGame::ExecuteCommand( c_Command *command )
{
	BOOL ret;

	CurrentCommand = command;
	EnterCriticalSection( &cs_vmMain );
	{
		ret = vmMain(CG_CONSOLE_COMMAND, 0,0,0,0,0,0,0,0,0,0,0,0);
	}
	LeaveCriticalSection( &cs_vmMain );
	return ret;
}

void c_ClientGame::CreateUserCmdMessage(int ServerTime)
{
	//Don't move mouse if in UI, console or talking
	if(!DQInput.GetKeyCatcher()) {
		ViewAngles[PITCH] += ANGLE2SHORT( DQInput.fDeltaMouse.y * DQInput.m_pitch.value );
		ViewAngles[YAW] -= ANGLE2SHORT( DQInput.fDeltaMouse.x * DQInput.m_yaw.value );
		ViewAngles[ROLL] = 0;
	}

	ZeroMemory( &UserCmd, sizeof(usercmd_t));
	//Fill the DQInput parts of the usercmd struct
	DQInput.CreateUserCmd(&UserCmd);

	//Fill the ClientGame parts
	//Subtract delta angles to get view angle relative to start direction
	UserCmd.angles[0] = ViewAngles[0];
	UserCmd.angles[1] = ViewAngles[1];
	UserCmd.angles[2] = ViewAngles[2];
//	DQPrint(va("UserCmd : angles[0] %d  angles[1] %d angles[2] %d",UserCmd.angles[0],UserCmd.angles[1],UserCmd.angles[2]));
//	UserCmd.angles[0] = UserCmd.angles[1] = UserCmd.angles[2] = 0;
	UserCmd.serverTime = ServerTime;
	UserCmd.weapon = CurrentWeapon;
}

//*****************************************************************************

void c_ClientGame::CreateTempClipBox( D3DXVECTOR3 *Min, D3DXVECTOR3 *Max )
{
	TempClipBox.MidpointPos = 0.5f*( *Min + *Max );
	TempClipBox.Extent = *Max - TempClipBox.MidpointPos;
}

void c_ClientGame::TraceBox( trace_t *results, D3DXVECTOR3 *start, D3DXVECTOR3 *end, 
			D3DXVECTOR3 *min, D3DXVECTOR3 *max, int ModelHandle, int contentmask )
{
	c_BSP::s_Trace Trace;

DQProfileSectionStart( 13, "ClientTraceBox" );

	DQBSP.CreateNewTrace( Trace, start, end, min, max, contentmask );

	if( ModelHandle )
	{
		//Only run from Q3 ClientGame DLL by CG_TouchTriggerPrediction, where ModelHandle is an inline model
		//Trace Axis-Aligned model
		c_Model *Model;
		D3DXVECTOR3 Position, Extent, Min, Max;

		Model = DQRender.GetModel( ModelHandle );
		Model->GetModelBounds( Min, Max );

		Position = 0.5f*(Min+Max);
		Max -= Position;
		Min -= Position;
		Extent.x = max( Max.x, -Min.x );
		Extent.y = max( Max.y, -Min.y );
		Extent.z = max( Max.z, -Min.z );
		
		DQBSP.TraceAgainstAABB( &Trace, Position, Extent, Trace.ContentMask, ModelHandle );

	}
	else {
		//Trace BSP
		DQBSP.TraceBox( Trace );
	}

	DQBSP.ConvertTraceToResults( &Trace, results );

DQProfileSectionEnd( 13 );
}

//Note : Q3 DLL only sets Angles for Inline Models
void c_ClientGame::TraceBoxAgainstOrientedBox(  trace_t *results, D3DXVECTOR3 *start, D3DXVECTOR3 *end, 
			D3DXVECTOR3 *min, D3DXVECTOR3 *max, int ModelHandle, int contentmask, D3DXVECTOR3 *Origin, D3DXVECTOR3 *Angles )
{
	c_BSP::s_Trace Trace;
	D3DXVECTOR3 Position;

	if( !ModelHandle ) return;
	//ModelHandle can be an inline model or a TempBox

	//TODO: Rotate the box using Angles?

	DQBSP.CreateNewTrace( Trace, start, end, min, max, contentmask );

	if(ModelHandle == 9999999) //TempBox handle
	{
		Position = *Origin + TempClipBox.MidpointPos;

		//Create brush
		DQBSP.TraceAgainstAABB( &Trace, Position, TempClipBox.Extent, Trace.ContentMask, ModelHandle );
	}
	else {
		//is a model handle for an inline model

		c_Model *Model;

		Model = DQRender.GetModel( ModelHandle );

		//Trace against inline model brush
		DQBSP.TraceInlineModel( Trace, Model->GetInlineModelIndex() );
	}

	DQBSP.ConvertTraceToResults( &Trace, results );
}
