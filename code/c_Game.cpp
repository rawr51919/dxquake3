// DXQuake3 Source
// Copyright (c) 2003 Richard Geary (richard.geary@dial.pipex.com)
//
// c_Game.cpp: implementation of the c_Game class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "c_Game.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

int QDECL Callback_Game(int command, ...);

#define GetGameEntity(i) ( (sharedEntity_t*)( (BYTE*)GameEntities+(i)*SizeofGameEntity ) )
#define GetPlayerState(i) ( (playerState_t*)( (BYTE*)PlayerStates+(i)*SizeofPlayerStates ) )

c_Game::c_Game()
{
	//Initialise
	hDLL = NULL;
	vmMain = NULL;
	dllEntry = NULL;
	isLoaded = FALSE;
	isPaused = FALSE;
	for( int i=0; i<MAX_CONFIGSTRINGS; ++i ) ConfigString[i] = NULL;

	InitializeCriticalSection( &cs_vmMain );
	InitializeCriticalSection( &csLastSnapshotNum );

	DQConsole.RegisterCVar( "g_NetworkInfo", "0", 0, eAuthServer, &g_NetworkInfo );
}

c_Game::~c_Game()
{
	Unload();
	DeleteCriticalSection( &cs_vmMain );
	DeleteCriticalSection( &csLastSnapshotNum );
}

void c_Game::Load() 
{
	char Path[MAX_OSPATH];

	DQPrint( "Loading Game Server..." );

	//Initialise
	hDLL = NULL;
	vmMain = NULL;
	dllEntry = NULL;
	for( int i=0; i<MAX_CONFIGSTRINGS; ++i ) ConfigString[i] = NULL;
	ZeroMemory( Client, sizeof(s_Client)*MAX_CLIENTS );
	ZeroMemory( bConfigStringModified, sizeof(bConfigStringModified) );
	ZeroMemory( SnapshotArray, sizeof(SnapshotArray) );
	GameEntities = NULL;
	NumGameEntities = 0;
	SizeofGameEntity = 0;
	PlayerStates = NULL;
	SizeofPlayerStates = 0;
	NumLinkedEntities = 0;
	CurrentCommand = NULL;
	ServerFrameTime = 0;
	LastSnapshotNum = 1;		//start at 1 otherwise the client thinks it already has the first snapshot
	ZeroMemory( &gamestate, sizeof(gameState_t) );

	//Open DLL

//	DQstrcpy( Path, "F:\\Quake III Arena\\TestMod\\qagamex86.dll", MAX_PATH);
//	DQstrcpy( Path, "H:\\qagamex86.dll", MAX_PATH);
	DQFS.GetDLLPath("qagamex86.dll", Path);

	if(!Path[0]) {
		CriticalError(ERR_FS, "Unable to load Game DLL (qagamex86.dll)");
	}
	hDLL = LoadLibrary(Path);

	if(!hDLL) {
		CriticalError(ERR_FS, "Unable to load Game DLL (qagamex86.dll)");
		return;
	}

	EnterCriticalSection( &cs_vmMain );
	{
		vmMain = (t_vmMain)GetProcAddress((HMODULE)hDLL, "vmMain");
		dllEntry = (t_dllEntry)GetProcAddress((HMODULE)hDLL, "dllEntry");

		//Set up callback func
		dllEntry( Callback_Game );
		//Initialise Game

		// ( int levelTime, int randomSeed, int restart );
		vmMain(GAME_INIT,ServerFrameTime,rand(),1, 0,0,0,0,0,0,0,0,0);
	}
	LeaveCriticalSection( &cs_vmMain );

	PrepareGameState();

	DQNetwork.InitialiseServer();

	SetTimer( DQRender.GetHWnd(), 1, 50, NULL );		//about 20fps

	DQPrint( "Game Server Loaded." );

	isLoaded = TRUE;
}

