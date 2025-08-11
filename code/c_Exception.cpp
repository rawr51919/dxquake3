// DXQuake3 Source
// Copyright (c) 2003 Richard Geary (richard.geary@dial.pipex.com)
//

#include "stdafx.h"

c_Exception::c_Exception( e_ErrorType ErrorType, const char *ErrorMessage )
{
	if(ErrorMessage) { 
		DQstrcpy( ExceptionText, ErrorMessage, 1000 ); 
		m_ErrorType = ErrorType; 
	}
}

const char * c_Exception::GetExceptionText()
{
	switch(m_ErrorType) {
	case ERR_ASSERT:
		return va("Assert Error : %s",ExceptionText);
	case ERR_MEM:
		return va("Memory Error : %s",ExceptionText);
	case ERR_CVAR:
		return va("CVar Error : %s",ExceptionText);
	case ERR_CONSOLE:
		return va("Console Error : %s",ExceptionText);
	case ERR_TIME:
		return va("Time Error : %s",ExceptionText);
	case ERR_FS:
		return va("FileSystem Error : %s",ExceptionText);
	case ERR_PARSE:
		return va("Parse Error : %s",ExceptionText);
	case ERR_BSP:
		return va("BSP Error : %s",ExceptionText);
	case ERR_RENDER:
		return va("Render Error : %s",ExceptionText);
	case ERR_DX:
		return va("DirectX Error : %s",ExceptionText);
	case ERR_D3D:
		return va("Direct3D Error : %s",ExceptionText);
	case ERR_SHADER:
		return va("Shader Error : %s",ExceptionText);
	case ERR_SOUND:
		return va("Sound Error : %s",ExceptionText);
	case ERR_INPUT:
		return va("Input Error : %s",ExceptionText);
	case ERR_NETWORK:
		return va("Network Error : %s",ExceptionText);
	case ERR_UI:
		return va("UserInterface Error : %s",ExceptionText);
	case ERR_GAME:
		return va("Q3Game Error : %s",ExceptionText);
	case ERR_CGAME:
		return va("Q3ClientGame Error : %s",ExceptionText);
	case ERR_MISC:
		return va("Miscellaneous Error : %s",ExceptionText);
	default:
		return va("Unknown Error : %s",ExceptionText);
	};
}

void c_Console::ProcessError( c_Exception &ErrorMessage, int Line, char *File )
{
	char Text[1001];
	sprintf(Text, "^1%s - Line : %d File : %s\n",ErrorMessage.GetExceptionText(),Line,File);

#if _DEBUG
	OutputDebugString( Text );
	if(DQRender.IsFullscreen()) {
		ShowWindow(DQRender.GetHWnd(), SW_HIDE);
	}
  	Breakpoint;
#endif

	if(!LogFile) {
		throw ErrorMessage;
	}

	DQPrint( Text );
	fflush(LogFile);

	LogContainsError = TRUE;
}

