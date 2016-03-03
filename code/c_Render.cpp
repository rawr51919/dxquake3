// DXQuake3 Source
// Copyright (c) 2003 Richard Geary (richard.geary@dial.pipex.com)
//
 //
// c_Render.cpp: Initialisation Only
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"

static char szWindowClassName[] = "DXQ3 WndClass";
static int r_VidModes[12][2] = {
	{ 320, 240 },
	{ 400, 300 },
	{ 512, 384 },
	{ 640, 480 },
	{ 800, 600 },
	{ 960, 720 },
	{ 1024, 768 },
	{ 1152, 864 },
	{ 1280, 1024 },
	{ 1600, 1200 },
	{ 2048, 1536 },
	{ 856, 480 }
};

c_Render::c_Render():isActive(TRUE)
{
	hInst = NULL;
	hWnd = NULL;
	device = NULL;
	ToggleVidRestart = FALSE;
	for(int i=0;i<MAX_MODELS;++i) ModelArray[i] = NULL;
	whiteimageShader = NULL;
	charsetShader = NULL;
	FrameNum = 0;
	NumTextures = 0;
	NumD3DCalls = 0;
	ShaderArray = NULL;
	NoShaderTexture = NULL;
	Direct3D9 = NULL;
	device = NULL;
}

c_Render::~c_Render()
{
	if(device) {
		//If necessary, end scene
		device->EndScene();
	}

	UnloadD3D();

	if( isFullscreen ) {
		ResetD3DDevice(TRUE);
	}

	SafeRelease( device );
	SafeRelease( Direct3D9 );

	DestroyWindow(hWnd);
	UnregisterClass(szWindowClassName, hInst);
}

//Unload everything but Direct3D9
void c_Render::UnloadD3D()
{
	int i;

	//Unload shaders
	c_Shader::UnloadAllStatics();
	if(ShaderArray) {
		for(i=0;i<NumShaders;++i) DQDelete(ShaderArray[i]);
		DQDeleteArray(ShaderArray);
	}
	for(i=0;i<NumModels;++i) DQDelete(ModelArray[i]);
	for(i=0;i<NumSkins;++i) DQDelete(SkinArray[i]);
	for(i=0;i<NumTextures;++i) DQDelete(TextureArray[i]);

	//Unload Textures
	WhiteImageTexture.Release();
	WhiteImageTexture.D3DTexture = NULL;		//We're releasing NoShaderTexture
	SafeRelease( NoShaderTexture );
#ifdef _PORTALS
	for(i=0;i<MAX_RENDERTOTEXTURE;++i) {
		SafeRelease( RenderToTexture[i].RenderSurface );
		SafeRelease( RenderToTexture[i].RenderTarget );
		SafeRelease( RenderToTexture[i].D3DXRenderToSurface );
	}
#endif

	RenderStack.Unload();
}

void c_Render::LoadWindow() {

	//Register Window class and open a window
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX); 

	wcex.style			= CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= (WNDPROC)WndProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= hInst;
	wcex.hIcon			= LoadIcon(NULL, (LPCTSTR)IDI_APPLICATION);
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= NULL;
	wcex.lpszMenuName	= NULL;
	wcex.lpszClassName	= szWindowClassName;
	wcex.hIconSm		= NULL;

	RegisterClassEx(&wcex);

	hWnd = CreateWindowEx(NULL, "DXQ3 WndClass", "DXQuake3", WS_OVERLAPPEDWINDOW,
		0, 0, 800, 600, NULL, NULL, hInst, NULL);

	if (!hWnd)
	{
		CriticalError(ERR_RENDER, "Unable to open window");
		return;
	}

	//Hide window
	ShowWindow(hWnd, SW_SHOW);
	UpdateWindow(hWnd);
}

//Load Direct3D
void c_Render::Initialise()
{
	if(!hWnd) return;

	//Init Direct3D
	//**************************************************************************
	Direct3D9 = Direct3DCreate9(D3D_SDK_VERSION);
	if( !Direct3D9 ) {
		CriticalError(ERR_RENDER, "Failed to load D3D9");
		return;
	}

	//Create a Direct3D device
	InitialiseD3D();

	//******Init Q3 Renderer CVars*************************************************
	DQConsole.RegisterCVar( "fs_restrict", "0", CVAR_ROM, eAuthServer );
	DQConsole.RegisterCVar( "sv_killserver", "0", NULL, eAuthServer );
	DQConsole.RegisterCVar( "com_errorMessage", "", NULL, eAuthClient );
	DQConsole.RegisterCVar( "cl_paused", "0", NULL, eAuthClient );
	DQConsole.RegisterCVar( "com_buildscript", "0", NULL, eAuthClient );	//dunno what this does! Seems to precache wavs in UI
	DQConsole.RegisterCVar( "cl_punkbuster", "0", NULL, eAuthClient );
	DQConsole.RegisterCVar( "session", "0", NULL, eAuthClient );
	DQConsole.RegisterCVar( "bot_enable", "0", NULL, eAuthClient );
	DQConsole.RegisterCVar( "r_RenderScene", "1", NULL, eAuthClient, &r_RenderScene );
	//******End Init Q3 Renderer CVars*********************************************

	D3DXMatrixIdentity( &matIdentity );
}

