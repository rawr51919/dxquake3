// DXQuake3 Source
// Copyright (c) 2003 Richard Geary (richard.geary@dial.pipex.com)
//
//
//	DirectPlay Server interface
//

#include "stdafx.h"

void c_Network::InitialiseServer()
{
	int i;

	dxcheckOK( CoCreateInstance( CLSID_DirectPlay8Server, NULL, CLSCTX_INPROC_SERVER, IID_IDirectPlay8Server, (LPVOID*)&DirectPlayServer ) );
	dxcheckOK( DirectPlayServer->Initialize( NULL, DirectPlayServerCallback, DPNINITIALIZE_HINT_LANSESSION) );

	//Get Network Address
	SafeRelease( DPHostAddress );		
	DPHostAddress = CreateAddress( "localhost" );

	dxcheckOK( DirectPlayServer->Host( &ApplicationDesc, &DPHostAddress, 1, NULL, NULL, 
		(void*)-1, NULL ) );

	//Create Group AllClients
	DPN_GROUP_INFO Group;
	ZeroMemory( &Group, sizeof(DPN_GROUP_INFO) );
	Group.dwSize = sizeof(DPN_GROUP_INFO);
	dxcheckOK( DirectPlayServer->CreateGroup( &Group, NULL, NULL, NULL, DPNCREATEGROUP_SYNC ) );

	//Zero memory
	for(i=0;i<MAX_CLIENTS;++i) {
		EnterCriticalSection( &csClientInfo );
		{
			SafeRelease( Server.ClientInfo[i].ClientAddress );
			ZeroMemory( &Server.ClientInfo[i], sizeof(s_ClientInfo) );
		}
		LeaveCriticalSection( &csClientInfo );
	}

	isServerLoaded = TRUE;
}

void c_Network::ServerAddPlayerToGroup( DPNID PlayerID )
{
	DPNHANDLE h;
	dxcheckOK( DirectPlayServer->AddPlayerToGroup( Server.DPGroupAllClients, PlayerID, NULL, &h, 0 ) );
}

void c_Network::ServerRemovePlayerFromGroup( DPNID PlayerID )
{
	DPNHANDLE h;
	DirectPlayServer->RemovePlayerFromGroup( Server.DPGroupAllClients, PlayerID, NULL, &h, 0 );
}

BOOL c_Network::ServerSendTo(  int ClientNum, void *data, int size, int TimeoutMS, DWORD DPNSendToFlags )
{
	return ServerAsyncSendTo( ClientNum, data, size, TimeoutMS, DPNSendToFlags, &ASyncSendTo, NULL );
}

//NB. ClientInfo already locked
BOOL c_Network::ServerAsyncSendTo( int ClientNum, void *data, int size, int TimeoutMS, DWORD DPNSendToFlags, DPNHANDLE *ASyncHandle, void *pASyncContext )
{
	HRESULT hr;
	DPN_BUFFER_DESC BufferDesc;
	BufferDesc.dwBufferSize = size;
	BufferDesc.pBufferData = (BYTE*)data;

	if(!Server.ClientInfo[ClientNum].DPN_ClientID) return FALSE;

	hr = DirectPlayServer->SendTo( Server.ClientInfo[ClientNum].DPN_ClientID, 
		&BufferDesc, 1, 
		TimeoutMS,
		pASyncContext, ASyncHandle, DPNSendToFlags );

	if(FAILED(hr)) {
		if(hr==DPNERR_CONNECTIONLOST) DQPrint( va("DPServer::SendTo - Connection Lost (Client %d)",ClientNum) );
		else if(hr==DPNERR_INVALIDFLAGS) DQPrint( "DPServer::SendTo - Invalid Flags" );
		else if(hr==DPNERR_INVALIDPARAM) DQPrint( "DPServer::SendTo - Invalid Parameters" );
		else if(hr==DPNERR_INVALIDPLAYER) DQPrint( va("DPServer::SendTo - Invalid Player ID (Client %d)",ClientNum) );
		else if(hr==DPNERR_TIMEDOUT) DQPrint( va("DPServer::SendTo - Timed Out (Client %d)",ClientNum) );
		else Error( ERR_NETWORK, "DPServer::SendTo - Unknown Error" );
		
		ServerRemovePlayerFromGroup( Server.ClientInfo[ClientNum].DPN_ClientID );
		Server.ClientInfo[ClientNum].DPN_ClientID = NULL;
		Server.ClientInfo[ClientNum].isInUse = FALSE;
		return FALSE;
	}
	return TRUE;
}

