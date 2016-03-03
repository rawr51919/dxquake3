// DXQuake3 Source
// Copyright (c) 2003 Richard Geary (richard.geary@dial.pipex.com)
//
// c_Network.cpp: implementation of the c_Network class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "c_Network.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

c_Network::c_Network()
{
	UIClientState.clientNum = 0;
	UIClientState.connectPacketCount = 0;
	UIClientState.connState = CA_UNINITIALIZED;
	UIClientState.messageString[0] = '\0';
	UIClientState.servername[0] = '\0';
	UIClientState.updateInfoString[0] = '\0';

	ZeroMemory( &Server, sizeof(s_Server) );
	ZeroMemory( &Client, sizeof(s_Client) );

	DirectPlayServer = NULL;
	DirectPlayClient = NULL;

	ZeroMemory( &ApplicationDesc, sizeof(DPN_APPLICATION_DESC) );
	ApplicationDesc.dwSize = sizeof(DPN_APPLICATION_DESC);
	ApplicationDesc.dwFlags = DPNSESSION_CLIENT_SERVER;
	ApplicationDesc.guidApplication = g_guidApp;
	WCHAR wSessionName[128] = {0};

    MultiByteToWideChar( CP_ACP, 0, "Temp Session Name", -1, wSessionName, 128 );
	ApplicationDesc.pwszSessionName = wSessionName;

	isClientLoaded = isServerLoaded = FALSE;
	NetworkPortNumber = 27960;

	//Initialise COM
    CoInitializeEx(NULL, COINIT_MULTITHREADED);

	InitializeCriticalSection(&csHostList);
	InitializeCriticalSection(&csHostQuery);
	InitializeCriticalSection(&csConnecting);
	InitializeCriticalSection(&csClientInfo);
	InitializeCriticalSection(&Client.csSnapshot);
	InitializeCriticalSection(&Client.csGameState);
	InitializeCriticalSection(&Client.csServerCommands);
	
	HostList = DQNew(c_Chain<s_Host>);

	DPHostAddress = NULL;
	DPDeviceAddress = NULL;

	ASyncEnumHandle = NULL;
	ASyncConnectHandle = NULL;
	ASyncSendTo = NULL;
	ASyncSendAll = NULL;
	Client.ASyncSendUserInfo = NULL;
	Client.ASyncSendUserCmd = NULL;

	isSearchingForServers = FALSE;
	ServerCount = 0;

	ZeroMemory( &ZeroEntity, sizeof(entityState_t) );
}

c_Network::~c_Network()
{
	int i;

	SafeRelease( DPHostAddress );
	SafeRelease( DPDeviceAddress );

	if( DirectPlayServer ) DirectPlayServer->Close(0);
	SafeRelease( DirectPlayServer );
	if( DirectPlayClient ) DirectPlayClient->Close(0);
	SafeRelease( DirectPlayClient );

	EnterCriticalSection( &csHostList );
	{
		DQDelete( HostList );
	}
	LeaveCriticalSection( &csHostList );
    DeleteCriticalSection( &csHostList );

	EnterCriticalSection( &csClientInfo );
	{
		for(i=0;i<MAX_CLIENTS;++i) {
			SafeRelease( Server.ClientInfo[i].ClientAddress );
		}
	}
	LeaveCriticalSection( &csClientInfo );

	DeleteCriticalSection( &csClientInfo );
    DeleteCriticalSection( &csHostQuery );
	DeleteCriticalSection( &csConnecting );
	DeleteCriticalSection( &Client.csSnapshot );
	DeleteCriticalSection( &Client.csGameState );
	DeleteCriticalSection( &Client.csServerCommands );
	
	//End COM
	CoUninitialize();
}

IDirectPlay8Address *c_Network::CreateAddress( char *HostName )
{
	IDirectPlay8Address *Address;

	dxcheckOK( CoCreateInstance( CLSID_DirectPlay8Address, NULL, CLSCTX_INPROC_SERVER, 
		IID_IDirectPlay8Address, (LPVOID*)&Address ) );
	dxcheckOK( Address->SetSP( &CLSID_DP8SP_TCPIP ) );

	if(HostName) {
		//Check if it is in IP:Port format
		int pos;
		char IP[16];
		
		pos = DQCopyUntil( IP, HostName, ':', 16);
		
		if(pos>1 && HostName[pos-1] == ':') {
			//is in IP:Port format
			NetworkPortNumber = atoi(&HostName[pos]);
			HostName = IP;
		}
		else {
			NetworkPortNumber = 27960;
		}

		//Add port
		dxcheckOK( Address->AddComponent( DPNA_KEY_PORT, &NetworkPortNumber, sizeof(DWORD), DPNA_DATATYPE_DWORD) );

		dxcheckOK( Address->AddComponent( DPNA_KEY_HOSTNAME, HostName, DQstrlen( HostName, MAX_STRING_CHARS )+1, 
			DPNA_DATATYPE_STRING_ANSI ) );
	}

	return Address;
}