void c_Render::ResetD3DDevice(BOOL ForceWindowedMode)
{
	D3DDISPLAYMODE D3DDisplayMode;
	HRESULT hr;

	if( ForceWindowedMode ) isFullscreen = FALSE;
	if( !isActive ) return;
	if( !Direct3D9 ) return;

	//Select D3D Device
	ZeroMemory(&D3DPresentParameters, sizeof(D3DPRESENT_PARAMETERS));
	D3DPresentParameters.SwapEffect = D3DSWAPEFFECT_DISCARD;
    D3DPresentParameters.BackBufferCount = 1; 
    D3DPresentParameters.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE; //Immediate = no v_sync, default = v_sync
	D3DPresentParameters.MultiSampleType = D3DMULTISAMPLE_NONE;

	//Get current display mode
	hr = Direct3D9->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &D3DDisplayMode);
	if(FAILED(hr)) {
		CriticalError(ERR_D3D, "Could not get Display Mode");
		return;
	}

	int VidMode = DQConsole.GetCVarInteger( "r_mode", eAuthClient );
	if( VidMode<0 || VidMode>=12 ) {
		Error( ERR_RENDER, "Invalid r_mode - Resetting to 640x480" );
		VidMode = 3;
		DQConsole.SetCVarValue( "r_mode", VidMode, eAuthClient );
	}
	if( DQConsole.IsDedicatedServer() ) VidMode = 3;

	GLconfig.vidWidth = r_VidModes[VidMode][0];
	GLconfig.vidHeight = r_VidModes[VidMode][1];

	D3DDisplayMode.Format = D3DFMT_A8R8G8B8;
	D3DDisplayMode.Width = GLconfig.vidWidth;
	D3DDisplayMode.Height = GLconfig.vidHeight;

	D3DPresentParameters.BackBufferWidth = D3DDisplayMode.Width;
	D3DPresentParameters.BackBufferHeight = D3DDisplayMode.Height;

	//Run in fullscreen?
	if( isFullscreen )
	{
		D3DPresentParameters.Windowed = FALSE;
		D3DPresentParameters.BackBufferFormat = D3DDisplayMode.Format;
	}
	else {
		//Run windowed
		D3DPresentParameters.Windowed = TRUE;
		D3DPresentParameters.BackBufferFormat = D3DDisplayMode.Format;
		SetWindowPos( hWnd, HWND_TOP, 0, 0, D3DDisplayMode.Width, D3DDisplayMode.Height, SWP_DRAWFRAME	| SWP_FRAMECHANGED | SWP_SHOWWINDOW );
	}

	//Enable D3D's Automatic Depth Buffer
	D3DPresentParameters.FullScreen_RefreshRateInHz = D3DPRESENT_RATE_DEFAULT;
	D3DPresentParameters.EnableAutoDepthStencil = TRUE; 
    D3DPresentParameters.AutoDepthStencilFormat = D3DFMT_D16;
	D3DPresentParameters.Flags = D3DPRESENTFLAG_DISCARD_DEPTHSTENCIL;
	D3DPresentParameters.hDeviceWindow = hWnd;

	if( !device ) {
		if( FAILED( Direct3D9->CreateDevice( D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd,
			D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_PUREDEVICE, &D3DPresentParameters, &device) ))
		if( FAILED( Direct3D9->CreateDevice( D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd,
			D3DCREATE_HARDWARE_VERTEXPROCESSING, &D3DPresentParameters, &device) ))
		if( FAILED( Direct3D9->CreateDevice( D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd,
			D3DCREATE_MIXED_VERTEXPROCESSING, &D3DPresentParameters, &device) ))
		if( FAILED( Direct3D9->CreateDevice( D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd,
			D3DCREATE_SOFTWARE_VERTEXPROCESSING, &D3DPresentParameters, &device) )) 
		{
			Error( ERR_RENDER, va("Unable to use video mode %d x %d",D3DDisplayMode.Width,D3DDisplayMode.Height) );
			D3DPresentParameters.BackBufferWidth = GLconfig.vidWidth = D3DDisplayMode.Width = 640;
			D3DPresentParameters.BackBufferHeight = GLconfig.vidHeight = D3DDisplayMode.Height = 480;
			DQConsole.SetCVarValue( "r_mode", 3, eAuthClient );

			if( FAILED( Direct3D9->CreateDevice( D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd,
				D3DCREATE_SOFTWARE_VERTEXPROCESSING, &D3DPresentParameters, &device) )) 
			{
				CriticalError(ERR_RENDER, "Could not create Direct3D Device - Incompatible graphics card");
				return;
			}
		}
	}
	else {
		//vidrestart
		hr = device->TestCooperativeLevel();
		if( (hr != D3D_OK) && (hr != D3DERR_DEVICENOTRESET) ) {
			CriticalError( ERR_RENDER, "Unable to reaquire D3D Device" );
		}

		if( FAILED( hr = device->Reset( &D3DPresentParameters ) ) ) {
			if( hr == D3DERR_DEVICELOST ) DQPrint( "device->Reset : Device Lost");
			if( hr == D3DERR_DRIVERINTERNALERROR ) DQPrint( "device->Reset : Driver internal error");
			if( hr == D3DERR_OUTOFVIDEOMEMORY ) DQPrint( "device->Reset : Out of Video Memory");
			if( hr == E_OUTOFMEMORY ) DQPrint( "device->Reset : Out of memory");
			CriticalError( ERR_RENDER, "Unable to reset Direct3D" );
			return;
		}

		if( !isFullscreen ) ShowWindow( hWnd, SW_SHOWNORMAL );
	}

	D3DCAPS9 Caps;
	d3dcheckOK( device->GetDeviceCaps( &Caps ) );
	if( !(Caps.Caps2 & D3DCAPS2_FULLSCREENGAMMA) ) {
		DQConsole.SetCVarValue( "r_OverbrightBits", 0 , eAuthClient );
		DQConsole.Reload();
	}
}