void c_Game::Unload()
{
	DQPrint( "Shutting Down Game Server..." );

	for(int i=0;i<MAX_CONFIGSTRINGS;++i) DQDeleteArray(ConfigString[i]);

	EnterCriticalSection( &cs_vmMain );
	{
		if(hDLL && vmMain) {
			vmMain(GAME_SHUTDOWN,0,0,0,0,0,0,0,0,0,0,0,0);
		
			FreeLibrary(hDLL);
		}
		isLoaded = FALSE;
	}
	LeaveCriticalSection( &cs_vmMain );

	DQBSP.Unload();

	for(int i=0; i<MAX_SNAPSHOTS; ++i) DQDeleteArray( SnapshotArray[i].Ent );

	hDLL = NULL;
	isLoaded = FALSE;
}

char *c_Game::ClientConnect( int ClientNum, BOOL firstTime )
{
	if(!vmMain) return "No vmMain";
	char *ErrorMessage;
	playerState_t *ps;

	EnterCriticalSection( &cs_vmMain );
	{
		ErrorMessage = (char*)vmMain( GAME_CLIENT_CONNECT, ClientNum, firstTime, 0,0,0,0,0,0,0,0,0,0 ); 
	}
	LeaveCriticalSection( &cs_vmMain );

	if(!ErrorMessage) {
		//Prepare PlayerState
		ps = GetPlayerState(ClientNum);
		ZeroMemory( ps, sizeof(playerState_t) );
		ps->clientNum = ClientNum;
		ZeroMemory( &UserCmd[ClientNum], sizeof(usercmd_t) );

		Client[ClientNum].LastReceivedSnapshotNum = -1;
	}

	return ErrorMessage;		//ErrorMessage == NULL if successful
}

void c_Game::ClientDisconnect( int ClientNum )
{
	if(!vmMain) return;

	EnterCriticalSection( &cs_vmMain );
	{
		vmMain( GAME_CLIENT_DISCONNECT, ClientNum, 0,0,0,0,0,0,0,0,0,0,0 ); 
	}
	LeaveCriticalSection( &cs_vmMain );
}

void c_Game::Update() 
{
	int i;

	if(!vmMain || !isLoaded || isPaused) return;
	c_Network::s_ClientInfo *ClientInfo;

	DQConsole.UpdateCVar( &g_NetworkInfo );

	ServerFrameTime = DQTime.GetMillisecs();

	ClientInfo = DQNetwork.ClientInfoLock();
	EnterCriticalSection( &cs_vmMain );
	{
		//Client UserInfo
		for(i=0;i<MAX_CLIENTS;++i) {
			if(ClientInfo[i].isInUse) {

				if(DQNetwork.ServerIsUserInfoModified(i)) {
					vmMain(GAME_CLIENT_USERINFO_CHANGED,i ,0,0,0,0,0,0,0,0,0,0,0);
					DQNetwork.ServerUserInfoUpdated(i);
				}

			}
		}

		//Run Game Frame
		vmMain(GAME_RUN_FRAME, ServerFrameTime, 0,0,0,0,0,0,0,0,0,0,0);
	}
	LeaveCriticalSection( &cs_vmMain );


	//Send Snapshot to each client
	++LastSnapshotNum;
	CreateBackupSnapshot();
	for(i=0;i<MAX_CLIENTS;++i) {
		if(ClientInfo[i].isInUse) {
			//Update client ping every 10s ish
			if(LastSnapshotNum%200==0) {
				GetPlayerState( i )->ping = DQNetwork.ServerGetClientPing( i );
			}

			SendSnapshot(i);
		}
	}
	SendConfigStringUpdate();

	DQNetwork.ClientInfoUnlock();
}

