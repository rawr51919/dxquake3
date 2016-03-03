// DXQuake3 Source
// Copyright (c) 2003 Richard Geary (richard.geary@dial.pipex.com)
//
// c_Input.cpp: implementation of the c_Input class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"

#define MAX_KEYNUMSTRING 20

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

c_Input::c_Input()
{
	bfKeyCatcher = 0;
	isOverwriteEnabled = FALSE;
	ZeroMemory( KeyStatus, sizeof(BOOL)*256 );
	for(int i=0;i<256;++i) {
		BindString[i][0] = '\0';
	}
	CaptureMouse = FALSE;
	NumKeyBuffer = 0;
	KeyboardDevice = NULL;
	MouseDevice = NULL;
	fDeltaMouse.x = fDeltaMouse.y = 0.0f;
	DeltaMouse.x = DeltaMouse.y = 0;
	OldDeltaMouse.x = OldDeltaMouse.y = 0;
	DXInput = NULL;
	ZeroMemory( BindAction, 256*sizeof(int) );
	ZeroMemory( ActionKeys, sizeof(ActionKeys) );
	MouseIsAquired = FALSE;
	KeyboardIsAquired = FALSE;
	FailedAquireCount = 0;
	isLoaded = FALSE;
	CurrentMessage[0] = '\0';
}

c_Input::~c_Input()
{
	Unload();
	SafeRelease( DXInput );
}

void c_Input::Unload()
{
	if(KeyboardDevice) KeyboardDevice->Unacquire();
	if(MouseDevice) MouseDevice->Unacquire();
	SafeRelease( KeyboardDevice );
	SafeRelease( MouseDevice );
	SetCaptureMouse(FALSE);
}

void c_Input::UnbindAll()
{
	for(int i=0;i<256;++i) BindString[i][0] = '\0';
}

//Fills buffer with the command bound to key keynum
void c_Input::GetBindingString( int keynum, char *buffer, int MaxLength)
{
	buffer[0] = '\0';
	if(keynum<0 || keynum>255) return;
	if(!BindString[keynum][0]) return;

	DQstrcpy(buffer, BindString[keynum], min(MaxLength, MAX_STRING_CHARS));
}

//Sets the binding to key keynum
void c_Input::SetBindingString( int keynum, char *buffer, int MaxLength)
{
	if(keynum<0 || keynum>255) return;

	DQstrcpy(BindString[keynum], buffer, min(MaxLength, MAX_STRING_CHARS));

	if(DQWordstrcmpi(buffer, "+attack", MAX_STRING_CHARS)==0) BindAction[keynum] = ACTION_ATTACK;
	else if(DQWordstrcmpi(buffer, "+moveup", MAX_STRING_CHARS)==0) BindAction[keynum] = ACTION_MOVEUP;
	else if(DQWordstrcmpi(buffer, "+movedown", MAX_STRING_CHARS)==0) BindAction[keynum] = ACTION_MOVEDOWN;
	else if(DQWordstrcmpi(buffer, "+forward", MAX_STRING_CHARS)==0) BindAction[keynum] = ACTION_MOVEFORWARD;
	else if(DQWordstrcmpi(buffer, "+back", MAX_STRING_CHARS)==0) BindAction[keynum] = ACTION_MOVEBACK;
	else if(DQWordstrcmpi(buffer, "+moveleft", MAX_STRING_CHARS)==0) BindAction[keynum] = ACTION_MOVELEFT;
	else if(DQWordstrcmpi(buffer, "+moveright", MAX_STRING_CHARS)==0) BindAction[keynum] = ACTION_MOVERIGHT;
	else if(DQWordstrcmpi(buffer, "+left", MAX_STRING_CHARS)==0) BindAction[keynum] = ACTION_MOVELEFT;
	else if(DQWordstrcmpi(buffer, "+right", MAX_STRING_CHARS)==0) BindAction[keynum] = ACTION_MOVERIGHT;
	else if(DQWordstrcmpi(buffer, "+speed", MAX_STRING_CHARS)==0) BindAction[keynum] = ACTION_WALKING;
	else if(DQWordstrcmpi(buffer, "+strafe", MAX_STRING_CHARS)==0) BindAction[keynum] = ACTION_STRAFE;
	else if(DQWordstrcmpi(buffer, "+button2", MAX_STRING_CHARS)==0) BindAction[keynum] = ACTION_USE;
	else if(DQWordstrcmpi(buffer, "messagemode", MAX_STRING_CHARS)==0) BindAction[keynum] = ACTION_TALK;
	else if(DQWordstrcmpi(buffer, "messagemode2", MAX_STRING_CHARS)==0) BindAction[keynum] = ACTION_TALK2;
	else if(DQWordstrcmpi(buffer, "toggleconsole", MAX_STRING_CHARS)==0) BindAction[keynum] = ACTION_CONSOLE;
	else if(buffer[0]=='+') BindAction[keynum] = ACTION_UNKNOWN_STATE;
	else BindAction[keynum] = ACTION_UNKNOWN;
}