void c_Render::InitialiseD3D()
{
	D3DDISPLAYMODE dispMode;
	D3DCAPS9 Caps;
	MATRIX mat;

	//************ Initialise Member Variables ******************************
	NumModels = 0;				//First Handle is 1 (0+1)
	NumShaders = 0;
	NumSkins = 0;
	NumDrawBillboards = 0;
	NextPolyVBPos = 0;
	NumLights = 0;
	bClearScreen = FALSE;
	NumD3DCalls = 0;
	NumTextures = 0;

#if _DEBUG
	isFullscreen = FALSE;
#else
	isFullscreen = ( DQConsole.GetCVarInteger( "r_fullscreen", eAuthClient ) )?TRUE:FALSE;
#endif
	if(DQConsole.IsDedicatedServer()) isFullscreen = FALSE;

	DQConsole.Reload();				//set OverbrightBits

	//************ End Initialise Member Variables ******************************

	ResetD3DDevice(FALSE);

	//Set Gamma Ramp
	//****************
	D3DGAMMARAMP GammaRamp, CurrentGammaRamp;
	int i, Overbrighten;
	WORD val;
	
	Overbrighten = DQConsole.OverbrightBits;
	Overbrighten = 1<<Overbrighten;
	for(i=0;i<256;++i) {
		val = 256 * min( 255, i*Overbrighten );
		GammaRamp.red[i] = val;
		GammaRamp.green[i] = val;
		GammaRamp.blue[i] = val;
	}

	if(isFullscreen) {
		device->SetGammaRamp( 0, D3DSGR_NO_CALIBRATION, &GammaRamp );
		Sleep( 200 );	//fix occasional gamma problem (Gamma won't change when told to)
		device->SetGammaRamp( 0, D3DSGR_NO_CALIBRATION, &GammaRamp );

		device->GetGammaRamp( 0, &CurrentGammaRamp );
		for(i=0;i<256;++i) {
			if( (GammaRamp.red[i] != CurrentGammaRamp.red[i]) || (GammaRamp.green[i] != CurrentGammaRamp.green[i])
				|| (GammaRamp.blue[i] != CurrentGammaRamp.blue[i]) ) {
				Error( ERR_RENDER, "GammaRamp not set" );
				break;
			}
		}
		DQPrint( va("GammaRamp Set at %d",DQConsole.OverbrightBits) );
	}

	ResetD3DDevice(FALSE);

	//******End Init Direct3D******************************************************

	//******Get Device Capabilities************************************************
	d3dcheckOK( device->GetDeviceCaps( &Caps ) );

	//Fill out GLconfig for passing to Quake Game DLLs
	ZeroMemory( &GLconfig, sizeof(glconfig_t) );
	DQstrcpy( GLconfig.renderer_string, "Direct 3D", 1);
	DQstrcpy( GLconfig.vendor_string, "", 1);
	DQstrcpy( GLconfig.version_string, "", 1);
	DQstrcpy( GLconfig.extensions_string, "", 1);
	GLconfig.maxTextureSize = min( Caps.MaxTextureWidth, Caps.MaxTextureHeight );
	GLconfig.maxActiveTextures = Caps.MaxSimultaneousTextures;
	GLconfig.colorBits = 32;
	GLconfig.depthBits = 16;
	GLconfig.stencilBits = 0;
	GLconfig.driverType = GLDRV_ICD;
	GLconfig.hardwareType = GLHW_GENERIC;
	GLconfig.deviceSupportsGamma = (Caps.Caps2 & D3DCAPS2_FULLSCREENGAMMA)?qtrue:qfalse;
	GLconfig.textureCompression = TC_NONE;
	GLconfig.textureEnvAddAvailable = qtrue;

	d3dcheckOK( device->GetDisplayMode( 0, &dispMode ) );
	GLconfig.vidWidth = D3DPresentParameters.BackBufferWidth;
	GLconfig.vidHeight = D3DPresentParameters.BackBufferHeight;
	GLconfig.windowAspect = D3DPresentParameters.BackBufferWidth / D3DPresentParameters.BackBufferHeight;	//Could be different for non 4/3 monitors
	GLconfig.displayFrequency = dispMode.RefreshRate;
	GLconfig.isFullscreen = (qboolean)isFullscreen;
	GLconfig.stereoEnabled = qfalse;
	GLconfig.smpActive = qfalse;

	bCaps_DynamicTextures = (Caps.Caps2&D3DCAPS2_DYNAMICTEXTURES)?TRUE:FALSE;
	MaxSimultaneousTextures = Caps.MaxSimultaneousTextures;

	D3DDEVICE_CREATION_PARAMETERS DCP;
	HRESULT hr;

	d3dcheckOK( device->GetCreationParameters( &DCP ) );

	if(Caps.Caps2&D3DCAPS2_CANAUTOGENMIPMAP) {
		hr = Direct3D9->CheckDeviceFormat( DCP.AdapterOrdinal, DCP.DeviceType, D3DPresentParameters.BackBufferFormat, D3DUSAGE_AUTOGENMIPMAP, D3DRTYPE_TEXTURE, D3DFMT_R8G8B8 );
		if(hr == D3D_OK) {
			hr = Direct3D9->CheckDeviceFormat( DCP.AdapterOrdinal, DCP.DeviceType, D3DPresentParameters.BackBufferFormat, D3DUSAGE_AUTOGENMIPMAP, D3DRTYPE_TEXTURE, D3DFMT_A8R8G8B8 );
		}
		if(hr == D3D_OK)
			bCaps_AutogenMipmaps = TRUE;
		else 
			bCaps_AutogenMipmaps = FALSE;
	}
	else 
		bCaps_AutogenMipmaps = FALSE;

	bCaps_SrcBlendfactor = (Caps.SrcBlendCaps&D3DPBLENDCAPS_BLENDFACTOR)?TRUE:FALSE;

	//******End Device Capabilities************************************************

	RenderStack.SetUseStreamOffsets( (Caps.DevCaps2 & D3DDEVCAPS2_STREAMOFFSET)?TRUE:FALSE );
#ifndef _USE_STREAM_OFFSETS
	RenderStack.SetUseStreamOffsets( 0 );		//it's quicker with Stream Offsets off
#endif
	RenderStack.Initialise();

	//******Create White Image & NoShader Textures**********************************
	int u,v;
	D3DLOCKED_RECT LockedRect;
	D3DSURFACE_DESC desc;
	DWORD *line;
	int colour, alpha;
	LPDIRECT3DTEXTURE9 TempTexture;

	//Since we can't lock video-memory textures without using D3DUSAGE_DYNAMIC, we have to copy them via a system memory texture
	d3dcheckOK( D3DXCreateTexture( device, 32, 32, 0, 0, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &NoShaderTexture) );
	d3dcheckOK( D3DXCreateTexture( device, 32, 32, 0, 0, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &TempTexture) );
	d3dcheckOK( TempTexture->GetLevelDesc( 0, &desc ) );

	//WhiteImage Texture
	d3dcheckOK( TempTexture->LockRect( 0, &LockedRect, NULL, 0 ) );

	for(u=0;u<32;++u) {
		line = (DWORD*)((BYTE*)LockedRect.pBits+u*LockedRect.Pitch);
		for(v=0;v<32;++v) {
			colour = 255/Overbrighten;
			alpha = 255;
			line[v] = D3DCOLOR_RGBA( colour, colour, colour, alpha );
		}
	}
	TempTexture->UnlockRect(0);
	WhiteImageTexture.LoadTextureFromSysmemTexture( TempTexture, FALSE, 0 );

	//NoShader Texture
	d3dcheckOK( TempTexture->GetLevelDesc( 0, &desc ) );
	d3dcheckOK( TempTexture->LockRect( 0, &LockedRect, NULL, 0 ) );

	for(u=0;u<32;++u) {
		line = (DWORD*)((BYTE*)LockedRect.pBits+u*LockedRect.Pitch);
		for(v=0;v<32;++v) {
			colour = ((u+v)%2)*255;
			line[v] = D3DCOLOR_RGBA( colour, colour, colour, 255 );
		}
	}
	TempTexture->UnlockRect(0);
	d3dcheckOK( NoShaderTexture->AddDirtyRect( NULL ) );
	d3dcheckOK( device->UpdateTexture( TempTexture, NoShaderTexture ) );	//Blit

	SafeRelease( TempTexture );

	//******End Create White Image & NoShader Textures*******************************

	//******Init DXQuake3 Shaders****************************************************
	ShaderArray = (c_Shader**) DQNewVoid( c_Shader*[MAX_SHADERS] );
	ZeroMemory(ShaderArray, sizeof(c_Shader*)*MAX_SHADERS);
	LoadShaders();
	//HACK - Console shaders have their own sort value so it draws infront of everything
	DQConsole.ConsoleShader = RegisterShader( "console", ShaderFlag_ConsoleShader );
	charsetShader = RegisterShader( "gfx/2d/bigchars", ShaderFlag_ConsoleShader );
	whiteimageShader = 0;		//register this later, so we don't get Q3's "white" shader set with a ShaderFlag_ConsoleShader
	NoShaderShader = RegisterShader( "noshader", 0 );
	ZeroPassShader = RegisterShader( "ZeroPassShader", ShaderFlag_SP(Shader_surfaceParam::NoDraw) );
	//******End Init DXQuake3 Shaders**********************************************

	//******Init Direct3D State****************************************************

	//Trilinear texture filtering, for both texture stages
	for(i=0; i<8; ++i) {
		d3dcheckOK( device->SetSamplerState(i, D3DSAMP_MINFILTER, D3DTEXF_LINEAR) );
		d3dcheckOK( device->SetSamplerState(i, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR) );
		d3dcheckOK( device->SetSamplerState( i, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR) );
		d3dcheckOK( device->SetSamplerState( i, D3DSAMP_MIPMAPLODBIAS, 0) );
	}

#ifdef _UseLightvols
	d3dcheckOK( device->SetRenderState( D3DRS_AMBIENT, 0xFF444444 ) );
#endif
//	d3dcheckOK( device->SetRenderState( D3DRS_AMBIENT, 0xFF777777 ) );
	
	//HACK - Any dynamic lighting is ADDED instead of MULTIPLED (creates realistic lights which saturate the texture)
	d3dcheckOK( device->SetRenderState( D3DRS_AMBIENT, 0 ) );
	for(i=0;i<8;++i) {
		d3dcheckOK( device->SetTextureStageState( i, D3DTSS_ALPHAOP, D3DTOP_MODULATE ) );
		d3dcheckOK( device->SetTextureStageState( i, D3DTSS_COLOROP, D3DTOP_MODULATE ) );
	}
	d3dcheckOK( device->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_TFACTOR ) );
	d3dcheckOK( device->SetTextureStageState( 0, D3DTSS_ALPHAARG2, D3DTA_TFACTOR ) );

	d3dcheckOK( device->SetTextureStageState( 1, D3DTSS_TEXCOORDINDEX, 0 ) );
	d3dcheckOK( device->SetTextureStageState( 2, D3DTSS_TEXCOORDINDEX, 0 ) );
	d3dcheckOK( device->SetTextureStageState( 3, D3DTSS_TEXCOORDINDEX, 0 ) );
	d3dcheckOK( device->SetTextureStageState( 4, D3DTSS_TEXCOORDINDEX, 0 ) );
	d3dcheckOK( device->SetTextureStageState( 5, D3DTSS_TEXCOORDINDEX, 0 ) );
	d3dcheckOK( device->SetTextureStageState( 6, D3DTSS_TEXCOORDINDEX, 0 ) );
	d3dcheckOK( device->SetTextureStageState( 7, D3DTSS_TEXCOORDINDEX, 0 ) );
	d3dcheckOK( DQDevice->SetRenderState( D3DRS_LIGHTING, FALSE ) );
	d3dcheckOK( device->SetRenderState( D3DRS_SPECULARENABLE, FALSE ) );
	d3dcheckOK( device->SetRenderState( D3DRS_DIFFUSEMATERIALSOURCE, D3DMCS_MATERIAL ) );
	d3dcheckOK( device->SetRenderState( D3DRS_AMBIENTMATERIALSOURCE, D3DMCS_MATERIAL ) );	
	d3dcheckOK( device->SetRenderState( D3DRS_FOGTABLEMODE, D3DFOG_EXP ) );
	float Zero = 0.0f;
	d3dcheckOK( device->SetRenderState( D3DRS_FOGSTART, *((DWORD*)&Zero) ) );

