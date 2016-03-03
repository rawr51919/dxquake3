// DXQuake3 Source
// Copyright (c) 2003 Richard Geary (richard.geary@dial.pipex.com)
//
//	DirectPlay Client interface
//

#include "stdafx.h"

//******************************* Direct Play Calls ******************************

void c_Network::InitialiseClient()
{
	CRITICAL_SECTION backup[3];

	dxcheckOK( CoCreateInstance( CLSID_DirectPlay8Client, NULL, CLSCTX_INPROC_SERVER, IID_IDirectPlay8Client, (LPVOID*)&DirectPlayClient ) );
	dxcheckOK( DirectPlayClient->Initialize( NULL, DirectPlayClientCallback, DPNINITIALIZE_HINT_LANSESSION) );

	//Get Network Address
	SafeRelease( DPDeviceAddress );
	DPDeviceAddress = CreateAddress();

	//Zero Memory
	backup[0] = Client.csGameState;
	backup[1] = Client.csServerCommands;
	backup[2] = Client.csSnapshot;
	ZeroMemory( &Client, sizeof(s_Client) );
	Client.csGameState		= backup[0];
	Client.csServerCommands = backup[1];
	Client.csSnapshot		= backup[2];
	for(int i=0;i<MAX_SNAPSHOTS;++i) Client.SnapshotArray[i].SnapshotNum = -1; //unused snapshot
	Client.CurrentSnapshotNum = -1;

	isClientLoaded = TRUE;
}

void c_Network::RefreshLocalServers()
{
	if(!isClientLoaded) InitialiseClient();

	DQPrint( "Scanning for local servers..." );

	EnterCriticalSection( &csHostList );
	{

		if(ASyncEnumHandle) {
			LeaveCriticalSection( &csHostList );
			DirectPlayClient->CancelAsyncOperation( NULL, DPNCANCEL_ENUM );
			EnterCriticalSection( &csHostList );
			ASyncEnumHandle = NULL;
		}

		isSearchingForServers = TRUE;
		ServerCount = 0;
		HostList->DestroyChain();

		DPN_APPLICATION_DESC AppDesc;
		ZeroMemory(&AppDesc, sizeof(DPN_APPLICATION_DESC));
		AppDesc.dwSize = sizeof(DPN_APPLICATION_DESC);
		AppDesc.guidApplication = g_guidApp;

		HRESULT hr = DirectPlayClient->EnumHosts( &AppDesc, NULL, 
			DPDeviceAddress, NULL, 0, 0, 0, 20000, (void*)eNetMsg_EnumHosts, &ASyncEnumHandle, 0 );
		if(FAILED(hr)) Error(ERR_NETWORK, "EnumHosts error");
		if(hr==DPNERR_INVALIDDEVICEADDRESS) Error(ERR_NETWORK, "Invalid Network Device Address");
		if(hr==DPNERR_INVALIDHOSTADDRESS) Error(ERR_NETWORK, "Invalid Host Device Address");
		if(hr==DPNERR_USERCANCEL) Error(ERR_NETWORK, "User Cancelled EnumHosts");
	}
	LeaveCriticalSection( &csHostList );
}

void c_Network::Disconnect()
{
	ZeroMemory( &UIClientState, sizeof(uiClientState_t) );
	
	if(isClientLoaded) {
		DirectPlayClient->CancelAsyncOperation( NULL, DPNCANCEL_ALL_OPERATIONS );
		dxcheckOK( DirectPlayClient->Close(DPNCLOSE_IMMEDIATE) );
		SafeRelease( DirectPlayClient );
	}
	isClientLoaded = FALSE;

	if(isServerLoaded) {
		dxcheckOK( DirectPlayServer->Close(0) );
		SafeRelease( DirectPlayServer );
	}
	isServerLoaded = FALSE;

	EnterCriticalSection( &Client.csSnapshot );
	{
		for(int i=0;i<MAX_SNAPSHOTS;++i) Client.SnapshotArray[i].SnapshotNum = -1;
	}
	LeaveCriticalSection( &Client.csSnapshot );

	EnterCriticalSection( &Client.csServerCommands );
	{
		ClientServerCommandsChain.DestroyChain();
	}
	LeaveCriticalSection( &Client.csServerCommands );

	Client.isConnected = FALSE;
}