BOOL c_Game::ExecuteConsoleCommand( c_Command *command )
{
	char mapname[MAX_QPATH];
	BOOL ret;
	CurrentCommand = command;
	c_Network::s_ClientInfo *ClientInfo;

	if(DQstrcmpi( command->GetArg(0), "vstr nextmap", MAX_STRING_CHARS )==0) {
		DQConsole.GetCVarString( "mapname", mapname, MAX_QPATH, eAuthServer );
		DQConsole.ExecuteCommandString( va("map %s",mapname), MAX_QPATH, FALSE );

		ClientInfo = DQNetwork.ClientInfoLock();
		{
			for(int i=0;i<MAX_CLIENTS;++i) {
				if(ClientInfo[i].isInUse) {
					DQNetwork.ServerSendClientCommand( i, "map_restart" );
				}
			}
		}
		DQNetwork.ClientInfoUnlock();

		return TRUE;
	}
	if(DQWordstrcmpi( command->GetArg(0), "map_restart", MAX_STRING_CHARS )==0) {
		DQConsole.GetCVarString( "mapname", mapname, MAX_QPATH, eAuthServer );
		DQConsole.ExecuteCommandString( va("map %s",mapname), MAX_QPATH, FALSE );
		return TRUE;
	}
	
	EnterCriticalSection( &cs_vmMain );
	{
		ret = vmMain(GAME_CONSOLE_COMMAND, 0,0,0,0,0,0,0,0,0,0,0,0);
	}
	LeaveCriticalSection( &cs_vmMain );

	return ret;
}

void c_Game::ExecuteClientCommand( int ClientNum, const char *buffer, int MaxLength )
{
	c_Command command;
	const char *NextCommand;

	NextCommand = buffer;
	CurrentCommand = &command;

	EnterCriticalSection( &cs_vmMain );
	{
		while(NextCommand) {
			NextCommand = CurrentCommand->MakeCommandString( NextCommand, MaxLength );

			vmMain(GAME_CLIENT_COMMAND,ClientNum, 0,0,0,0,0,0,0,0,0,0,0);
		}
	}
	LeaveCriticalSection( &cs_vmMain );
}

void c_Game::ClientUpdateUserInfo( int ClientNum )
{
	c_Network::s_ClientInfo *ClientInfo;

	if(!vmMain) {
		Error( ERR_GAME, "Game not initialised" );
		return;
	}

	ClientInfo = DQNetwork.ClientInfoLock();
	{
		if(ClientInfo[ClientNum].isInUse) {

			if(DQNetwork.ServerIsUserInfoModified(ClientNum)) {
				EnterCriticalSection( &cs_vmMain );
				{
					vmMain(GAME_CLIENT_USERINFO_CHANGED,ClientNum ,0,0,0,0,0,0,0,0,0,0,0);
				}
				LeaveCriticalSection( &cs_vmMain );

				DQNetwork.ServerUserInfoUpdated(ClientNum);
			}

		}
	}
	DQNetwork.ClientInfoUnlock();
}

void c_Game::ClientThink( int ClientNum, usercmd_t *usercmd, int ConfirmedSnapshotNum )
{
	if(ClientNum<0 || ClientNum>=MAX_CLIENTS) return;

	//Update LastReceivedSnapshotNum
	EnterCriticalSection( &csLastSnapshotNum );
	{
		if(ConfirmedSnapshotNum==-1) {		//Client requests non-delta compressed snapshot
			Client[ClientNum].LastReceivedSnapshotNum = -1;
		}

		if(ConfirmedSnapshotNum>LastSnapshotNum) {
			Error(ERR_NETWORK, "DPServer : Invalid packet - LastSnapshotNum invalid");
			return;
		}
		if(ConfirmedSnapshotNum>Client[ClientNum].LastReceivedSnapshotNum) {
			Client[ClientNum].LastReceivedSnapshotNum = ConfirmedSnapshotNum;
		}
	}
	LeaveCriticalSection( &csLastSnapshotNum );

	//Update usercmd_t
	EnterCriticalSection( &cs_vmMain );
	{
		memcpy( &UserCmd[ClientNum], usercmd, sizeof(usercmd_t) );
		vmMain(GAME_CLIENT_THINK, ClientNum, 0,0,0,0,0,0,0,0,0,0,0);
	}
	LeaveCriticalSection( &cs_vmMain );
}

void c_Game::ClientBegin( int ClientNum )
{
	EnterCriticalSection( &cs_vmMain );
	{
		vmMain(GAME_CLIENT_BEGIN,ClientNum, 0,0,0,0,0,0,0,0,0,0,0);
	}
	LeaveCriticalSection( &cs_vmMain );
}