//	d3dcheckOK( device->SetRenderState( D3DRS_COLORVERTEX, FALSE ) );
//	d3dcheckOK( device->SetRenderState( D3DRS_DIFFUSEMATERIALSOURCE, D3DMCS_MATERIAL) );

	//Create Default Material
	ZeroMemory( &DefaultMaterial, sizeof(D3DMATERIAL9) );
	DefaultMaterial.Diffuse  = D3DXCOLOR(1.0f, 1.0f, 1.0f, 0.0f);
	DefaultMaterial.Ambient  = D3DXCOLOR(1.0f, 1.0f, 1.0f, 0.0f);
	DefaultMaterial.Specular = D3DXCOLOR(1.0f, 1.0f, 1.0f, 0.0f);
	DefaultMaterial.Power	 = 100.0f;
	d3dcheckOK( device->SetMaterial( &DefaultMaterial ) );

	
	ZeroMemory( &CurrentRS, sizeof(s_RenderState) );

	//******End Init Direct3D State************************************************

	Camera.Initialise();
	pCamera = &Camera;

	DQInput.Initialise();

	ZeroMemory( &SpriteRenderInfo, sizeof(SpriteRenderInfo) );

	SpriteRenderInfo.DrawAsSprite = TRUE;
	CurrentSpriteInfo.Clear();

	BillboardVBHandle = DQRenderStack.NewDynamicVertexBufferHandle( MAX_BILLBOARDS*6, FALSE, TRUE, FALSE );
	PolyVBHandle = DQRenderStack.NewDynamicVertexBufferHandle( MAX_CUSTOMPOLYVERTS, FALSE, TRUE, FALSE );
	SpriteVBhandle = DQRenderStack.NewDynamicVertexBufferHandle( MAX_SPRITES*6, FALSE, TRUE, FALSE );

	//***** Update CVars ******
	DQConsole.SetCVarValue( "r_fullscreen", isFullscreen, eAuthClient );
	DQConsole.SetCVarValue( "r_customwidth", GLconfig.vidWidth, eAuthClient );
	DQConsole.SetCVarValue( "r_customheight", GLconfig.vidHeight, eAuthClient );

	DQPrint( GetDeviceInfo() );

	D3DVIEWPORT9 V;
	V.X = V.Y = 0;
	V.Width = D3DPresentParameters.BackBufferWidth/2;
	V.Height = D3DPresentParameters.BackBufferHeight/2;
	V.MinZ = 0.0f; V.MaxZ = 1.0f;