BOOL c_Network::ServerSendToAll( void *data, int size, int TimeoutMS, DWORD DPSendToFlags )
{
	HRESULT hr;
	DPN_BUFFER_DESC BufferDesc;
	BufferDesc.dwBufferSize = size;
	BufferDesc.pBufferData = (BYTE*)data;

	hr = DirectPlayServer->SendTo( Server.DPGroupAllClients, 
		&BufferDesc, 1, 
		TimeoutMS,
		NULL, &ASyncSendAll, DPSendToFlags );

	if(FAILED(hr)) {
		if(hr==DPNERR_CONNECTIONLOST) DQPrint( "DPServer::SendTo - Connection Lost (Group AllClients)" );
		else if(hr==DPNERR_INVALIDFLAGS) DQPrint( "DPServer::SendTo - Invalid Flags" );
		else if(hr==DPNERR_INVALIDPARAM) DQPrint( "DPServer::SendTo - Invalid Parameters" );
		else if(hr==DPNERR_INVALIDPLAYER) DQPrint( "DPServer::SendTo - Invalid Player ID (Group AllClients)" );
		else if(hr==DPNERR_TIMEDOUT) DQPrint( "DPServer::SendTo - Timed Out (Group AllClients)" );
		else Error( ERR_NETWORK, "DPServer::SendTo - Unknown Error" );
		
		return FALSE;
	}
	return TRUE;
}

void c_Network::ServerSendClientCommand( int ClientNum, char *text )
{
	s_ServerCommandPacket ServerCommand;
	int size;

	ServerCommand.IdentityChar = ePacketServerCommand;
	ServerCommand.stringSize   = DQstrlen( text, MAX_STRING_CHARS )+1;
	DQstrcpy( ServerCommand.CommandString, text, MAX_STRING_CHARS );

	size = (int)&ServerCommand.CommandString[ServerCommand.stringSize]-(int)&ServerCommand;

	if(ClientNum<0) {
		ServerSendToAll( &ServerCommand, size, 0, DPNSEND_GUARANTEED | DPNSEND_PRIORITY_HIGH );
	}
	else {
		ServerSendTo( ClientNum, &ServerCommand, size, 0, DPNSEND_GUARANTEED | DPNSEND_PRIORITY_HIGH );
	}
}

//Return the UserInfo string for ClientNum, containing all the USERINFO cvars (denoted by CVAR_USERINFO)
void c_Network::ServerGetUserInfo(int ClientNum, char *buffer, int bufsize)
{
	EnterCriticalSection( &csClientInfo );
	{
		if(ClientNum<0 || ClientNum>=MAX_CLIENTS) {
			buffer[0] = '\0';
		}
		else {
			DQstrcpy(buffer, Server.ClientInfo[ClientNum].UserInfoString, bufsize);
		}
	}
	LeaveCriticalSection( &csClientInfo );
}

BOOL c_Network::ServerIsUserInfoModified( int ClientNum )
{
	BOOL x;
	if(ClientNum<0 || ClientNum>=MAX_CLIENTS) return FALSE;

	EnterCriticalSection( &csClientInfo );
	{
		x = Server.ClientInfo[ClientNum].bUserInfoModified;
	}
	LeaveCriticalSection( &csClientInfo );

	return x;
}

void c_Network::ServerUserInfoUpdated( int ClientNum )
{
	if(ClientNum<0 || ClientNum>=MAX_CLIENTS) return;

	EnterCriticalSection( &csClientInfo );
	{
		Server.ClientInfo[ClientNum].bUserInfoModified = FALSE;
	}
	LeaveCriticalSection( &csClientInfo );
}

//ClientInfo already locked
void c_Network::ServerSetUserInfo(int ClientNum, char *buffer, int bufsize)
{
	if(ClientNum<0 || ClientNum>=MAX_CLIENTS)
		return;

	if(!buffer) {
		Error( ERR_NETWORK, "Server : Invalid user info" );
		return;
	}

	EnterCriticalSection( &csClientInfo );
	{
		DQstrcpy(Server.ClientInfo[ClientNum].UserInfoString, buffer, bufsize);
		Server.ClientInfo[ClientNum].bUserInfoModified = TRUE;
	}
	LeaveCriticalSection( &csClientInfo );
}