void c_Network::ConnectToServer(char *ServerName)
{
	if(!isClientLoaded) InitialiseClient();
	HRESULT hr;
	char IPAddress[100];

	DQPrint( va("Connect to %s",ServerName ));

	//if localhost, DPHostAddress is already set by DPServer
	if(!DQConsole.IsLocalServer()) {
		SafeRelease( DPHostAddress );
		DPHostAddress = CreateAddress( ServerName );
	}

	if(ASyncConnectHandle) {
		DirectPlayClient->CancelAsyncOperation( NULL, DPNCANCEL_CONNECT );
		ASyncConnectHandle = NULL;
	}

	//Create AppDesc for Connect
	DPN_APPLICATION_DESC AppDesc;
    ZeroMemory(&AppDesc, sizeof(DPN_APPLICATION_DESC));
    AppDesc.dwSize = sizeof(DPN_APPLICATION_DESC);
    AppDesc.guidApplication = g_guidApp;
	AppDesc.dwFlags = DPNSESSION_CLIENT_SERVER;

	//Send UserInfo on connect
	DQConsole.UpdateUserInfo();

	hr = DirectPlayClient->Connect( &AppDesc, DPHostAddress, DPDeviceAddress, NULL, NULL,
		DQConsole.UserInfoString, DQConsole.UserInfoStringSize, //UserConnect Data
		(void*)eNetMsg_Connect, &ASyncConnectHandle, 0 );

	if(FAILED(hr)) { 
		if( hr == DPNERR_HOSTREJECTEDCONNECTION ) Error(ERR_NETWORK, "DirectPlayClient->Connect failed : Host rejected connection")
		else if( hr == DPNERR_INVALIDAPPLICATION ) Error(ERR_NETWORK, "DirectPlayClient->Connect failed : Host and client are different programs")
		else if( hr == DPNERR_INVALIDDEVICEADDRESS ) Error(ERR_NETWORK, "DirectPlayClient->Connect failed : Local device failure")
		else if( hr == DPNERR_INVALIDFLAGS ) Error(ERR_NETWORK, "DirectPlayClient->Connect failed : Invalid parameters")
		else if( hr == DPNERR_INVALIDHOSTADDRESS ) Error(ERR_NETWORK, "DirectPlayClient->Connect failed : Invalid host address")
		else if( hr == DPNERR_INVALIDINSTANCE ) Error(ERR_NETWORK, "DirectPlayClient->Connect failed : Invalid GUID")
		else if( hr == DPNERR_INVALIDINTERFACE ) Error(ERR_NETWORK, "DirectPlayClient->Connect failed : Invalid Interface")
		else if( hr == DPNERR_NOCONNECTION ) Error(ERR_NETWORK, "DirectPlayClient->Connect failed : Unable to make connection")
		else if( hr == DPNERR_NOTHOST ) Error(ERR_NETWORK, "DirectPlayClient->Connect failed : Host address is not a host")
		else if( hr == DPNERR_SESSIONFULL ) Error(ERR_NETWORK, "DirectPlayClient->Connect failed : Host is full")
		else if( hr == DPNERR_ALREADYCONNECTED ) Error(ERR_NETWORK, "DirectPlayClient->Connect failed : Already connected")
		else Error(ERR_NETWORK, "DirectPlayClient->Connect failed : Unknown reason");

		return; 
	}

	//Set up UI ClientState (for connection screen)
	ZeroMemory( &UIClientState, sizeof(uiClientState_t) );
	DQstrcpy( UIClientState.servername, ServerName, MAX_STRING_CHARS );

	GetIPFromDPAddress( DPHostAddress, IPAddress, 100 );
	DQConsole.SetCVarString( "net_ip", IPAddress, eAuthServer );
	DQConsole.SetCVarString( "ip", IPAddress, eAuthServer );

	for(int i=0;i<MAX_SNAPSHOTS;++i) Client.SnapshotArray[i].SnapshotNum = -1;
	Client.CurrentServerTime = 0;
	Client.SnapshotClientTime = DQTime.GetPerfMillisecs();
}

//*****************User Info*********************

BOOL c_Network::ClientSendUserInfo(const char *UserInfoString, DWORD StringSize)
{
	if(!isClientLoaded) {
		Error(ERR_NETWORK, "SendUserInfo - DPClient not loaded");
		return FALSE;
	}
	HRESULT hr;

	//Make Client Packet
	char ClientPacket[MAX_INFO_STRING+1];
	int  PacketSize;
	ClientPacket[0] = (U8)ePacketUserInfo;
	PacketSize = min(StringSize,MAX_INFO_STRING);
	memcpy( &ClientPacket[1], UserInfoString, PacketSize );

	DPN_BUFFER_DESC BufferDesc;
	BufferDesc.dwBufferSize = PacketSize+1;
	BufferDesc.pBufferData = (BYTE*)ClientPacket;

	hr = DirectPlayClient->Send( &BufferDesc, 1, 0, //No Timeout
		NULL, &Client.ASyncSendUserInfo, DPNSEND_GUARANTEED | DPNSEND_PRIORITY_HIGH );

	if(FAILED(hr)) {
		DQPrint( "^1Disconnected from Server" );
		return FALSE;
	}

	return TRUE;
}

//**************Client Commands**************************

BOOL c_Network::ClientSendCommandToServer( const char *buffer )
{
	HRESULT hr;

	//Make Client Packet
	char ClientPacket[MAX_STRING_CHARS+1];
	int  PacketSize;
	ClientPacket[0] = (U8)ePacketClientCommand;
	PacketSize = DQstrlen( buffer, MAX_STRING_CHARS )+2;
	DQstrcpy( &ClientPacket[1], buffer, PacketSize-1 );

	DPN_BUFFER_DESC BufferDesc;
	BufferDesc.dwBufferSize = PacketSize;
	BufferDesc.pBufferData = (BYTE*)ClientPacket;

	hr = DirectPlayClient->Send( &BufferDesc, 1, 0, //No Timeout
		NULL, &Client.ASyncSendClientCommand, DPNSEND_GUARANTEED | DPNSEND_PRIORITY_HIGH );

	if(FAILED(hr)) {
		DQPrint( "^1Disconnected from Server" );
		return FALSE;
	}

	return TRUE;
}

//********************************************************

//**************User Cmds*********************************

BOOL c_Network::ClientGetUserCmd( int CmdNum, usercmd_t *usercmd )
{
	int Index;

	if(CmdNum>Client.CurrentUserCmdNum || CmdNum<=0) {
		//usercmd doesn't exist
		ZeroMemory( usercmd, sizeof(usercmd_t) );
		return FALSE;
	}

	if(Client.UserCmd[Client.LastUserCmd].UserCmdNum == CmdNum) {
		memcpy(usercmd, &Client.UserCmd[Client.LastUserCmd].UserCmd, sizeof(usercmd_t) );
		return TRUE;
	}

	//search for usercmd (assume consequtive usercmds)
	Index = Client.LastUserCmd - (Client.UserCmd[Client.LastUserCmd].UserCmdNum-CmdNum);
	if(Index<0) Index += CMD_BACKUP;
	if(Index<0) Index = 0;
	if(Client.UserCmd[Index].UserCmdNum == CmdNum) {
		memcpy(usercmd, &Client.UserCmd[Index].UserCmd, sizeof(usercmd_t) );
		return TRUE;
	}

	//brute force search
	Index = 0;
	while(Index<CMD_BACKUP) {
		if(Client.UserCmd[Index].UserCmdNum == CmdNum) {
			memcpy(usercmd, &Client.UserCmd[Index].UserCmd, sizeof(usercmd_t) );
			return TRUE;
		}
		++Index;
	};
	//not found
	ZeroMemory( usercmd, sizeof(usercmd_t) );
	return FALSE;
}

