// DXQuake3 Source
// Copyright (c) 2003 Richard Geary (richard.geary@dial.pipex.com)
//
// c_Render.h: interface for the c_Render class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_C_RENDER_H__41E300F7_F745_471F_81BD_C30ECF698839__INCLUDED_)
#define AFX_C_RENDER_H__41E300F7_F745_471F_81BD_C30ECF698839__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class c_Model;
class c_Skin;
#define MAX_SPRITES			5000
#define MAX_CUSTOMPOLYVERTS 2000
#define MAX_BILLBOARDS		300
#define MAX_SKINS			1000
#define MAX_LIGHTS			8
#define MAX_TEXTURES		5000
#define MAX_RENDERTOTEXTURE 2

//PORTALS
//*******
//Portals do work (using RenderToTexture) but are very slow and cause more problems than are worth
//To enable them, uncomment the next line
//#define _PORTALS

//Render State
//There is one global render state (RS_) and a global render state for each Texture Stage (RS_TSS_)
//Shader functions modify the desired Render States to what they need. This method allows easy cleanup.

//defined earlier
//struct s_RenderState {
//	int RS[8];
//};

//Default RS values are all zero

//RS_TSS_

//TSS_TexCoordIndex = 1 (else TSS_TCI = 0)
#define RS_TSS_TCI_1	0x00000001
//TSS_CameraSpaceReflectionVector = true
#define RS_TSS_TCI_CSRV 0x00000002
//Clamp Tex coords (else wrap)				(Note : Clamp and mirror are actually set with SetSamplerState)
#define RS_TSS_CLAMP	0x00000004
//Mirror Tex coords (overrides RS_SS_CLAMP)
#define RS_TSS_MIRROR	0x00000008
//TSS_TextureTransformFlags = Enabled
#define RS_TSS_TTF		0x00000010
//TSS_TextureTransformFlags = Count3 (else Count2)
//This is ignored if RS_TSS_TTF is zero
#define RS_TSS_TTFC3	0x00000020
//use complement (1-x) of TSS_COLORARG1
#define RS_TSS_COMPLEMENT_1				0x00000040
//use complement (1-x) of TSS_COLORARG2
#define RS_TSS_COMPLEMENT_2				0x00000080

//use complement (1-x) of TSS_ALPHAARG1
#define RS_TSS_ALPHA_COMPLEMENT_1	0x40000000
//use complement (1-x) of TSS_ALPHAARG2
#define RS_TSS_ALPHA_COMPLEMENT_2	0x80000000

//Color Op
#define RS_TSS_COLOROP		0x2003FF00
//Alpha Op
#define RS_TSS_ALPHAOP		0x0FF00000

//If all RS_TSS_COLOR bits are zero, use D3DTOP_MODULATE
//Use D3DTOP_ADD for colour
#define RS_TSS_COLOR_ADD		0x00000100
//Use D3DTOP_SELECTARG1 for colour op
#define RS_TSS_COLOR_SEL1		0x00000200
//Use D3DTOP_SELECTARG2 for colour op
#define RS_TSS_COLOR_SEL2		0x00000400
//Use D3DTOP_DISABLE for colour op
#define RS_TSS_COLOR_DISABLE	0x00000800
//Use D3DTOP_ADDSMOOTH for colour op
#define RS_TSS_COLOR_ADDSMOOTH	0x00001000
//Use D3DTOP_MODULATE2X for colour op
#define RS_TSS_COLOR_MODULATE2X	0x00002000
//Use D3DTOP_MODULATE4X for colour op
#define RS_TSS_COLOR_MODULATE4X	0x00004000
//Use D3DTOP_BLENDTEXTUREALPHA for colour
#define RS_TSS_COLOR_BLENDTEXTUREALPHA	0x00008000
//Use D3DTOP_MODULATEALPHA_ADDCOLOR
#define RS_TSS_MODALPHA_ADDCOLOR		0x00010000
//Use D3DTOP_MODULATEINVALPHA_ADDCOLOR
#define RS_TSS_MODINVALPHA_ADDCOLOR		0x00020000