#ifdef _PORTALS
	for(i=0;i<MAX_RENDERTOTEXTURE; ++i) {
		ZeroMemory( &RenderToTexture[i], sizeof(s_RenderToTexture) );
		RenderToTexture[i].Viewport = V;
	}
	NumRenderToTextures = 0;
#endif
}

void c_Render::ToggleFullscreen()
{
	ToggleVidRestart = TRUE;
	DQConsole.SetCVarValue( "r_fullscreen", !isFullscreen, eAuthClient );
}

void c_Render::VidRestart()
{
	BOOL isInGame;

	DQPrint("Begin VidRestart...");

	DQGame.SetPauseState( TRUE );

	DQBSP.UnloadD3D();

	//reload D3D
	UnloadD3D();

	if( DQClientGame.isLoaded || DQClientGame.LoadMe) isInGame = TRUE;
	else isInGame = FALSE;

//	OldHWnd = hWnd;	
//	LoadWindow();

//	DestroyWindow(OldHWnd);

	DQConsole.Reload();

	InitialiseD3D();

	DQUI.Unload();
	if( isInGame ) {
		DQClientGame.Unload();
	}

	DQSound.Unload();
	DQSound.Initialise();
	DQInput.Initialise();		//new hwnd

	DQGame.SetPauseState( FALSE );

	DQUI.Initialise();

	if( isInGame ) {
		DQClientGame.Load();	
	}

	//Load DQUI.Connect after ClientGame as it checks DQClientGame.isLoaded
	if( DQUI.isConnecting ) {
		//Display Connection Screen
		DQUI.Connect();
	}

	if( !isInGame && !DQUI.isConnecting && !DQConsole.IsDedicatedServer()) {
		//Open Main Menu
		DQUI.OpenMenu();
	}

	DQPrint("VidRestart Complete");
}