//Fills buffer with the string (English) representation of a key keynum
void c_Input::GetKeynumString( int keynum, char *buffer, int MaxLength)
{
	if(keynum<0 || keynum>255) return;

	DQstrcpy(buffer, KeyNumString[keynum], min(MAX_KEYNUMSTRING, MaxLength));
}

void c_Input::SetBindingString( char *keystring, int keystringLength, char *bindString, int bindStringLength)
{
	int i;
	for(i=0;i<256;++i) {
		if(DQWordstrcmpi(keystring, KeyNumString[i], keystringLength)==0) {
			SetBindingString(i,bindString, bindStringLength);
			return;
		}
	}
	//keystring not found
	Error(ERR_INPUT, va("bind : Key %s not found",keystring));
}

void c_Input::Initialise()
{
	//Get CVars
	DQConsole.GetCVar( "m_forward", &m_forward );
	DQConsole.GetCVar( "m_side", &m_side );
	DQConsole.GetCVar( "m_pitch", &m_pitch );
	DQConsole.GetCVar( "m_yaw", &m_yaw );
	DQConsole.GetCVar( "m_filter", &m_filter );
	DQConsole.GetCVar( "sensitivity", &sensitivity );
	DQConsole.GetCVar( "cl_run", &AlwaysRun );

	//Set MidPoint for win32 mouse input
	RECT rect;
	GetWindowRect( DQRender.GetHWnd(), &rect );
	MidPoint.x = (rect.right-rect.left)/2;
	MidPoint.y = (rect.bottom-rect.top)/2;

	//Create Win32Input CVars
	DQConsole.RegisterCVar( "win32_keyboard", "0", 0, eAuthClient, &win32_keyboard );
	DQConsole.RegisterCVar( "win32_mouse", "0", 0, eAuthClient, &win32_mouse );

	SetCaptureMouse(TRUE);

	if(!DXInput) {
		dxcheckOK( DirectInput8Create( DQRender.GetHInstance(), DIRECTINPUT_VERSION, IID_IDirectInput8,
			(void**)&DXInput, NULL ) );
	}

	//Set Event Buffer properties for mouse
	DIPROPDWORD dipdw;
	dipdw.diph.dwSize		= sizeof(DIPROPDWORD);
	dipdw.diph.dwHeaderSize = sizeof(DIPROPHEADER);
	dipdw.diph.dwObj		= 0;
	dipdw.diph.dwHow		= DIPH_DEVICE;
	dipdw.dwData			= 32;					//Mouse buffer size

	//KEYBOARD
	//********
	if(KeyboardDevice) KeyboardDevice->Unacquire();
	SafeRelease( KeyboardDevice );

	if( FAILED( DXInput->CreateDevice( GUID_SysKeyboard, &KeyboardDevice, NULL ) )
	 || FAILED( KeyboardDevice->SetDataFormat( &c_dfDIKeyboard ) )
	 || FAILED( KeyboardDevice->SetCooperativeLevel( DQRender.GetHWnd(), DISCL_BACKGROUND | DISCL_NONEXCLUSIVE ) )
//	 || FAILED( KeyboardDevice->SetCooperativeLevel( DQRender.GetHWnd(), DISCL_FOREGROUND | DISCL_EXCLUSIVE ) )
	 || FAILED( KeyboardDevice->Acquire() ))
	{
		DQConsole.SetCVarString( "win32_keyboard", "1", eAuthClient );
		KeyboardIsAquired = FALSE;
		InitialiseWin32KeyboardInput();
		DQPrint("^1Warning : Not using DirectInput for Keyboard. To reset, set win32_keyboard 0");
	}
	else {
		KeyboardIsAquired = TRUE;
	}

	//MOUSE
	//*****
	if(MouseDevice) MouseDevice->Unacquire();
	SafeRelease( MouseDevice );

	if( FAILED( DXInput->CreateDevice( GUID_SysMouse, &MouseDevice, NULL ) )
	 || FAILED( MouseDevice->SetDataFormat( &c_dfDIMouse ) )
	 || FAILED( MouseDevice->SetCooperativeLevel( DQRender.GetHWnd(), DISCL_FOREGROUND | DISCL_EXCLUSIVE ) )
	 || FAILED( MouseDevice->SetProperty( DIPROP_BUFFERSIZE, &dipdw.diph ) )
	 || FAILED( MouseDevice->Acquire() ) )
	{
		//DXInput Failed
		DQConsole.SetCVarString( "win32_mouse", "1", eAuthClient );
		MouseIsAquired = FALSE;
		InitialiseWin32MouseInput();
		DQPrint("^1Warning : Not using DirectInput for Mouse. To reset, set win32_mouse 0");
	}
	else {
		//DXInput Succeeded
		MouseIsAquired = TRUE;
	}
	isLoaded = TRUE;

	DQConsole.UpdateCVar( &win32_keyboard );
	DQConsole.UpdateCVar( &win32_mouse );
}

