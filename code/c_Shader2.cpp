// DXQuake3 Source
// Copyright (c) 2003 Richard Geary (richard.geary@dial.pipex.com)
//
// c_ShaderCommand.cpp: implementation of the c_ShaderCommand class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"

s_TextureStage		* c_Shader::CurrentStage;
s_RenderPass		* c_Shader::CurrentRenderPass;
Shader_BaseFunc		* c_Shader::CurrentCommand;
Shader_BaseFunc		* c_Shader::CurrentGlobalCommand;
c_Shader			* c_Shader::LastRunShader;
LPDIRECT3DTEXTURE9	  c_Shader::CurrentLightmapTexture;

#define ifcompare(x) if(DQWordstrcmpi(line, x, MAX_STRING_CHARS)==0)

//Run by c_Shader::LoadShader
//This adds a shader func to the command list if required

//Returns true if a new Shader added to the command list
BOOL c_Shader::LoadShaderFunc( char *line ) 
{
	char *params;
	Shader_BaseFunc *TempCommand;

	//Set Params to point to the first parameter
	params = line+DQSkipWord(line, MAX_STRING_CHARS);
	params += DQSkipWhitespace(params, MAX_STRING_CHARS);

	//Stage Commands
	if(CurrentStage) {

		ifcompare("map") { 
			DQDelete( CurrentStage->map );
			CurrentStage->map = (Shader_BaseFunc*)DQNewVoid( Shader_Map(params) );
			
			if( ((Shader_Map*)CurrentStage->map)->type == Shader_Map::lightmap ) {
				Flags |= ShaderFlag_UseLightmap;
				CurrentStage->TSSFlags |= RS_TSS_TCI_1;
				CurrentStage->OverbrightBits = 0;
			}
			return TRUE;
		}
		ifcompare("clampmap") { 
			DQDelete( CurrentStage->map );
			CurrentStage->map = (Shader_BaseFunc*)DQNewVoid( Shader_ClampMap(params) );
			return TRUE;
		}
		ifcompare("animmap") { 
			DQDelete( CurrentStage->map );
			CurrentStage->map = (Shader_BaseFunc*)DQNewVoid( Shader_AnimMap(params) );
			return TRUE;
		}

		ifcompare("rgbgen") { 
			//Same as alphagen

			//Add a rgbgen command to the Shader
			TempCommand = (Shader_BaseFunc*)DQNewVoid( Shader_rgbgen(params) );

			switch(((Shader_rgbgen*)TempCommand)->type) {

			case Shader_rgbgen::identity:
				CurrentStage->OverbrightBits = 0;
				DQDelete( TempCommand );
				return FALSE;

			case Shader_rgbgen::identityLighting:
				CurrentStage->OverbrightBits = DQConsole.OverbrightBits;
				DQDelete( TempCommand );
				return FALSE;

			case Shader_rgbgen::lightingDiffuse:
				Flags |= ShaderFlag_UseNormals;
				CurrentRenderPass->RSFlags |= ( RS_LIGHTING | RS_COLOR_DIFFUSE );
				CurrentRenderPass->StageChain->TSSFlags &= ~RS_TSS_COLOROP;
#ifndef _UseLightvols
				CurrentRenderPass->StageChain->TSSFlags |= RS_TSS_COLOR_ADDSMOOTH;
#endif
				DQDelete( TempCommand );
				return FALSE;

			case Shader_rgbgen::exactVertex:
			case Shader_rgbgen::vertex:
				CurrentRenderPass->RSFlags &= ~RS_LIGHTING;
				CurrentRenderPass->RSFlags |= RS_COLOR_DIFFUSE;
				Flags |= ShaderFlag_UseDiffuse;
				DQDelete( TempCommand );
				return FALSE;

			case Shader_rgbgen::oneMinusVertex:
				CurrentRenderPass->RSFlags &= ~RS_LIGHTING;
				CurrentRenderPass->RSFlags |= RS_COLOR_DIFFUSE;
				CurrentStage->TSSFlags |= RS_TSS_COMPLEMENT_2;
				Flags |= ShaderFlag_UseDiffuse;
				DQDelete( TempCommand );
				return FALSE;

			case Shader_rgbgen::entity:
			case Shader_rgbgen::oneMinusEntity:
				//ShaderRGBA set by RenderStack
				CurrentRenderPass->RSFlags &= ~( RS_LIGHTING | RS_COLOR_DIFFUSE );
				CurrentRenderPass->StageChain->TSSFlags &= ~RS_TSS_COLOROP;
				DQDelete( TempCommand );
				return FALSE;
			};

			//If this is a not an identity or identityLighting rgbgen
			//this stage has to be the first stage
			if( CurrentStage != CurrentRenderPass->StageChain ) {
				AddRenderPass();
			}

			DQDelete( CurrentRenderPass->rgbgen );
			CurrentRenderPass->rgbgen = (Shader_rgbgen*)TempCommand;

			return TRUE;
		}
		ifcompare("alphagen") {
			//Same as rgbgen
			TempCommand = (Shader_BaseFunc*)DQNewVoid( Shader_alphagen(params) );

			switch(((Shader_alphagen*)TempCommand)->type) {

			case Shader_alphagen::identity:
				CurrentStage->OverbrightBits = 0;
				DQDelete( TempCommand );
				return FALSE;

			case Shader_alphagen::identityLighting:
				CurrentStage->OverbrightBits = DQConsole.OverbrightBits;
				DQDelete( TempCommand );
				return FALSE;

			case Shader_alphagen::lightingDiffuse:
				Flags |= ShaderFlag_UseNormals;
				CurrentRenderPass->RSFlags |= ( RS_LIGHTING | RS_ALPHA_DIFFUSE );
				CurrentRenderPass->StageChain->TSSFlags &= ~RS_TSS_ALPHAOP;
				CurrentRenderPass->StageChain->TSSFlags |= RS_TSS_ALPHA_ADD;
				DQDelete( TempCommand );
				return FALSE;

			case Shader_alphagen::vertex:
				CurrentRenderPass->RSFlags &= ~RS_LIGHTING;
				CurrentRenderPass->RSFlags |= RS_ALPHA_DIFFUSE;
				Flags |= ShaderFlag_UseDiffuse;
				DQDelete( TempCommand );
				return FALSE;

			case Shader_alphagen::oneMinusVertex:
				CurrentRenderPass->RSFlags &= ~RS_LIGHTING;
				CurrentRenderPass->RSFlags |= RS_ALPHA_DIFFUSE;
				CurrentStage->TSSFlags |= RS_TSS_ALPHA_COMPLEMENT_2;
				Flags |= ShaderFlag_UseDiffuse;
				DQDelete( TempCommand );
				return FALSE;

			case Shader_alphagen::entity:
			case Shader_alphagen::oneMinusEntity:
				//ShaderRGBA set by RenderStack
				CurrentRenderPass->RSFlags &= ~( RS_LIGHTING | RS_ALPHA_DIFFUSE );
				break;

			case Shader_alphagen::portal:
				SortValue = 1;
				break;
			};

			//If this is a not an identity or identityLighting rgbgen
			//this stage has to be a multipass if this occurs twice
			if( CurrentRenderPass->alphagen ) {
				AddRenderPass();
			}

			DQDelete( CurrentRenderPass->alphagen );
			CurrentRenderPass->alphagen = (Shader_alphagen*)TempCommand;
			return TRUE;
		}

		ifcompare("tcMod") {
			AddCommand( (Shader_BaseFunc*)DQNewVoid( Shader_tcMod(params) ) );
			return TRUE;
		}
		ifcompare("tcGen") { 
			TempCommand = (Shader_BaseFunc*)DQNewVoid( Shader_tcGen(params) );

			switch( ((Shader_tcGen*)TempCommand)->type ) {

			case Shader_tcGen::base:
				CurrentStage->TSSFlags &= ~RS_TSS_TCI_1;
				DQDelete( TempCommand );
				return FALSE;

			case Shader_tcGen::lightmap:
				CurrentStage->TSSFlags |= RS_TSS_TCI_1;
				DQDelete( TempCommand );
				return FALSE;

			case Shader_tcGen::environment:
				Flags |= ShaderFlag_UseNormals;
				//fall through

			default:
				AddCommand( TempCommand );
				return TRUE;
			};
		}
		ifcompare("blendfunc") { 
			DQDelete( CurrentStage->blendFunc );
			CurrentStage->blendFunc = (Shader_blendFunc*)DQNewVoid( Shader_blendFunc(params) );

			if( (CurrentStage != CurrentRenderPass->StageChain) && CurrentStage->blendFunc->bMultipass ) {
				//This stage has to be a multipass stage
				AddRenderPass();
			}

			//if filter stage, default to rgbgen identity
			if( CurrentStage->blendFunc->IsFilterStage() ) {
				CurrentStage->OverbrightBits = 0;
			}
			else {
				//default to rgbgen identityLighting
				CurrentStage->OverbrightBits = DQConsole.OverbrightBits;
			}

			//if 1st stage of 1st pass is transparent, set SortValue to 9
			if( CurrentStage == RenderPassChain.StageChain ) {
				if( (CurrentStage->blendFunc->Dest != D3DBLEND_ZERO) || ((CurrentStage->blendFunc->Src >= D3DBLEND_DESTALPHA) && (CurrentStage->blendFunc->Src <= D3DBLEND_INVDESTCOLOR)) ) {
					if(SortValue==0) SortValue = 9; //only set SortValue if not specified
				}
				//no blendfunc on 1st stage - set RenderPass's BlendSrc & BlendDest
				//This is done at Initialise stage due to any possible AddRenderPass calls
			}

			Flags |= ShaderFlag_UseCustomBlendfunc;

			return TRUE;
		}
	}

	//Global Commands
	ifcompare("cull") { 
		AddGlobalCommand( (Shader_BaseFunc*)DQNewVoid( Shader_cull(params) ) );
		return TRUE;
	}
	ifcompare("alphaFunc") { 
		AddCommand( (Shader_BaseFunc*)DQNewVoid( Shader_alphaFunc(params) ) );
		if(SortValue==0) SortValue = 9; //only set SortValue if not specified
		return TRUE;
	}
	ifcompare("depthFunc") {
		//if this is not the 1st texture stage, use a multipass
		TempCommand = (Shader_BaseFunc*)DQNewVoid( Shader_depthFunc(params) );

		if( CurrentStage != CurrentRenderPass->StageChain ) {
			//This stage has to be a multipass stage
			AddRenderPass();
		}

		//Add the new depth command
		AddCommand( TempCommand );
		return TRUE;
	}
	ifcompare("depthWrite") { 
		AddCommand( (Shader_BaseFunc*)DQNewVoid( Shader_depthWrite(params) ) ); 
		return TRUE;
	}
	ifcompare("nomipmaps") {
		Flags |= ShaderFlag_NoMipmaps; 
		return FALSE;
	}
	ifcompare("nopicmip") { 
		Flags |= ShaderFlag_NoPicmips;
		return FALSE; 
	}
	ifcompare("skyparms") { 
		AddGlobalCommand( (Shader_BaseFunc*)DQNewVoid( Shader_Sky(params, FALSE) ) ); 
		Flags |= ShaderFlag_SP(Shader_surfaceParam::Sky);
		if(SortValue==0) SortValue = 2;	//sky sort value
		return TRUE;
	}
	ifcompare("sky") { 	//same as skyparms, but only 1st param given
		AddGlobalCommand( (Shader_BaseFunc*)DQNewVoid( Shader_Sky(params, TRUE) ) ); 
		Flags |= ShaderFlag_SP(Shader_surfaceParam::Sky);
		if(SortValue==0) SortValue = 2;	//sky sort value
		return TRUE;
	}
	ifcompare("surfaceparm") { 
		TempCommand = (Shader_BaseFunc*)DQNewVoid( Shader_surfaceParam(params) ); 
		if( ((Shader_surfaceParam*)TempCommand)->type > 0 ) {
			Flags |= ShaderFlag_SP( ((Shader_surfaceParam*)TempCommand)->type );
		}
		DQDelete( TempCommand );
		return FALSE;
	}

	ifcompare("sort") { 
		TempCommand = (Shader_BaseFunc*)DQNewVoid( Shader_Sort(params) ); 
		SortValue = ((Shader_Sort*)TempCommand)->value;
		DQDelete( TempCommand );
		return FALSE;
	}
	//Non texture-only shaders
	ifcompare("deformvertexes") { 
		//Only handles one deformVertexes
		DQDelete( DeformVertexes );
		DeformVertexes = (Shader_deformVertexes*) DQNewVoid( Shader_deformVertexes(params) );
		Flags |= ShaderFlag_UseDeformVertex;

		return TRUE;
	}
	ifcompare("fogParms") { 
		DQDelete( Fog );
		Fog = (Shader_fogParams*) DQNewVoid( Shader_fogParams(params) );
		
		return TRUE;
	}
	ifcompare("portal") {
#ifdef _PORTALS
		Flags |= ShaderFlag_IsPortal;

		//Add portal stage
		NewStage();
		CurrentStage->map = (Shader_BaseFunc*)DQNewVoid( Shader_Map("$portaltexture") );
#endif
		return TRUE;
	}

	//Q3map only stuff
	//Do Nothing
	ifcompare("qer_editorimage") { return FALSE; }
	ifcompare("qer_nocarve") { return FALSE; }
	ifcompare("qer_trans") { return FALSE; }
	ifcompare("q3map_lightsubdivide	") { return FALSE; }
	ifcompare("q3map_backshader") { return FALSE; }
	ifcompare("tessSize") { return FALSE; }
	ifcompare("q3map_globaltexture") { return FALSE; }
	ifcompare("q3map_sun") { return FALSE; }
	ifcompare("q3map_surfaceLight") { return FALSE; }
	ifcompare("q3map_lightimage") { return FALSE; }
	ifcompare("q3map_backsplash") { return FALSE; }
	ifcompare("q3map_lightsubdivide") { return FALSE; }
	ifcompare("q3map_flare") { return FALSE; }
	ifcompare("q3map_globaltexture") { return FALSE; }
	ifcompare("qer_lightimage") { return FALSE; }

	//Obsolete
	ifcompare("light") { return FALSE; }
	ifcompare("light1") { return FALSE; }
	ifcompare("lightning") { return FALSE; }
	ifcompare("fogonly") { return FALSE; }
	ifcompare("foggen") { return FALSE; }
	ifcompare("alphamap") { return FALSE; }
	ifcompare("backsided") { return FALSE; }
 
	//TODO
	ifcompare("polygonOffset") { return FALSE; }			//(Global)
	ifcompare("detail") { return FALSE; }
	ifcompare("entityMergable") { return FALSE; }
	ifcompare("cloudparms") { return FALSE; }
 
	char Text[100];
	DQstrcpy(Text, line, min(100,DQSkipWord(line, MAX_STRING_CHARS)+1));
	Warning(ERR_FS, va("Unknown shader function : %s in %s",Text, this->Filename) );
	return FALSE;
}