//Alpha Op (Default is D3DTOP_MODULATE)
//Use D3DTOP_SELECTARG1 for alpha
#define RS_TSS_ALPHA_SEL1				0x00100000
//Use D3DTOP_SELECTARG1 for alpha
#define RS_TSS_ALPHA_SEL2				0x00200000
//Use D3DTOP_ADDSMOOTH for alpha
#define RS_TSS_ALPHA_ADDSMOOTH			0x00400000
//Use D3DTOP_DISABLE for alpha
#define RS_TSS_ALPHA_DISABLE			0x00800000
#define RS_TSS_ALPHA_ADD				0x01000000
#define RS_TSS_ALPHA_MODULATE2X			0x02000000
#define RS_TSS_ALPHA_MODULATE4X			0x04000000
#define RS_TSS_ALPHA_BLENDTEXTUREALPHA	0x08000000
//NB. 0x40000000 and 0x80000000 are used above
#define RS_TSS_SAMP_POINT				0x10000000
#define RS_TSS_BLENDCURRENTALPHA		0x20000000


//RS_

//D3DRS_NORMALIZENORMALS = true
#define RS_NORMALIZENORMALS			0x00000001
//D3DRS_LIGHTING = true
#define RS_LIGHTING					0x00000002
//TSS_COLORARG2 is Diffuse (only stage 0 - all other stages are set to D3DTA_CURRENT)
//default is TFACTOR
#define RS_COLOR_DIFFUSE			0x00000004
//ditto for ALPHAARG2
#define RS_ALPHA_DIFFUSE			0x00000008
//D3DRS_ALPHABLENDENABLE
#define RS_ALPHABLENDENABLE			0x00000010
//D3DRS_CULLMODE = D3DCULL_NONE	 (else cull back)
#define RS_CULLNONE					0x00000020
//D3DRS_CULLMODE = D3DCULL_CW (else cull back)
#define RS_CULLFRONT				0x00000040
//D3DRS_ZWRITEENABLE = Disabled
#define RS_ZWRITEDISABLE			0x00000080
//D3DRS_ZFUNC = Equal (else less than or equal)
#define RS_ZFUNC_EQUAL				0x00000100
//D3DRS_ALPHAREF = 0x00000080 (else 0)
#define RS_ALPHAREF80				0x00000200
//D3DRS_ALPHATEST = TRUE
#define RS_ALPHATEST				0x00000400

//If all Alpha Funcs = 0, Alpha Func = Always
#define RS_ALPHAFUNC				0x00003800
//Alpha Func = Greater
#define RS_ALPHAFUNC_GREATER		0x00000800
//Alpha Func = Greater or Equal (else 
#define RS_ALPHAFUNC_GREATEREQUAL	0x00001000
//Alpha Func = Less
#define RS_ALPHAFUNC_LESS			0x00002000

#define RS_FOGENABLE				0x00004000



/*
//based on refEntity_t
struct s_DQEntity {
	refEntityType_t	reType;
	int			renderfx;
	int			ModelHandle;

	D3DXVECTOR3	lightingOrigin;		// so multi-part models can be lit identically (RF_LIGHTING_ORIGIN)
	float		shadowPlane;		// projection shadows go here, stencils go slightly lower

	MATRIX		ModellingMatrix;
	c_Model		*Model;
//	vec3_t		axis[3];			// rotation vectors
//	qboolean	nonNormalizedAxes;	// axis are not normalized, i.e. they have scale
//	float		origin[3];			// also used as MODEL_BEAM's "from"

	int			ThisFrame;
	int			OldFrame;
	D3DXVECTOR3	OldOrigin;			// also used as MODEL_BEAM's "to"
	float		lerp;				// 1.0 = current, 0.0 = old

	// texturing
	int			skinNum;			// inline skin index
	int			customSkin;			// NULL for default skin
	int			customShader;		// use one image for the entire thing

	// misc
	DWORD		shaderRGBA;			// colors used by rgbgen entity shaders
	D3DXVECTOR2	shaderTexCoord;		// texture coordinates used by tcMod entity modifiers
	float		shaderTime;			// subtracted from refdef time to control effect start times

	// extra sprite information
	float		radius;
	float		rotation;
};*/

struct s_Billboard {
	int ShaderHandle;
	D3DXVECTOR3 Axis1, Axis2, Normal;
	D3DXVECTOR3 Position[4], AveragePosition;
	D3DXVECTOR2 TexCoord[4];
	s_RenderFeatureInfo RenderFeatures;
	BOOL OnlyAlignAxis1;
};


//Notation :
//Draw_() will register an object to be drawn, called by anything
//Render_() will do the rendering, only called by DrawScene()

class c_Render : public c_Singleton<c_Render>
{
	friend class c_RenderStack;
public:
	c_Render();
	virtual ~c_Render();
	void UnloadD3D();
	void LoadWindow();
	void Initialise();
	void InitialiseD3D();
	void ResetD3DDevice(BOOL ForceWindowedMode);