//************Snapshots***********************

//Checked: This function locks and unlocks csSnapshot properly
BOOL c_Network::ClientGetSnapshot( int SnapshotNum, snapshot_t *Snapshot )
{
	int SnapArrayPos;


	EnterCriticalSection( &Client.csSnapshot );

	if( SnapshotNum > Client.CurrentSnapshotNum ) goto NotFoundSnapshot;

	//Try to predict position of SnapshotNum in array (assume sequentially recieved snapshots)
	SnapArrayPos = SnapshotNum - Client.SnapshotArray[0].SnapshotNum;

	if(SnapArrayPos<0) SnapArrayPos += MAX_SNAPSHOTS;
	
	if(SnapArrayPos>=0 && SnapArrayPos<MAX_SNAPSHOTS) {
		if(Client.SnapshotArray[SnapArrayPos].SnapshotNum == SnapshotNum) {
			goto FoundSnapshot;
		}
	}

	//brute force
	for(SnapArrayPos=0; SnapArrayPos<MAX_SNAPSHOTS; ++SnapArrayPos) {
		if(Client.SnapshotArray[SnapArrayPos].SnapshotNum == SnapshotNum) {
			goto FoundSnapshot;
		}
	}
	goto NotFoundSnapshot;

FoundSnapshot:
	memcpy(Snapshot, &Client.SnapshotArray[SnapArrayPos].Snapshot, sizeof(snapshot_t) );
	if(!DQClientGame.isLoaded) Snapshot->snapFlags |= SNAPFLAG_NOT_ACTIVE;

	LeaveCriticalSection( &Client.csSnapshot );
	return TRUE;

NotFoundSnapshot:
	ZeroMemory( Snapshot, sizeof(snapshot_t) );

	LeaveCriticalSection( &Client.csSnapshot );
	return FALSE;
}

int c_Network::ClientUpdateServerTime()
{
	double currentTime;
	int x;

	EnterCriticalSection( &Client.csSnapshot );
	{
		currentTime = DQTime.GetPerfMillisecs();

		//CurrentServerTime mustn't go backwards - freeze for a while to catch up if necessary
		x = Client.CurrentSnapshotTime + (int)(currentTime - Client.SnapshotClientTime);
		if(Client.Ping>20) x += Client.Ping/2;
		if( x>=Client.CurrentServerTime ) Client.CurrentServerTime = x;

		x = Client.CurrentServerTime;
	}
	LeaveCriticalSection( &Client.csSnapshot );

	return x;
}

//***************************************************************************************

void c_Network::ClientGetLastSnapshotInfo( int *snapshotNum, int *serverTime )
{
	EnterCriticalSection( &Client.csSnapshot );
	{
		if(	Client.CurrentSnapshotNum == -1 ) *snapshotNum = 0;
		else *snapshotNum = Client.CurrentSnapshotNum;
		*serverTime = Client.CurrentSnapshotTime;
	}
	LeaveCriticalSection( &Client.csSnapshot );
}

void c_Network::ClientGetUIState(uiClientState_t *state)
{
	memcpy(state, &UIClientState, sizeof(uiClientState_t));
}

void c_Network::ClientGetGameState( gameState_t *gamestate )
{ 
	EnterCriticalSection( &Client.csGameState );
	{
		memcpy( gamestate, &Client.gamestate, sizeof(gameState_t) );
	}
	LeaveCriticalSection( &Client.csGameState );
}

//Called by ClientCallback
void c_Network::ClientReceivedGameState( gameState_t *_gamestate )
{
	EnterCriticalSection( &Client.csGameState );
	{
		memcpy(&Client.gamestate, _gamestate, sizeof(gameState_t));
	}
	LeaveCriticalSection( &Client.csGameState );
}

//Called by UI DLL
BOOL c_Network::ClientGetConfigString( int iTemp1, char *buffer, int bufsize )
{
	if(iTemp1<0 || iTemp1>=MAX_CONFIGSTRINGS) return FALSE;

	EnterCriticalSection( &Client.csGameState );
	{
		DQstrcpy( buffer, &Client.gamestate.stringData[Client.gamestate.stringOffsets[iTemp1]], min(MAX_INFO_STRING, bufsize) );
	}
	LeaveCriticalSection( &Client.csGameState );

	return TRUE;
}

c_Command *c_Network::ClientGetServerCommand( int ServerCommandNum )
{
	c_Command *command = NULL;
	s_ChainNode<s_ServerCommand> *node;

	EnterCriticalSection( &Client.csServerCommands );
	{
		if(ServerCommandNum>Client.LastServerCommandSequenceNum) {
			command = NULL;
		}
		else {
			//Work backwards from most recent
			node = ClientServerCommandsChain.GetLastNode();
			while(node) {
				if(node->current->SequenceNum==ServerCommandNum) {
					//Mark for deletion
					node->current->SequenceNum = -1;
					command = &node->current->Command;
					break;
				}
				node = node->prev;
			};
		}
	}
	LeaveCriticalSection( &Client.csServerCommands );

	return command;
}

void c_Network::ClientCleanServerCommandChain()
{
	s_ChainNode<s_ServerCommand> *node, *nextnode;

	EnterCriticalSection( &Client.csServerCommands );
	{
		node = ClientServerCommandsChain.GetFirstNode();
		while(node) {
			nextnode = node->next;
			if(node->current->SequenceNum==-1) {
				ClientServerCommandsChain.RemoveNode2( node );
			}
			node = nextnode;
		};
	}
	LeaveCriticalSection( &Client.csServerCommands );
}

