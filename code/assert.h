// DXQuake3 Source
// Copyright (c) 2003 Richard Geary (richard.geary@dial.pipex.com)
//
// Assert and Error macros
//

/*
CriticalAssert(value)
	 - Assert that value is non-zero, and exit program if zero
Assert
	 - Notify user if zero, but do not exit

CriticalDebugAssert
DebugAssert
	 - As above, but only evaluated in Debug Build

Error(char *Message)
	 - Notify user with error message

ErrorCritical(char *Message)
	 - Notify and exit with error message
*/

#ifndef __ASSERT_H
#define __ASSERT_H

//Error Types
enum e_ErrorType {
	ERR_ASSERT = 1,
	ERR_MEM,
	ERR_CVAR,
	ERR_CONSOLE,
	ERR_TIME,
	ERR_FS,
	ERR_PARSE,
	ERR_BSP,
	ERR_RENDER,
	ERR_DX,
	ERR_D3D,
	ERR_SHADER,
	ERR_SOUND,
	ERR_INPUT,
	ERR_NETWORK,
	ERR_UI,
	ERR_GAME,
	ERR_CGAME,
	ERR_MISC,
	ERR_UNKNOWN
};

//Prototypes
void WindowsErrorMessage(e_ErrorType ErrorType, const char *ErrorMessage, int Line, char *File);

#define CriticalError(ErrorType, ErrorMessage) { \
	if(c_Console::GetSingletonPtr()) { c_Console::GetSingletonPtr()->ProcessError( c_Exception( ErrorType, ErrorMessage ), __LINE__, __FILE__ ); }\
	throw c_Exception( ErrorType, ErrorMessage ); \
}

#define Error(ErrorType, ErrorMessage) { \
	if(!c_Console::GetSingletonPtr()) { throw c_Exception( ErrorType, ErrorMessage ); } \
	else { \
		c_Console::GetSingletonPtr()->ProcessError( c_Exception( ErrorType, ErrorMessage ), __LINE__, __FILE__ ); \
	} \
}

#define Warning(ErrorType, ErrorMessage)	 \
	DQPrint( c_Exception( ErrorType, ErrorMessage ).GetExceptionText() )

#define Assert(value) { \
	if(!(value)) Error(ERR_ASSERT, #value); \
}

#define CriticalAssert(value) { \
	if(!(value)) CriticalError(ERR_ASSERT, #value); \
}

#if _DEBUG
#define DebugAssert(value) { if(!(value)) Error(ERR_ASSERT, #value); }
#define DebugCriticalAssert(value) { if(!(value)) CriticalError(ERR_ASSERT, #value); }
#else
	#define DebugAssert(value)
	#define DebugCriticalAssert(value)
#endif //_DEBUG

//For DirectX COM interfaces
#define SafeRelease(x) if(x) { x->Release(); x = NULL; }

class c_Exception {
public:
	c_Exception::c_Exception( e_ErrorType ErrorType, const char *ErrorMessage );
	const char * c_Exception::GetExceptionText();

private:
	char ExceptionText[1000];
	e_ErrorType  m_ErrorType;
};


#endif //__ASSERT_H