void c_Render::LoadShaders()
{
	FILEHANDLE ShaderListHandle, ShaderHandle;
	char *buffer = NULL, filename[MAX_QPATH];
	long fileLength,pos,pos2;
	int i, num;

	//Open Shaderlist.txt and get Shader files to parse
	ShaderListHandle = DQFS.OpenFile("scripts\\shaderlist.txt", "rb");
	if( !ShaderListHandle ) {

		//Create shaderlist.txt
		buffer = (char*)DQNewVoid( char[10000] );
		num = DQFS.GetFileList( "scripts", "shader", buffer, 10000, FALSE );
		
		ShaderListHandle = DQFS.OpenFile( "scripts\\shaderlist.txt", "w+b");
		if(!ShaderListHandle) {
			ShaderListHandle = DQFS.OpenFile( "c:\\shaderlist.txt", "w+b");
		}
		if(!ShaderListHandle) {
			CriticalError( ERR_FS, "Could not find or create shaderlist.txt" );
		}

		//Parse file list
		pos = 0;
		for(i=0;i<num;++i) {
			sprintf( filename, "%s",&buffer[pos] );
			DQStripExtention( filename, MAX_QPATH );
			DQstrcat( filename, "\n", MAX_QPATH );
			pos2 = DQstrlen( (char*)filename, MAX_QPATH );
			DQFS.WriteFile( filename, 1, pos2, ShaderListHandle );
			pos += DQstrlen(&buffer[pos], 10000 )+1;
		}
		DQFS.CloseFile( ShaderListHandle );		//flush

		//Reopen
		ShaderListHandle = DQFS.OpenFile("scripts\\shaderlist.txt", "rb");
		if(!ShaderListHandle) {
			ShaderListHandle = DQFS.OpenFile( "c:\\shaderlist.txt", "rb");
		}

		DQDeleteArray( buffer );
	}

	fileLength = DQFS.GetFileLength(ShaderListHandle);
	buffer = (char*) DQNewVoid( char[fileLength] );
	DQFS.ReadFile(buffer, sizeof(char), fileLength, ShaderListHandle);
	DQFS.CloseFile(ShaderListHandle);

	//Parse File
	pos = 0;
	for(i=0;i<MAX_SHADERS;++i) {
		pos += DQSkipWhitespace(&buffer[pos], fileLength);
		pos2 = pos + DQSkipWord(&buffer[pos], fileLength-pos);
		if(pos2==pos) break;
		DQstrcpy((char*)filename, &buffer[pos], min(pos2-pos+1,MAX_QPATH) );
		ShaderHandle = DQFS.OpenFile(va("scripts\\%s.shader",filename), "rb");
		if(ShaderHandle) {
			LoadShaderFile(ShaderHandle);
			DQFS.CloseFile(ShaderHandle);
		}
		else {
			Error( ERR_FS, va("Missing shader file %s",filename) );
		}
		pos += DQSkipWord(&buffer[pos], fileLength-pos);
	}
	
	//cleanup
	DQDeleteArray(buffer);
}

void c_Render::SetSpriteColour(float *rgba)
{
	if(rgba == NULL) { 
		CurrentSpriteInfo.Colour = 0xFFFFFFFF;
	}
	else {
		CurrentSpriteInfo.Colour = D3DCOLOR_RGBA( (int)(255.0f*rgba[0]), (int)(255.0f*rgba[1]), (int)(255.0f*rgba[2]), (int)(255.0f*rgba[3]) );
	}
}

c_Model *c_Render::GetModel(int ModelHandle)
{
	if(ModelHandle<1 || ModelHandle>NumModels) return NULL;
	return ModelArray[ModelHandle-1];
}

c_Skin *c_Render::GetSkin(int SkinHandle)
{
	if(SkinHandle<1 || SkinHandle>NumSkins) return NULL;
	return SkinArray[SkinHandle-1];
}

int c_Render::ShaderGetFlags(int ShaderHandle)
{
	if(ShaderHandle-1<0 || ShaderHandle-1>NumShaders) return 0;
	return ShaderArray[ShaderHandle-1]->GetFlags();
}

void c_Render::GetQ3ModelBounds( int ModelHandle, float mins[3], float maxs[3] )
{
	if(ModelHandle<1 || ModelHandle>NumModels) return;
	ModelArray[ModelHandle-1]->GetQ3ModelBounds( mins, maxs );
}

c_Texture *c_Render::OpenTextureFile( char *TextureFilename, BOOL NoMipmaps, int OverBrightBits )
{
	int i;
	c_Texture *Tex;

	DQStripGfxExtention( TextureFilename, MAX_QPATH );

	for( i=0; i<NumTextures; ++i ) {
		if( TextureArray[i]->IsEqual( TextureFilename, NoMipmaps, OverBrightBits ) ) {
			TextureArray[i]->AddRef();
			return TextureArray[i];
		}
	}
	
	//Not found - add new texture
	if( NumTextures>=MAX_TEXTURES ) {
		Error( ERR_RENDER, "Too many textures" );
		return TextureArray[0];
	}

	Tex = TextureArray[NumTextures++] = (c_Texture*) DQNewVoid( c_Texture );
	Tex->LoadTexture( TextureFilename, NoMipmaps, OverBrightBits );

	return Tex;
}