void c_Network::ClientReceivedSnapshot( s_SnapshotPacket *Snapshot, int PacketSize )
{
	s_SnapshotWrapper *out;
	s_SnapshotWrapper *ReferenceSnapshotWrapper;		//for Delta Compression
	snapshot_t		  *ReferenceSnapshot;
	entityState_t	  *outEnt, *refEnt;
	int i,u;
	BYTE *pEntityPacket;
	BYTE *pEndByte;

	pEndByte = (BYTE*)Snapshot + PacketSize;

	ClientUpdateServerTime();

	EnterCriticalSection( &Client.csSnapshot );
	{
		out = &Client.SnapshotArray[Snapshot->ThisSnapshotNum%MAX_SNAPSHOTS];

		//Find Reference snapshot (Delta Compression)
		if(Snapshot->ReferenceSnapshotNum == -1) {
			ReferenceSnapshot = NULL; //DC not used
		}
		else {
			ReferenceSnapshotWrapper = &Client.SnapshotArray[Snapshot->ReferenceSnapshotNum%MAX_SNAPSHOTS];

			//Double check this is the right snapshot
			if(ReferenceSnapshotWrapper->SnapshotNum != Snapshot->ReferenceSnapshotNum) {
				//Server Snapshot is using unavailable Reference snapshot
				//Throw away and tell server to not use DC for next snapshot
				Client.CurrentSnapshotNum = -1;
				LeaveCriticalSection( &Client.csSnapshot );
				Error(ERR_NETWORK, "ClientGame : Reference Snapshot unavailable. Requesting Server for full snapshot.");
				return;
			}
			else {
				ReferenceSnapshot = &ReferenceSnapshotWrapper->Snapshot;
			}
		}

		//Process snapshot

		//Copy across data
		out->SnapshotNum				= Snapshot->ThisSnapshotNum;
		out->Snapshot.serverTime		= Snapshot->serverTime;
		out->Snapshot.ping				= Client.Ping;

		//Player State Packet
		pEntityPacket = (BYTE*)&Snapshot->ps;
		if( *pEntityPacket == 0x00 ) {
			++pEntityPacket;
			memcpy( &out->Snapshot.ps, pEntityPacket, sizeof(playerState_t) );
			pEntityPacket += sizeof(playerState_t);
			//Clear the entries which shouldn't have been transmitted
			out->Snapshot.ps.pmove_framecount = 0;
			out->Snapshot.ps.jumppad_frame = 0;
			out->Snapshot.ps.entityEventSequence = 0;
		}
		else if( ReferenceSnapshot && (*pEntityPacket == 0xFF) ) {
			++pEntityPacket;
			//Delta compressed ps
			DQGame.ReadPlayerStatePacket( &pEntityPacket, &out->Snapshot.ps, &ReferenceSnapshot->ps );		//NB. Static function
		}
		else {
			Client.CurrentSnapshotNum = -1;
			LeaveCriticalSection( &Client.csSnapshot );
			Error( ERR_NETWORK, "ClientGame : invalid snapshot packet" );
			return;
		}

		//Client added data
		out->Snapshot.snapFlags			= 0;
		ZeroMemory( out->Snapshot.areamask, sizeof(byte)*MAX_MAP_AREA_BYTES );

		EnterCriticalSection( &Client.csServerCommands );
		{
			out->Snapshot.numServerCommands	= ClientServerCommandsChain.GetQuantity();		
			out->Snapshot.serverCommandSequence = Client.LastServerCommandSequenceNum;
		}
		LeaveCriticalSection( &Client.csServerCommands );


		out->Snapshot.numEntities = Snapshot->numEntities;

		BOOL isChanged;
		BYTE *bfChangedComponents;
		short EntityNumber;

		for(i=0; i<Snapshot->numEntities; ++i) {
			outEnt = &out->Snapshot.entities[i];

			if( pEntityPacket >= pEndByte ) {
				Error( ERR_NETWORK, "Client : Snapshot Packet is truncated. Requesting full snapshot from Server." );
				Client.CurrentSnapshotNum = -1;
				LeaveCriticalSection( &Client.csSnapshot );
				return;
			}

			EntityNumber = *(short*)pEntityPacket;
			pEntityPacket += sizeof(short);

/*			if(EntityNumber & 0x8000) {
				//whole entityState_t
				memcpy( outEnt, pEntityPacket, sizeof(entityState_t) );
				pEntityPacket += sizeof(entityState_t);
				continue;
			}*/

/*			if(!ReferenceSnapshot) {
				//Should have had EntityNumber & 0x8000
				Error( ERR_NETWORK, "ClientGame : Corrupt snapshot" );
				Client.CurrentSnapshotNum = -1;
				LeaveCriticalSection( &Client.csSnapshot );
				return;
			}*/

			isChanged = (EntityNumber & 0x4000)?TRUE:FALSE;
			EntityNumber &= ~0x4000;

			//Find refEnt
			refEnt = &ZeroEntity;
			if(ReferenceSnapshot) {
				for(u=0; u<ReferenceSnapshot->numEntities; ++u) {
					if(ReferenceSnapshot->entities[u].number == EntityNumber) {
						refEnt = &ReferenceSnapshot->entities[u];
						break;
					}
				}
			}

/*			if(!refEnt) {
				Error( ERR_NETWORK, "ClientGame : Unable to find reference entity. Requesting Server for full snapshot." );
				Client.CurrentSnapshotNum = -1;
				LeaveCriticalSection( &Client.csSnapshot );
				return;
			}*/

			if(isChanged) {
				//Unravel Delta-Compression
				bfChangedComponents = pEntityPacket;
				pEntityPacket += 4;

				//int number
				outEnt->number = (EntityNumber&MAX_GENTITIES-1);

				//int eType
				if(bfChangedComponents[0]&0x02) {
					outEnt->eType = *(int*)pEntityPacket;
					pEntityPacket += sizeof(int);
				}
				else {
					outEnt->eType = refEnt->eType;
				}
				//int eFlags
				if(bfChangedComponents[0]&0x04) {
					outEnt->eFlags = *(int*)pEntityPacket;
					pEntityPacket += sizeof(int);
				}
				else {
					outEnt->eFlags = refEnt->eFlags;
				}
				//trajectory_t	pos
				if(bfChangedComponents[0]&0x08) {
					outEnt->pos.trType = (trType_t) (*(BYTE*)pEntityPacket);
					pEntityPacket += sizeof(BYTE);

					outEnt->pos.trTime = *(int*)pEntityPacket;
					pEntityPacket += sizeof(int);

					outEnt->pos.trDuration = *(int*)pEntityPacket;
					pEntityPacket += sizeof(int);

					outEnt->pos.trBase[0] = ((float*)pEntityPacket)[0];
					outEnt->pos.trBase[1] = ((float*)pEntityPacket)[1];
					outEnt->pos.trBase[2] = ((float*)pEntityPacket)[2];
					pEntityPacket += 3*sizeof(float);

					outEnt->pos.trDelta[0] = ((float*)pEntityPacket)[0];
					outEnt->pos.trDelta[1] = ((float*)pEntityPacket)[1];
					outEnt->pos.trDelta[2] = ((float*)pEntityPacket)[2];
					pEntityPacket += 3*sizeof(float);
				}
				else {
					memcpy( &outEnt->pos, &refEnt->pos, sizeof(trajectory_t) );
				}
				//trajectory_t	apos
				if(bfChangedComponents[0]&0x10) {
					outEnt->apos.trType = (trType_t) (*(BYTE*)pEntityPacket);
					pEntityPacket += sizeof(BYTE);

					outEnt->apos.trTime = *(int*)pEntityPacket;
					pEntityPacket += sizeof(int);

					outEnt->apos.trDuration = *(int*)pEntityPacket;
					pEntityPacket += sizeof(int);

					outEnt->apos.trBase[0] = ((float*)pEntityPacket)[0];
					outEnt->apos.trBase[1] = ((float*)pEntityPacket)[1];
					outEnt->apos.trBase[2] = ((float*)pEntityPacket)[2];
					pEntityPacket += 3*sizeof(float);

					outEnt->apos.trDelta[0] = ((float*)pEntityPacket)[0];
					outEnt->apos.trDelta[1] = ((float*)pEntityPacket)[1];
					outEnt->apos.trDelta[2] = ((float*)pEntityPacket)[2];
					pEntityPacket += 3*sizeof(float);
				}
				else {
					memcpy( &outEnt->apos, &refEnt->apos, sizeof(trajectory_t) );
				}
				//int time
				if(bfChangedComponents[0]&0x20) {
					outEnt->time = *(int*)pEntityPacket;
					pEntityPacket += sizeof(int);
				}
				else {
					outEnt->time = refEnt->time;
				}
				//int time2
				if(bfChangedComponents[0]&0x40) {
					outEnt->time2 = *(int*)pEntityPacket;
					pEntityPacket += sizeof(int);
				}
				else {
					outEnt->time2 = refEnt->time2;
				}
				//vec3_t origin
				if(bfChangedComponents[0]&0x80) {
					outEnt->origin[0] = ((float*)pEntityPacket)[0];
					outEnt->origin[1] = ((float*)pEntityPacket)[1];
					outEnt->origin[2] = ((float*)pEntityPacket)[2];
					pEntityPacket += 3*sizeof(float);
				}
				else {
					memcpy( &outEnt->origin, &refEnt->origin, sizeof(vec3_t) );
				}
				//vec3_t origin2
				if(bfChangedComponents[1]&0x01) {
					outEnt->origin2[0] = ((float*)pEntityPacket)[0];
					outEnt->origin2[1] = ((float*)pEntityPacket)[1];
					outEnt->origin2[2] = ((float*)pEntityPacket)[2];
					pEntityPacket += 3*sizeof(float);
				}
				else {
					memcpy( &outEnt->origin2, &refEnt->origin2, sizeof(vec3_t) );
				}
				//vec3_t angles
				if(bfChangedComponents[1]&0x02) {
					outEnt->angles[0] = ((float*)pEntityPacket)[0];
					outEnt->angles[1] = ((float*)pEntityPacket)[1];
					outEnt->angles[2] = ((float*)pEntityPacket)[2];
					pEntityPacket += 3*sizeof(float);
				}
				else {
					memcpy( &outEnt->angles, &refEnt->angles, sizeof(vec3_t) );
				}
				//vec3_t angles2
				if(bfChangedComponents[1]&0x04) {
					outEnt->angles2[0] = ((float*)pEntityPacket)[0];
					outEnt->angles2[1] = ((float*)pEntityPacket)[1];
					outEnt->angles2[2] = ((float*)pEntityPacket)[2];
					pEntityPacket += 3*sizeof(float);
				}
				else {
					memcpy( &outEnt->angles2, &refEnt->angles2, sizeof(vec3_t) );
				}
				//int otherEntityNum
				if(bfChangedComponents[1]&0x08) {
					outEnt->otherEntityNum = *(int*)pEntityPacket;
					pEntityPacket += sizeof(int);
				}
				else {
					outEnt->otherEntityNum = refEnt->otherEntityNum;
				}
				//int otherEntityNum2
				if(bfChangedComponents[1]&0x10) {
					outEnt->otherEntityNum2 = *(int*)pEntityPacket;
					pEntityPacket += sizeof(int);
				}
				else {
					outEnt->otherEntityNum2 = refEnt->otherEntityNum2;
				}
				//int groundEntityNum
				if(bfChangedComponents[1]&0x20) {
					outEnt->groundEntityNum = *(int*)pEntityPacket;
					pEntityPacket += sizeof(int);
				}
				else {
					outEnt->groundEntityNum = refEnt->groundEntityNum;
				}
				//int constantLight
				if(bfChangedComponents[1]&0x40) {
					outEnt->constantLight = *(int*)pEntityPacket;
					pEntityPacket += sizeof(int);
				}
				else {
					outEnt->constantLight = refEnt->constantLight;
				}
				//int loopSound
				if(bfChangedComponents[1]&0x80) {
					outEnt->loopSound = *(int*)pEntityPacket;
					pEntityPacket += sizeof(int);
				}
				else {
					outEnt->loopSound = refEnt->loopSound;
				}
				//int modelindex
				if(bfChangedComponents[2]&0x01) {
					outEnt->modelindex = *(int*)pEntityPacket;
					pEntityPacket += sizeof(int);
				}
				else {
					outEnt->modelindex = refEnt->modelindex;
				}
				//int modelindex2
				if(bfChangedComponents[2]&0x02) {
					outEnt->modelindex2 = *(int*)pEntityPacket;
					pEntityPacket += sizeof(int);
				}
				else {
					outEnt->modelindex2 = refEnt->modelindex2;
				}
				//int clientNum
				if(bfChangedComponents[2]&0x04) {
					outEnt->clientNum = *(int*)pEntityPacket;
					pEntityPacket += sizeof(int);
				}
				else {
					outEnt->clientNum = refEnt->clientNum;
				}
				//int frame
				if(bfChangedComponents[2]&0x08) {
					outEnt->frame = *(int*)pEntityPacket;
					pEntityPacket += sizeof(int);
				}
				else {
					outEnt->frame = refEnt->frame;
				}
				//int solid
				if(bfChangedComponents[2]&0x10) {
					outEnt->solid = *(int*)pEntityPacket;
					pEntityPacket += sizeof(int);
				}
				else {
					outEnt->solid = refEnt->solid;
				}
				//int event
				if(bfChangedComponents[2]&0x20) {
					outEnt->event = *(int*)pEntityPacket;
					pEntityPacket += sizeof(int);
				}
				else {
					outEnt->event = refEnt->event;
				}
				//int eventParm
				if(bfChangedComponents[2]&0x40) {
					outEnt->eventParm = *(int*)pEntityPacket;
					pEntityPacket += sizeof(int);
				}
				else {
					outEnt->eventParm = refEnt->eventParm;
				}
				//int powerups
				if(bfChangedComponents[2]&0x80) {
					outEnt->powerups = *(int*)pEntityPacket;
					pEntityPacket += sizeof(int);
				}
				else {
					outEnt->powerups = refEnt->powerups;
				}
				//int weapon
				if(bfChangedComponents[3]&0x01) {
					outEnt->weapon = *(int*)pEntityPacket;
					pEntityPacket += sizeof(int);
				}
				else {
					outEnt->weapon = refEnt->weapon;
				}
				//int legsAnim
				if(bfChangedComponents[3]&0x02) {
					outEnt->legsAnim = *(int*)pEntityPacket;
					pEntityPacket += sizeof(int);
				}
				else {
					outEnt->legsAnim = refEnt->legsAnim;
				}
				//int torsoAnim
				if(bfChangedComponents[3]&0x04) {
					outEnt->torsoAnim = *(int*)pEntityPacket;
					pEntityPacket += sizeof(int);
				}
				else {
					outEnt->torsoAnim = refEnt->torsoAnim;
				}
				//int generic1
				if(bfChangedComponents[3]&0x08) {
					outEnt->generic1 = *(int*)pEntityPacket;
					pEntityPacket += sizeof(int);
				}
				else {
					outEnt->generic1 = refEnt->generic1;
				}
			}//if(isChanged)

			else {
				//is identical to reference snapshot
				memcpy( &out->Snapshot.entities[i], refEnt, sizeof(entityState_t) );
			}
		} //for each snapshot entity

		Client.ClientNum = Snapshot->ps.clientNum;

		if(Snapshot->ThisSnapshotNum>Client.CurrentSnapshotNum) {
			Client.CurrentSnapshotNum = Snapshot->ThisSnapshotNum;

			//Update ping every 10s ish
			if(Client.CurrentSnapshotNum%200==0) Client.Ping = ClientGetPing();

			Client.SnapshotClientTime = DQTime.GetPerfMillisecs();
			Client.CurrentSnapshotTime = Snapshot->serverTime;			
		}

		if(DQClientGame.cg_NetworkInfo.integer) {
			DQPrint( va("Client Network Info : Received Snapshot %d  size : %d, using snapshot %d as ref", Client.CurrentSnapshotNum, PacketSize, Snapshot->ReferenceSnapshotNum ) );
		}

	}
	LeaveCriticalSection( &Client.csSnapshot );
}