void c_Network::ServerPrintConnectionStatus( int ClientNum )
{
	if(ClientNum<0 || ClientNum>=MAX_CLIENTS)
		return;

	HRESULT hr;
	DPN_CONNECTION_INFO ConnectionInfo;
	ConnectionInfo.dwSize = sizeof(DPN_CONNECTION_INFO);

	EnterCriticalSection( &csClientInfo );
	{
		if(Server.ClientInfo[ClientNum].isInUse) {
			hr = DirectPlayServer->GetConnectionInfo( Server.ClientInfo[ClientNum].DPN_ClientID, &ConnectionInfo, 0 );
			if(FAILED(hr)) {
				Error( ERR_NETWORK, va("Server failed to get connection info for client %d",ClientNum) );
			}
			else {
				DQPrint( va("Server ConnectionInfo : Client %d Ping ms %d  Average bytes/s %d Peak bps %d Bytes Retried %d Bytes Dropped %d", ClientNum,
					ConnectionInfo.dwRoundTripLatencyMS, ConnectionInfo.dwThroughputBPS, ConnectionInfo.dwPeakThroughputBPS, ConnectionInfo.dwBytesRetried, ConnectionInfo.dwBytesDropped) );
			}
		}
	}
	LeaveCriticalSection( &csClientInfo );
}

void c_Network::ServerSendSnapshot( int ClientNum, void *SnapshotPacket, int PacketSize )
{
	EnterCriticalSection( &csClientInfo );
	{
		//If previous snapshot hasn't been sent yet, cancel it and send the new one
		if(!Server.ClientInfo[ClientNum].bSnapshotSent) {
			DirectPlayServer->CancelAsyncOperation( Server.ClientInfo[ClientNum].ASyncSnapshot, 0 );
		}
		Server.ClientInfo[ClientNum].bSnapshotSent = FALSE;

		//Add MAX_CLIENTS to clientnum so we don't get confused with NULL contexts
		ServerAsyncSendTo( ClientNum, SnapshotPacket, PacketSize, 200, DPNSEND_NONSEQUENTIAL | DPNSEND_PRIORITY_HIGH, &Server.ClientInfo[ClientNum].ASyncSnapshot, (void*)(ClientNum+MAX_CLIENTS) );
	}
	LeaveCriticalSection( &csClientInfo );
}

int c_Network::ServerGetClientPing( int ClientNum )
{
	HRESULT hr;
	DPNID ClientID = NULL;
	DPN_CONNECTION_INFO ConnectionInfo;
	ConnectionInfo.dwSize = sizeof(DPN_CONNECTION_INFO);

	if( !isServerLoaded ) return 0;
	if(ClientNum<0 || ClientNum>=MAX_CLIENTS)
		return 0;

	EnterCriticalSection( &csClientInfo );
	{
		if(Server.ClientInfo[ClientNum].isInUse) {
			ClientID = Server.ClientInfo[ClientNum].DPN_ClientID;
		}
	}
	LeaveCriticalSection( &csClientInfo );

	if(!ClientID) return 0;

	hr = DirectPlayServer->GetConnectionInfo( ClientID, &ConnectionInfo, 0 );
	if(FAILED(hr)) {
		Error( ERR_NETWORK, "Server failed to get Client connection info" );
		return 0;
	}

	return ConnectionInfo.dwRoundTripLatencyMS;
}

//************ Direct Play Callback Function ********************************************

HRESULT WINAPI DirectPlayServerCallback( PVOID pvUserContext, DWORD dwMessageType, PVOID pMessage ) {
	return DQNetwork.ServerCallback( pvUserContext, dwMessageType, pMessage );
}

#ifdef DebugServerMessages
#define DebugName(x) DQPrint( va("DPServerMsg : %s",#x) )
#else
#define DebugName(x)
#endif