//Run every frame and on WM_ACTIVATE message
void c_Input::Acquire() {

	if(!isLoaded) return;

	if(!KeyboardIsAquired && !win32_keyboard.integer) {
		if(!KeyboardDevice) Initialise();
		if(FAILED(KeyboardDevice->Acquire()) ) {
			++FailedAquireCount;
			if(FailedAquireCount>100) {
				Error(ERR_INPUT, "DirectInput Keyboard Device - unable to reaquire, switching to Win32 KeyboardInput");
				InitialiseWin32KeyboardInput();
			}
		}
		else {
			KeyboardIsAquired = TRUE;
			FailedAquireCount = 0;
		}
	}
	if(KeyboardIsAquired && win32_keyboard.integer) InitialiseWin32KeyboardInput();

	if(!MouseIsAquired && !win32_mouse.integer) {
		if(!MouseDevice) Initialise();
		if(MouseDevice) {
			if(FAILED(MouseDevice->Acquire()) ) {
				++FailedAquireCount;
				if(FailedAquireCount>100) {
					Error(ERR_INPUT, "DirectInput Mouse Device - unable to reaquire, switching to Win32 MouseInput");
					InitialiseWin32MouseInput();
				}
			}
			else {
				MouseIsAquired = TRUE;
				FailedAquireCount = 0;
			}
		}
	}
	if(MouseIsAquired && win32_mouse.integer) InitialiseWin32MouseInput();
}

//Only run if necessary, by c_Input::Initialise and c_Input::Aquire
void c_Input::InitialiseWin32MouseInput() 
{
	SetCaptureMouse(TRUE);
	SetCursorPos(MidPoint.x, MidPoint.y);
	MouseIsAquired = FALSE;

	if(MouseDevice) MouseDevice->Unacquire();
	SafeRelease(MouseDevice);

	DQConsole.SetCVarString( "win32_mouse", "1", eAuthClient );
	DQConsole.UpdateCVar( &win32_mouse );
}

void c_Input::InitialiseWin32KeyboardInput() 
{
	ZeroMemory( KeyboardState, sizeof(KeyboardState) );
	KeyboardIsAquired = FALSE;

	if(KeyboardDevice) KeyboardDevice->Unacquire();
	SafeRelease(KeyboardDevice);

	DQConsole.SetCVarString( "win32_keyboard", "1", eAuthClient );
	DQConsole.UpdateCVar( &win32_keyboard );
}

void c_Input::SetCaptureMouse(BOOL SetCapture)
{
	if(CaptureMouse == !SetCapture) {
		ShowCursor(!SetCapture);
		DQDevice->ShowCursor(!SetCapture);
	}
	CaptureMouse = SetCapture;
}	