#undef ifcompare

//Puts the current Stage as the first of a new pass, and finalises the previous pass
void c_Shader::AddRenderPass()
{
	s_TextureStage** x = nullptr;

	//Put this current stage in a new pass
	CurrentRenderPass->Next = DQNew( s_RenderPass );
	CurrentRenderPass->Next->StageChain = CurrentStage;

	//Stop the old pass from executing the first stage of the next pass
	if(CurrentStage) {
		for(s_TextureStage **x = &CurrentRenderPass->StageChain;(*x)->Next;x = &(*x)->Next) NULL;
		*x = nullptr;
	}
	
	//Update CurrentRenderPass
	CurrentRenderPass = CurrentRenderPass->Next;

	//Update CurrentStage
	CurrentStage = CurrentRenderPass->StageChain;

	//Update CurrentGlobalCommand
	CurrentGlobalCommand = NULL;

	TextureStageNum = 1;
}

void c_Shader::AddCommand( Shader_BaseFunc *Command )
{
	if(!CurrentCommand) {
		CurrentCommand = Command;
		CurrentStage->CommandChain = Command;
	}
	else {
		CurrentCommand->Next = Command;
		CurrentCommand = CurrentCommand->Next;
	}
}

void c_Shader::AddGlobalCommand( Shader_BaseFunc *Command )
{
	if(!CurrentGlobalCommand) {
		CurrentGlobalCommand = Command;
		CurrentRenderPass->GlobalCommandChain = Command;
	}
	else {
		CurrentGlobalCommand->Next = Command;
		CurrentGlobalCommand = CurrentGlobalCommand->Next;
	}
}

void c_Shader::NewStage()
{
	++TextureStageNum;

	if(TextureStageNum>DQRender.MaxSimultaneousTextures) {
		//Force new render pass
		CurrentStage = NULL;
		AddRenderPass();
	}

	if(!CurrentStage) {
		CurrentRenderPass->StageChain = DQNew( s_TextureStage );
		CurrentStage = CurrentRenderPass->StageChain;
		CurrentStage->OverbrightBits = 0;		//default to rgbgen identity for a new stage
		TextureStageNum = 1;
	}
	else {
		CurrentStage->Next = DQNew( s_TextureStage );
		CurrentStage = CurrentStage->Next;

		//force new pass if rgbgen or alphagen exist
		if(CurrentRenderPass->rgbgen || CurrentRenderPass->alphagen) {
			AddRenderPass();
		}
		CurrentStage->OverbrightBits = DQConsole.OverbrightBits;
	}

	CurrentCommand = NULL;
}