void c_Network::ClientReceivedCSU( BYTE* MessageReceived, int size )
{
	s_ConfigStringUpdatePacket CSU;		//note : in this case, we are using this struct in uncompressed form
	BYTE *Msg;
	int num, length;

	Msg = MessageReceived;

	//Uncompress the CSU Packet
	CSU.IdentityChar = *Msg;
	++Msg;
	CSU.NumConfigStrings = *(unsigned short*)Msg;
	num = CSU.NumConfigStrings;
	Msg += sizeof(unsigned short);

	memcpy( CSU.stringNumber, Msg, num*sizeof(unsigned short) );
	Msg += num * sizeof(unsigned short);

	memcpy( CSU.stringOffsets, Msg, num*sizeof(int) );
	Msg += num * sizeof(int);

	//Copy the remaining bytes to stringData
	length = size - ((int)Msg-(int)MessageReceived);
	if(length<0) {
		Error(ERR_NETWORK, "Invalid ConfigStringUpdate packet");
		return;
	}
	memcpy( CSU.stringData, Msg, length );
	Msg += length;

	//Process CSU into gameState_t
	int i, u, NextCSU, offset;
	gameState_t OldGamestate;
	s_ServerCommand *command;

	EnterCriticalSection( &Client.csGameState );
	{
		memcpy( &OldGamestate, &Client.gamestate, sizeof(gameState_t) );

		//ConfigStrings are put in packet in order of their number
		u = 0;		//position in CSU array
		offset = 0;
		for(i=0;i<MAX_CONFIGSTRINGS;++i) {

			//NextCSU is the ConfigString Number of the next string in the CSU
			if(u>=CSU.NumConfigStrings) 
				NextCSU = MAX_CONFIGSTRINGS;
			else
				NextCSU = CSU.stringNumber[u];

			if(i==NextCSU) {
				//Copy from CSU
				Client.gamestate.stringOffsets[i] = offset;
				offset += DQstrcpy( &Client.gamestate.stringData[offset], &CSU.stringData[CSU.stringOffsets[u]], MAX_INFO_STRING )+1;
				++Client.gamestate.dataCount;
				++u;
			}
			else {
				//Copy from existing gamestate
				Client.gamestate.stringOffsets[i] = offset;
				offset += DQstrcpy( &Client.gamestate.stringData[offset], &OldGamestate.stringData[OldGamestate.stringOffsets[i]], MAX_INFO_STRING )+1;
				++Client.gamestate.dataCount;
			}
		}
	}
	LeaveCriticalSection( &Client.csGameState );

	//Tell the ClientGame the ConfigStrings have been updated
	EnterCriticalSection( &Client.csServerCommands );
	{
		for(i=0;i<CSU.NumConfigStrings;++i) {
			command = ClientServerCommandsChain.AddUnit();
			command->SequenceNum	= ++Client.LastServerCommandSequenceNum;
			command->Command.MakeCommandString( va("cs %d",CSU.stringNumber[i]), 20 );
		}
	}
	LeaveCriticalSection( &Client.csServerCommands );

}