//Run every frame
void c_Input::Update()
{
	POINT CursorPos;
	DIDEVICEOBJECTDATA MouseData;
	DWORD NumElements = 1;
	HRESULT hResult;
	int i, Q3Key;

	UpdateCVars();
	
	if(!DQRender.isActive) return;

	Acquire();		//Reaquire lost input devices

	if(bfKeyCatcher & KEYCATCH_MESSAGE) {
		//Render message
		RenderTalkMessage();
	}

	//If in console
	if(bfKeyCatcher & (KEYCATCH_CONSOLE | KEYCATCH_MESSAGE) ) {
		//Input handled by Win32 Messages
		return;
	}

	//Keyboard Input
	if(!win32_keyboard.integer) {
		hResult = KeyboardDevice->GetDeviceState( KEYBOARD_BUFSIZE, KeyboardState );
		if(hResult == DIERR_INPUTLOST) {
			if(FAILED(KeyboardDevice->Acquire())) {
				KeyboardIsAquired = FALSE;
			}
			goto SkipKeyboard;
		}

		for(i=0;i<256;++i) {
			if(KeyboardState[i] & 0x80) {
				if( !(prevKeyboardState[i] & 0x80) ) {
					Q3Key = ConvertDIKeyToQ3(i);
					ExecuteKeyDown( Q3Key );
				}
			}
			else if(prevKeyboardState[i] & 0x80) {
				Q3Key = ConvertDIKeyToQ3(i);
				ExecuteKeyUp( Q3Key );
			}
		}

		//If Alt-Enter, toggle fullscreen
		if( (KeyboardState[DIK_LMENU] & 0x80) && (KeyboardState[DIK_RETURN] & 0x80) ) {
			if( !(prevKeyboardState[DIK_RETURN] & 0x80) ) {
				DQRender.ToggleFullscreen();
			}
		}

		memcpy( prevKeyboardState, KeyboardState, sizeof( KeyboardState ) );
	}

SkipKeyboard:
	//Mouse Input
	if(!win32_mouse.integer && !MouseDevice) InitialiseWin32MouseInput();

	if(win32_mouse.integer) {
		if(!CaptureMouse) return;

		GetCursorPos(&CursorPos);

		DeltaMouse.x = (CursorPos.x-MidPoint.x)/2;
		DeltaMouse.y = (CursorPos.y-MidPoint.y)/2;

		SetCursorPos(MidPoint.x, MidPoint.y);
	}
	else {
		DeltaMouse.x = DeltaMouse.y = 0;

		while(1) {
			hResult = MouseDevice->GetDeviceData( sizeof(DIDEVICEOBJECTDATA), &MouseData, &NumElements, 0 );
			if(hResult == DIERR_INPUTLOST) {
				if(FAILED(MouseDevice->Acquire())) {
					MouseIsAquired = FALSE;
				}
				break;
			}
			if(FAILED(hResult) || NumElements == 0 ) break;
			switch(MouseData.dwOfs) {
			case DIMOFS_X:
				DeltaMouse.x += MouseData.dwData;
				break;
			case DIMOFS_Y:
				DeltaMouse.y += MouseData.dwData;
				break;
			case DIMOFS_Z:
				if( (int)MouseData.dwData > 0 ) {
					ExecuteKeyDown( K_MWHEELUP );
				}
				else {
					ExecuteKeyDown( K_MWHEELDOWN );
				}
				break;
			case DIMOFS_BUTTON0:
				if(MouseData.dwData & 0x80) {
					ExecuteKeyDown( K_MOUSE1 );
				}
				else {
					ExecuteKeyUp( K_MOUSE1 );
				}
				break;
			case DIMOFS_BUTTON1:
				if(MouseData.dwData & 0x80) {
					ExecuteKeyDown( K_MOUSE2 );
				}
				else {
					ExecuteKeyUp( K_MOUSE2 );
				}
				break;
			case DIMOFS_BUTTON2:
				if(MouseData.dwData & 0x80) {
					ExecuteKeyDown( K_MOUSE3 );
				}
				else {
					ExecuteKeyUp( K_MOUSE3 );
				}
				break;
			default:
				break;
			}
		}
	}
	if(m_filter.value) {
		fDeltaMouse.x = 0.5f*(DeltaMouse.x+OldDeltaMouse.x)*sensitivity.value*DQClientGame.ZoomSensitivity;
		fDeltaMouse.y = 0.5f*(DeltaMouse.y+OldDeltaMouse.y)*sensitivity.value*DQClientGame.ZoomSensitivity;
		OldDeltaMouse.x = DeltaMouse.x;
		OldDeltaMouse.y = DeltaMouse.y;
	}
	else {
		fDeltaMouse.x = DeltaMouse.x*sensitivity.value*DQClientGame.ZoomSensitivity;
		fDeltaMouse.y = DeltaMouse.y*sensitivity.value*DQClientGame.ZoomSensitivity;
	}
}

//Only run when in console, by WM_CHAR messages
void c_Input::ConsoleInputChar( unsigned char ch ) 
{
	if(bfKeyCatcher & KEYCATCH_CONSOLE) {

		if(BindAction[ch] == ACTION_CONSOLE) {
			if((DQTime.GetSecs() - DQConsole.ShowConsoleTimer) < 0.2f) return;
			DQConsole.ShowConsole( eShowConsoleNone );
			return;
		}
		DQConsole.AddInputChar( ch );
	}
	else if(bfKeyCatcher & KEYCATCH_MESSAGE) {
		if( ch == 27 ) { //escape
			bfKeyCatcher &= ~KEYCATCH_MESSAGE;	//exit talk mode, but leave message intact
		}
		else if( ch == 8 ) { //backspace
			int len = DQstrlen(CurrentMessage, MAX_STRING_CHARS);
			if(len>0) CurrentMessage[len-1] = '\0';
		}
		else if( ch == 13 ) { //enter
			bfKeyCatcher &= ~KEYCATCH_MESSAGE;
			ActionKeys[ACTION_TALK].isEnabled = FALSE;
			DQConsole.ExecuteCommandString( va("say %s",CurrentMessage), MAX_STRING_CHARS, FALSE );
			CurrentMessage[0] = '\0';
		}
		else {
			DQstrcat( CurrentMessage, va("%c",ch), MAX_STRING_CHARS );
		}
	}
}

