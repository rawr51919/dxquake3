// DXQuake3 Source
// Copyright (c) 2003 Richard Geary (richard.geary@dial.pipex.com)
//
// c_Sound.h: interface for the c_Sound class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_C_SOUND_H__BEAF78E5_C404_4DE7_A725_F2BC2257DC89__INCLUDED_)
#define AFX_C_SOUND_H__BEAF78E5_C404_4DE7_A725_F2BC2257DC89__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class c_Sound;

class c_SoundManager : public c_Singleton<c_SoundManager>
{
public:
	c_SoundManager();
	virtual ~c_SoundManager();
	void Unload();
	void Initialise();
	int RegisterSound(char *Filename, BOOL isCompressed);
	void Play2DSound( int SoundHandle, int Channel, int EntityNum = ENTITYNUM_NONE );
	void Play3DSound( int SoundHandle, int Channel, int EntityNum, D3DXVECTOR3 *Position );
	void UpdateEntityPosition( int EntityNum, D3DXVECTOR3 *Position );
	void Play3DLoopingSound( int SoundHandle, int EntityNum, D3DXVECTOR3 *Position, D3DXVECTOR3 *Velocity );
	void ClearLoopingSounds();
	void StopLoopingSound( int EntityNum );
		
	void SetListenerOrientation( D3DXVECTOR3 *Position, float *axis, BOOL inWater );
	void Update();
	void UnloadSound( int SoundHandle );

	LPDIRECTSOUND		DirectSound;
	int					*SoundChannel;				//Currently playing sound
	
private:

	c_Sound *pSoundArray[MAX_SOUNDS];
	int NumSounds;
	const int NumChannels;
	BOOL isInitialised;
	BOOL bClearLoopingSounds;
	int	SoundFrameNum;

	IDirectSound3DListener8	*DSListener;
	DSBUFFERDESC			PrimaryBufferDesc;
	LPDIRECTSOUNDBUFFER		PrimaryBuffer;

	D3DXVECTOR3 pSoundEntityPos[ENTITYNUM_MAX_NORMAL];

	c_Sound * GetNextInstance(c_Sound *Sound);

	BOOL noSound;
};

#define DQSound c_SoundManager::GetSingleton()

#endif // !defined(AFX_C_SOUND_H__BEAF78E5_C404_4DE7_A725_F2BC2257DC89__INCLUDED_)