	void SetHInstance(HINSTANCE hInstance) { if(!hInst) hInst = hInstance; }
	HINSTANCE GetHInstance() { return hInst; }
	HWND GetHWnd() { return hWnd; }
	BOOL IsFullscreen() { return isFullscreen; }
	void ToggleFullscreen();
	D3DPRESENT_PARAMETERS *GetPresentParameters() { return &D3DPresentParameters; }
	BOOL c_Render::CheckOverbright();

	void CreateSquare( D3DXVECTOR3 Direction, D3DXVECTOR3 *Verts, D3DXVECTOR2 *BoxTexCoords, WORD *Indices, int Tesselation );
	void GetSquareArrayDimensions( int *pNumIndices, int *pNumVerts, int Tesselation );
	void CreateDome( float radius, int CloudHeight, D3DXVECTOR3 *Verts, D3DXVECTOR2 *TexCoords, WORD *Indices, int Tesselation, float TexCoordScale );
	void GetDomeArrayDimensions( int *pNumIndices, int *pNumVerts, int Tesselation );

	int GetCurrentRS() { return CurrentRS.RS; }
	int GetCurrentTSS(int TextureStage) { return CurrentRS.TSS[TextureStage]; }
	void c_Render::DisableStage( int TextureStage );

	int RegisterModel(const char *Filename);
	int RegisterShader(const char *Shadername, int AdditionalFlags = NULL);
	int RegisterSkin(const char *Filename);
	void LoadShaders();
	void UpdateRS(int RequiredRS);
	void UpdateTSS(int TextureStage, int RequiredTSS, MATRIX *pTCM);	 //Set pTCM to NULL for identity matrix
	void SetTFactor( DWORD rgba );
	void SetBlendFuncs( D3DBLEND BlendSrc, D3DBLEND BlendDest );

	char * c_Render::GetDeviceInfo();
	int ShaderGetNumPasses(int ShaderHandle);
	LPDIRECT3DDEVICE9 GetD3DDevice() { return device; }
	void BeginRender();
	void DrawScene();
	void EndRender();
	void DrawSprite(float x, float y, float w, float h, float s1, float t1, float s2, float t2, int hShader );
	void RenderSpriteCache();
	void DrawString( float x, float y, int CharWidth, int CharHeight, char *str, int MaxLength );
	void DrawRefEntity(refEntity_t *refEntity);
	void SetSpriteColour(float *rgba);
	void AddLight( D3DXVECTOR3 origin, float radius, D3DCOLORVALUE colour );
	int  AddEffectToShader( int ShaderHandle, int EffectShaderHandle );
	inline void SetPortalShaderPosition( int ShaderHandle, D3DXVECTOR3 &Position ) {
		if( ShaderHandle<1 || ShaderHandle>NumShaders ) return;
		ShaderArray[ShaderHandle-1]->SetPortalShaderPosition( Position );
	}

	c_Model *GetModel(int ModelHandle);
	c_Skin *GetSkin(int SkinHandle);
	int ShaderGetFlags(int ShaderHandle);
	void GetQ3ModelBounds( int ModelHandle, float mins[3], float maxs[3] );
	int GetSortValue(int ShaderHandle);
	void SetSortValue(int ShaderHandle, int SortValue) { ShaderArray[ShaderHandle-1]->SortValue = SortValue; }
	void AddPoly( polyVert_t *polyVerts, int NumVerts, int ShaderHandle );
	void PrepareBillboards();
	void AddBillboard( D3DXVECTOR3 Position[4], D3DXVECTOR2 TexCoord[4], int ShaderHandle );
	void AddBillboard( s_VertexStreamP *Position, s_VertexStreamT *TexCoord, int ShaderHandle, BOOL bOnlyAlignAxis1 );
	c_Texture *c_Render::OpenTextureFile( char *TextureFilename, BOOL NoMipmaps, int OverBrightBits );

	BOOL		isActive;
	c_Camera	Camera;
	c_Camera	*pCamera;
	c_RenderStack	RenderStack;
	glconfig_t	GLconfig;		//For pretending we're OpenGL
	BOOL		bClearScreen, ToggleVidRestart, bCaps_DynamicTextures, bCaps_AutogenMipmaps, bCaps_SrcBlendfactor;
	LPDIRECT3DTEXTURE9 NoShaderTexture;
	c_Texture	WhiteImageTexture;
	int			charsetShader, whiteimageShader, NoShaderShader, ZeroPassShader;
	BOOL		RenderNewFrame;
	MATRIX		matIdentity;
	int			FrameNum;
	int			NumD3DCalls;
	int			MaxSimultaneousTextures;

private:
	void VidRestart();