void c_Input::ExecuteKeyDown( int Q3Key )
{
	int Action;

	if(BindAction[Q3Key]<=0 || BindAction[Q3Key]>=MAX_ACTIONS) return;
	Action = BindAction[Q3Key];

	//Check for ToggleConsole
	if( Action == ACTION_CONSOLE && !ActionKeys[ACTION_CONSOLE].isEnabled ) {
		DQConsole.ShowConsole( eShowConsoleHalf );
		ActionKeys[Action].isEnabled = TRUE;
		return;
	}
	
	AddToKeyBuffer( Q3Key, TRUE, 0 );

	//Check for Escape to go to menu
	if( Q3Key == K_ESCAPE ) {
		//don't go to menu if in talk mode
		//Since pressing esc will have released the keycatcher by now, we have to use a different method
		if(!ActionKeys[ACTION_TALK].isEnabled) {
			DQUI.OpenMenu();
		}
		ActionKeys[ACTION_TALK].isEnabled = FALSE;
		return;
	}

	if(!DQClientGame.isLoaded) return;	//Don't execute bind strings if ClientGame not loaded
	if(bfKeyCatcher) return;	//...or in console,UI or talking

	if( Action == ACTION_TALK ) {
		//go into talk mode
		bfKeyCatcher |= KEYCATCH_MESSAGE;
		ActionKeys[Action].isEnabled = TRUE;
		return;
	}

	if( Action == ACTION_UNKNOWN || Action == ACTION_UNKNOWN_STATE ) {
		DQConsole.ExecuteCommandString( BindString[Q3Key], MAX_STRING_CHARS, FALSE );
		return;
	}

	ActionKeys[Action].isEnabled = TRUE;

	if(ActionKeys[Action].KeyNum1==Q3Key || ActionKeys[Action].KeyNum2==Q3Key) return; 

	if(ActionKeys[Action].KeyNum1==0) {
		ActionKeys[Action].KeyNum1 = Q3Key;
		return;
	}
	if(ActionKeys[Action].KeyNum2==0) {
		ActionKeys[Action].KeyNum2 = Q3Key;
		return;
	}
	Error(ERR_INPUT, "More than two keys pressed per action");
}

//already checked prevKeyboardState
void c_Input::ExecuteKeyUp( int Q3Key )
{
	int Action;

	if(BindAction[Q3Key]<0 || BindAction[Q3Key]>=MAX_ACTIONS) return;
	Action = BindAction[Q3Key];

	if(Action==ACTION_CONSOLE) ActionKeys[Action].isEnabled = FALSE;

	AddToKeyBuffer( Q3Key, FALSE, 0 );

	if(!DQClientGame.isLoaded) return;	//Don't execute bind strings if ClientGame not loaded
	if(bfKeyCatcher) return;	//...or catching key

	if(Action == ACTION_UNKNOWN)
		return;
	if(Action == ACTION_UNKNOWN_STATE && BindString[Q3Key][0]!='\0' ) {
		BindString[Q3Key][0] = '-';
		DQConsole.ExecuteCommandString( BindString[Q3Key], MAX_STRING_CHARS, FALSE );
		BindString[Q3Key][0] = '+';
		return;
	}

	if(ActionKeys[Action].KeyNum1 == 0) return;
	if(ActionKeys[Action].KeyNum1 == Q3Key) {
		if(ActionKeys[Action].KeyNum2 == 0) {
			ActionKeys[Action].isEnabled = FALSE;
			ActionKeys[Action].KeyNum1 = 0;
			return;
		}
		else {
			ActionKeys[Action].KeyNum1 = ActionKeys[Action].KeyNum2;
			ActionKeys[Action].KeyNum2 = 0;
		}
	}
	if(ActionKeys[Action].KeyNum2 == Q3Key) {
		ActionKeys[Action].KeyNum2 = 0;
		return;
	}
	//KeyUp executed without a KeyDown
	//Do nothing
}

void c_Input::UpdateCVars()
{
	DQConsole.UpdateCVar( &m_forward );
	DQConsole.UpdateCVar( &m_side );
	DQConsole.UpdateCVar( &m_yaw );
	DQConsole.UpdateCVar( &m_pitch );
	DQConsole.UpdateCVar( &m_filter );
	DQConsole.UpdateCVar( &sensitivity );
	DQConsole.UpdateCVar( &AlwaysRun );
	DQConsole.UpdateCVar( &win32_keyboard );
	DQConsole.UpdateCVar( &win32_mouse );
}

//Called twice, once by c_Input::Update() with key=0, once by WM_CHAR with key code
void c_Input::AddToKeyBuffer( int Q3Key, BOOL isDown, int key )
{
	//Don't add key to KeyBuffer if we don't need it or we're in the console (uses ConsoleInputChar)
	if(!bfKeyCatcher || (bfKeyCatcher & KEYCATCH_CONSOLE) ) return;
	if(NumKeyBuffer>=MAX_KEYBUFFER-1) return;

	if(Q3Key==0) return;		//don't record 0 or Escape when not in UI
	if(Q3Key==K_ESCAPE && !(bfKeyCatcher & KEYCATCH_UI)) return;

	if(key==0) {
		KeyBuffer[NumKeyBuffer].key = Q3Key;
		KeyBuffer[NumKeyBuffer].isDown = isDown;
		++NumKeyBuffer;
	}
	else if(Q3Key<K_MOUSE1) {	//WM_CHAR
		KeyBuffer[NumKeyBuffer].key = key | K_CHAR_FLAG;
		KeyBuffer[NumKeyBuffer].isDown = isDown;
		++NumKeyBuffer;
	}
	KeyStatus[Q3Key] = isDown;
}

