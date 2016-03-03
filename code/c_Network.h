// DXQuake3 Source
// Copyright (c) 2003 Richard Geary (richard.geary@dial.pipex.com)
//
// c_Network.h: interface for the c_Network class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_C_NETWORK_H__F8FFAD5E_2428_45CE_B959_78753F48FC13__INCLUDED_)
#define AFX_C_NETWORK_H__F8FFAD5E_2428_45CE_B959_78753F48FC13__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

//#define DebugServerMessages
//#define DebugClientMessages

#define MAX_SNAPSHOTS  200

HRESULT WINAPI DirectPlayServerCallback( PVOID pvUserContext, DWORD dwMessageType, PVOID pMessage );
HRESULT WINAPI DirectPlayClientCallback( PVOID pvUserContext, DWORD dwMessageType, PVOID pMessage );

enum eNetMsg { eNetMsg_UnknownMsg, eNetMsg_EnumHosts, eNetMsg_Connect, eNetMsg_SendSnapshot, 
	eNetMsg_SendUserInfo, eNetMsg_SendUserCmd };
enum ePacketIdentity { ePacketUserCmd=1, ePacketUserInfo, ePacketSnapshot, ePacketServerCommand,
		ePacketConfigStringUpdate, ePacketClientCommand };

class s_Host;

//This is only the *LAYOUT* of the packet - be careful when accessing .stringOffsets and .stringData
struct s_ConfigStringUpdatePacket {
	U8				IdentityChar;
	unsigned short	NumConfigStrings;				//number of config strings in this update
	unsigned short	stringNumber[MAX_CONFIGSTRINGS];	//there is only NumConfigStrings of these in the packet
	int				stringOffsets[MAX_CONFIGSTRINGS];	//there is only NumConfigStrings of these in the packet
	char			stringData[MAX_GAMESTATE_CHARS];
};

//Sent from server to clients, guaranteed, in sequence
struct s_ServerCommandPacket {
	U8				IdentityChar;
	unsigned short	stringSize;
	char			CommandString[MAX_STRING_CHARS];
};

struct s_ServerCommand {
	int				SequenceNum;
	c_Command		Command;
};

//Server does NOT send a packet of size sizeof(s_Snapshot)
//Server constructs a snapshot packet
//Client constructs a snapshot_t struct from the packet
struct s_SnapshotPacket {
	U8				IdentityChar;
	int				ThisSnapshotNum;
	int				ReferenceSnapshotNum;		//for delta-compression. Is -1 if DC not used

	int				serverTime;		// server time the message is valid for (in msec)

	short			numEntities;			//Number of entities in the snapshot

	// **not all these exist in the packet** This is only here to allocate sufficient space in the struct
	//The actual format is dynamic, see below
	playerState_t	ps;
	entityState_t	entities[MAX_ENTITIES_IN_SNAPSHOT];	
};

//EntityPacket format
//*******************
// If numEntities<=0, this isn't sent
// for each entity :
//	short EntityNumber
//	if entity is not in ref snapshot, refent=ZeroEntity
//  if entity is changed from ref snapshot, EntityNumber |= 0x4000
//  if changed :
//		4 byte bitfield : true if entityState_t component is changed, false if unchanged
//		for each changed entityState_t component :
//			next x bytes are the new value (x=sizeof(component))

struct s_SnapshotWrapper {
	int SnapshotNum;
	snapshot_t Snapshot;
};

class c_Network : public c_Singleton<c_Network>
{
public:
	c_Network();
	virtual ~c_Network();

	BOOL c_Network::ServerSendTo( int ClientNum, void *data, int size, int TimeoutMS, DWORD DPSendToFlags );
	BOOL c_Network::ServerSendToAll( void *data, int size, int TimeoutMS, DWORD DPSendToFlags );
	void c_Network::ServerSendSnapshot( int ClientNum, void *SnapshotPacket, int PacketSize );
	void c_Network::ServerSendClientCommand( int ClientNum, char *text );
	BOOL c_Network::ServerIsUserInfoModified( int ClientNum );
	void c_Network::ServerUserInfoUpdated( int ClientNum );
	void c_Network::ServerGetUserInfo(int ClientNum, char *buffer, int bufsize);
	void c_Network::ServerSetUserInfo(int ClientNum, char *buffer, int bufsize);
	void c_Network::ServerAddPlayerToGroup( DPNID PlayerID );
	void c_Network::ServerRemovePlayerFromGroup( DPNID PlayerID );
	void c_Network::ServerPrintConnectionStatus( int ClientNum );
	void c_Network::ServerReceiveUserCmd( int ClientNum, void *UserCmdPacket, DWORD dwPacketSize );
	int  c_Network::ServerGetClientPing( int ClientNum );

	void c_Network::ClientGetUIState(uiClientState_t *state);
	BOOL c_Network::ClientSendUserInfo(const char *UserInfoString, DWORD StringSize);
	BOOL c_Network::ClientSendUserCmd(usercmd_t *usercmd);
	BOOL c_Network::ClientGetUserCmd( int CmdNum, usercmd_t *usercmd );
	int	 c_Network::ClientGetCurrentUserCmdNum() { return Client.CurrentUserCmdNum; }
	BOOL c_Network::ClientGetSnapshot( int SnapshotNum, snapshot_t *Snapshot );
	int  c_Network::ClientUpdateServerTime();
	void c_Network::ClientReceivedGameState( gameState_t *_gamestate);
	void c_Network::ClientGetGameState( gameState_t *gamestate );
	BOOL c_Network::ClientGetConfigString( int iTemp1, char *buffer, int bufsize );
	void c_Network::ClientReceivedSnapshot( s_SnapshotPacket *Snapshot, int PacketSize );
	void c_Network::ClientReceivedCSU( BYTE* MessageReceived, int size );
	void c_Network::ClientGetLastSnapshotInfo( int *snapshotNum, int *serverTime );
	BOOL c_Network::ClientSendCommandToServer( const char *buffer );
	c_Command* c_Network::ClientGetServerCommand( int ServerCommandNum );
	void c_Network::ClientCleanServerCommandChain();
	void c_Network::ClientPrintConnectionStatus();
	int  c_Network::ClientGetPing();

