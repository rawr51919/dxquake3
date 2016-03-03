// DXQuake3 Source
// Copyright (c) 2003 Richard Geary (richard.geary@dial.pipex.com)
//

#include "stdafx.h"

int QDECL Callback_Game(int command, ...)
{
	vmCvar_t *vmCvar;
	char *name;
	char *value;
	int flags, iTemp1, iTemp2, iTemp3;
	char *buffer, *buffer2;
	int bufsize;
	c_Command *pCommand;
	int *pInt;
	int length;
	int FileHandle;
	fsMode_t FileMode;
	sharedEntity_t *pEntity;
	playerState_t *clients;
	trace_t	*pTrace;
	float *pFloat1, *pFloat2, *pFloat3, *pFloat4;
	float fTemp1;
	usercmd_t *UserCmd;
	D3DXVECTOR3 Vec3a, Vec3b, Vec3c, Vec3d;

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
	case G_PRINT:
		DQConsole.Print(va_arg(ArgList, char *), TRUE);
		break;
	case G_ERROR:
		Error(ERR_GAME, va_arg(ArgList, char *));
		break;

	case G_MILLISECONDS:
		// ( void );
		// get current time for profiling reasons. this should NOT be used for any game related tasks,
		// because it is not journaled
		va_end(ArgList);
		return DQTime.GetMillisecs();
	case G_CVAR_REGISTER:	
		// ( vmCvar_t *vmCvar, const char *varName, const char *defaultValue, int flags );
		vmCvar = va_arg(ArgList, vmCvar_t *);
		name = va_arg(ArgList, char *);
		value = va_arg(ArgList, char *);
		flags = va_arg(ArgList, int);
		DQConsole.RegisterCVar(name, value, flags, eAuthServer, vmCvar);
		break;
	case G_CVAR_UPDATE:
		// ( vmCvar_t *vmCvar );
		vmCvar = va_arg(ArgList, vmCvar_t *);
		DQConsole.UpdateCVar(vmCvar);
		break;
	case G_CVAR_SET:
		// ( const char *var_name, const char *value );
		name = va_arg(ArgList, char *);
		value = va_arg(ArgList, char *);
		DQConsole.SetCVarString(name,value, eAuthServer);
		break;
	case G_CVAR_VARIABLE_INTEGER_VALUE:
		// ( const char *var_name );
		name = va_arg(ArgList, char *);
		va_end(ArgList);
		return DQConsole.GetCVarInteger(name, eAuthServer);
	case G_CVAR_VARIABLE_STRING_BUFFER:
		// ( const char *var_name, char *buffer, int bufsize );
		name = va_arg(ArgList, char *);
		buffer = va_arg(ArgList, char *);
		bufsize = va_arg(ArgList, int);
		DQConsole.GetCVarString(name, buffer, bufsize, eAuthServer);
		break;
	case G_ARGC:
		// ( void );
		// ServerCommand parameter access
		va_end(ArgList);
		pCommand = DQGame.CurrentCommand;
		if(!pCommand) {
			return 0;
		}
		iTemp1 = pCommand->GetNumArgs();
		return iTemp1;
	case G_ARGV:
		//void	trap_Argv( int n, char *buffer, int bufferLength );
		// ( int n, char *buffer, int bufferLength );
		pCommand = DQGame.CurrentCommand;
		iTemp1 = va_arg(ArgList, int);
		buffer = va_arg(ArgList, char*);
		bufsize = va_arg(ArgList, int);
		if(!pCommand || iTemp1>=pCommand->GetNumArgs()) {
			buffer[0]='\0'; 
			break;
		}
		buffer2 = pCommand->GetArg(iTemp1);
		//NB. different to ClientGame. Causes amusing "say" bug if the same
		DQWordstrcpy( buffer, buffer2, bufsize );
		break;

	case G_FS_FOPEN_FILE:	// ( const char *qpath, fileHandle_t *file, fsMode_t mode );
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
		default:
			CriticalError( ERR_GAME, "Unknown FileMode" );
		};
		va_end(ArgList);
		return (*pInt)?DQFS.GetFileLength(*pInt):0;
		break;
	case G_FS_READ:		// ( void *buffer, int len, fileHandle_t f );
		buffer = va_arg(ArgList, char*);
		length = va_arg(ArgList, int);
		FileHandle = va_arg(ArgList, int);

		DQFS.ReadFile(buffer, length, 1, FileHandle);
		break;
	case G_FS_WRITE:		// ( const void *buffer, int len, fileHandle_t f );
		buffer = va_arg(ArgList, char*);
		length = va_arg(ArgList, int);
		FileHandle = va_arg(ArgList, int);

		DQFS.WriteFile(buffer, length, 1, FileHandle);
		break;
	case G_FS_FCLOSE_FILE:		// ( fileHandle_t f );
		FileHandle = va_arg(ArgList, int);
		DQFS.CloseFile(FileHandle);
		break;
	case G_FS_SEEK:
		//int trap_FS_Seek( fileHandle_t f, long offset, int origin )
		FileHandle = va_arg(ArgList, int);
		iTemp1 = va_arg(ArgList, int);
		iTemp2 = va_arg(ArgList, int);
		DQFS.Seek(iTemp1, iTemp2, FileHandle);	//Guess at return value
		va_end(ArgList);
		return iTemp2;
	case G_FS_GETFILELIST:
		//int trap_FS_GetFileList(  const char *path, const char *extension, char *listbuf, int bufsize )
		name = va_arg(ArgList, char*);
		buffer2 = va_arg(ArgList, char*);
		buffer = va_arg(ArgList, char*);
		bufsize = va_arg(ArgList, int);
		va_end(ArgList);
		return DQFS.GetFileList(name, buffer2, buffer, bufsize, FALSE);

	case G_SEND_CONSOLE_COMMAND:
		//void	trap_SendConsoleCommand( int exec_when, const char *text )
		// add commands to the console as if they were typed in
		// for map changing, etc
		iTemp1 = va_arg(ArgList, int );
		buffer = va_arg(ArgList, char *);
		DQConsole.AddServerConsoleCommand( buffer );
		break;

	//=========== server specific functionality =============
	case G_SET_CONFIGSTRING:
		//void trap_SetConfigstring( int num, const char *string );
		// config strings hold all the index strings, and various other information
		// that is reliably communicated to all clients
		// All of the current configstrings are sent to clients when
		// they connect, and changes are sent to all connected clients.
		// All confgstrings are cleared at each level start.
		iTemp1 = va_arg(ArgList, int);
		buffer = va_arg(ArgList, char *);
		DQGame.SetConfigString(iTemp1, buffer);
		break;
	case G_GET_CONFIGSTRING:
		//void trap_GetConfigstring( int num, char *buffer, int bufferSize )
		iTemp1 = va_arg(ArgList, int);
		buffer = va_arg(ArgList, char *);
		bufsize = va_arg(ArgList, int);
		DQGame.GetConfigString(iTemp1, buffer, bufsize);
		break;
	case G_GET_SERVERINFO:
		//void trap_GetServerinfo( char *buffer, int bufferSize )
		// the serverinfo info string has all the cvars visible to server browsers
		buffer = va_arg(ArgList, char *);
		bufsize = va_arg(ArgList, int);
		DQConsole.GetServerInfo(buffer, bufsize);
		break;
	case G_GET_USERINFO:
		//void trap_GetUserinfo( int num, char *buffer, int bufferSize )
		// userinfo strings are maintained by the server system, so they
		// are persistant across level loads, while all other game visible
		// data is completely reset
		iTemp1 = va_arg(ArgList, int); //client num
		buffer = va_arg(ArgList, char *);
		bufsize = va_arg(ArgList, int);
		DQNetwork.ServerGetUserInfo( iTemp1, buffer, bufsize );
		break;
	case G_SET_USERINFO:
		//void trap_SetUserinfo( int num, const char *buffer )
		iTemp1 = va_arg( ArgList, int );
		buffer = va_arg( ArgList, char*);
		DQNetwork.ServerSetUserInfo( iTemp1, buffer, MAX_INFO_STRING );
		break;
	case G_GET_USERCMD:
		//void trap_GetUsercmd( int clientNum, usercmd_t *cmd )
		iTemp1 = va_arg(ArgList, int);
		UserCmd = va_arg(ArgList, usercmd_t*);
		DQGame.GetUserCmd( iTemp1, UserCmd );
		break;

	case G_LOCATE_GAME_DATA:
		// void ( gentity_t *gEnts, int numGEntities, int sizeofGEntity_t,
		//						playerState_t *clients, int sizeofGameClient );
		// the game needs to let the server system know where and how big the gentities
		// are, so it can look at them directly without going through an interface
		pEntity = va_arg(ArgList, sharedEntity_t *);
		iTemp1 = va_arg(ArgList, int);
		iTemp2 = va_arg(ArgList, int);
		clients = va_arg(ArgList, playerState_t *);
		iTemp3 = va_arg(ArgList, int);
		DQGame.LocateGameData( pEntity, iTemp1, iTemp2, clients, iTemp3);
		break;

	case G_GET_ENTITY_TOKEN:
		//qboolean trap_GetEntityToken( char *buffer, int bufferSize )
		// Retrieves the next string token from the entity spawn text, returning
		// false when all tokens have been parsed.
		// This should only be done at GAME_INIT time.
		buffer = va_arg(ArgList, char *);
		bufsize = va_arg(ArgList, int);
		va_end(ArgList);
		return DQBSP.GetEntityToken( buffer, bufsize );

	case G_UNLINKENTITY:
		//void trap_UnlinkEntity( gentity_t *ent )
		// call before removing an interactive entity
		pEntity = va_arg(ArgList, sharedEntity_t*);
		DQGame.UnlinkEntity( pEntity );
		break;
	case G_LINKENTITY:
		//void trap_LinkEntity( gentity_t *ent );
		// an entity will never be sent to a client or used for collision
		// if it is not passed to linkentity.  If the size, position, or
		// solidity changes, it must be relinked.
		pEntity = va_arg(ArgList, sharedEntity_t*);
		DQGame.LinkEntity( pEntity );
		break;

	case G_TRACE:
		//void trap_Trace( trace_t *results, const vec3_t start, const vec3_t mins, 
		//			const vec3_t maxs, const vec3_t end, int passEntityNum, int contentmask );
		// collision detection against all linked entities
		pTrace = va_arg(ArgList, trace_t*);
		pFloat1 = va_arg(ArgList, float*);
		pFloat2 = va_arg(ArgList, float*);
		pFloat3 = va_arg(ArgList, float*);
		pFloat4 = va_arg(ArgList, float*);
		iTemp1 = va_arg(ArgList, int);
		iTemp2 = va_arg(ArgList, int);
		Vec3a = MakeD3DXVec3FromFloat3(pFloat1);
		Vec3b = MakeD3DXVec3FromFloat3(pFloat4);
		if(pFloat2) {
			Vec3c = MakeD3DXVec3FromFloat3(pFloat2);
		}
		if(pFloat3) {
			Vec3d = MakeD3DXVec3FromFloat3(pFloat3);
		}
		DQGame.TraceBox(pTrace, &Vec3a, &Vec3b, (pFloat2)?&Vec3c:NULL, (pFloat3)?&Vec3d:NULL, iTemp1, iTemp2);
		break;

	case G_SEND_SERVER_COMMAND:
		//void trap_SendServerCommand( int clientNum, const char *text )
		// reliably sends a command string to be interpreted by the given
		// client.  If clientNum is -1, it will be sent to all clients
		iTemp1 = va_arg(ArgList, int); //client num
		buffer = va_arg(ArgList, char *);

		if(!buffer) break;

		DQNetwork.ServerSendClientCommand( iTemp1, buffer );
		break;

	case G_ENTITIES_IN_BOX:
		//int trap_EntitiesInBox( const vec3_t mins, const vec3_t maxs, int *list, int maxcount )
		// EntitiesInBox will return brush models based on their bounding box,
		// so exact determination must still be done with EntityContact

		//Fills list with Entity Numbers of entities in BBox mins->maxs
		// Return Value : Number of entities in **list
		pFloat1 = va_arg(ArgList, float*);
		pFloat2 = va_arg(ArgList, float*);
		pInt = va_arg(ArgList, int *);
		iTemp1 = va_arg(ArgList, int);
		va_end(ArgList);
		Vec3a = MakeD3DXVec3FromFloat3(pFloat1);
		Vec3b = MakeD3DXVec3FromFloat3(pFloat2);
		return DQGame.EntitiesInBox( &Vec3a, &Vec3b, pInt, iTemp1 );

	case G_ENTITY_CONTACT:
		//qboolean trap_EntityContact( const vec3_t mins, const vec3_t maxs, const gentity_t *ent )
		// perform an exact check against inline brush models of non-square shape
		// returns true if AABB mins->maxs collides with the brush of ent
		pFloat1 = va_arg( ArgList, float* );
		pFloat2 = va_arg( ArgList, float* );
		pEntity = va_arg( ArgList, sharedEntity_t* );
		va_end(ArgList);
		Vec3a = MakeD3DXVec3FromFloat3(pFloat1);
		Vec3b = MakeD3DXVec3FromFloat3(pFloat2);
		return DQGame.EntityContact( &Vec3a, &Vec3b, pEntity );

	case G_POINT_CONTENTS:
		//int trap_PointContents( const vec3_t point, int passEntityNum )
		// point contents against all linked entities
		// Return value : Contents at point
		pFloat1 = va_arg(ArgList, float *);
		iTemp1 = va_arg(ArgList, int );
		va_end(ArgList);
		Vec3a = MakeD3DXVec3FromFloat3( pFloat1 );
		return DQGame.PointContents( &Vec3a, iTemp1 );

	case G_SNAPVECTOR:
		//void trap_SnapVector( float *v )
		pFloat1 = va_arg(ArgList, float *);
		fTemp1 = floor(pFloat1[0]);
		pFloat1[0] = (pFloat1[0]-fTemp1>0.5f)?fTemp1+1.0f:fTemp1;
		fTemp1 = floor(pFloat1[1]);
		pFloat1[1] = (pFloat1[1]-fTemp1>0.5f)?fTemp1+1.0f:fTemp1;
		fTemp1 = floor(pFloat1[2]);
		pFloat1[2] = (pFloat1[2]-fTemp1>0.5f)?fTemp1+1.0f:fTemp1;
		break;

	case G_SET_BRUSH_MODEL:
		// ( gentity_t *ent, const char *name );
		// sets mins and maxs based on the brushmodel name
		pEntity = va_arg( ArgList, sharedEntity_t *);
		buffer = va_arg( ArgList, char *);
		
		if( !buffer ) break;
		if( !buffer[0] ) break;
		if( buffer[0] != '*' ) break;

		iTemp1 = atoi( &buffer[1] );

		DQBSP.SetBrushModel( pEntity, iTemp1 );
		break;

	case G_DROP_CLIENT:
		// ( int clientNum, const char *reason );
		// kick a client off the server with a message
		iTemp1 = va_arg( ArgList, int );
		buffer = va_arg( ArgList, char*);
		//do nothing
		break;

	case G_IN_PVS:
		//qboolean trap_InPVS( const vec3_t p1, const vec3_t p2 )
		pFloat1 = va_arg( ArgList, float*);
		pFloat2 = va_arg( ArgList, float*);
		va_end(ArgList);

		Vec3a = MakeD3DXVec3FromFloat3( pFloat1 );
		Vec3b = MakeD3DXVec3FromFloat3( pFloat2 );
		return DQBSP.InPVS( &Vec3a, &Vec3b );		

	//Not implemented
	case G_ADJUST_AREA_PORTAL_STATE:	// ( gentity_t *ent, qboolean open );
		break;
	case G_REAL_TIME:
		break;
	case G_BOT_ALLOCATE_CLIENT:	// ( void );
		va_end( ArgList );
		return -1;	//Bots not implemented

	case G_DEBUG_POLYGON_CREATE:
	case G_DEBUG_POLYGON_DELETE:
	case G_BOT_FREE_CLIENT:
	case G_TRACECAPSULE:
	case G_ENTITY_CONTACTCAPSULE:
	case G_IN_PVS_IGNORE_PORTALS:
	case G_AREAS_CONNECTED:
		//Not used
		Error(ERR_GAME, "trap command not implemented");
		break;
		
	default:
		Error(ERR_GAME, "Unknown GAME Command");
	};

	va_end(ArgList);
	return 0;
}
