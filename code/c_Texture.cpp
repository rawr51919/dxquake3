// DXQuake3 Source
// Copyright (c) 2003 Richard Geary (richard.geary@dial.pipex.com)
//
//
//		Wrapper Class for LPDIRECT3DTEXTURE9
//		Deals with texture management (in c_Render), and mipmap generation
//

#include "stdafx.h"

c_Texture::c_Texture()
{
	isLoaded = FALSE;
	m_Filename[0] = '\0';
	D3DTexture = NULL;
	m_bNoMipmaps = FALSE;
	m_OverbrightBits = 0;
	RefCount = 1;
}

c_Texture::~c_Texture()
{
	LPDIRECT3DTEXTURE9 NoShaderTexture = DQRender.NoShaderTexture;

	if( D3DTexture != NoShaderTexture ) {
		SafeRelease( D3DTexture );
	}
}

void c_Texture::AddRef()
{
	++RefCount;
}

void c_Texture::Release()
{
	if( RefCount==1 ) {
		if( D3DTexture != DQRender.NoShaderTexture ) {
			SafeRelease( D3DTexture );
		}
		isLoaded = FALSE;
		D3DTexture = DQRender.NoShaderTexture; //in case we try to access it again
	}
	if( RefCount>0 ) --RefCount;
}

void c_Texture::LoadTexture(const char *TextureFilename, BOOL bNoMipmaps, int OverbrightBits)
{
	int Handle, FileSize;
	LPDIRECT3DTEXTURE9 TempTexture;

	DQstrcpy( m_Filename, TextureFilename, MAX_QPATH );
	//Can't use DQStripExtention (due to railgun2.glow)
	DQStripGfxExtention( m_Filename, MAX_QPATH );

	//Find and open file
	DQstrcat(m_Filename, ".tga", MAX_OSPATH);
	Handle = DQFS.OpenFile( m_Filename, "rb" );
	if(!Handle) {
		DQStripExtention(m_Filename, MAX_OSPATH);
		DQstrcat(m_Filename, ".jpg", MAX_OSPATH);
		Handle = DQFS.OpenFile(m_Filename, "rb" );
	}
	if(!Handle) {
		D3DTexture = DQRender.NoShaderTexture;

		//No warning message for special cases
		if( (DQstrcmpi( TextureFilename, "noshader.jpg", 90 )==0) || (DQstrcmpi( TextureFilename, "ZeroPassShader.jpg", 90 )==0) )
			return;

		#if _DEBUG
			DQPrint( va("Could not find %s", TextureFilename) );
		#endif

		return;
	}

	DQStripExtention( m_Filename, MAX_QPATH );
	FileSize = DQFS.GetFileLength( Handle );

	if(OverbrightBits==0) {
		//Quick load for Autogen non-brightened textures
		//No overbrightening necessary

		if(DQRender.bCaps_AutogenMipmaps) {
			//Use Hardware Autogen Mipmaps
			if( D3DXCreateTextureFromFileInMemoryEx(DQDevice, DQFS.GetPointerToData(Handle), FileSize, D3DX_DEFAULT, D3DX_DEFAULT, 
				(bNoMipmaps)?1:0, (bNoMipmaps)?0:D3DUSAGE_AUTOGENMIPMAP, D3DFMT_UNKNOWN, 
				D3DPOOL_DEFAULT, D3DX_DEFAULT, D3DX_DEFAULT, 0, 
				NULL, NULL, &D3DTexture)==D3D_OK )
			{
				//Success
				m_bNoMipmaps = bNoMipmaps;
				m_OverbrightBits = OverbrightBits;
				DQFS.CloseFile( Handle );
				return;
			}
		}
	}

	//Load texture file into (SystemMem) TempTexture, then blit across
	if( FAILED( D3DXCreateTextureFromFileInMemoryEx(DQDevice, DQFS.GetPointerToData(Handle), FileSize, D3DX_DEFAULT, D3DX_DEFAULT, 
		(bNoMipmaps)?1:0, 0, D3DFMT_UNKNOWN, 
		D3DPOOL_SYSTEMMEM, D3DX_DEFAULT , D3DX_DEFAULT, 0, 
		NULL, NULL, &TempTexture) ) )
	{
		Error(ERR_FS, va("Could not load texture %s", TextureFilename) );
		DQFS.CloseFile( Handle );
		D3DTexture = DQRender.NoShaderTexture;
		return;
	}

	LoadTextureFromSysmemTexture( TempTexture, bNoMipmaps, OverbrightBits );

	SafeRelease( TempTexture );

	DQFS.CloseFile( Handle );
}

