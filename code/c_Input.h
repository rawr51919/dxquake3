// DXQuake3 Source
// Copyright (c) 2003 Richard Geary (richard.geary@dial.pipex.com)
//
// c_Input.h: interface for the c_Input class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_C_INPUT_H__FFD4D33F_993C_4EBF_83AC_EE0C6D7C122C__INCLUDED_)
#define AFX_C_INPUT_H__FFD4D33F_993C_4EBF_83AC_EE0C6D7C122C__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#define KEYBOARD_BUFSIZE 256
#define MAX_KEYBUFFER	 200

class c_Input : public c_Singleton<c_Input>
{
public:
	c_Input();
	virtual ~c_Input();
	void Unload();
	void SetKeyCatcher(int KeyCatcher) { bfKeyCatcher = KeyCatcher; }
	int GetKeyCatcher() { return bfKeyCatcher; }
	void GetBindingString( int keynum, char *buffer, int MaxLength);
	void SetBindingString( int keynum, char *buffer, int MaxLength);
	void SetBindingString( char *keystring, int keystringLength, char *bindString, int bindStringLength);
	void UnbindAll();
	void GetKeynumString( int keynum, char *buffer, int MaxLength);
	BOOL KeyIsDown(int Key) { return (Key>0 && Key<256)?KeyStatus[Key]:0; }
	int GetMouse_dx() { return DeltaMouse.x; }
	int GetMouse_dy() { return DeltaMouse.y; }
	void ClearKeyStatus() { ZeroMemory(KeyStatus, sizeof(BOOL)*256 ); }
	BOOL isOverwriteEnabled;
	void Initialise();
	void InitialiseWin32KeyboardInput();
	void InitialiseWin32MouseInput();
	void Acquire();
	void SetCaptureMouse(BOOL SetCapture);
	void CreateUserCmd( usercmd_t *UserCmd );
	void Update();
	void KeyEvent(unsigned char key, BOOL isDown);
	void MouseEvent(int WMessage, BOOL isDown);
	void AddToKeyBuffer( int Q3Key, BOOL isDown, int key );

	BOOL CaptureMouse;
	struct s_KeyBuffer { int key; BOOL isDown; };
	int GetKeyBuffer(c_Input::s_KeyBuffer *pKeyBuffer);

	D3DXVECTOR2 fDeltaMouse;
	vmCvar_t m_yaw, m_side, m_forward, m_pitch, m_filter, sensitivity, AlwaysRun;
	vmCvar_t win32_keyboard, win32_mouse;

	void ConsoleInputChar( unsigned char ch );		//handles misc keys for console management
	int ConvertWin32KeyToQ3(int key);

private:
	BOOL isLoaded;
	LPDIRECTINPUT8 DXInput;
	LPDIRECTINPUTDEVICE8 KeyboardDevice;
	LPDIRECTINPUTDEVICE8 MouseDevice;
	BOOL MouseIsAquired, KeyboardIsAquired;
	int FailedAquireCount;

	int bfKeyCatcher;
	BOOL KeyStatus[256];
	char BindString[256][MAX_STRING_CHARS];
	static char KeyNumString[256][20];
	POINT MidPoint, DeltaMouse, OldDeltaMouse;
	s_KeyBuffer KeyBuffer[256];						//can hold 256 keys
	int NumKeyBuffer;								//Number of keys in KeyBuffer
	char KeyboardState[KEYBOARD_BUFSIZE], prevKeyboardState[KEYBOARD_BUFSIZE];
	char CurrentMessage[MAX_STRING_CHARS];

	//UNKNOWN_STATE refers to bind strings starting with a +, which require a -bindstring to execute on key-up
	enum eActions { ACTION_NONE=0, ACTION_ATTACK, ACTION_MOVEUP, ACTION_MOVEDOWN, 
		ACTION_MOVEFORWARD, ACTION_MOVEBACK, ACTION_MOVELEFT, ACTION_MOVERIGHT, 
		ACTION_TALK, ACTION_TALK2, ACTION_USE, ACTION_GESTURE, ACTION_WALKING, ACTION_STRAFE,
		ACTION_AFFIRMATIVE, ACTION_NEGATIVE, ACTION_GETFLAG, ACTION_GUARDBASE, ACTION_PATROL, ACTION_FOLLOWME, 
		ACTION_CONSOLE, 	
		ACTION_UNKNOWN, ACTION_UNKNOWN_STATE, MAX_ACTIONS };
	int BindAction[256];
	struct s_keypress {
		int KeyNum1, KeyNum2;		//upto two keys can be pressed which correspond to the same action
		BOOL isEnabled;
	} ActionKeys[MAX_ACTIONS];

	int c_Input::ConvertDIKeyToQ3(int i);
	void UpdateCVars();
	void ExecuteKeyDown( int Q3Key );
	void ExecuteKeyUp( int Q3Key );
	void RenderTalkMessage();

};

#define DQInput c_Input::GetSingleton()

#endif // !defined(AFX_C_INPUT_H__FFD4D33F_993C_4EBF_83AC_EE0C6D7C122C__INCLUDED_)
