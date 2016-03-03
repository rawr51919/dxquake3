// DXQuake3 Source
// Copyright (c) 2003 Richard Geary (richard.geary@dial.pipex.com)
//
// stdafx.h : include file for standard system include files,
//  or project specific include files that are used frequently, but
//      are changed infrequently
//

#if !defined(AFX_STDAFX_H__DBDE617F_E31E_4A41_9193_6EA5BC94B8E6__INCLUDED_)
#define AFX_STDAFX_H__DBDE617F_E31E_4A41_9193_6EA5BC94B8E6__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

//typedefs
typedef unsigned char	U8;
typedef short			S16;
typedef int				S32;
typedef float			F32;
typedef F32				vec3[3];

enum eEndian { LITTLE_ENDIAN, BIG_ENDIAN };

//includes
#include <stdio.h>
#include "windows.h"

//We don't want the Win32 macro PlaySound
#undef PlaySound

//Basic defines
#if _DEBUG
	#define Breakpoint _asm{ int 3 }
#else
	#define Breakpoint
#endif

#pragma warning(disable:4239)

//Includes
#include "d3d9.h"
#include "d3dx9.h"
#define DIRECTINPUT_VERSION 0x0800
#include "dinput.h"
#include "dsound.h"
#include "dplay8.h"

#include "Q3\\q_shared.h"
#include "Q3\\tr_types.h"
#include "Q3\\ui_public.h"
#include "Q3\\cg_public.h"
#include "Q3\\g_public.h"
#include "Q3\\keycodes.h"
#include "Q3\\be_aas.h"

#include "assert.h"
#include "c_Memory.h"
#include "zlib/contrib/minizip/unzip.h"
#include "c_Singleton.h"
#include "c_Chain.h"
#include "c_Queue.h"
#include "utils.h"
#include "c_Time.h"
#include "c_Quicksort.h"

#include "c_Command.h"
#include "c_Console.h"

#include "c_FS.h"

#include "D3DStreams.h"
#include "c_Texture.h"
#include "ShaderFuncs.h"
#include "c_ShaderCommand.h"
#include "c_Shader.h"
#include "c_RenderStack.h"

#include "c_Skin.h"
#include "c_MD3.h"
#include "c_CurvedSurface.h"

#include "c_Camera.h"
#include "c_Render.h"
#include "c_BSP.h"
#include "c_SoundManager.h"
#include "c_Sound.h"
#include "c_Input.h"
#include "c_Network.h"

#include "c_BSPInlineModel.h"
#include "c_Model.h"

#include "c_UI.h"
#include "c_Game.h"
#include "c_ClientGame.h"

//Global Prototypes
LRESULT CALLBACK WndProc(HWND hWnd, unsigned int message, WPARAM wParam, LPARAM lParam);
extern HRESULT DXError_hResult;
extern HANDLE MainLoopEvent;

#ifdef _DEBUG
	extern int d3dcheckNum;
	extern char d3dcheckList[6000][200];
#endif

const GUID g_guidApp = {0xD9D4A731, 0x0048, 0x41D5, {0xAC, 0xB7, 0x74, 0xB9, 0x9E, 0xF6, 0x56, 0x1C}};

//Other Useful defines
#define SafeRelease(x) if(x) { x->Release(); x = NULL; }
#define MakeD3DXVec3FromFloat3(f3) D3DXVECTOR3((f3)[0], (f3)[1], (f3)[2])
#define MakeD3DXVec2FromFloat2(f2) D3DXVECTOR2((f2)[0], (f2)[1])
#define MakeD3DXMatrixFromQ3(axis,origin) MATRIX( axis[0][0], axis[0][1], axis[0][2], 0.0f, axis[1][0], axis[1][1], axis[1][2], 0.0f, axis[2][0], axis[2][1], axis[2][2], 0.0f, origin[0], origin[1], origin[2], 1.0f)
#define MakeFloat3FromD3DXVec3( f3, vec ) { (f3)[0]=(vec).x; (f3)[1]=(vec).y; (f3)[2]=(vec).z; }
#define HomogeniseVec4( vec4 ) { vec4.w = 1.0f/vec4.w; vec4.x *= vec4.w; vec4.y *= vec4.w; vec4.z *= vec4.w; }

#define square(x) ( (x) * (x) )
#define modulus(x) ( ((x)>0)?(x):-(x) )

#define dxcheckOK(condition) { if( FAILED(DXError_hResult = condition) ) Error(ERR_DX, #condition) }
/*
#ifdef _DEBUG
	#define d3dcheckOK(condition) { \
		DQPrint( #condition ); \
		if( (condition) !=D3D_OK ) Error(ERR_D3D, #condition) }
#else
	#define d3dcheckOK(condition) { ++DQRender.NumD3DCalls; if( (condition) !=D3D_OK ) Error(ERR_D3D, #condition) }
#endif
/*
/*
#ifdef _DEBUG
	#define d3dcheckOK(condition) { ++DQRender.NumD3DCalls; if( (condition) !=D3D_OK ) Error(ERR_D3D, #condition) }
#else 
	#define d3dcheckOK(condition) condition
#endif
*/

#ifdef _DEBUG
	#define d3dcheckOK(condition) { ++DQRender.NumD3DCalls; if( FAILED(DXError_hResult = condition) ) CriticalError(ERR_D3D, va("%s\n%d",#condition,DXError_hResult) ) }
#else
	#define d3dcheckOK(condition) { if( FAILED(DXError_hResult = condition) ) CriticalError(ERR_D3D, va("%s\n%d",#condition,DXError_hResult) ) }
#endif
//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_STDAFX_H__DBDE617F_E31E_4A41_9193_6EA5BC94B8E6__INCLUDED_)