void c_Game::GetUserCmd( int ClientNum, usercmd_t *usercmd )
{
	if(ClientNum<0 || ClientNum>=MAX_CLIENTS) return;

	memcpy( usercmd, &UserCmd[ClientNum], sizeof(usercmd_t) );
}

//*******************Send Snapshot**************************************

//Create a reference snapshot to compare against for delta-compression
void c_Game::CreateBackupSnapshot()
{
	int i;
	s_BackupSnapshot *BackupSnapshot;
	sharedEntity_t *ptrGameEnt;

	//Backup snapshot contains only non-client customized information
	BackupSnapshot = &SnapshotArray[LastSnapshotNum%MAX_SNAPSHOTS];
	BackupSnapshot->NumEntities							= 0;
	BackupSnapshot->serverTime							= ServerFrameTime;
	BackupSnapshot->SnapshotNum							= LastSnapshotNum;

	DQDeleteArray(BackupSnapshot->Ent);
	BackupSnapshot->Ent = (s_EntityWrapper*) DQNewVoid( s_EntityWrapper[NumLinkedEntities] );

	for(i=0;i<NumLinkedEntities;++i) {
		ptrGameEnt = GetGameEntity( LinkedEntities[i] );

		memcpy( &BackupSnapshot->Ent[BackupSnapshot->NumEntities].s, &ptrGameEnt->s, sizeof(entityState_t) );
		BackupSnapshot->Ent[BackupSnapshot->NumEntities].svFlags		= ptrGameEnt->r.svFlags;
		BackupSnapshot->Ent[BackupSnapshot->NumEntities].SingleClient	= ptrGameEnt->r.singleClient;

	}
	BackupSnapshot->NumEntities = NumLinkedEntities;
}

void c_Game::GetConfigString( int num, char *buffer, int bufferSize )
{
	if(num<0 || num>=MAX_CONFIGSTRINGS) {
		buffer[0] = '\0';
		return;
	}
	if(ConfigString[num]==NULL) {
		buffer[0] = '\0';
		return;
	}

	DQstrcpy(buffer, ConfigString[num], min( bufferSize, MAX_INFO_STRING) );		
}

void c_Game::SetConfigString( int num, const char *string )
{
	if(num<0 || num>=MAX_CONFIGSTRINGS)
		return;

	if(ConfigString[num]) {
		if(DQstrcmpi(ConfigString[num], string, MAX_INFO_STRING)==0) return;	//slow?
		DQDeleteArray( ConfigString[num] );
	}
	ConfigString[num] = (char*)DQNewVoid( char[DQstrlen(string, MAX_INFO_STRING)+1] );
	
	DQstrcpy(ConfigString[num], string, MAX_INFO_STRING);
	bConfigStringModified[num] = TRUE;
}

void c_Game::PrepareGameState()
{
	int i, offset = 1;
	char ServerInfo[MAX_INFO_STRING];

	DQConsole.GetServerInfo(ServerInfo, MAX_INFO_STRING);
	SetConfigString( CS_SERVERINFO, ServerInfo );
	//TODO : SystemInfo config string

	gamestate.dataCount = 0;
	gamestate.stringData[0] = '\0';
	for(i=0;i<MAX_CONFIGSTRINGS;++i) {
		if(!ConfigString[i] || ConfigString[i][0] == '\0') {
			gamestate.stringOffsets[i] = 0;
			continue;
		}
		gamestate.stringOffsets[i] = offset;
		offset += DQstrcpy(&gamestate.stringData[offset], ConfigString[i], MAX_INFO_STRING)+1;
		++gamestate.dataCount;
	}
}