void c_Network::ClientPrintConnectionStatus()
{
	HRESULT hr;
	DPN_CONNECTION_INFO ConnectionInfo;
	ConnectionInfo.dwSize = sizeof(DPN_CONNECTION_INFO);

	if( !isClientLoaded ) return;

	hr = DirectPlayClient->GetConnectionInfo( &ConnectionInfo, 0 );
	if(FAILED(hr)) {
		Error( ERR_NETWORK, "Client failed to get connection info" );
		return;
	}

	DQPrint( va("Client ConnectionInfo : Ping ms %d  Average bytes/s %d Peak bps %d Bytes Retried %d Bytes Dropped %d",
		ConnectionInfo.dwRoundTripLatencyMS, ConnectionInfo.dwThroughputBPS, ConnectionInfo.dwPeakThroughputBPS, ConnectionInfo.dwBytesRetried, ConnectionInfo.dwBytesDropped) );
}

int c_Network::ClientGetPing()
{
	HRESULT hr;
	DPN_CONNECTION_INFO ConnectionInfo;
	ConnectionInfo.dwSize = sizeof(DPN_CONNECTION_INFO);

	if( !isClientLoaded ) return 0;

	hr = DirectPlayClient->GetConnectionInfo( &ConnectionInfo, 0 );
	if(FAILED(hr)) {
		Error( ERR_NETWORK, "Client failed to get connection info" );
		return 0;
	}

	return ConnectionInfo.dwRoundTripLatencyMS;
}

