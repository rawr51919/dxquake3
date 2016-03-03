// DXQuake3 Source
// Copyright (c) 2003 Richard Geary (richard.geary@dial.pipex.com)
//
// c_Shader.cpp: implementation of the c_Shader class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "c_Shader.h"

int c_Shader::TextureStageNum;

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

c_Shader::c_Shader()
{
	Flags = 0;
	NumPasses = 0;

	isInitialised = FALSE;
	SortValue = 0;		//not specified = 0. Default is 3, set on Initialise()
	isCopy = FALSE;
	isSpriteShader = FALSE;
	DeformVertexes = NULL;
	Fog = NULL;
	bMultipassFog = FALSE;
	NextFoggedShaderHandle = 0;
}

c_Shader::c_Shader(c_Shader *original, int AdditionalFlags)
{
	isInitialised = FALSE;
	SortValue = original->SortValue;
	isSpriteShader = original->isSpriteShader;

	//Since Initialise() and Run() do not modify the Shader structs, we can use the same memory location for a copy
	memcpy( &RenderPassChain, &original->RenderPassChain, sizeof(s_RenderPass) );
	//Add Lightmap stage if is BSP Shader
/*	if( (AdditionalFlags & ShaderFlag_BSPShader) && !(original->Flags & ShaderFlag_BSPShader) ) {

		//Go to end of shader
		for( CurrentRenderPass = &RenderPassChain; CurrentRenderPass->Next; CurrentRenderPass = CurrentRenderPass->Next ) { ; }
		for( CurrentStage = CurrentRenderPass->StageChain; CurrentStage && CurrentStage->Next; CurrentStage = CurrentStage->Next ) { ; }
		CurrentCommand = NULL;
		CurrentGlobalCommand = NULL;

		if( !(original->Flags & ShaderFlag_UseLightmap) )
		{
			//New shader is going to be used for BSP Shading and original wasn't, so add additional BSP Shading stage
			NewStage();
//			if( CurrentStage == RenderPassChain.StageChain) DeleteMe = CurrentStage;
			LoadShaderFunc("map $lightmap\n");
			LoadShaderFunc("rgbgen lightingDiffuse\n");
			AdditionalFlags |= ShaderFlag_UseLightmap;
		}
	}*/
	Flags = original->Flags | AdditionalFlags;
	DeformVertexes = original->DeformVertexes;
	Fog = original->Fog;
	bMultipassFog = original->bMultipassFog;
	NextFoggedShaderHandle = 0;

	//make a note this is a copy so we don't delete twice
	isCopy = TRUE;

	NumPasses = 0;			//set when shader is initialised
	DQstrcpy(Filename, original->Filename, MAX_QPATH);
}

c_Shader::~c_Shader()
{
	if(isCopy) {
		//Prevent multiple deletion of the same memory
		ZeroMemory( &RenderPassChain, sizeof(s_RenderPass) );
		DeformVertexes = NULL;
		Fog = NULL;
	}

	DQDelete( DeformVertexes );
	DQDelete( Fog );

	isInitialised = FALSE;
}

int c_Shader::LoadShader(char *buffer)
{
	int pos = 0, brackets = 0;

	//Copy Shader Name
	pos = DQWordstrcpy( Filename, buffer, MAX_QPATH );
	DQStripGfxExtention(Filename, MAX_QPATH);	//Keep Shadername as extentionless

	//skip to first {
	pos += DQSkipWhitespace(&buffer[pos], 65535); //arbitrary length

	//Initialise Shader Load variables
	CurrentRenderPass = &RenderPassChain;
	CurrentStage = NULL;
	CurrentCommand = NULL;
	CurrentGlobalCommand = NULL;
	TextureStageNum = 0;

	while(buffer[pos]!='\0') {

		if(buffer[pos] == '{') {
			++brackets;
			if(brackets==2) {
				//New Stage
				NewStage();
			}
			if(brackets>2) {
				CriticalError(ERR_FS, "Invalid Shader Format : Too many {");
				return 1000; //skip over shader
			}
			//Skip to next command
			pos += DQNextLine(&buffer[pos], 65535); //arbitrary length
			pos += DQSkipWhitespace(&buffer[pos], 65535);
			continue;

		}

		if(buffer[pos] == '}') {
			--brackets;
			if(brackets==0) {
				//End of shader definition
				++pos;
				break;
			}

			pos += DQNextLine(&buffer[pos], 65535); //arbitrary length
			pos += DQSkipWhitespace(&buffer[pos], 65535);
			continue;
		}
		//else Found a Shader Command
		if(brackets==0) CriticalError(ERR_FS, "Invalid Shader Format");

		//If required, AddShaderFunc adds as new function onto the command list, and handles
		//new stages and new passes
		//AddShaderFunc returns true if a new command is added
		LoadShaderFunc( &buffer[pos] );

		pos += DQNextLine(&buffer[pos], 65535); //arbitrary length
		pos += DQSkipWhitespace(&buffer[pos], 65535);
	}

	return pos;
}

