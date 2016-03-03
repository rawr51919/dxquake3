// DXQuake3 Source
// Copyright (c) 2003 Richard Geary (richard.geary@dial.pipex.com)
//
// c_Skin.h: interface for the c_Skin class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_C_SKIN_H__E13D2556_9C38_45E0_A912_65B2C7FE4945__INCLUDED_)
#define AFX_C_SKIN_H__E13D2556_9C38_45E0_A912_65B2C7FE4945__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class c_Skin  
{
public:
	c_Skin();
	virtual ~c_Skin();
	void c_Skin::LoadSkin(const char *Filename);
	int GetShaderFromSurfaceName(const char *SurfaceName);

	char SkinFilename[MAX_QPATH];
private:
	struct s_SkinInfo {
		char SurfaceName[MAX_QPATH];
		char ShaderName[MAX_QPATH];
		int ShaderHandle;
	};
	c_Chain<s_SkinInfo> SkinChain;
};

#endif // !defined(AFX_C_SKIN_H__E13D2556_9C38_45E0_A912_65B2C7FE4945__INCLUDED_)
