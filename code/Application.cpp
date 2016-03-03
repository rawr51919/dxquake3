// DXQuake3 Source
// Copyright (c) 2003 Richard Geary (richard.geary@dial.pipex.com)

#include "stdafx.h"
#include "eh.h"

void LoadWindow();
void __cdecl OnExit(void);

void MessageLoop();
void MainLoop();
void ForceExit();

HANDLE MainLoopEvent;

static void __cdecl se_translator(unsigned uint, EXCEPTION_POINTERS *p)
{
	Breakpoint;

	if(uint==EXCEPTION_ACCESS_VIOLATION && p->ExceptionRecord->NumberParameters>=2) {
		if(p->ExceptionRecord->ExceptionInformation[0] == 0) {
			CriticalError( ERR_UNKNOWN, va("Access Violation at address 0x%08X, Attempt to READ from address 0x%08X",p->ExceptionRecord->ExceptionAddress, p->ExceptionRecord->ExceptionInformation[1]) );
		}
		else if(p->ExceptionRecord->ExceptionInformation[0] == 1) {
			CriticalError( ERR_UNKNOWN, va("Access Violation at address 0x%08X, Attempt to WRITE to address 0x%08X",p->ExceptionRecord->ExceptionAddress, p->ExceptionRecord->ExceptionInformation[1]) );
		}
	}
	CriticalError( ERR_UNKNOWN, va("Exception 0x%08X at address 0x%08X",uint, p->ExceptionRecord->ExceptionAddress) );
}

//File Global variables
MSG msg;
HWND hWnd;
c_FS *pFS = NULL;
c_Time *pTime = NULL;
c_Console *pConsole = NULL;
c_Render *pRender = NULL;
c_SoundManager *pSound = NULL;
c_Input *pInput = NULL;
c_Network *pNetwork = NULL;
c_BSP *pBSP = NULL;
c_UI *pUI = NULL;
c_ClientGame *pClientGame = NULL;
c_Game *pGame = NULL;


//Entry Point from Windows
int APIENTRY WinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPSTR     lpCmdLine,
                     int       nCmdShow)
{
	MainLoopEvent = NULL;

  //atexit(OnExit);
  _set_se_translator( se_translator );
  try {

	MainLoopEvent = CreateEvent( 0, FALSE, FALSE, NULL );

	//Create Singletons

	pFS = DQNew(c_FS);
	pTime = DQNew(c_Time);
	pConsole = DQNew(c_Console);
	pRender = DQNew(c_Render);
	pSound = DQNew(c_SoundManager);
	pInput = DQNew(c_Input);
	pNetwork = DQNew(c_Network);
	pBSP = DQNew(c_BSP);
	pUI = DQNew(c_UI);
	pClientGame = DQNew(c_ClientGame);
	pGame = DQNew(c_Game);

	//Create Loading window
	DQRender.SetHInstance(hInstance);
	DQRender.LoadWindow();
	DQFS.Initialise();	//load fs_game variable before DQConsole.Initialise loads q3config.cfg
	DQConsole.ProcessCommandLine( lpCmdLine );
	DQConsole.Initialise();

	DQRender.Initialise();
	DQTime.Initialise();
	DQInput.Initialise();
	DQSound.Initialise();

	hWnd = DQRender.GetHWnd();

#ifdef _DEDICATED
	DQConsole.SetCVarString("dedicated", "1", eAuthServer);		//Dedicated Server
	DQConsole.SetCVarString("mapname", "q3dm1", eAuthServer);
#endif

	if(DQConsole.IsDedicatedServer()) {
		DQConsole.ShowConsole( eShowConsoleFull );
	}

	DQUI.Load();

	DQTime.ProfileLogEveryFrame = FALSE;

	DQRenderStack.SaveState();

	// Main message loop:
	MessageLoop();

	DQConsole.Disconnect();
  }
  catch( c_Exception &e ) {
	  ForceExit();
	  MessageBox( NULL, e.GetExceptionText(), "DXQuake3 Critical Error", MB_OK | MB_SYSTEMMODAL );
  }
  catch( unsigned int u ) {
	ForceExit();
	c_Exception e(ERR_UNKNOWN, va("Standard Exception : %x",u));
	DQConsole.ProcessError( e, __LINE__, __FILE__ );
	MessageBox( NULL, e.GetExceptionText(), "DXQuake3 Critical Error", MB_OK | MB_SYSTEMMODAL );
  }
  catch(...) {
	ForceExit();
	c_Exception e(ERR_UNKNOWN, "Critical Program Error");
	DQConsole.ProcessError( e, __LINE__, __FILE__ );
	MessageBox( NULL, e.GetExceptionText(), "DXQuake3 Critical Error", MB_OK | MB_SYSTEMMODAL );
  }

  try {
	DQDelete(pGame);
	DQDelete(pClientGame);
	DQDelete(pUI);
	DQDelete(pBSP);
	DQDelete(pNetwork);
	DQDelete(pInput);
	DQDelete(pSound);
	DQDelete(pRender);
	DQDelete(pConsole);
	DQDelete(pTime);
	DQDelete(pFS);

	if(MainLoopEvent) CloseHandle( MainLoopEvent );
  }
  catch(...)
  {
  }

	return msg.wParam;
}