void c_Input::KeyEvent(unsigned char key, BOOL isDown)
{
	int Q3Key = ConvertWin32KeyToQ3(key);

	if(bfKeyCatcher & KEYCATCH_CONSOLE) {			//if in console
		
		if(Q3Key<K_MOUSE1 && isDown)
			DQConsole.InputKey( Q3Key );
	}

	if(win32_keyboard.integer) {
		if(isDown) {
			if(!KeyboardState[key] ) {
				ExecuteKeyDown( Q3Key );
				KeyboardState[key] = TRUE;
			}
		}
		else if(KeyboardState[key]) {
			ExecuteKeyUp( Q3Key );
			KeyboardState[key] = FALSE;
		}
	}
}

void c_Input::MouseEvent(int WMessage, BOOL isDown)
{
	if(!win32_mouse.integer) return;

	switch(WMessage) {
	case MK_LBUTTON:
		if(isDown)	ExecuteKeyDown( K_MOUSE1 );
		else		ExecuteKeyUp( K_MOUSE1 );
		break;
	case MK_RBUTTON:
		if(isDown)	ExecuteKeyDown( K_MOUSE2 );
		else		ExecuteKeyUp( K_MOUSE2 );
		break;
	case MK_MBUTTON:
		if(isDown)	ExecuteKeyDown( K_MOUSE3 );
		else		ExecuteKeyUp( K_MOUSE3 );
		break;
	default : break;
	};
}

int c_Input::GetKeyBuffer(c_Input::s_KeyBuffer *pKeyBuffer)
{
	int temp;

	temp = NumKeyBuffer;
	NumKeyBuffer = 0;
	if(!bfKeyCatcher) temp = 0;
	else memcpy( pKeyBuffer, KeyBuffer, sizeof(s_KeyBuffer)*temp );

	return temp;
}

void c_Input::CreateUserCmd( usercmd_t *UserCmd )
{
	BOOL Walk;
	char InputSpeed;
	D3DXVECTOR3 OtherVel;

	UserCmd->buttons = (BUTTON_ATTACK		* ActionKeys[ACTION_ATTACK].isEnabled)
					 | (BUTTON_TALK			* (ActionKeys[ACTION_TALK].isEnabled | ActionKeys[ACTION_CONSOLE].isEnabled)	//display talk icon when in console
					 | (BUTTON_USE_HOLDABLE * ActionKeys[ACTION_USE].isEnabled)
					 | (BUTTON_GESTURE		* ActionKeys[ACTION_GESTURE].isEnabled)
					 | (BUTTON_AFFIRMATIVE	* ActionKeys[ACTION_AFFIRMATIVE].isEnabled)
					 | (BUTTON_NEGATIVE		* ActionKeys[ACTION_NEGATIVE].isEnabled)
					 | (BUTTON_GETFLAG		* ActionKeys[ACTION_GETFLAG].isEnabled)
					 | (BUTTON_GUARDBASE	* ActionKeys[ACTION_GUARDBASE].isEnabled)
					 | (BUTTON_PATROL		* ActionKeys[ACTION_PATROL].isEnabled)
					 | (BUTTON_FOLLOWME		* ActionKeys[ACTION_FOLLOWME].isEnabled) );
	if(AlwaysRun.integer)
		Walk = ActionKeys[ACTION_WALKING].isEnabled;
	else
		Walk = !ActionKeys[ACTION_WALKING].isEnabled;

	if(Walk) {
		InputSpeed = 64;
		UserCmd->buttons |= BUTTON_WALKING;
	}
	else {
		InputSpeed = 127;
		UserCmd->buttons &= ~BUTTON_WALKING;
	}
	UserCmd->forwardmove = InputSpeed * (ActionKeys[ACTION_MOVEFORWARD].isEnabled - ActionKeys[ACTION_MOVEBACK].isEnabled);
	UserCmd->rightmove = InputSpeed * (ActionKeys[ACTION_MOVERIGHT].isEnabled - ActionKeys[ACTION_MOVELEFT].isEnabled);
	UserCmd->upmove = InputSpeed * (ActionKeys[ACTION_MOVEUP].isEnabled - ActionKeys[ACTION_MOVEDOWN].isEnabled);

	if(UserCmd->buttons) UserCmd->buttons |= BUTTON_ANY;
}