	vmCvar_t r_RenderScene;
/*
	struct s_Sprite {
		int SpriteVBPos;
		int ShaderHandle;
	};
	struct s_Poly {
		int PolyVBPos;
		int NumVerts;
		int ShaderHandle;
	};*/
/*	struct s_RenderObject {
		enum { eSprite, eEntity, ePoly, eBillboard } Type;
		int Index;
	};*/

	HINSTANCE	hInst;
	HWND		hWnd;
	BOOL		isFullscreen, isDrawing;
	IDirect3D9	*Direct3D9;
	D3DPRESENT_PARAMETERS D3DPresentParameters;
	LPDIRECT3DDEVICE9 device;
	c_Model		*ModelArray[MAX_MODELS];
	int			NumModels;
	c_Shader	**ShaderArray;
	int			NumShaders;
	c_Skin		*SkinArray[MAX_SKINS];
	int			NumSkins;
	D3DLIGHT9	LightArray[MAX_LIGHTS];
	c_Texture	*TextureArray[MAX_TEXTURES];
	int			NumTextures;

	int SpriteVBhandle;
	s_RenderFeatureInfo SpriteRenderInfo;

	struct s_Sprites {
		DWORD Colour, LastColour;
		int NextOffset;
		int CurrentShader;
		int StartOffset;

		inline void Clear() {
			LastColour = Colour = 0xFFFFFFFF;
			NextOffset = CurrentShader = StartOffset = 0;
		};
	} CurrentSpriteInfo;

/*	LPDIRECT3DVERTEXBUFFER9 SpriteVB;
	BOOL		SpriteVBLocked;
	int			NextSpriteVBPos;
	s_Sprite	SpriteArray[MAX_SPRITES];
	int			NumDrawSprites;

	LPDIRECT3DVERTEXBUFFER9 PolyVB;
	BOOL		PolyVBLocked;
	int			NextPolyVBPos;
	s_Poly		PolyArray[MAX_CUSTOMPOLYS];
	int			NumDrawPolys;
	

	s_DQEntity  EntityArray[MAX_ENTITIES];
	int			NumDrawEntities;

	s_RenderObject	RenderObjectArray[16][MAX_SPRITES+MAX_ENTITIES];
	int			NumRenderObjects[16];				//16 different sort values
	int			OverrideShaderHandle;
*/
	int			PolyVBHandle;
	int			NextPolyVBPos;

	s_Billboard	BillboardArray[MAX_BILLBOARDS];
	int			NumDrawBillboards;
	int			BillboardVBHandle;

	D3DMATERIAL9 DefaultMaterial;

	void LoadShaderFile(FILEHANDLE Handle);
//	void RenderSprite(s_Sprite *Sprite);
//	void RenderEntity(s_DQEntity *Entity);
//	void RenderPoly(s_Poly *Poly);
//	void RenderBillboard(int BillboardIndex);

	struct s_RenderState {
		DWORD RS;
		DWORD TSS[8];
		U8 bf_TTF_IsIdentity;
		DWORD TFactorRGBA;
		D3DBLEND BlendSrc, BlendDest;
	};
	s_RenderState CurrentRS;

public:
	void c_Render::AddBillboard( s_Billboard *BB );
	int			NumLights;

	struct s_RenderToTexture {
		c_Camera Camera;
		LPDIRECT3DTEXTURE9 RenderTarget;
		LPDIRECT3DSURFACE9 RenderSurface;
		LPD3DXRENDERTOSURFACE D3DXRenderToSurface;
		D3DVIEWPORT9 Viewport;
		BOOL bEnable;
		BOOL bCaps_RenderToSurface;
		D3DXVECTOR3 Position;			//position of the texture in world space (used to match up with RT_PORTALSURFACE entity)
	} RenderToTexture[MAX_RENDERTOTEXTURE];
	int NumRenderToTextures;

};

#define DQRender c_Render::GetSingleton()
#define DQDevice DQRender.GetD3DDevice()
#define DQCamera (*DQRender.pCamera)
#define DQRenderStack DQRender.RenderStack

#endif // !defined(AFX_C_RENDER_H__41E300F7_F745_471F_81BD_C30ECF698839__INCLUDED_)