void c_Shader::Initialise()
{
	Shader_BaseFunc::Flags = Flags;
	Shader_BaseFunc::TextureStage = 0;

	NumPasses = 0;
	for(CurrentRenderPass = &RenderPassChain; CurrentRenderPass; CurrentRenderPass = CurrentRenderPass->Next) {
		//Initialise Global Shader Commands
		for(CurrentCommand = CurrentRenderPass->GlobalCommandChain; CurrentCommand; CurrentCommand = CurrentCommand->Next) {
			CurrentCommand->Initialise();
		}

		for(CurrentStage = CurrentRenderPass->StageChain; CurrentStage ; CurrentStage = CurrentStage->Next, ++Shader_BaseFunc::TextureStage) {

			Shader_BaseFunc::OverbrightBits = CurrentStage->OverbrightBits;
			if( Flags & ShaderFlag_NoPicmips ) CurrentStage->TSSFlags |= RS_TSS_SAMP_POINT;
			
			if(CurrentStage->map) CurrentStage->map->Initialise();
			if( CurrentStage->blendFunc ) {
				if(CurrentStage == CurrentRenderPass->StageChain ) {
					CurrentRenderPass->SrcBlend = CurrentStage->blendFunc->Src;
					CurrentRenderPass->DestBlend = CurrentStage->blendFunc->Dest;
					if( !(CurrentRenderPass->SrcBlend == D3DBLEND_ONE && CurrentRenderPass->DestBlend == D3DBLEND_ZERO) ) {
						CurrentRenderPass->RSFlags |= ( RS_ALPHABLENDENABLE | RS_ZWRITEDISABLE );
					}
					//Keep blendfunc for future initialisations
				}
				else {
					CurrentStage->TSSFlags &= ~( RS_TSS_COLOROP | RS_TSS_ALPHAOP );
					CurrentStage->TSSFlags |= CurrentStage->blendFunc->BlendTSSFlags;
				}
			}

			//None of blendFunc, rgbgen, alphagen or tcGen require initialising
			//Initialise other Shader Commands
			for(CurrentCommand = CurrentStage->CommandChain; CurrentCommand; CurrentCommand = CurrentCommand->Next) {
				CurrentCommand->Initialise();
			}
		}
		++NumPasses;
	}

	Shader_BaseFunc::OverbrightBits = DQConsole.OverbrightBits;
	if( Fog ) Fog->Initialise();

	//Check if NumPasses should be zero
	if(!RenderPassChain.StageChain) NumPasses = 0;
	else {
		if(!RenderPassChain.StageChain->map) NumPasses = 0;
	}
	if(Flags&ShaderFlag_SP(Shader_surfaceParam::NoDraw)) NumPasses=0;	//but draw if fogged

	if( NumPasses==0 && Fog ) {
		bMultipassFog = TRUE;
		SortValue = 9;
		Flags &= ~ShaderFlag_BSPShader;		//fix for q3tourney5
	}
	if( bMultipassFog ) ++NumPasses;

	if(SortValue == 0) SortValue = 3;	//default sort if not specified

	isInitialised = TRUE;
}

void c_Shader::RunShader() {

	int &TextureStage = Shader_BaseFunc::TextureStage;

	//Shader_BaseFunc holds static state variables which the Shader classes modify as they run
	//This saves a lot of needless parameter passing
	Shader_BaseFunc::TextureStage = 0;
	Shader_BaseFunc::Flags = Flags;
	Shader_BaseFunc::TFactor = 0xFFFFFFFF;

	//for debug
//	if(DQstrcmpi(Filename, "models/players/klesk/flisk", MAX_QPATH)==0) Breakpoint;

	if(LastRunShader == this && CurrentLightmapTexture == Shader_BaseFunc::LightmapTexture) {
		//Check for unnecessary shader runs
		if( NumPasses == 1 ) {
			return;
		}
		CurrentRenderPass = CurrentRenderPass->Next;
		if( !CurrentRenderPass ) {
			LastRunShader = NULL;
			CurrentRenderPass = &RenderPassChain;

			if( bMultipassFog ) {
				Fog->RunFogMultipass();
				return;
			}
		}
	}
	else {
		CurrentRenderPass = &RenderPassChain;
	}

	CurrentStage = CurrentRenderPass->StageChain;
	CurrentLightmapTexture = Shader_BaseFunc::LightmapTexture;
	LastRunShader = this;

	if(bMultipassFog) {
		//Force fog draw on first pass for NoDraw (Fix for q3tourney5)
		if( (!CurrentStage) || (Flags&ShaderFlag_SP(Shader_surfaceParam::NoDraw)) ) {
			Fog->RunFogMultipass();
			return;
		}
	}

	Shader_BaseFunc::CurrentRS = CurrentRenderPass->RSFlags;

	if(CurrentRenderPass->rgbgen) CurrentRenderPass->rgbgen->Run();
	if(CurrentRenderPass->alphagen) CurrentRenderPass->alphagen->Run();
	DQRender.SetBlendFuncs( CurrentRenderPass->SrcBlend, CurrentRenderPass->DestBlend );

	//Run Global Pass commands (from first pass node)
	for(CurrentGlobalCommand = RenderPassChain.GlobalCommandChain; CurrentGlobalCommand; CurrentGlobalCommand = CurrentGlobalCommand->Next) {
		CurrentGlobalCommand->Run();
	}

	//Apply fog on last pass
	if( Fog && !CurrentRenderPass->Next && !bMultipassFog)
	{
		Fog->Run();
	}

	//Run Stage commands
	for( ; CurrentStage ; CurrentStage = CurrentStage->Next, ++TextureStage ) 
	{
		Shader_BaseFunc::CurrentTSS = CurrentStage->TSSFlags;

		if(CurrentStage->map) CurrentStage->map->Run();

		//Run other shader commands
		for(CurrentCommand = CurrentStage->CommandChain; CurrentCommand; CurrentCommand = CurrentCommand->Next) {
			CurrentCommand->Run();
		}

		//Update Texture Stage
		UpdateD3DState(TextureStage);
	}

	//Disable any further stages
	DQRender.DisableStage( TextureStage );
}