void c_Input::RenderTalkMessage()
{
	if( !(DQInput.GetKeyCatcher() & KEYCATCH_MESSAGE) ) return;

	DQRender.DrawString( 10, 10, BIGCHAR_WIDTH, BIGCHAR_HEIGHT, va("say : %s",CurrentMessage), MAX_STRING_CHARS );
}


int c_Input::ConvertWin32KeyToQ3(int key)
{
	if(key<0) return 0;

	//Convert upper to lower case
	if(key>='A' && key<='Z') return key-'A'+'a';

	if(key>=VK_NUMPAD0 && key<=VK_NUMPAD9) return key-VK_NUMPAD0+'0';

	//need to check this works with other keyboards
	switch(key) {
	case VK_UP: return K_UPARROW;
	case VK_DOWN: return K_DOWNARROW;
	case VK_LEFT: return K_LEFTARROW;
	case VK_RIGHT: return K_RIGHTARROW;
	case VK_HOME: return K_HOME;
	case VK_END: return K_END;
	case VK_PRIOR: return K_PGUP;
	case VK_NEXT: return K_PGDN;
	case VK_ESCAPE: return K_ESCAPE;
	case VK_DELETE: return K_DEL;
	case VK_INSERT: return K_INS;
	case VK_SHIFT: return K_SHIFT;
	case VK_CONTROL: return K_CTRL;
	case VK_MENU: return K_ALT;
	case VK_PAUSE: return K_PAUSE;
	case VK_CAPITAL: return K_CAPSLOCK;
	case VK_MULTIPLY: return K_KP_STAR;
	case VK_ADD: return K_KP_PLUS;
	case VK_SUBTRACT:return K_KP_MINUS;
	case VK_DIVIDE: return K_KP_SLASH;
	case 223:
	case '`':
		return '`';
	default:
		if(key<K_MOUSE1) return key;
		return 0;
	};
}

int c_Input::ConvertDIKeyToQ3(int i)
{
	if(i>=DIK_1 && i<=DIK_0) return (i-DIK_1+'1');

	switch(i) {
	case DIK_ESCAPE: return K_ESCAPE;
	case DIK_MINUS: return '-';
	case DIK_EQUALS: return '=';
	case DIK_BACK:	return K_BACKSPACE;
	case DIK_TAB:	return K_TAB;
	case DIK_Q: return 'q';
	case DIK_W: return 'w';
	case DIK_E: return 'e';
	case DIK_R: return 'r';
	case DIK_T: return 't';
	case DIK_Y: return 'y';
	case DIK_U: return 'u';
	case DIK_I: return 'i';
	case DIK_O: return 'o';
	case DIK_P: return 'p';
	case DIK_LBRACKET: return '(';
	case DIK_RBRACKET: return ')';
	case DIK_RETURN: return K_ENTER;
	case DIK_LCONTROL: return K_CTRL;
	case DIK_A: return 'a';
	case DIK_S: return 's';
	case DIK_D: return 'd';
	case DIK_F: return 'f';
	case DIK_G: return 'g';
	case DIK_H: return 'h';
	case DIK_J: return 'j';
	case DIK_K: return 'k';
	case DIK_L: return 'l';
	case DIK_SEMICOLON: return ';';
	case DIK_APOSTROPHE: return '\'';
	case DIK_GRAVE: return '`';
	case DIK_LSHIFT: return K_SHIFT;
	case DIK_BACKSLASH: return '\\';
	case DIK_Z: return 'z';
	case DIK_X: return 'x';
	case DIK_C: return 'c';
	case DIK_V: return 'v';
	case DIK_B: return 'b';
	case DIK_N: return 'n';
	case DIK_M: return 'm';
	case DIK_COMMA: return ',';
	case DIK_PERIOD: return '.';
	case DIK_SLASH: return '/';
	case DIK_RSHIFT: return K_SHIFT;
	case DIK_MULTIPLY: return '*';
	case DIK_LMENU: return K_ALT;
	case DIK_SPACE: return K_SPACE;
	case DIK_CAPITAL: return K_CAPSLOCK;
	case DIK_F1: return K_F1;
	case DIK_F2: return K_F2;
	case DIK_F3: return K_F3;
	case DIK_F4: return K_F4;
	case DIK_F5: return K_F5;
	case DIK_F6: return K_F6;
	case DIK_F7: return K_F7;
	case DIK_F8: return K_F8;
	case DIK_F9: return K_F9;
	case DIK_F10: return K_F10;
	case DIK_NUMLOCK: return K_KP_NUMLOCK;
	case DIK_NUMPAD7: return K_KP_HOME;
	case DIK_NUMPAD8: return K_KP_UPARROW;
	case DIK_NUMPAD9: return K_KP_PGUP;
	case DIK_SUBTRACT: return K_KP_MINUS;
	case DIK_NUMPAD4: return K_KP_LEFTARROW;
	case DIK_NUMPAD5: return K_KP_5;
	case DIK_NUMPAD6: return K_KP_RIGHTARROW;
	case DIK_ADD: return K_KP_PLUS;
	case DIK_NUMPAD1: return K_KP_END;
	case DIK_NUMPAD2: return K_KP_DOWNARROW;
	case DIK_NUMPAD3: return K_KP_PGDN;
	case DIK_NUMPAD0: return K_KP_INS;
	case DIK_DECIMAL: return K_KP_DEL;
	case DIK_OEM_102: return K_KP_ENTER;		//not sure this is right
	case DIK_F11: return K_F11;
	case DIK_F12: return K_F12;
	case DIK_F13: return K_F13;
	case DIK_F14: return K_F14;
	case DIK_F15: return K_F15;
	case DIK_NUMPADEQUALS: return K_KP_EQUALS;
	case DIK_AT: return '@';
	case DIK_COLON: return ':';
	case DIK_UNDERLINE: return '_';
	case DIK_NUMPADENTER: return K_KP_ENTER;
	case DIK_RCONTROL: return K_COMMAND;
	case DIK_DIVIDE: return K_KP_SLASH;
	case DIK_RMENU: return K_ALT;
	case DIK_HOME: return K_HOME;
	case DIK_UP: return K_UPARROW;
	case DIK_PRIOR: return K_PGUP;
	case DIK_LEFT: return K_LEFTARROW;
	case DIK_RIGHT: return K_RIGHTARROW;
	case DIK_END: return K_END;
	case DIK_DOWN: return K_DOWNARROW;
	case DIK_NEXT: return K_PGDN;
	case DIK_INSERT: return K_INS;
	case DIK_DELETE: return K_DEL;
	default: return 0;
	}
}

