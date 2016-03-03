// DXQuake3 Source
// Copyright (c) 2003 Richard Geary (richard.geary@dial.pipex.com)
//

#include "stdafx.h"

int QDECL Callback_ClientGame(int command, ...)
{
	vmCvar_t *vmCvar;
	char *buffer2;
	int iTemp1, iTemp2, iTemp3;
	char *buffer;
	int bufsize;
	c_Command *pCommand;
	int *pInt1, *pInt2;
	int length;
	int FileHandle;
	float *pFloat1, *pFloat2, *pFloat3, *pFloat4,*pFloat5,*pFloat6;
	float fTemp1, fTemp2, fTemp3, fTemp4, s1, t1, s2, t2;
	fsMode_t FileMode;
	qboolean qBool;
	glconfig_t *pGLconfig;
	gameState_t *gamestate;
	snapshot_t *snapshot;
	usercmd_t *usercmd;
	orientation_t *tag;
	c_Model *Model;
	trace_t	*pTrace;
	markFragment_t *MarkFragment;
	refEntity_t *refEntity;
	refdef_t *refdef;
	polyVert_t *polyVerts;
	D3DXVECTOR3 Vec3a,Vec3b,Vec3c,Vec3d,Vec3e,Vec3f;

	va_list ArgList;
	va_start(ArgList, command);

/*	char *Text;
	int handle;
	handle = DQFS.OpenFile("log.txt", "a");
	Text = va("Command %d\n",command);
	DQFS.WriteFile(Text, strlen(Text), 1, handle);
	DQFS.CloseFile(handle);
*/
	switch(command) {
	case CG_PRINT:
		DQConsole.Print(va_arg(ArgList, char *), TRUE);
		break;
	case CG_ERROR:
		Error(ERR_CGAME, va_arg(ArgList, char *));
		break;
	case CG_MILLISECONDS:
		va_end(ArgList);
		return DQTime.GetMillisecs();
	case CG_CVAR_REGISTER:
		//buffer = name, buffer2 = value
		vmCvar = va_arg(ArgList, vmCvar_t *);
		buffer = va_arg(ArgList, char *);
		buffer2 = va_arg(ArgList, char *);
		iTemp1 = va_arg(ArgList, int);
		DQConsole.RegisterCVar(buffer, buffer2, iTemp1, eAuthClient, vmCvar);
		break;
	case CG_CVAR_UPDATE:
		vmCvar = va_arg(ArgList, vmCvar_t *);
		DQConsole.UpdateCVar(vmCvar );
		break;
	case CG_CVAR_SET:
		buffer = va_arg(ArgList, char *);
		buffer2 = va_arg(ArgList, char *);
		DQConsole.SetCVarString(buffer,buffer2, eAuthClient);
		break;
	case CG_CVAR_VARIABLESTRINGBUFFER:
		buffer = va_arg(ArgList, char *);
		buffer2 = va_arg(ArgList, char *);
		bufsize = va_arg(ArgList, int);
		DQConsole.GetCVarString(buffer, buffer2, bufsize, eAuthClient);
		break;
	case CG_ARGC:
		//int trap_Argc( void )
		va_end(ArgList);
		pCommand = DQClientGame.CurrentCommand;
		if(!pCommand) {
			return 0;
		}
		iTemp1 = pCommand->GetNumArgs();
		return iTemp1;
	case CG_ARGV:
		//void trap_Argv( int n, char *buffer, int bufferLength )
		pCommand = DQClientGame.CurrentCommand;
		iTemp1 = va_arg(ArgList, int);
		buffer = va_arg(ArgList, char*);
		bufsize = va_arg(ArgList, int);
		if(!pCommand || iTemp1>=pCommand->GetNumArgs()) {
			buffer[0]='\0'; 
			break;
		}
		buffer2 = pCommand->GetArg(iTemp1);
		if(iTemp1==0)
			DQWordstrcpy( buffer, buffer2, bufsize );
		else
			DQstrcpy(buffer, buffer2, bufsize);
		break;
	case CG_ARGS:
		//void trap_Args( char *buffer, int bufferLength )
		//Get full command
		pCommand = DQClientGame.CurrentCommand;
		buffer = va_arg(ArgList, char*);
		bufsize = va_arg(ArgList, int);
		if(!pCommand) {
			buffer[0]='\0';
			break;
		}
		buffer2 = pCommand->GetArg(0);
		DQstrcpy(buffer, buffer2, bufsize);
		break;
	case CG_SENDCLIENTCOMMAND:
		//void trap_SendClientCommand( const char *s )
		//Send a command from client to server
		buffer = va_arg(ArgList, char *);
		DQNetwork.ClientSendCommandToServer( buffer );		//if disconnected, do nothing
		break;
	case CG_FS_FOPENFILE:
		buffer = va_arg(ArgList, char*);
		pInt1 = va_arg(ArgList, int*);
		FileMode = va_arg(ArgList, fsMode_t);
		switch(FileMode) {
		case FS_READ:
			iTemp1 = DQFS.OpenFile(buffer, "rb"); break;
		case FS_WRITE:
			iTemp1 = DQFS.OpenFile(buffer, "wb"); break;
		case FS_APPEND:
			iTemp1 = DQFS.OpenFile(buffer, "ab"); break;
		case FS_APPEND_SYNC:
			iTemp1 = DQFS.OpenFile(buffer, "a+b"); break;
		default: iTemp1=0;
		};
		if(pInt1) *pInt1 = iTemp1;
		va_end(ArgList);
		return (iTemp1)?DQFS.GetFileLength(iTemp1):0;
	case CG_FS_READ:
		buffer = va_arg(ArgList, char*);
		length = va_arg(ArgList, int);
		FileHandle = va_arg(ArgList, int);

		DQFS.ReadFile(buffer, length, 1, FileHandle);
		break;
	case CG_FS_WRITE:
		buffer = va_arg(ArgList, char*);
		length = va_arg(ArgList, int);
		FileHandle = va_arg(ArgList, int);

		DQFS.WriteFile(buffer, length, 1, FileHandle);
		break;
	case CG_FS_FCLOSEFILE:
		FileHandle = va_arg(ArgList, int);
		DQFS.CloseFile(FileHandle);
		break;
	case CG_FS_SEEK:
		FileHandle = va_arg(ArgList, int);
		iTemp1 = va_arg(ArgList, int);
		iTemp2 = va_arg(ArgList, int);
		DQFS.Seek(iTemp1, iTemp2, FileHandle);	//Guess at return value
		va_end(ArgList);
		return iTemp2;
	case CG_R_REGISTERSHADER:
		//qhandle_t trap_R_RegisterShader( const char *name )
		buffer = va_arg(ArgList, char *);
		va_end(ArgList);
		return DQRender.RegisterShader(buffer);
	case CG_R_REGISTERSHADERNOMIP:
		//qhandle_t trap_R_RegisterShaderNoMip( const char *name )
		buffer = va_arg(ArgList, char *);
		va_end(ArgList);
		return DQRender.RegisterShader(buffer, ShaderFlag_NoMipmaps);
	case CG_ADDCOMMAND:
		//void	trap_AddCommand( const char *cmdName )
		buffer = va_arg(ArgList, char *);
		DQConsole.AddTabCommand( buffer );
		break;
	case CG_GETGLCONFIG:
		//void trap_GetGlconfig( glconfig_t *glconfig )
		pGLconfig = va_arg(ArgList, glconfig_t*);
		memcpy(pGLconfig, &DQRender.GLconfig, sizeof(glconfig_t) );
		break;
	case CG_GETGAMESTATE:
		//void		trap_GetGameState( gameState_t *gamestate )
		gamestate = va_arg(ArgList, gameState_t *);
		DQNetwork.ClientGetGameState(gamestate);
		break;
	case CG_UPDATESCREEN:
		//void	trap_UpdateScreen( void )
		DQRender.BeginRender();
		DQClientGame.UpdateLoadingScreen();
		DQRender.DrawScene();
		DQRender.EndRender();
		break;
	case CG_CM_LOADMAP:
		//void	trap_CM_LoadMap( const char *mapname )
		buffer = va_arg(ArgList, char *);
		DQBSP.LoadBSP(buffer);
		break;
	case CG_S_REGISTERSOUND:
		//sfxHandle_t	trap_S_RegisterSound( const char *sample, qboolean compressed )
		buffer = va_arg(ArgList, char *);
		qBool = va_arg(ArgList, qboolean);
		va_end(ArgList);
		return DQSound.RegisterSound(buffer, (BOOL)(qBool==qtrue));
	case CG_R_CLEARSCENE:
		DQRender.bClearScreen = TRUE;
		break;
	case CG_R_LOADWORLDMAP:
		//void	trap_R_LoadWorldMap( const char *mapname )
		buffer = va_arg(ArgList, char *);
		if(DQstrcmpi(buffer, DQBSP.LoadedMapname, MAX_QPATH)==0) {
			DQBSP.LoadIntoD3D();
		}
		else {
			Error(ERR_CGAME, "LoadWorldMap doesn't correspond to LoadMap");
		}
		break;
	case CG_R_REGISTERMODEL:
		buffer = va_arg(ArgList, char *);
		va_end(ArgList);
		return DQRender.RegisterModel(buffer);
	case CG_R_REGISTERSKIN:
		buffer = va_arg(ArgList, char *);
		va_end(ArgList);
		return DQRender.RegisterSkin(buffer);
	case CG_R_MODELBOUNDS:
		//void	trap_R_ModelBounds( clipHandle_t model, vec3_t mins, vec3_t maxs )
		//Get the model bounds and fill mins and maxs
		iTemp1 = va_arg(ArgList, int);
		pFloat1 = va_arg(ArgList, float *);
		pFloat2 = va_arg(ArgList, float *);
		DQRender.GetQ3ModelBounds( iTemp1, pFloat1, pFloat2 );
		break;
	case CG_CM_NUMINLINEMODELS:
		//int		trap_CM_NumInlineModels( void )
		iTemp1 = va_arg(ArgList, int);
		va_end(ArgList);
		return DQBSP.GetNumInlineModels();
	case CG_GETCURRENTSNAPSHOTNUMBER:
		//void		trap_GetCurrentSnapshotNumber( int *snapshotNumber, int *serverTime )
		pInt1 = va_arg(ArgList, int*);
		pInt2 = va_arg(ArgList, int*);
		DQNetwork.ClientGetLastSnapshotInfo( pInt1, pInt2 );
		break;
	case CG_GETSNAPSHOT:
		//qboolean	trap_GetSnapshot( int snapshotNumber, snapshot_t *snapshot )
		iTemp1 = va_arg(ArgList, int);
		snapshot = va_arg(ArgList, snapshot_t *);
		va_end(ArgList);
		return DQNetwork.ClientGetSnapshot( iTemp1, snapshot );
	case CG_R_SETCOLOR:
		pFloat1 = va_arg(ArgList, float *);
		DQRender.SetSpriteColour(pFloat1);
		break;
	case CG_R_DRAWSTRETCHPIC:
		//void	trap_R_DrawStretchPic( float x, float y, float w, float h, 
		//					   float s1, float t1, float s2, float t2, qhandle_t hShader ) {
		fTemp1 = *((float*)&va_arg(ArgList, int));
		fTemp2 = *((float*)&va_arg(ArgList, int));
		fTemp3 = *((float*)&va_arg(ArgList, int));
		fTemp4 = *((float*)&va_arg(ArgList, int));
		s1 = *((float*)&va_arg(ArgList, int));
		t1 = *((float*)&va_arg(ArgList, int));
		s2 = *((float*)&va_arg(ArgList, int));
		t2 = *((float*)&va_arg(ArgList, int));
		iTemp1 = va_arg(ArgList, int);
		DQRender.DrawSprite(fTemp1,fTemp2,fTemp3, fTemp4,s1,t1,s2,t2,iTemp1);
		break;
	case CG_MEMORY_REMAINING:
		//int trap_MemoryRemaining( void )
		va_end(ArgList);
		return DQMemory.GetRemaining();
	case CG_R_LERPTAG:
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
	case CG_SETUSERCMDVALUE:
		//void		trap_SetUserCmdValue( int stateValue, float sensitivityScale )
		//In Q3 : void ( int WeaponNum, float Zoom )
		iTemp1 = va_arg(ArgList, int);
		fTemp1 = va_arg(ArgList, float);
		DQClientGame.CurrentWeapon = iTemp1;
		DQClientGame.ZoomSensitivity = fTemp1;
		break;
	case CG_GETCURRENTCMDNUMBER:
		//int		trap_GetCurrentCmdNumber( void )
		va_end(ArgList);
		return DQNetwork.ClientGetCurrentUserCmdNum();
	case CG_GETUSERCMD:
		//qboolean	trap_GetUserCmd( int cmdNumber, usercmd_t *ucmd )
		iTemp1 = va_arg(ArgList, int);
		usercmd = va_arg(ArgList, usercmd_t*);
		va_end(ArgList);
		return DQNetwork.ClientGetUserCmd( iTemp1, usercmd );
	case CG_CM_BOXTRACE:
		//void	trap_CM_BoxTrace( trace_t *results, const vec3_t start, const vec3_t end,
		//				  const vec3_t mins, const vec3_t maxs,
		//				  clipHandle_t model, int brushmask )
		pTrace = va_arg(ArgList, trace_t*);
		pFloat1 = va_arg(ArgList, float*);
		pFloat2 = va_arg(ArgList, float*);
		pFloat3 = va_arg(ArgList, float*);
		pFloat4 = va_arg(ArgList, float*);
		iTemp1 = va_arg(ArgList, int);
		iTemp2 = va_arg(ArgList, int);
		Vec3a = MakeD3DXVec3FromFloat3(pFloat1);
		Vec3b = MakeD3DXVec3FromFloat3(pFloat2);
		if(pFloat3) {
			Vec3c = MakeD3DXVec3FromFloat3(pFloat3);
		}
		if(pFloat4) {
			Vec3d = MakeD3DXVec3FromFloat3(pFloat4);
		}
		DQClientGame.TraceBox( pTrace, &Vec3a, &Vec3b, (pFloat3)?&Vec3c:NULL, (pFloat4)?&Vec3d:NULL, iTemp1, iTemp2);
		break;

	case CG_CM_POINTCONTENTS:
		//int trap_CM_PointContents( const vec3_t Point, clipHandle_t NOTUSED )
		// point contents against BSP
		// Return value : Contents at point
		pFloat1 = va_arg(ArgList, float *);
		iTemp1 = va_arg(ArgList, int );
		va_end(ArgList);

		Vec3a = MakeD3DXVec3FromFloat3( pFloat1 );
		return DQBSP.PointContents( &Vec3a );

	case CG_CM_MARKFRAGMENTS:
		//int		trap_CM_MarkFragments( int numPoints, const vec3_t *points, 
//				const vec3_t projection,
//				int maxPoints, vec3_t pointBuffer,
//				int maxFragments, markFragment_t *fragmentBuffer )
		// Returns the projection of a polygon onto the solid brushes in the world

		iTemp1 = va_arg(ArgList, int);
		pFloat3 = va_arg(ArgList, float*);
		pFloat1 = va_arg(ArgList, float*);
		iTemp2 = va_arg(ArgList, int);
		pFloat2 = va_arg(ArgList, float*);
		iTemp3 = va_arg(ArgList, int);
		MarkFragment = va_arg(ArgList, markFragment_t *);
		va_end(ArgList);

		Vec3a = MakeD3DXVec3FromFloat3(pFloat1);

		return DQBSP.GetMarkFragments( pFloat3, iTemp1, &Vec3a, pFloat2, iTemp2, MarkFragment, iTemp3 );
	case CG_R_ADDREFENTITYTOSCENE:
		refEntity = va_arg(ArgList, refEntity_t *);
		DQRender.DrawRefEntity(refEntity);
		break;
	case CG_R_ADDPOLYTOSCENE:
		//void	trap_R_AddPolyToScene( qhandle_t hShader , int numVerts, const polyVert_t *verts )
		iTemp1 = va_arg(ArgList, int);
		iTemp2 = va_arg(ArgList, int);
		polyVerts = va_arg(ArgList, polyVert_t *);
		if(!polyVerts) break;
		DQRender.AddPoly( polyVerts, iTemp2, iTemp1 );
		break;
	case CG_R_ADDLIGHTTOSCENE:
		//void	trap_R_AddLightToScene( const vec3_t org, float intensity, float r, float g, float b )
		pFloat1 = va_arg(ArgList, float *);
		fTemp1 = va_arg(ArgList, float);
		fTemp2 = va_arg(ArgList, float);
		fTemp3 = va_arg(ArgList, float);
		fTemp4 = va_arg(ArgList, float);
		DQRender.AddLight( D3DXVECTOR3( pFloat1[0], pFloat1[1], pFloat1[2]), fTemp1, D3DXCOLOR( fTemp2, fTemp3, fTemp4, 1.0f ) );
		break;
	case CG_R_RENDERSCENE:
		refdef = va_arg(ArgList, refdef_t *);
		DQCamera.SetWithRefdef(refdef);

		DQRender.DrawScene();
		break;

		//
		//Sound
		//

	case CG_S_STARTBACKGROUNDTRACK:
		//void	trap_S_StartBackgroundTrack( const char *intro, const char *loop )
		buffer = va_arg(ArgList, char *);
		buffer2 = va_arg(ArgList, char *);
		//TODO
		break;
	case CG_S_STOPBACKGROUNDTRACK:
		//void	trap_S_StopBackgroundTrack( void )
		//TODO
		break;
	case CG_S_CLEARLOOPINGSOUNDS:
		//void	trap_S_ClearLoopingSounds( qboolean DoesNothing? )
		iTemp1 = va_arg(ArgList, int);
		DQSound.ClearLoopingSounds();
		break;
	case CG_S_RESPATIALIZE:
		//void	trap_S_Respatialize( int entityNum, const vec3_t origin, vec3_t axis[3], 
		//int inwater )
		// This function passes the player's entityNum, origin, rotation and water-state for spatializing
		iTemp1 = va_arg(ArgList, int);
		pFloat1 = va_arg(ArgList, float*);
		pFloat2 = va_arg(ArgList, float*);
		iTemp2 = va_arg(ArgList, int);

		Vec3a = MakeD3DXVec3FromFloat3(pFloat1);

		DQSound.SetListenerOrientation( &Vec3a, pFloat2, iTemp2 );
		break;
	case CG_S_STARTSOUND:
		//void trap_S_StartSound(vec3_t origin, int entityNum, int entchannel, sfxHandle_t sfx )
		//EntityNum is used as an index to the sound, so you can update position, velocity, etc.
		pFloat1 = va_arg(ArgList, float*);
		iTemp1 = va_arg(ArgList, int);
		iTemp2 = va_arg(ArgList, int);
		iTemp3 = va_arg(ArgList, int);

		if(pFloat1) {
			Vec3a = MakeD3DXVec3FromFloat3(pFloat1);
			DQSound.Play3DSound( iTemp3, iTemp2, iTemp1, &Vec3a );
		}
		else
			DQSound.Play2DSound( iTemp3, iTemp2, iTemp1 );
		break;
	case CG_S_STARTLOCALSOUND:
		//void	trap_S_StartLocalSound( sfxHandle_t sfx, int channelNum )
		iTemp1 = va_arg(ArgList, int);
		iTemp2 = va_arg(ArgList, int);
		DQSound.Play2DSound( iTemp1, iTemp2 );
		break;
	case CG_S_ADDLOOPINGSOUND:
		//void	trap_S_AddLoopingSound( int entityNum, const vec3_t origin, 
		//const vec3_t velocity, sfxHandle_t sfx )
		//No difference to CG_S_ADDREALLOOPINGSOUND? - fall through
	case CG_S_ADDREALLOOPINGSOUND:
		//void	trap_S_AddRealLoopingSound( int entityNum, const vec3_t origin, 
		//const vec3_t velocity, sfxHandle_t sfx )
		iTemp1 = va_arg(ArgList, int);
		pFloat1 = va_arg(ArgList, float*);
		pFloat2 = va_arg(ArgList, float*);
		iTemp2 = va_arg(ArgList, int);
	
		Vec3a = MakeD3DXVec3FromFloat3(pFloat1);
		Vec3b = MakeD3DXVec3FromFloat3(pFloat2);

		DQSound.Play3DLoopingSound( iTemp2, iTemp1, &Vec3a, &Vec3b );
		break;
	case CG_S_STOPLOOPINGSOUND:
		//void	trap_S_StopLoopingSound( int entityNum )
		iTemp1 = va_arg(ArgList, int);
		DQSound.StopLoopingSound( iTemp1 );
		break;
	case CG_S_UPDATEENTITYPOSITION:
		//void	trap_S_UpdateEntityPosition( int entityNum, const vec3_t origin )
		iTemp1 = va_arg(ArgList, int );
		pFloat1 = va_arg(ArgList, float *);

		Vec3a = MakeD3DXVec3FromFloat3( pFloat1 );

		DQSound.UpdateEntityPosition( iTemp1, &Vec3a );
		break;

	case CG_SNAPVECTOR:
		pFloat1 = va_arg(ArgList, float *);

		fTemp1 = floor(pFloat1[0]);
		pFloat1[0] = (pFloat1[0]-fTemp1>0.5f)?fTemp1+1.0f:fTemp1;
		fTemp1 = floor(pFloat1[1]);
		pFloat1[1] = (pFloat1[1]-fTemp1>0.5f)?fTemp1+1.0f:fTemp1;
		fTemp1 = floor(pFloat1[2]);
		pFloat1[2] = (pFloat1[2]-fTemp1>0.5f)?fTemp1+1.0f:fTemp1;
		break;

	case CG_REMOVECOMMAND:
		//Not used by Q3
		DQPrint( "CG_REMOVECOMMAND Not implemented" );
		break;

	case CG_SENDCONSOLECOMMAND:
		//void	trap_SendConsoleCommand( const char *text )
		buffer = va_arg(ArgList, char *);
		DQConsole.ExecuteCommandString( buffer, MAX_STRING_CHARS, FALSE );
		break;

	case CG_GETSERVERCOMMAND:
		//qboolean	trap_GetServerCommand( int serverCommandNumber )
		//Returns true if command serverCommandNumber exists, and loads it

		iTemp1 = va_arg(ArgList, int);
		va_end(ArgList);

		DQClientGame.CurrentCommand = DQNetwork.ClientGetServerCommand( iTemp1 );
		if(DQClientGame.CurrentCommand) return TRUE;
		else return FALSE;
		
	case CG_CM_INLINEMODEL:
		//clipHandle_t trap_CM_InlineModel( int index )
		//Get model handle from InlineModel index for tracing with
		iTemp1 = va_arg( ArgList, int );
		va_end(ArgList);
		return DQBSP.GetModelHandleOfInlineModel( iTemp1 );

	case CG_CM_TEMPBOXMODEL:
		//clipHandle_t trap_CM_TempBoxModel( const vec3_t mins, const vec3_t maxs )
		pFloat1 = va_arg(ArgList, float*);
		pFloat2 = va_arg(ArgList, float*);
		va_end(ArgList);
		Vec3a = MakeD3DXVec3FromFloat3(pFloat1);
		Vec3b = MakeD3DXVec3FromFloat3(pFloat2);

		DQClientGame.CreateTempClipBox( &Vec3a, &Vec3b );

		return 9999999;	//handle for TempBox

	case CG_CM_TRANSFORMEDBOXTRACE:
		//void	trap_CM_TransformedBoxTrace( trace_t *results, const vec3_t start, const vec3_t end,const vec3_t mins, 
		//			const vec3_t maxs, clipHandle_t model, int brushmask, const vec3_t origin, const vec3_t angles )
		//Trace against an oriented box
		//model will be either an Inline Model or a TempBox

		pTrace = va_arg(ArgList, trace_t*);
		pFloat1 = va_arg(ArgList, float*);
		pFloat2 = va_arg(ArgList, float*);
		pFloat3 = va_arg(ArgList, float*);
		pFloat4 = va_arg(ArgList, float*);
		iTemp1 = va_arg(ArgList, int);
		iTemp2 = va_arg(ArgList, int);
		pFloat5 = va_arg(ArgList, float*);
		pFloat6 = va_arg(ArgList, float*);

		Vec3a = MakeD3DXVec3FromFloat3(pFloat1);
		Vec3b = MakeD3DXVec3FromFloat3(pFloat2);
		if(pFloat3) {
			Vec3c = MakeD3DXVec3FromFloat3(pFloat3);
		}
		if(pFloat4) {
			Vec3d = MakeD3DXVec3FromFloat3(pFloat4);
		}
		Vec3e = MakeD3DXVec3FromFloat3(pFloat5);
		Vec3f = MakeD3DXVec3FromFloat3(pFloat6);

		DQClientGame.TraceBoxAgainstOrientedBox( pTrace, &Vec3a, &Vec3b, &Vec3c, &Vec3d, iTemp1, iTemp2, &Vec3e, &Vec3f );

		break;

		//TODO
	case CG_CM_LOADMODEL:
	case CG_CM_TRANSFORMEDPOINTCONTENTS:
	case CG_R_REGISTERFONT:
	case CG_KEY_ISDOWN:
	case CG_KEY_GETCATCHER:
	case CG_KEY_SETCATCHER:
	case CG_KEY_GETKEY:
 	case CG_PC_ADD_GLOBAL_DEFINE:
	case CG_PC_LOAD_SOURCE:
	case CG_PC_FREE_SOURCE:
	case CG_PC_READ_TOKEN:
	case CG_PC_SOURCE_FILE_AND_LINE:
	case CG_REAL_TIME:
	case CG_R_LIGHTFORPOINT:
	case CG_CIN_PLAYCINEMATIC:
	case CG_CIN_STOPCINEMATIC:
	case CG_CIN_RUNCINEMATIC:
	case CG_CIN_DRAWCINEMATIC:
	case CG_CIN_SETEXTENTS:
	case CG_R_REMAP_SHADER:

	case CG_CM_TEMPCAPSULEMODEL:
	case CG_CM_CAPSULETRACE:
	case CG_CM_TRANSFORMEDCAPSULETRACE:
	case CG_R_ADDADDITIVELIGHTTOSCENE:
	case CG_GET_ENTITY_TOKEN:
	case CG_R_ADDPOLYSTOSCENE:
	case CG_R_INPVS:

	case CG_MEMSET:
	case CG_MEMCPY:
	case CG_STRNCPY:
	case CG_SIN:
	case CG_COS:
	case CG_ATAN2:
	case CG_SQRT:
	case CG_FLOOR:
	case CG_CEIL:
	case CG_TESTPRINTINT:
	case CG_TESTPRINTFLOAT:
	case CG_ACOS:
	default:
		Error(ERR_CGAME, "Unknown CGAME Command");
	};

	va_end(ArgList);
	return 0;
}