//Update D3D State
void c_Shader::UpdateD3DState(int TextureStage)
{
	DQRender.SetTFactor( Shader_BaseFunc::TFactor );

	DQRender.UpdateRS(Shader_BaseFunc::CurrentRS);
	if(Shader_BaseFunc::CurrentTSS & RS_TSS_TTF) {
		DQRender.UpdateTSS(TextureStage, Shader_BaseFunc::CurrentTSS, &Shader_BaseFunc::TexCoordMultiply[Shader_BaseFunc::TextureStage]);
	}
	else {
		DQRender.UpdateTSS(TextureStage, Shader_BaseFunc::CurrentTSS, NULL);
	}
}

void c_Shader::SpriteShader()
{
	if(isSpriteShader) return;
	if(!(Flags & ShaderFlag_UseCustomBlendfunc)) {

		for( CurrentRenderPass = &RenderPassChain; CurrentRenderPass; CurrentRenderPass = CurrentRenderPass->Next ) {
			//Render the sprite without z-write and with alpha blending
			CurrentRenderPass->RSFlags |= ( RS_ALPHABLENDENABLE | RS_ZWRITEDISABLE | RS_COLOR_DIFFUSE | RS_ALPHA_DIFFUSE );
			CurrentRenderPass->SrcBlend = D3DBLEND_SRCALPHA;
			CurrentRenderPass->DestBlend = D3DBLEND_INVSRCALPHA;
		}

	}
	else {

		for( CurrentRenderPass = &RenderPassChain; CurrentRenderPass; CurrentRenderPass = CurrentRenderPass->Next ) {
			CurrentRenderPass->RSFlags |= ( RS_ZWRITEDISABLE | RS_COLOR_DIFFUSE | RS_ALPHA_DIFFUSE );
		}

	}
	Flags |= ShaderFlag_UseDiffuse;
	isSpriteShader = TRUE;
}

void c_Shader::AddFog( c_Shader *FogShader )
{
	if( !FogShader->Fog ) return;

	Fog = FogShader->Fog;

	//Add fog pass if multipass shader
	if(RenderPassChain.Next || (RenderPassChain.RSFlags&RS_ALPHABLENDENABLE)) {
		if( !DQRender.bCaps_SrcBlendfactor ) return;		//not supported

		bMultipassFog = TRUE;

		//Since the fog blender is applied *before* the alpha blend (why Microsoft, why?!?) we have to hack it so it looks right
		//Fog = FogFactor * Src + (1-FogFactor) * FogColour
		//Set FogColour=White, Src=Black
		//This produces a map of 1-FogFactor
		//Then blend : Src * FogColour + Dest * (1-Src)

	}
	else {
		bMultipassFog = FALSE;
	}
}

void c_Shader::SetAlphaPortalDistance( float Distance )
{
	s_RenderPass *Pass;

	//Find Alphagen Portal command
	for( Pass = &RenderPassChain; Pass; Pass=Pass->Next ) 
	{
		if(Pass->alphagen) {
			if(Pass->alphagen->type==Shader_alphagen::portal) {
				((Shader_alphagen::s_Portal*)Pass->alphagen->data)->Distance = Distance;
				break;
			}
		}
	}
}

void c_Shader::SetPortalShaderPosition( D3DXVECTOR3 &Position ) {
	if(!(Flags&ShaderFlag_IsPortal)) return;
	if( RenderPassChain.StageChain && RenderPassChain.StageChain->map && ((Shader_Map*)RenderPassChain.StageChain->map)->type == Shader_Map::portaltexture ) {
		((c_Render::s_RenderToTexture*)((Shader_Map*)RenderPassChain.StageChain->map)->Texture)->Position = Position;
	}
}