	BOOL IsClientConnected() { return Client.isConnected; }

	//DirectPlay stuff
	IDirectPlay8Server		*DirectPlayServer;
	IDirectPlay8Client		*DirectPlayClient;
	IDirectPlay8Address		*DPDeviceAddress, *DPHostAddress;
	DPN_APPLICATION_DESC	ApplicationDesc;

	void c_Network::InitialiseServer();
	void c_Network::InitialiseClient();
	void c_Network::Disconnect();
	void c_Network::ConnectToServer(char *ServerName);

	//Enum Servers
	int  GetServerCount();
	int	 GetPing(int ServerNum, char *address, int MaxLength);
	void RefreshLocalServers();
	void GetServerAddressString(int ServerNum, char *address, int MaxLength);
	void GetLANServerInfoString( int ServerNum, char *buffer, int MaxLength );

	HRESULT ServerCallback( PVOID pvUserContext, DWORD dwMessageType, PVOID pMessage );
	HRESULT ClientCallback( PVOID pvUserContext, DWORD dwMessageType, PVOID pMessage );

	struct s_ClientInfo {
		BOOL				isInUse;
		DPNID				DPN_ClientID;
		IDirectPlay8Address *ClientAddress;
		char				UserInfoString[MAX_INFO_STRING];
		BOOL				bUserInfoModified;
		BOOL				bSnapshotSent;
		DPNHANDLE			ASyncSnapshot;
	};

	BOOL isClientLoaded, isServerLoaded;
	entityState_t ZeroEntity;

private:
	BOOL c_Network::ServerAsyncSendTo( int ClientNum, void *data, int size, int TimeoutMS, DWORD DPNSendToFlags, DPNHANDLE *ASyncHandle, void *ASyncContext );

	IDirectPlay8Address *CreateAddress( char *HostName = NULL );
	void GetIPFromDPAddress( IDirectPlay8Address *Address, char *IPAddress, int MaxLength);
	int GetNewClientNum();

	struct s_UserCmdWrapper {
		usercmd_t UserCmd;
		int UserCmdNum;
	};

	struct s_Server {
		//Information recieved by Server
		s_ClientInfo ClientInfo[MAX_CLIENTS];

		DPNID DPGroupAllClients;

	} Server;

	struct s_Client {
		gameState_t gamestate;
		int CurrentUserCmdNum;
		int UserCmdSendCount;		//this is for usercmd_t delta-compression
		int LastUserCmd;					//this is the position in ClientUserCmd array
		s_UserCmdWrapper UserCmd[CMD_BACKUP];

		//Locked with Client.csSnapshot
		int CurrentSnapshotNum;	//Client snapshot handling
		int CurrentSnapshotTime;

		DPNHANDLE ASyncSendUserInfo, ASyncSendUserCmd, ASyncSendClientCommand;
		CRITICAL_SECTION csSnapshot, csGameState, csServerCommands;
		DPNID DPN_PlayerID;
		int ClientNum;

		BOOL isConnected;

		s_SnapshotWrapper SnapshotArray[MAX_SNAPSHOTS];

		int	LastServerCommandSequenceNum;

		int CurrentServerTime;
		double SnapshotClientTime;
		int Ping;
	} Client;

	uiClientState_t UIClientState;
	c_Chain<s_ServerCommand> ClientServerCommandsChain;

	struct s_String {
		char s[MAX_STRING_CHARS];
	};
	c_Chain<s_String> ClientCommandChain;	//commands to be sent to the server

	int ServerCount;
	CRITICAL_SECTION csHostList, csHostQuery, csConnecting, csClientInfo;
	DPNHANDLE ASyncEnumHandle, ASyncConnectHandle, ASyncSendTo, ASyncSendAll;
	BOOL isSearchingForServers;

	c_Chain<s_Host> *HostList;

	DWORD NetworkPortNumber;

	//Server Connection info
	struct s_ConnectInfo {
		gameState_t gamestate;
		int ClientNum;
		int LastSnapshotNum;
		int LastServerCommandNum;
	};	//sent by the server to a connecting client

public:
	s_ClientInfo *ClientInfoLock() { EnterCriticalSection( &csClientInfo ); return Server.ClientInfo; }
	void ClientInfoUnlock() { LeaveCriticalSection( &csClientInfo ); }

};

class s_Host
{
public:
	DPN_APPLICATION_DESC AppDesc;
	IDirectPlay8Address	 *Address;
	int					 Ping;
	char				 ServerInfoString[MAX_INFO_STRING];
//		WCHAR				 SessionName[200];
	s_Host::s_Host() { };
	s_Host::~s_Host() {
		SafeRelease(Address);
	}
};

#define DQNetwork c_Network::GetSingleton()

#endif // !defined(AFX_C_NETWORK_H__F8FFAD5E_2428_45CE_B959_78753F48FC13__INCLUDED_)
