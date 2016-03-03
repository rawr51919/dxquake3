// DXQuake3 Source
// Copyright (c) 2003 Richard Geary (richard.geary@dial.pipex.com)
//
// c_Sound.h: interface for the c_Sound class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_C_SOUND_H__54EFEB72_A2A9_4E21_B7DB_7C2E186E156E__INCLUDED_)
#define AFX_C_SOUND_H__54EFEB72_A2A9_4E21_B7DB_7C2E186E156E__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class c_Sound  
{
	friend c_SoundManager;		//for PlaySound
public:
	virtual ~c_Sound();
	BOOL Load2DSound(const char *SoundFilename);
	BOOL Load3DSound(const char *SoundFilename);
	BOOL ReloadBuffer( BOOL use3DBuffer, BOOL useHardware );
	void Unload();

	char Filename[MAX_QPATH];
private:
	c_Sound();					//Can only be created by c_SoundManager
	c_Sound(c_Sound *Original);

	FILEHANDLE	 FileHandle;
	WAVEFORMATEX *WaveHeader;
	DWORD		 FileLength;
	DWORD		 WaveLength;		//Note : This is Length of Wave, not 1/Frequency!!
	BYTE		 *WaveData;
	BOOL		 isLoaded;
	BOOL		 isLoopingSound;
	BOOL		 is3DSound;
	int			 EntityNum;
	c_Sound		 *NextInstanceSound;		//The next instance of this sound
	int			 CurrentFrame;					//For ClearLoopingSounds - this is the frame that the most recent PlayLoopingSound was called
	BOOL		 isPlaying;					//FALSE if defintely not playing sound, TRUE if playing or status unknown

	LPDIRECTSOUNDBUFFER		Buffer;
	LPDIRECTSOUND3DBUFFER	Buffer3D;
	DSBUFFERDESC			BufferDesc;

	BOOL LoadSound(const char *Filename);
};

#endif // !defined(AFX_C_SOUND_H__54EFEB72_A2A9_4E21_B7DB_7C2E186E156E__INCLUDED_)