void c_Network::GetIPFromDPAddress( IDirectPlay8Address *Address, char *IPAddress, int MaxLength)
{
	DWORD bufsize = 1024;
	U8 buffer[1024];
	DWORD type;

	dxcheckOK( Address->GetComponentByName( 
		DPNA_KEY_HOSTNAME, buffer, &bufsize, &type ) );
	DQWCharToChar( (WCHAR*)buffer, bufsize/sizeof(WCHAR), IPAddress, MaxLength );

	//add Port
	dxcheckOK( Address->GetComponentByName( 
		DPNA_KEY_PORT, buffer, &bufsize, &type ) );
	if(buffer)
		DQstrcat( IPAddress, va(":%d",*(DWORD*)buffer), MaxLength );
}


//********************** Fill out server search information structs *****

int c_Network::GetServerCount()
{
	if(isSearchingForServers && ServerCount==0) return -1;
	return ServerCount; 
}

void c_Network::GetServerAddressString(int ServerNum, char *address, int MaxLength)
{
	s_ChainNode<s_Host> *HostNode;
	int i;

	if(MaxLength<=0) return;
	address[0] = '\0';

	EnterCriticalSection( &csHostList );

	if(ServerNum>=ServerCount) goto EndGetServerAddress;

	for(i=0, HostNode=HostList->GetFirstNode(); i<ServerNum; ++i)
		HostNode=HostNode->next;

	if(!HostNode) {
		Error(ERR_NETWORK, "Invalid ServerCount");
		goto EndGetServerAddress; //shouldn't happen
	}

	GetIPFromDPAddress( HostNode->current->Address, address, MaxLength );

EndGetServerAddress:
	LeaveCriticalSection( &csHostList );
}


int c_Network::GetPing(int ServerNum, char *address, int MaxLength)
{
	s_ChainNode<s_Host> *HostNode;
	int i, ping;

	if(MaxLength<=0) return 0;
	address[0] = '\0';
	ping = 1;

	EnterCriticalSection( &csHostList );

	if(ServerNum>=ServerCount) goto EndGetPing;

	for(i=0, HostNode=HostList->GetFirstNode(); i<ServerNum; ++i)
		HostNode=HostNode->next;

	if(!HostNode) {
		Error(ERR_NETWORK, "Invalid ServerCount");
		goto EndGetPing; //shouldn't happen
	}

	GetServerAddressString( ServerNum, address, MaxLength );
	ping = HostNode->current->Ping;

EndGetPing:
	LeaveCriticalSection( &csHostList );
	ping = max( 1, ping );
	return ping;
}

void c_Network::GetLANServerInfoString( int ServerNum, char *buffer, int MaxLength )
{
	s_ChainNode<s_Host> *HostNode;
	int i;

	if(MaxLength==0) return;
	buffer[0] = '\0';

	EnterCriticalSection( &csHostList );

	if(ServerNum>=ServerCount) goto EndGetServerInfoString;

	for(i=0, HostNode=HostList->GetFirstNode(); i<ServerNum; ++i)
		HostNode=HostNode->next;

	if(!HostNode) {
		Error(ERR_NETWORK, "Invalid ServerCount");
		goto EndGetServerInfoString; //shouldn't happen
	}

	DQstrcpy( buffer, HostNode->current->ServerInfoString, min( MAX_INFO_STRING, MaxLength ) );

EndGetServerInfoString:
	LeaveCriticalSection( &csHostList );
}
//***********************************************************************



//************Misc Bookkeeping****************
int c_Network::GetNewClientNum()
{
	int i;

	for(i=0;i<MAX_CLIENTS;++i)
		if(!Server.ClientInfo[i].isInUse)
			return i;
	return -1;
}

//********************************************