void c_Game::SendConfigStringUpdate()
{
	int i,num,pos,size;
	s_ConfigStringUpdatePacket CSU{};
	//CSU Compressed
	BYTE OutputPacket[sizeof(s_ConfigStringUpdatePacket)]{};
	BYTE *out;

	num = 0;
	pos = 0;

	for(i=0;i<MAX_CONFIGSTRINGS;++i) {
		if(bConfigStringModified[i]) {

			//Add Config String to update packet
			CSU.stringNumber[num] = i;
			CSU.stringOffsets[num] = pos;
			if(pos>MAX_GAMESTATE_CHARS-MAX_INFO_STRING) {
				Error(ERR_NETWORK, "ConfigString update too big (>MAX_GAMESTATE_CHARS)");
				return;
			}
			if(!ConfigString[i]) {
				pos += DQstrcpy( &CSU.stringData[pos], "", 1)+1;
			}
			else {
				pos += DQstrcpy( &CSU.stringData[pos], ConfigString[i], MAX_INFO_STRING)+1;
			}
			++num;

			bConfigStringModified[i] = FALSE;
		}
	}

	//Only send if there is a change
	if(num==0) return;

	CSU.NumConfigStrings = num;

	//Compress packet
	//NB. ConfigStrings are put in packet in order of their number
	out = OutputPacket;
	*out = ePacketConfigStringUpdate;
	++out;
	*(unsigned short *)(out)	= CSU.NumConfigStrings;
	out += sizeof(unsigned short);
	memcpy( out, CSU.stringNumber, sizeof(unsigned short)*num );
	out += sizeof(unsigned short)*num;
	memcpy( out, CSU.stringOffsets, sizeof(int)*num );
	out += sizeof(int)*num;
	memcpy( out, CSU.stringData, pos );
	out += pos;

	size = (int)out - (int)OutputPacket;

	DQNetwork.ServerSendToAll( OutputPacket, size, 0, DPNSEND_GUARANTEED | DPNSEND_PRIORITY_HIGH );
}


//Makes a record of where the game entities are stored, how many, how big, etc.
void c_Game::LocateGameData( sharedEntity_t *gEnts, int numGEntities, int sizeofGEntity, 
							playerState_t *clients, int sizeofPlayerState )
{
	GameEntities = gEnts;
	NumGameEntities = numGEntities;
	SizeofGameEntity = sizeofGEntity;
	PlayerStates = clients;
	SizeofPlayerStates = sizeofPlayerState;
}

void c_Game::LinkEntity(sharedEntity_t *Entity)
{
	D3DXVECTOR3 vec;
	int i;
	float *origin;

	if(NumLinkedEntities>=MAX_GENTITIES) {
		Error(ERR_GAME, "LinkEntity: Too many Game Entities");
		return;
	}
	int num = Entity->s.number;

	if(num>NumGameEntities) { Error(ERR_GAME, "Cannot link non-existant entity"); return; }

	for(i=0;i<NumLinkedEntities;++i) {
		if(LinkedEntities[i] == num) {
			break;
		}
	}
	if(i==NumLinkedEntities) {
		//Create new linked entity
		LinkedEntities[NumLinkedEntities++] = num;
	}

	//Increment Linkcount
	++Entity->r.linkcount;
	Entity->r.linked = qtrue;

	//Set s.solid
	//Reference : cg_predict.c, Line 89 onwards

	//s.solid can be :
	// - SOLID_BMODEL : Entity is an inline model (regardless of whether it is solid or not)
	// - BBox : solid is an encoded min/max, see ref.
	// s.solid is nothing to do with SOLID_BBOX or SOLID_BSP - those are BotAI

	if (Entity->s.solid == SOLID_BMODEL) {
		D3DXVECTOR3 Min, Max;

		i = DQBSP.GetBrushModelIndex( Entity );

		DQBSP.GetInlineModelBounds( i, Min, Max );
		vec = 0.5f*(Min+Max);
		MakeFloat3FromD3DXVec3( Entity->r.currentOrigin, vec );
		//Calculate relative Min/Max
		Min -= vec;
		Max -= vec;
		MakeFloat3FromD3DXVec3( Entity->r.mins, Min );
		MakeFloat3FromD3DXVec3( Entity->r.maxs, Max );
	}
	else {
		i = (int)max( Entity->r.maxs[0], -Entity->r.mins[0] );
		i = max( i, (int)max( Entity->r.maxs[1], -Entity->r.mins[1] ) );
		Entity->s.solid = (i&255);
		
		i = (int)-Entity->r.mins[2];
		Entity->s.solid |= (i&255)<<8;

		i = (int)Entity->r.maxs[2] + 32;
		Entity->s.solid |= (i&255)<<16;
	}

/*	if( Entity->r.svFlags & SVF_USE_CURRENT_ORIGIN || num<MAX_CLIENTS) {
		origin = Entity->r.currentOrigin;
	}
	else {
		origin = Entity->s.origin;
	}
*/

	origin = Entity->r.currentOrigin;
	//NB. mins is -ve
	//Entity->r.absmin = origin + Entity->r.mins - 1
	Entity->r.absmin[0] = origin[0] + Entity->r.mins[0] - 1;
	Entity->r.absmin[1] = origin[1] + Entity->r.mins[1] - 1;
	Entity->r.absmin[2] = origin[2] + Entity->r.mins[2] - 1;

	Entity->r.absmax[0] = origin[0] + Entity->r.maxs[0] + 1;
	Entity->r.absmax[1] = origin[1] + Entity->r.maxs[1] + 1;
	Entity->r.absmax[2] = origin[2] + Entity->r.maxs[2] + 1;
}