HRESULT c_Network::ServerCallback( PVOID pvUserContext, DWORD dwMessageType, PVOID pMessage )
{
	char *Buffer;
	int bufsize;
	int ClientNum;

	switch( dwMessageType ) {

	case DPN_MSGID_ADD_PLAYER_TO_GROUP:
		break;
	case DPN_MSGID_CREATE_GROUP:
		
		Server.DPGroupAllClients = ((DPNMSG_CREATE_GROUP*)pMessage)->dpnidGroup;
		break;

	case DPN_MSGID_ENUM_HOSTS_QUERY:
		DebugName(DPN_MSGID_ENUM_HOSTS_QUERY);

		#define Msg ((DPNMSG_ENUM_HOSTS_QUERY*)pMessage)
		int size;

		size = min( MAX_INFO_STRING, Msg->dwMaxResponseDataSize );

		EnterCriticalSection( &csHostQuery );
		{
			Buffer = (char*)DQNewVoid( char[size] );
			DQConsole.GetServerInfo( Buffer, size );
			Msg->pvResponseData = Buffer;
			Msg->dwResponseDataSize = size;
		}
		LeaveCriticalSection( &csHostQuery );

		#undef Msg
		break;
	case DPN_MSGID_INDICATE_CONNECT:
		DebugName(DPN_MSGID_INDICATE_CONNECT);
		//Connect message recieved
		#define Msg ((DPNMSG_INDICATE_CONNECT*)pMessage)
		char *ErrorMessage;
		s_ConnectInfo *ConnectInfo;

		EnterCriticalSection( &csClientInfo );
		{
			ClientNum = GetNewClientNum();

			if(ClientNum<0) {
				Buffer = (char*)DQNewVoid( char[MAX_STRING_CHARS] );
				bufsize = DQstrcpy( Buffer, "Too many clients", MAX_STRING_CHARS );
				Msg->pvReplyData = Buffer;
				Msg->dwReplyDataSize = bufsize + 1;
				LeaveCriticalSection( &csClientInfo );
				return -1;
			}

			Server.ClientInfo[ClientNum].isInUse = TRUE;

			//Connect data (User Info string)
			ServerSetUserInfo( ClientNum, (char*)(Msg->pvUserConnectData), Msg->dwUserConnectDataSize );
			DQGame.ClientUpdateUserInfo( ClientNum );

			//Call Q3 Game DLL - Client Connect
			ErrorMessage = DQGame.ClientConnect( ClientNum, TRUE );		//TODO : firstTime!=TRUE

			if(ErrorMessage) {
				Error( ERR_NETWORK, va("Unable to connect client : %s", ErrorMessage) );

				Buffer = (char*)DQNewVoid( char[MAX_STRING_CHARS] );
				bufsize = DQstrcpy( Buffer, ErrorMessage, MAX_STRING_CHARS );
				Msg->pvReplyData = Buffer;
				Msg->dwReplyDataSize = bufsize + 1;

				LeaveCriticalSection( &csClientInfo );
				return -1;		//fail
			}

			Msg->pAddressPlayer->Duplicate( &Server.ClientInfo[ClientNum].ClientAddress );
			Msg->pvPlayerContext = (void*)ClientNum;

			//Reply data - send gamestate and clientnum
			DQGame.PrepareGameState();
			ConnectInfo = DQNew(s_ConnectInfo);
			memcpy( &ConnectInfo->gamestate, &DQGame.gamestate, sizeof(gameState_t) );
			ConnectInfo->ClientNum = ClientNum;
			ConnectInfo->LastSnapshotNum = DQGame.LastSnapshotNum;
			Msg->pvReplyData = ConnectInfo;
			Msg->dwReplyDataSize = sizeof(s_ConnectInfo);

			//Debug message
			DQPrint( va("DPServer : Client %d initialised connection - sending gamestate",ClientNum) );
		}
		LeaveCriticalSection( &csClientInfo );

		#undef Msg
		break;

	case DPN_MSGID_CREATE_PLAYER:
		DebugName(DPN_MSGID_CREATE_PLAYER);
		#define Msg ((DPNMSG_CREATE_PLAYER*)pMessage)
		EnterCriticalSection( &csClientInfo );
		{
			ClientNum = (int)Msg->pvPlayerContext;
			if(ClientNum>=0 && ClientNum<MAX_CLIENTS) {
				Server.ClientInfo[ClientNum].DPN_ClientID = Msg->dpnidPlayer;

				DQGame.ClientBegin( ClientNum );

				ServerAddPlayerToGroup( Msg->dpnidPlayer );
			}
		}
		LeaveCriticalSection( &csClientInfo );
		#undef Msg
		break;

	case DPN_MSGID_INDICATED_CONNECT_ABORTED:
		DebugName(DPN_MSGID_INDICATED_CONNECT_ABORTED);
		#define Msg ((DPNMSG_INDICATED_CONNECT_ABORTED*)pMessage)
		EnterCriticalSection( &csClientInfo );
		{
			ClientNum = (int)Msg->pvPlayerContext;
			if(ClientNum>=0 && ClientNum<MAX_CLIENTS) {
				if(c_Game::GetSingletonPtr()) {
					DQGame.ClientDisconnect( ClientNum );
				}
				SafeRelease( Server.ClientInfo[ClientNum].ClientAddress );
				ZeroMemory( &Server.ClientInfo[ClientNum], sizeof(s_ClientInfo) );
			}
		}
		LeaveCriticalSection( &csClientInfo );
		#undef Msg
		break;

	case DPN_MSGID_DESTROY_PLAYER:
		DebugName(DPN_MSGID_DESTROY_PLAYER);
		//Disconnect Player
		#define Msg ((DPNMSG_DESTROY_PLAYER*)pMessage)
		EnterCriticalSection( &csClientInfo );
		{
			ClientNum = (int)Msg->pvPlayerContext;

			if(ClientNum>=0 && ClientNum<MAX_CLIENTS) {
				ServerRemovePlayerFromGroup( Server.ClientInfo[ClientNum].DPN_ClientID );
				if(c_Game::GetSingletonPtr())
					DQGame.ClientDisconnect( ClientNum );
				SafeRelease( Server.ClientInfo[ClientNum].ClientAddress );
				ZeroMemory( &Server.ClientInfo[ClientNum], sizeof(s_ClientInfo) );
			}
		}
		LeaveCriticalSection( &csClientInfo );
		#undef Msg
		break;

	case DPN_MSGID_RECEIVE:
		DebugName(DPN_MSGID_RECEIVE);
		#define Msg ((DPNMSG_RECEIVE*)pMessage)
		void *data;
		data = (void*) (((U8*)(Msg->pReceiveData))+1);

		ClientNum = (int)Msg->pvPlayerContext;
		if(ClientNum<0 || ClientNum>=MAX_CLIENTS) {
			Error(ERR_NETWORK, "DPServer: Invalid ClientNum");
			break;
		}

		switch( ((U8*)Msg->pReceiveData)[0] ) {
		case ePacketUserCmd:

			ServerReceiveUserCmd( ClientNum, data, Msg->dwReceiveDataSize-1 );
			break;

		case ePacketUserInfo:
			EnterCriticalSection( &csClientInfo );
			{
				ServerSetUserInfo( ClientNum, (char*)data, Msg->dwReceiveDataSize-1 );
				DQGame.ClientUpdateUserInfo( ClientNum );
			}
			LeaveCriticalSection( &csClientInfo );
			break;

		case ePacketClientCommand:
			DQGame.ExecuteClientCommand( ClientNum, (char*)data, Msg->dwReceiveDataSize-1 );
			break;

		default:
			Error(ERR_NETWORK, va("DPServer: Invalid message : Identity char %d, size %d bytes",((U8*)Msg->pReceiveData)[0],Msg->dwReceiveDataSize) );
			break;
		};
		#undef Msg
		break;

	case DPN_MSGID_RETURN_BUFFER:
		DebugName(DPN_MSGID_RETURN_BUFFER);
		void *ptr;
		
		ptr = ((DPNMSG_RETURN_BUFFER*)pMessage)->pvBuffer;
		DQDeleteArray( ptr );

		break;

	case DPN_MSGID_SEND_COMPLETE:
		DebugName(DPN_MSGID_SEND_COMPLETE);
		#define Msg ((DPNMSG_SEND_COMPLETE*)pMessage)
		int ClientNum;

		if(FAILED(Msg->hResultCode)) break;

		ClientNum = ((int)Msg->pvUserContext)-MAX_CLIENTS;

		EnterCriticalSection( &csClientInfo );
		{
			if( ClientNum>=0 && ClientNum<MAX_CLIENTS ) {
				Server.ClientInfo[ClientNum].bSnapshotSent = TRUE;
			}
		}
		LeaveCriticalSection( &csClientInfo );

		#undef Msg
		break;

	case DPN_MSGID_ASYNC_OP_COMPLETE:
		break;
	case DPN_MSGID_CLIENT_INFO:
		break;
	case DPN_MSGID_DESTROY_GROUP:
		break;
	case DPN_MSGID_GROUP_INFO:
		break;
	case DPN_MSGID_HOST_MIGRATE:
		break;
	case DPN_MSGID_REMOVE_PLAYER_FROM_GROUP:
		break;
	case DPN_MSGID_PEER_INFO:
		break;
	case DPN_MSGID_SERVER_INFO:
		break;
	case DPN_MSGID_TERMINATE_SESSION:
		break;
	default:
		Error(ERR_NETWORK, "Invalid Server Callback Message");
	};
	return S_OK;
}
#undef DebugName
