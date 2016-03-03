// DXQuake3 Source
// Copyright (c) 2003 Richard Geary (richard.geary@dial.pipex.com)
//
// c_ClientGame.h: interface for the c_ClientGame class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_C_CLIENTGAME_H__E4278798_C9D7_4254_8559_198D96F143DB__INCLUDED_)
#define AFX_C_CLIENTGAME_H__E4278798_C9D7_4254_8559_198D96F143DB__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class c_ClientGame : public c_Singleton<c_ClientGame> 
{
public:
	c_ClientGame();
	virtual ~c_ClientGame();
	void Load();
	void Unload();
	void Update();
	void UpdateLoadingScreen();
	int	 PointContents( D3DXVECTOR3 *Point, int PassModelNum);
	void TraceBox( trace_t *results, D3DXVECTOR3 *start, D3DXVECTOR3 *end, D3DXVECTOR3 *min, D3DXVECTOR3 *max, int ModelHandle, int contentmask );
	void TraceBoxAgainstOrientedBox(  trace_t *results, D3DXVECTOR3 *start, D3DXVECTOR3 *end, D3DXVECTOR3 *min, D3DXVECTOR3 *max, int ModelHandle, int contentmask, D3DXVECTOR3 *Origin, D3DXVECTOR3 *Angles );
	void CreateTempClipBox( D3DXVECTOR3 *Min, D3DXVECTOR3 *Max );

	BOOL c_ClientGame::ExecuteCommand( c_Command *command );

	int CurrentWeapon;
	float ZoomSensitivity;
	BOOL isLoaded;
	BOOL LoadMe;
	int ClientNum;
	int LoadHint_SnapshotNum;

	c_Command *CurrentCommand;

	HANDLE ClientLoadedEvent;
	vmCvar_t cg_NetworkInfo;

private:
	void CreateUserCmdMessage(int ServerTime);

	HINSTANCE hDLL;
	t_vmMain vmMain;
	t_dllEntry dllEntry;
	CRITICAL_SECTION cs_vmMain;

	usercmd_t UserCmd;

	unsigned short ViewAngles[3];

	s_Box TempClipBox;

};

#define DQClientGame c_ClientGame::GetSingleton()

#endif // !defined(AFX_C_CLIENTGAME_H__E4278798_C9D7_4254_8559_198D96F143DB__INCLUDED_)