//Returns the array dimensions required for CreateSquare
void c_Render::GetSquareArrayDimensions( int *pNumIndices, int *pNumVerts, int Tesselation )
{
	*pNumIndices = 2*(Tesselation+2)*Tesselation-2;
	*pNumVerts = square( Tesselation+1 );
}

//Fills verts to create a tesselated square
//Used for skybox creation
//[in] Direction - normal to square and also used to translate the square by this amount and the square's size
//[out] everything else
void c_Render::CreateSquare( D3DXVECTOR3 Direction, D3DXVECTOR3 *Verts, D3DXVECTOR2 *BoxTexCoords, 
							WORD *Indices, int Tesselation )
{
	static D3DXVECTOR3 YAxis(0,1,0);
	static D3DXVECTOR3 ZAxis(0,0,1);
	D3DXVECTOR3 Axis1, Axis2, Normal, StartPos;
	int i, u, IndexNum;
	const float size = D3DXVec3Length( &Direction );
	float x,y;

	D3DXVec3Normalize( &Normal, &Direction );

	//Pick Axes
	//choose arbitrary vector (carefully)
	if(fabs( Normal.z )>0.8f)
		//normal is close to z-axis. Use y.
		D3DXVec3Cross( &Axis1, &Normal, &YAxis );
	else
		D3DXVec3Cross( &Axis1, &Normal, &ZAxis );

	D3DXVec3Normalize( &Axis1, &Axis1 );
	D3DXVec3Cross( &Axis2, &Normal, &Axis1 );
	D3DXVec3Normalize( &Axis2, &Axis2 );

	Axis1 *= size*2;
	Axis2 *= size*2;
	StartPos = Direction - 0.5f*(Axis1+Axis2);
	IndexNum = 0;

	for( i=0; i<=Tesselation; ++i ) {
		for( u=0; u<=Tesselation; ++u,++IndexNum) {
			x = ((float)i / (float)Tesselation);
			y = ((float)u / (float)Tesselation);

			*Verts = StartPos + Axis1 * x + Axis2 * y;
			*BoxTexCoords = D3DXVECTOR2( x,y );
			++Verts;
			++BoxTexCoords;

			if(i!=Tesselation) {
				*Indices = IndexNum;
				++Indices;
				*Indices = IndexNum+Tesselation+1;
				++Indices;
			}
		}
		if(i<Tesselation-1) {
			//Move tristrip to beginning of next strip
			*Indices = IndexNum+Tesselation;
			++Indices;
			*Indices = IndexNum;
			++Indices;
		}
	}
}


//Returns the array dimensions required for CreateDome
void c_Render::GetDomeArrayDimensions( int *pNumIndices, int *pNumVerts, int Tesselation )
{
	*pNumIndices = 2*(Tesselation+2)*Tesselation-2 +6;
	*pNumVerts = square( Tesselation+1 )+4;
}

//Fills verts to create a tesselated square
//Used for skydome creation
void c_Render::CreateDome( float radius, int CloudHeight, D3DXVECTOR3 *Verts, D3DXVECTOR2 *TexCoords, 
							WORD *Indices, int Tesselation, float TexCoordScale )
{
	int i, u, IndexNum;
	float theta,phi;
	float scale;

	CloudHeight = max( 1, min( 512, CloudHeight ) );
	scale = (float)CloudHeight/512.0f;

	IndexNum = 0;

	for( i=0; i<=Tesselation; ++i ) {
		for( u=0; u<=Tesselation; ++u,++IndexNum) {
			theta = D3DX_PI * ((float)i / (float)Tesselation);
			phi = D3DX_PI * ((float)u / (float)Tesselation);

			*Verts = D3DXVECTOR3( (float)(radius*cos(theta)*sin(phi)), (float)(radius*cos(phi)), (float)(radius*sin(theta)*sin(phi))-radius/2.0f );

			TexCoords->x = (scale*(theta - D3DX_PI/2.0f) + D3DX_PI/2.0f) / D3DX_PI * TexCoordScale;
			TexCoords->y = (scale*(phi - D3DX_PI/2.0f) + D3DX_PI/2.0f) / D3DX_PI * TexCoordScale;
			
			++Verts;
			++TexCoords;

			if(i!=Tesselation) {
				*Indices = IndexNum;
				++Indices;
				*Indices = IndexNum+Tesselation+1;
				++Indices;
			}
		}
		if(i<Tesselation-1) {
			//Move tristrip to beginning of next strip
			*Indices = IndexNum+Tesselation;
			++Indices;
			*Indices = IndexNum;
			++Indices;
		}
	}
	//Add dome base
	*Indices = IndexNum-1;	++Indices;
	*Indices = IndexNum;	++Indices;
	*Indices = IndexNum;	++Indices;
	*Indices = IndexNum+1;	++Indices;
	*Indices = IndexNum+2;	++Indices;
	*Indices = IndexNum+3;	++Indices;
	*Verts = D3DXVECTOR3( radius, radius, -radius/2.0f );	++Verts;
	*Verts = D3DXVECTOR3( radius, -radius, -radius/2.0f );	++Verts;
	*Verts = D3DXVECTOR3( -radius, radius, -radius/2.0f );	++Verts;
	*Verts = D3DXVECTOR3( -radius, -radius, -radius/2.0f );	++Verts;
	*TexCoords = D3DXVECTOR2( 0,1 );	++TexCoords;
	*TexCoords = D3DXVECTOR2( 0,0 );	++TexCoords;
	*TexCoords = D3DXVECTOR2( 1,1 );	++TexCoords;
	*TexCoords = D3DXVECTOR2( 1,0 );	++TexCoords;
}

