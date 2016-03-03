// DXQuake3 Source
// Copyright (c) 2003 Richard Geary (richard.geary@dial.pipex.com)
//
// c_Shader.h: interface for the c_Shader class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_C_SHADER_H__6352C650_B88C_49E5_B125_E9E86AA15901__INCLUDED_)
#define AFX_C_SHADER_H__6352C650_B88C_49E5_B125_E9E86AA15901__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#define MAX_TEXTURE_STAGES 8

//Flags
//#define ShaderFlag_SortBit1			0x00000001
//#define ShaderFlag_SortBit2			0x00000002
//#define ShaderFlag_SortBit3			0x00000004
//#define ShaderFlag_SortBit4			0x00000008
#define ShaderFlag_IsPortal			0x00000001
#define ShaderFlag_NoMipmaps	0x00000002
#define ShaderFlag_UseDeformVertex	0x00000004
//shader is going to be used to texture a BSP face
#define ShaderFlag_BSPShader		0x00000008
//shader specifies a lightmap stage
#define ShaderFlag_UseLightmap		0x00000010
#define ShaderFlag_UseNormals	 0x00000020
#define ShaderFlag_UseDiffuse		0x00000040
#define ShaderFlag_UseCustomBlendfunc		0x00000080
#define ShaderFlag_ConsoleShader		0x00000100
#define ShaderFlag_SkyboxShader			0x00000200
#define ShaderFlag_NoPicmips	0x0000000400
//Surface Params
//SPType 1 to 20
#define ShaderFlag_SP(SPType)		1<<(10+SPType)
//SPType is one of Shader_surfaceParam::e_SPType

//Basic Render information per Texture Stage
struct s_TextureStage {
	s_TextureStage():map(NULL),Next(NULL),CommandChain(NULL),TSSFlags(0),blendFunc(NULL) {};
	~s_TextureStage() { DQDelete(map); DQDelete(blendFunc); DQDelete(CommandChain); DQDelete(Next); }

	//if map is null, it indicates that there is no more stages in this pass
	Shader_BaseFunc			*map;
	Shader_blendFunc	*blendFunc;		//only used for initialisation
	Shader_BaseFunc			*CommandChain;
	DWORD						TSSFlags;
	int						OverbrightBits;
	s_TextureStage			*Next;
};

struct s_RenderPass {
	s_RenderPass():StageChain(NULL),rgbgen(NULL),alphagen(NULL),SrcBlend(D3DBLEND_ONE),DestBlend(D3DBLEND_ZERO),RSFlags(0),Next(NULL),GlobalCommandChain(NULL) {};
	~s_RenderPass() { DQDelete(StageChain); DQDelete(rgbgen); DQDelete(alphagen); DQDelete(GlobalCommandChain); DQDelete(Next); }

	s_TextureStage		*StageChain;
	Shader_rgbgen		*rgbgen;
	Shader_alphagen		*alphagen;
	D3DBLEND			SrcBlend, DestBlend;
	DWORD					RSFlags;
	s_RenderPass		*Next;
	Shader_BaseFunc		*GlobalCommandChain;
};

class c_Shader
{
public:
	c_Shader();
	c_Shader(c_Shader *original, int AdditionalFlags);
	virtual ~c_Shader();

	int  LoadShader(char *buffer);
	void Initialise();
	void RunShader();
	int  GetFlags() { return Flags; }
	void SetFlags(int setFlags) { Flags = setFlags; }
	void AddFog( c_Shader *FogShader );
	void SetPortalShaderPosition( D3DXVECTOR3 &Position );

	void SpriteShader();	//convert shader to a sprite shader

	void SetAlphaPortalDistance( float Distance );

	char Filename[MAX_QPATH];
	
	BOOL isInitialised;
	BOOL bMultipassFog;
	short NumPasses;
	U8  SortValue;
	short NextFoggedShaderHandle;		//holds the ShaderHandle of the next identical shader with a different fog

	//Used to stop identitcal shaders from being run twice if unnecessary
	static c_Shader *LastRunShader;
	static LPDIRECT3DTEXTURE9 CurrentLightmapTexture;

	static void UnloadAllStatics() { LastRunShader = NULL; CurrentStage = NULL; CurrentRenderPass = NULL; CurrentCommand = NULL; CurrentGlobalCommand = NULL; }

	Shader_deformVertexes *DeformVertexes;
	Shader_fogParams *Fog;

private:
	unsigned int Flags;						//Bitfield
	BOOL isCopy;
	BOOL isSpriteShader;

	s_RenderPass RenderPassChain;

	//The following are used for temp storage while constructing,initialising or running shaders
	static s_TextureStage	*CurrentStage;
	static s_RenderPass		*CurrentRenderPass;
	static Shader_BaseFunc	*CurrentCommand;
	static Shader_BaseFunc	*CurrentGlobalCommand;
	static int				TextureStageNum;		//while loading

	BOOL LoadShaderFunc( char *line );
	void AddRenderPass();
	void AddCommand( Shader_BaseFunc *Command );
	void AddGlobalCommand( Shader_BaseFunc *Command );
	void NewStage();

	static void c_Shader::UpdateD3DState(int TextureStage);
};


#endif // !defined(AFX_C_SHADER_H__6352C650_B88C_49E5_B125_E9E86AA15901__INCLUDED_)