void c_Texture::LoadTextureFromSysmemTexture(LPDIRECT3DTEXTURE9 SysmemTexture, BOOL bNoMipmaps, int OverbrightBits)
{
	int darken;
	isLoaded = TRUE;	//even if this function fails, D3DTexture is valid
	D3DSURFACE_DESC desc;
	D3DLOCKED_RECT LockedRect;
	HRESULT hr;

	if( !SysmemTexture ) {
		Error( ERR_RENDER, "c_Texture::LoadTextureFromSysmemTexture - Invalid parameter" );
		D3DTexture = DQRender.NoShaderTexture;
		return;
	}
	if( D3DTexture && D3DTexture != DQRender.NoShaderTexture ) {
		Error( ERR_RENDER, "c_Texture::LoadTextureFromSysmemTexture - Texture already loaded" );
		SafeRelease( D3DTexture );
		//continue loading
	}

	d3dcheckOK( SysmemTexture->GetLevelDesc( 0, &desc ) );

	//Create blank video memory texture
	if(DQRender.bCaps_AutogenMipmaps) {
		hr = D3DXCreateTexture( DQDevice, desc.Width, desc.Height, (bNoMipmaps)?1:0, (bNoMipmaps)?0:D3DUSAGE_AUTOGENMIPMAP, desc.Format, D3DPOOL_DEFAULT, &D3DTexture);
	}
	else {
		hr = D3DXCreateTexture( DQDevice, desc.Width, desc.Height, (bNoMipmaps)?1:0, 0, desc.Format, D3DPOOL_DEFAULT, &D3DTexture);
	}
	if( FAILED(hr) ) {
		if( hr == D3DERR_OUTOFVIDEOMEMORY ) {
			CriticalError( ERR_RENDER, "Insufficient Video memory for textures - try a lower resolution" );
		}
		else CriticalError( ERR_RENDER, "Unable to create texture in video memory" );
	}

	//Apply overbright (darken the texture)
	if(OverbrightBits!=0) {
		if (OverbrightBits != 1) OverbrightBits  = 2;
		darken = (1<<OverbrightBits);

		d3dcheckOK( SysmemTexture->LockRect( 0, &LockedRect, NULL, 0 ) );

		BYTE PixelFmtSize, *line;
		int u,v;

		switch(desc.Format) {
		case D3DFMT_R8G8B8: PixelFmtSize = 3; break;
		case D3DFMT_A8R8G8B8: 
		case D3DFMT_X8R8G8B8:
		case D3DFMT_A8B8G8R8:
		case D3DFMT_X8B8G8R8:
			PixelFmtSize = 4; break;
		default:
			SafeRelease( D3DTexture );
			Error(ERR_RENDER, "Unrecognised Texture Format" );
			D3DTexture = DQRender.NoShaderTexture;
			return;
		}

		for(u=0;u<desc.Height;++u) {
			line = (BYTE*)LockedRect.pBits+u*LockedRect.Pitch;

			for(v=0;v<desc.Width*PixelFmtSize;v+=PixelFmtSize) {
				//RGB only (not Alpha)
				//Least Significant bit is first, MSB last (alpha)
				line[v] = line[v]/darken;
				line[v+1] = line[v+1]/darken;
				line[v+2] = line[v+2]/darken;
			}
		}

		SysmemTexture->UnlockRect(0);
		d3dcheckOK( SysmemTexture->AddDirtyRect( NULL ) );
	}

	//Blit the texture to video memory
	if(DQRender.bCaps_AutogenMipmaps) {
		if( FAILED( DQDevice->UpdateTexture( SysmemTexture, D3DTexture ) )) {
			Error(ERR_RENDER, "UpdateTexture failed" );
			D3DTexture = DQRender.NoShaderTexture;
			return;
		}
		D3DTexture->GenerateMipSubLevels();
	}
	else {
		//Use Software Mipmap generation
		d3dcheckOK( D3DXFilterTexture( SysmemTexture, NULL, 0, D3DX_DEFAULT ) );
		if( FAILED( DQDevice->UpdateTexture( SysmemTexture, D3DTexture ) )) {
			Error(ERR_RENDER, "UpdateTexture failed" );
			D3DTexture = DQRender.NoShaderTexture;
			return;
		}
	}

	//Success
	m_bNoMipmaps = bNoMipmaps;
	m_OverbrightBits = OverbrightBits;
	RefCount = 1;
}