void c_Game::UnlinkEntity(sharedEntity_t *Entity)
{
	int num = Entity->s.number;
	int i = 0;

	if(Entity->r.linked == qfalse) return;

	for(int i=0;i<NumLinkedEntities;++i) {
		if(LinkedEntities[i] == num) {
			Entity->r.linked = qfalse;
			break;
		}
	}
	if(i==NumLinkedEntities) return;		//If Entity not found

	for(;i<NumLinkedEntities-1;++i) {
		LinkedEntities[i] = LinkedEntities[i+1];
	}
	--NumLinkedEntities;
}

//Iterate through linked entities and check which are inside the box
int c_Game::EntitiesInBox( D3DXVECTOR3 *min, D3DXVECTOR3 *max, int *list, int maxcount )
{
	int i, numlist;
	sharedEntity_t *ent;

	numlist = 0;
	for(i=0;i<NumLinkedEntities;++i) {
		ent = GetGameEntity( LinkedEntities[i] );

		//mins/maxs is a AABB about identity origin
		//absmins/absmaxs is BB with 1 unit pad centred on model origin
		if( ent->r.absmin[0]>max->x || ent->r.absmin[1]>max->y || ent->r.absmin[2]>max->z
		 || ent->r.absmax[0]<min->x || ent->r.absmax[1]<min->y || ent->r.absmax[2]<min->z)
			continue;

		list[numlist++] = LinkedEntities[i];
		if(numlist>=maxcount) break;
	}
	return numlist;
}

// perform an exact check against inline brush models of non-square shape
// returns true if AABB mins->maxs collides with the brush of ent
BOOL c_Game::EntityContact( D3DXVECTOR3 *min, D3DXVECTOR3 *max, sharedEntity_t *ent )
{
	if(ent->r.bmodel) {
		c_BSP::s_Trace Trace;
		D3DXVECTOR3 ZeroVec = D3DXVECTOR3(0,0,0);
		
		DQBSP.CreateNewTrace( Trace, &ZeroVec, &ZeroVec, min, max, 0xFFFFFFFF );

		DQBSP.TraceInlineModel( Trace, DQBSP.GetBrushModelIndex( ent ) );

		if(Trace.StartSolid) return TRUE;
		else return FALSE;
	}
	if( (ent->r.absmin[0]+1)>max->x || (ent->r.absmin[1]+1)>max->y || (ent->r.absmin[2]+1)>max->z
	 || (ent->r.absmax[0]-1)<min->x || (ent->r.absmax[1]-1)<min->y || (ent->r.absmax[2]-1)<min->z)
		return FALSE;

	return TRUE;
}