//See keycodes.h for enum keyNum_t
char c_Input::KeyNumString[256][MAX_KEYNUMSTRING] =
{ 
// 0=\ ,1,2,3,4,5,6,7,8 (1..8 are made up)
	"", "", "", "", "", "", "", "", "", 
// 9=Tab, 10, 11, 12,
	"Tab", "", "", "",
// 13 = Enter, 14...26
	"Enter", "", "", "", "", "", "", "", "", "", "", "", "", "", 
// 27 = Esc, 28...31
	"Escape", "", "", "", "", 
// 32 = Space, 33...96
	"Space", "", "", "", "", "", "&", "", "", "", "/", "+", "", "-", "", "", "0", "1", 
// 50
	"2", "3", "4", "5", "6", "7", "8", "9", "", "", 
// 60
	"", "=", "", "", "", "", "", "", "", "", 
// 70
	"", "", "", "", "", "", "", "", "", "", 
// 80
	"", "", "", "", "", "", "", "", "", "", 
// 90...96
	"", "[", "\\", "]", "", "_", "`", 
	//97...122 = a to z
	"A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P", "Q", "R", 
	"S", "T", "U", "V", "W", "X", "Y", "Z", 
// 123...126
	"", "", "", "~", 
// 127 = Backspace, 128 = Command, etc. see keycodes.h
	"Backspace", "Command", "Caps Lock", "Power", "Pause", "UpArrow", "DownArrow",
	"LeftArrow", "RightArrow", "Alt", "Ctrl", "Shift", "Insert", "Del", "Page Down", 
	"Page Up", "Home", "End", "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", 
	"F10", "F11", "F12", "F13", "F14", "F15", "Keypad Home", "Keypad Up Arrow", "Keypad Left Arrow", "PgUp", "Keypad 5", 
	"Keypad Right Arrow", "Keypad End", "Keypad Down", "PgDn", "Keypad Enter", "Keypad Insert", 
	"Keypad Delete", "Keypad /", "Keypad -", "Keypad +", "Num Lock", "Keypad *", 
	"Keypad =", "Mouse1", "Mouse2", "Mouse3", "Mouse4", "Mouse5", "MWheelDown", 
	"MWheelUp", "Joy 1", "Joy 2", "Joy 3", "Joy 4", "Joy 5", "Joy 6", "Joy 7", "Joy 8", 
	"Joy 9", "Joy 10", "Joy 11", "Joy 12", "Joy 13", "Joy 14", "Joy 15", "Joy 16", "Joy 17", 
	"Joy 18", "Joy 19", "Joy 20", "Joy 21", "Joy 22", "Joy 23", "Joy 24", "Joy 25", "Joy 26", 
	"Joy 27", "Joy 28", "Joy 29", "Joy 30", "Joy 31", "Joy 32", "Aux 1", "Aux 2", "Aux 3", 
	"Aux 4", "Aux 5", "Aux 6", "Aux 7", "Aux 8", "Aux 9", "Aux 10", "Aux 11", "Aux 12", "Aux 13", 
	"Aux 14", "Aux 15", "Aux 16", "Last Key" };
