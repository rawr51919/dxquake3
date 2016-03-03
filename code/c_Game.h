// DXQuake3 Source
// Copyright (c) 2003 Richard Geary (richard.geary@dial.pipex.com)
//
// c_Game.h: interface for the c_Game class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_C_GAME_H__B227C86D_1D65_4FC2_B999_E6EE75C5BAF3__INCLUDED_)
#define AFX_C_GAME_H__B227C86D_1D65_4FC2_B999_E6EE75C5BAF3__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class c_Game : public c_Singleton<c_Game>
{
public:
	c_Game();
	virtual ~c_Game();
	void Load();
	void Unload();
	void Update();
	void ClientThink( int ClientNum, usercmd_t *usercmd, int ConfirmedSnapshotNum );
	void ClientBegin( int ClientNum );
	void ClientUpdateUserInfo( int ClientNum );

	void GetUserCmd( int ClientNum, usercmd_t *usercmd );
	void GetConfigString( int num, char *buffer, int bufferSize );
	void SetConfigString( int num, const char *string );
	void PrepareGameState();
	void PrepareConfigStringUpdate(int ClientNum);
	void c_Game::CreateBackupSnapshot();
	void c_Game::SendSnapshot( int ClientNum );
	void c_Game::PrepareSnapshot();
	void c_Game::SendConfigStringUpdate();

	void LocateGameData( sharedEntity_t *gEnts, int numGEntities, int sizeofGEntity, playerState_t *clients, int sizeofGameClient );
	void LinkEntity(sharedEntity_t *Entity);
	void UnlinkEntity(sharedEntity_t *Entity);
	int	 EntitiesInBox( D3DXVECTOR3 *min, D3DXVECTOR3 *max, int *list, int maxcount );
	BOOL EntityContact( D3DXVECTOR3 *min, D3DXVECTOR3 *max, sharedEntity_t *ent );
	int	 PointContents( D3DXVECTOR3 *Point, int PassEntityNum);
	void TraceBox( trace_t *results, D3DXVECTOR3 *start, D3DXVECTOR3 *end, D3DXVECTOR3 *min, D3DXVECTOR3 *max, int passEntityNum, int contentmask );

	BOOL c_Game::ExecuteConsoleCommand( c_Command *command );
	void c_Game::ExecuteClientCommand( int ClientNum, const char *buffer, int MaxLength );

	char* ClientConnect( int ClientNum, BOOL firstTime );
	void ClientDisconnect( int ClientNum );

	//Note: sharedEntity_t::r.s is not used by Q3, so it's not filled by DXQ3
	sharedEntity_t *GameEntities;		//WARNING : DO NOT USE GameEntities[index] - it has a custom defined stride!!!
	c_Command *CurrentCommand;
	gameState_t gamestate;

	BOOL isLoaded;

	void SetPauseState( BOOL _isPaused );
	static void ReadPlayerStatePacket( BYTE **pIn, playerState_t *psNew, playerState_t *psRef );

	int LastSnapshotNum;

private:
	HINSTANCE hDLL;
	t_vmMain vmMain;
	t_dllEntry dllEntry;
	BOOL isPaused;

	int NumGameEntities;
	int SizeofGameEntity;
	playerState_t *PlayerStates;
	int SizeofPlayerStates;
	int LinkedEntities[MAX_GENTITIES];
	int NumLinkedEntities;
	
	int ServerFrameTime;

	char *ConfigString[MAX_CONFIGSTRINGS];
	BOOL bConfigStringModified[MAX_CONFIGSTRINGS];
	
	//Previous Snapshots sent out (stored for delta compression comparison)
	//Use this structure so we can store svFlags details
	//Store the entityState_t struct inside the s_EntityWrapper struct
	struct s_EntityWrapper {
		entityState_t s;
		int svFlags;
		int SingleClient;
	};

	struct s_BackupSnapshot {
		int SnapshotNum;
		int	serverTime;

		s_EntityWrapper *Ent;
		int NumEntities;

		playerState_t PlayerState[MAX_CLIENTS];

		s_BackupSnapshot():Ent(NULL) {}
	};

	s_BackupSnapshot SnapshotArray[MAX_SNAPSHOTS];

	static void MakePlayerStatePacket( BYTE **out, playerState_t *psNew, playerState_t *psOld );

	//Network Game Information for each client
	struct s_Client {
		int LastReceivedSnapshotNum;

	} Client[MAX_CLIENTS];

	usercmd_t UserCmd[MAX_CLIENTS];

	CRITICAL_SECTION cs_vmMain, csLastSnapshotNum;

	vmCvar_t g_NetworkInfo;

};

#define DQGame c_Game::GetSingleton()

#endif // !defined(AFX_C_GAME_H__B227C86D_1D65_4FC2_B999_E6EE75C5BAF3__INCLUDED_)
