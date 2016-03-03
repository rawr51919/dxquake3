// DXQuake3 Source
// Copyright (c) 2003 Richard Geary (richard.geary@dial.pipex.com)
//
//
//		c_Texture
//

#ifndef __C_TEXTURE_H
#define __C_TEXTURE_H

class c_Texture
{
public:
	c_Texture();
	virtual ~c_Texture();
	void LoadTexture(const char *TextureFilename, BOOL bNoMipmaps, int OverbrightBits);
	void LoadTextureFromSysmemTexture(LPDIRECT3DTEXTURE9 SysmemTexture, BOOL bNoMipmaps, int OverbrightBits);

	void AddRef();
	void Release();
	inline BOOL IsEqual( const char *TextureFilename, BOOL bNoMipmaps, int OverbrightBits)
	{
		return ( (DQstrcmpi(TextureFilename, m_Filename, MAX_QPATH)==0) && (bNoMipmaps==m_bNoMipmaps) && (OverbrightBits==m_OverbrightBits) );
	}

	char m_Filename[MAX_QPATH];

	LPDIRECT3DTEXTURE9	D3DTexture;

private:
	BOOL isLoaded;
	BOOL m_bNoMipmaps;
	int m_OverbrightBits;
	int RefCount;
};

#endif // __C_TEXTURE_H