//************ Direct Play Callback Function ********************************************

HRESULT WINAPI DirectPlayClientCallback( PVOID pvUserContext, DWORD dwMessageType, PVOID pMessage ) {
	return DQNetwork.ClientCallback( pvUserContext, dwMessageType, pMessage );
}

#ifdef DebugClientMessages
#define DebugName(x) DQPrint( va("DPClientMsg : %s",#x) )
#else
#define DebugName(x)
#endif

HRESULT c_Network::ClientCallback( PVOID pvUserContext, DWORD dwMessageType, PVOID pMessage )
{
	switch( dwMessageType )
	{
	case DPN_MSGID_ENUM_HOSTS_RESPONSE:
		DebugName(DPN_MSGID_ENUM_HOSTS_RESPONSE);
		//A host has been found from an EnumHosts call
		#define Msg	((DPNMSG_ENUM_HOSTS_RESPONSE*)pMessage)
		s_ChainNode<s_Host>				*HostNode;
		s_Host							*Host;

		EnterCriticalSection( &csHostList );
		{
			//Check if host is already on the list
			HostNode = HostList->GetFirstNode();
			while(HostNode) {
				if(Msg->pApplicationDescription->guidInstance == HostNode->current->AppDesc.guidInstance)
				{
					//This host is already listed
					HostNode->current->Ping = Msg->dwRoundTripLatencyMS;
					break;
				}
				HostNode = HostNode->next;
			}

			if(!HostNode) {
				//Add host to list
				Host = HostList->AddUnitToFront();
				dxcheckOK( Msg->pAddressSender->Duplicate( &Host->Address ) );
				memcpy( (void*)&Host->AppDesc, Msg->pApplicationDescription, sizeof(DPN_APPLICATION_DESC) );

				Host->Ping = Msg->dwRoundTripLatencyMS;
				if(Msg->pvResponseData) {
					DQstrcpy( Host->ServerInfoString, (char*)Msg->pvResponseData, min( MAX_INFO_STRING, Msg->dwResponseDataSize ) );
				}
				else Host->ServerInfoString[0] = '\0';
				
				++ServerCount;
			}
		}
		LeaveCriticalSection( &csHostList );
		#undef Msg
		break;

	case DPN_MSGID_ASYNC_OP_COMPLETE:
		DebugName(DPN_MSGID_ASYNC_OP_COMPLETE);

		switch( (int)((DPNMSG_ASYNC_OP_COMPLETE*)pMessage)->pvUserContext ) {
		case eNetMsg_EnumHosts:
			EnterCriticalSection( &csHostList );
			{
				isSearchingForServers = FALSE;
				ASyncEnumHandle = NULL;
			}
			LeaveCriticalSection( &csHostList );
			break;

		case eNetMsg_Connect:
			EnterCriticalSection( &csConnecting );
			{
				ASyncConnectHandle = NULL;
			}
			LeaveCriticalSection( &csConnecting );
			break;

		default: break;
		};

		break;

	case DPN_MSGID_CONNECT_COMPLETE:
		DebugName(DPN_MSGID_CONNECT_COMPLETE);
		s_ConnectInfo *ConnectInfo;
		DWORD ReturnVal;
		HRESULT hr;

		#define Msg	((DPNMSG_CONNECT_COMPLETE*)pMessage)
		EnterCriticalSection( &csConnecting );
		{
			hr = Msg->hResultCode;
			if( FAILED(hr) ) {
				if(Msg->pvApplicationReplyData) {
					Error( ERR_NETWORK, va("Unable to connect to server : %s", (char*)Msg->pvApplicationReplyData ) );
				}
				else {
					if( hr == DPNERR_HOSTREJECTEDCONNECTION ) Error(ERR_NETWORK, "DPClient : Connect failed : Host rejected connection")
					else if( hr == DPNERR_INVALIDAPPLICATION ) Error(ERR_NETWORK, "DPClient : Connect failed : Host and client are different programs")
					else if( hr == DPNERR_INVALIDDEVICEADDRESS ) Error(ERR_NETWORK, "DPClient : Connect failed : Local device failure")
					else if( hr == DPNERR_INVALIDFLAGS ) Error(ERR_NETWORK, "DPClient : Connect failed : Invalid parameters")
					else if( hr == DPNERR_INVALIDHOSTADDRESS ) Error(ERR_NETWORK, "DPClient : Connect failed : Invalid host address")
					else if( hr == DPNERR_INVALIDINSTANCE ) Error(ERR_NETWORK, "DPClient : Connect failed : Invalid GUID")
					else if( hr == DPNERR_INVALIDINTERFACE ) Error(ERR_NETWORK, "DPClient : Connect failed : Invalid Interface")
					else if( hr == DPNERR_NOCONNECTION ) Error(ERR_NETWORK, "DPClient : Connect failed : Unable to make connection")
					else if( hr == DPNERR_NOTHOST ) Error(ERR_NETWORK, "DPClient : Connect failed : Host address is not a host")
					else if( hr == DPNERR_SESSIONFULL ) Error(ERR_NETWORK, "DPClient : Connect failed : Host is full")
					else if( hr == DPNERR_ALREADYCONNECTED ) Error(ERR_NETWORK, "DPClient : Connect failed : Already connected")
					else Error(ERR_NETWORK, "DPClient : Connect failed : Unknown reason");
				}

				//disconnect
				if(c_Console::GetSingletonPtr())
					DQConsole.ExecuteCommandString( "disconnect", MAX_STRING_CHARS, FALSE );

				//Go to Main Menu
				if(c_UI::GetSingletonPtr())
					DQUI.OpenMenu();

				LeaveCriticalSection( &csConnecting );
				return -1;
			}
			Client.isConnected = TRUE;
			Client.DPN_PlayerID = Msg->dpnidLocal;

			//Reply data is GameState
			if(Msg->dwApplicationReplyDataSize != sizeof(s_ConnectInfo) ) {
				Error(ERR_NETWORK, "Reply data is not a s_ConnectInfo struct");
				DQConsole.ExecuteCommandString( "disconnect", MAX_STRING_CHARS, TRUE );

				LeaveCriticalSection( &csConnecting );
				return -1;
			}

			ConnectInfo = (s_ConnectInfo*)Msg->pvApplicationReplyData;
			Client.ClientNum = ConnectInfo->ClientNum;
			DQClientGame.ClientNum = Client.ClientNum;
			DQClientGame.LoadHint_SnapshotNum = ConnectInfo->LastSnapshotNum;

			ClientReceivedGameState( &ConnectInfo->gamestate );

			DQPrint( va("ClientGame connected as client %d",Client.ClientNum) );

			DQClientGame.LoadMe = TRUE;		//load ClientGame on next iteration of game loop
			ReturnVal = WaitForSingleObject( DQClientGame.ClientLoadedEvent, 20000 );
		}
		LeaveCriticalSection( &csConnecting );

		if(ReturnVal==WAIT_OBJECT_0) break;		//success
		else return -1;			//failed to load

		#undef Msg

	case DPN_MSGID_RECEIVE:
		DebugName(DPN_MSGID_RECEIVE);
		#define Msg ((DPNMSG_RECEIVE*)pMessage)
		s_ServerCommand *command;
		s_ServerCommandPacket *MsgCommand;
		char *NextCommand;
		char *EndMsg;

		switch( ((U8*)Msg->pReceiveData)[0] ) {
		
		case ePacketSnapshot:
			ClientReceivedSnapshot( (s_SnapshotPacket*)(Msg->pReceiveData), Msg->dwReceiveDataSize );
			break;

		case ePacketServerCommand:
			MsgCommand = (s_ServerCommandPacket*)(Msg->pReceiveData);
			EnterCriticalSection( &Client.csServerCommands );
			{
				//Convert string into c_Command's
				EndMsg				= MsgCommand->CommandString + Msg->dwReceiveDataSize;
				NextCommand			= MsgCommand->CommandString;
				while(NextCommand) {
					command = ClientServerCommandsChain.AddUnit();
					command->SequenceNum	= ++Client.LastServerCommandSequenceNum;
					NextCommand = command->Command.MakeCommandString( NextCommand, (int)EndMsg-(int)NextCommand );
				}
			}
			LeaveCriticalSection( &Client.csServerCommands );
			break;

		case ePacketConfigStringUpdate:
			ClientReceivedCSU( Msg->pReceiveData, Msg->dwReceiveDataSize );
			break;

		default:
			Error(ERR_NETWORK, va("DPClient: Invalid message : Identity char %d, size %d bytes",((U8*)Msg->pReceiveData)[0],Msg->dwReceiveDataSize) );
			break;
		};

		#undef Msg
		break;
	case DPN_MSGID_SEND_COMPLETE:
		break;

	default:
		return 0;
	};
	return S_OK;
}
#undef DebugName