char * c_Render::GetDeviceInfo()
{
	const int MaxLength = 2000;
	int Len;
	static char String[MaxLength];

	D3DDEVICE_CREATION_PARAMETERS DCP;
	int NumSwapChains;
	D3DPRESENT_PARAMETERS *DPP;
	D3DCAPS9 Caps;

	d3dcheckOK( device->GetCreationParameters( &DCP ) );
	NumSwapChains = device->GetNumberOfSwapChains();
	DPP = GetPresentParameters();
	d3dcheckOK( device->GetDeviceCaps( &Caps ) );

	Len = MaxLength;

	Len -= DQstrcpy( String, "^2Render Device Info :\n", Len );

	switch(DCP.DeviceType) {
	case D3DDEVTYPE_HAL:
		Len -= DQstrcat( String, "Device Type : Hardware Acceleration\n", Len );
		break;
	case D3DDEVTYPE_REF:
		Len -= DQstrcat( String, "Device Type : ^1Software Acceleration (D3D9 Reference Driver)\n", Len );
		break;
	};

	if(DCP.BehaviorFlags & D3DCREATE_PUREDEVICE) {
		Len -= DQstrcat( String, "Pure Device\n", Len );
	}
	if(DCP.BehaviorFlags & D3DCREATE_HARDWARE_VERTEXPROCESSING) {
		Len -= DQstrcat( String, "Hardware Vertex Processing\n", Len );
	}
	if(DCP.BehaviorFlags & D3DCREATE_SOFTWARE_VERTEXPROCESSING) {
		Len -= DQstrcat( String, "Software Vertex Processing\n", Len );
	}
	if(DCP.BehaviorFlags & D3DCREATE_MIXED_VERTEXPROCESSING) {
		Len -= DQstrcat( String, "Hardware/Software Mixed Vertex Processing\n", Len );
	}

	Len -= DQstrcat( String, va("Device Resolution : %d x %d\n", DPP->BackBufferWidth, DPP->BackBufferHeight), Len );
	Len -= DQstrcat( String, "Device Format : ", Len );
	switch(DPP->BackBufferFormat) {
	case D3DFMT_R8G8B8:
		Len -= DQstrcat( String, "^1R8G8B8 - 24bit Colour, No Alpha\n", Len );
		break;
	case D3DFMT_A8R8G8B8:
		Len -= DQstrcat( String, "A8R8G8B8 - 32bit Colour\n", Len );
		break;
	case D3DFMT_X8R8G8B8:
		Len -= DQstrcat( String, "^1X8R8G8B8 - 24bit Colour, No Alpha\n", Len );
		break;
	default:
		Len -= DQstrcat( String, "^1Unrecognised Format\n", Len );
		break;
	};

	Len -= DQstrcat( String, va("MaxSimultaneousTextures : %d\n",MaxSimultaneousTextures), Len );

	Len -= DQstrcat( String, va("OverbrightBits : %d\n", DQConsole.OverbrightBits), Len );
	Len -= DQstrcat( String, va("Fullscreen? : %s\n", (isFullscreen)?"True":"False"), Len );
	Len -= DQstrcat( String, va("Can Adjust Gamma : %s\n", (Caps.Caps2 & D3DCAPS2_FULLSCREENGAMMA)?"True":"False"), Len );

	if(NumSwapChains!=1) Len -= DQstrcat( String, "^1", Len );
	Len -= DQstrcat( String, va("NumSwapChains : %d\n \n", NumSwapChains), Len );

	Assert( Len>0 );
	return String;
}

//Returns false if overbright is wrong
BOOL c_Render::CheckOverbright()
{
	D3DGAMMARAMP GammaRamp, CurrentGammaRamp;
	int i,val, Overbrighten;
	
	if(!isFullscreen) return TRUE;

	Overbrighten = DQConsole.OverbrightBits;
	Overbrighten = 1<<Overbrighten;
	for(i=0;i<256;++i) {
		val = 256 * min( 255, i*Overbrighten );
		GammaRamp.red[i] = val;
		GammaRamp.green[i] = val;
		GammaRamp.blue[i] = val;
	}

	device->GetGammaRamp( 0, &CurrentGammaRamp );
	for(i=0;i<256;++i) {
		if( (GammaRamp.red[i] != CurrentGammaRamp.red[i]) || (GammaRamp.green[i] != CurrentGammaRamp.green[i])
			|| (GammaRamp.blue[i] != CurrentGammaRamp.blue[i]) ) {
			return FALSE;
		}
	}

	//Win32
/*	HDC hDC;
	hDC = GetDC( DQRender.hWnd );
	GetDeviceGammaRamp( hDC, &CurrentGammaRamp );
	for(i=0;i<256;++i) {
		if( (GammaRamp.red[i] != CurrentGammaRamp.red[i]) || (GammaRamp.green[i] != CurrentGammaRamp.green[i])
			|| (GammaRamp.blue[i] != CurrentGammaRamp.blue[i]) ) {
			return FALSE;
		}
	}
	ReleaseDC( DQRender.hWnd, hDC );
*/
//	DQPrint( va("Overbright check OK, set at %d", Overbrighten ) );

	return TRUE;
}