int c_Game::PointContents( D3DXVECTOR3 *Point, int PassEntityNum)
{
	int i, contents = 0;
	sharedEntity_t *ent;

	//Check against BSP first
	contents = DQBSP.PointContents( Point );

	for(i=0;i<NumLinkedEntities;++i) {
		ent = GetGameEntity( LinkedEntities[i] );

		//Skip PassEntityNum
		if(ent->s.number == PassEntityNum || ent->r.ownerNum == PassEntityNum 
		 || (ent->r.ownerNum>0 && GetGameEntity( ent->r.ownerNum )->r.ownerNum == PassEntityNum) )
			continue;

		//Check if point is outside entity box
		if( Point->x<ent->r.absmin[0] || Point->y<ent->r.absmin[1] || Point->z<ent->r.absmin[2]
		 || Point->x>ent->r.absmax[0] || Point->y>ent->r.absmax[1] || Point->z>ent->r.absmax[2])
			continue;

		contents |= ent->r.contents;

	}

	return contents;
	
}

void c_Game::TraceBox( trace_t *results, D3DXVECTOR3 *start, D3DXVECTOR3 *end, D3DXVECTOR3 *min, 
					  D3DXVECTOR3 *max, int PassEntityNum, int contentmask )
{
	int i;
	sharedEntity_t *ent;
	c_BSP::s_Trace Trace;
	D3DXVECTOR3 EntityPosition, EntityExtent;

DQProfileSectionStart( 14, "Game TraceBox" );

	DQBSP.CreateNewTrace( Trace, start, end, min, max, contentmask);

	//Trace entities first
	for(i=0;i<NumLinkedEntities;++i) {
		ent = GetGameEntity( LinkedEntities[i] );

		if(!(ent->r.contents & contentmask)) continue;
		//Skip PassEntityNum
		if(ent->s.number == PassEntityNum || ent->r.ownerNum == PassEntityNum 
		 || (ent->r.ownerNum<=ENTITYNUM_WORLD && ent->r.ownerNum>=0 && GetGameEntity( ent->r.ownerNum )->r.ownerNum == PassEntityNum) )
			continue;

		if(ent->r.bmodel == qtrue) {
			//trap_SetBrushModel
			DQBSP.TraceInlineModel( Trace, DQBSP.GetBrushModelIndex(ent) );
			continue;
		}

		//Calculate entity midpoint and extent
		EntityExtent = ( - MakeD3DXVec3FromFloat3(ent->r.mins) + MakeD3DXVec3FromFloat3(ent->r.maxs) ) / 2.0f;
		EntityPosition = MakeD3DXVec3FromFloat3(ent->r.currentOrigin) + MakeD3DXVec3FromFloat3(ent->r.maxs) - EntityExtent;

		DQBSP.TraceAgainstAABB( &Trace, EntityPosition, EntityExtent, ent->r.contents, ent->s.number );

	}

	//Trace BSP
	if( Trace.CollisionFraction > 0.0f ) {
		DQBSP.TraceBox( Trace );
	}

	DQBSP.ConvertTraceToResults( &Trace, results );

DQProfileSectionEnd( 14 );
}

void c_Game::SetPauseState( BOOL _isPaused )
{
	EnterCriticalSection( &cs_vmMain );
	{
		isPaused = _isPaused;
	}
	LeaveCriticalSection( &cs_vmMain );
}

//GAME ONLY
//****************************
void c_BSP::SetBrushModel( sharedEntity_t *pEntity, int InlineModelNum )
{
	if( InlineModelNum<=0 || InlineModelNum>=NumInlineModels ) {
		Error( ERR_BSP, "Invalid SetBrushModel Model" );
		return;
	}
		
	//Use the brush model of InlineModelNum for clipping with this entity
	Model[InlineModelNum].ent = pEntity;

	//Set SOLID_BMODEL
	pEntity->s.solid = SOLID_BMODEL;

	//Set bmodel
	pEntity->r.bmodel = qtrue;
}

int c_BSP::GetBrushModelIndex( sharedEntity_t *pEntity )
{
	int i;

	if( !pEntity ) return 0;

	for( i=0; i<NumInlineModels; ++i) {
		if( Model[i].ent == pEntity ) break;
	}
	if( i==NumInlineModels ) return 0;

	return i;
}
//****************************


#undef GetGameEntity
#undef GetPlayerState