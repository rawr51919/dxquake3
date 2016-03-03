// DXQuake3 Source
// Copyright (c) 2003 Richard Geary (richard.geary@dial.pipex.com)
//
//
//	Abstract Class : Automatic Singleton utility
//	Reference : Game Programming Gems 1, Mark DeLoura, article by Scott Bilas
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_C_SINGLETON_H__450B4028_BA64_466B_90CF_8715870B22CE__INCLUDED_)
#define AFX_C_SINGLETON_H__450B4028_BA64_466B_90CF_8715870B22CE__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

//
// Usage : Derive a class from c_Singleton, say c_Example
//		   Create an instance of c_Example
//		   To access c_Example, use c_Example::GetSingleton()
//		   To destroy instance, you can get the pointer using c_Example::GetSingletonPtr()
//

template <typename TClass>
class c_Singleton
{
private:
	static TClass *m_pSingleton;
public:
	c_Singleton();
	virtual ~c_Singleton();
	static TClass& GetSingleton();
	static TClass* GetSingletonPtr();
};

//initialise pointer
template <typename TClass>
TClass *c_Singleton<TClass>::m_pSingleton = NULL;				

//On construction of a derived class, update Singleton pointer to point to new object, 
//and assert if there's more than one created
template <typename TClass>
c_Singleton<TClass>::c_Singleton()
{
	DebugCriticalAssert( !m_pSingleton );
	int offset = (int)(TClass*)(0x1)-(int)(c_Singleton<TClass>*)(TClass*)(0x1);
	m_pSingleton = (TClass*)((int)this + offset);
}

template <typename TClass>
c_Singleton<TClass>::~c_Singleton()
{
	DebugAssert( m_pSingleton );
	m_pSingleton = NULL;	
}

template <typename TClass>
TClass &c_Singleton<TClass>::GetSingleton() { return *m_pSingleton; }

template <typename TClass>
TClass *c_Singleton<TClass>::GetSingletonPtr() { return m_pSingleton; }

#endif // !defined(AFX_C_SINGLETON_H__450B4028_BA64_466B_90CF_8715870B22CE__INCLUDED_)