void MessageLoop()
{
	BOOL Quit;
	Quit = FALSE;

	do
	{
		if(!DQConsole.IsDedicatedServer() && DQRender.isActive) {

			while(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) 
			{
				DQProfileSectionStart(6,"Win32 Messages");

				TranslateMessage(&msg);
				DispatchMessage(&msg);

				DQProfileSectionEnd(6);

				if(msg.message==WM_QUIT) { Quit=TRUE; break; }
			}
			//Continue to Main Loop once all messages are processed

		}
		else {			//is Dedicated Server or is not active

			SetTimer( DQRender.GetHWnd(), 1, 50, NULL );		//about 20fps

			do {
				Quit = !GetMessage(&msg, NULL, 0, 0);
				if( Quit) break;
				
				DQProfileSectionStart(6,"Win32 Messages");

				TranslateMessage(&msg);
				DispatchMessage(&msg);

				DQProfileSectionEnd(6);
			}while(msg.message!=WM_TIMER && msg.message!=WM_QUIT);
			//Wait until WM_TIMER message before going to Main Loop

		}
		if(msg.message==WM_QUIT || Quit) break;

		//Main Loop
		if(DQRender.isActive) {
			MainLoop();
		}

	}while(msg.message!=WM_QUIT && !Quit);
}

void MainLoop()
{
#ifdef _PROFILE
		DQTime.ProfileMarkFrame();
#endif
		SetEvent( MainLoopEvent );
		ResetEvent( MainLoopEvent );

#if _DEBUG
		DQMemory.ValidateMemory();
#endif

		if(DQClientGame.LoadMe) DQClientGame.Load();

		DQRender.BeginRender();

DQProfileSectionStart(3,"Update Input");
		DQInput.Update();
DQProfileSectionEnd(3);

DQProfileSectionStart(8,"Update Sound");
		DQSound.Update();
DQProfileSectionEnd(8);

//DQProfileSectionStart(4, "Client Game");
		DQClientGame.Update();
//DQProfileSectionEnd(4);

		DQRender.DrawScene();

		DQUI.Update();

DQProfileSectionStart(9, "Update Console");
		DQConsole.Update();
DQProfileSectionEnd(9);

		DQRender.DrawScene();
		DQRender.EndRender();
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	U8 key;

	key = wParam;

	if( !( c_Console::GetSingletonPtr() && c_Input::GetSingletonPtr() ) )
		return DefWindowProc(hWnd, message, wParam, lParam);

	switch (message) 
	{
	case WM_KEYDOWN:
		DQInput.KeyEvent(key, TRUE);
		return 0;
	case WM_KEYUP:
		DQInput.KeyEvent(key, FALSE);
		return 0;
	case WM_CHAR:
		DQInput.AddToKeyBuffer( DQInput.ConvertWin32KeyToQ3(key), TRUE, key );
		DQInput.AddToKeyBuffer( DQInput.ConvertWin32KeyToQ3(key), FALSE, key );
		//Only run when in console, instead of Update()
		DQInput.ConsoleInputChar(key);
		break;
	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
	case WM_MBUTTONDOWN:
		DQInput.MouseEvent(wParam, TRUE);
		return 0;
	case WM_LBUTTONUP:
		DQInput.MouseEvent(MK_LBUTTON, FALSE);
		return 0;
	case WM_RBUTTONUP:
		DQInput.MouseEvent(MK_RBUTTON, FALSE);
		return 0;
	case WM_MBUTTONUP:
		DQInput.MouseEvent(MK_MBUTTON, FALSE);
		return 0;
/*	case WM_MOUSEWHEEL:
		if(HIWORD(wParam)>0)
			DQInput.MouseEvent(WM_MOUSEWHEEL, FALSE);
		else if(HIWORD(wParam)<0)
			DQInput.MouseEvent(WM_MOUSEWHEEL, TRUE);
		return 0;*/

	case WM_ACTIVATEAPP:
		if(wParam == FALSE) {
			DQRender.isActive = FALSE;
#ifndef _DEBUG
			if( !DQRender.IsFullscreen() ) {
				ShowWindow( DQRender.GetHWnd(), SW_MINIMIZE );
			}
#endif
		}
		else {
			DQRender.isActive = TRUE;
			DQConsole.SetCVarValue( "r_RenderScene", 1, eAuthClient );
			if( DQRender.IsFullscreen() ) {
				DQRender.ToggleVidRestart = TRUE;
			}
#ifndef _DEBUG
			else {
				ShowWindow( DQRender.GetHWnd(), SW_RESTORE );
			}
#endif
			DQInput.Acquire();
		}
		break;

	case WM_TIMER:
		//Server Update
DQProfileSectionStart(5, "Server Game");

		DQGame.Update();

#if 0
		double time;
		static double LastTime;
		time = DQTime.GetPerfMillisecs();
		DQPrint(va("Game FPS : %f",1000.0f/(time-LastTime)));
		LastTime = time;
#endif
DQProfileSectionEnd(5);
		break;

	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
   }
   return 0;
}

void ForceExit()
{
	try {
		//Make sure Game doesn't try and run
		if(c_Game::GetSingletonPtr()) {
			DQGame.SetPauseState( TRUE );
		}

		//Exit D3D and show windows
		if(c_Console::GetSingletonPtr()) {
			DQConsole.FlushLogfile();
		}
		if(c_BSP::GetSingletonPtr()) {
			DQBSP.UnloadD3D();
		}
		if(c_Render::GetSingletonPtr()) {
			DQRender.UnloadD3D();
			DQRender.ResetD3DDevice(TRUE);
		}
	}
	catch(...) {
		if(c_Console::GetSingletonPtr()) {
			Error( ERR_MISC, "Unable to force exit" );
			DQConsole.FlushLogfile();
		}
	}
}