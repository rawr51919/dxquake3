// DXQuake3 Source
// Copyright (c) 2003 Richard Geary (richard.geary@dial.pipex.com)
//
//
//	Read the shaders into a usable format, and implement them

#include "stdafx.h"

#define ifcompare(x) if(DQWordstrcmpi(word, x, MAX_STRING_CHARS)==0)

#define NEXTPARAM \
	line += DQSkipWhitespace(line, MAX_STRING_CHARS); \
    line += DQWordstrcpy(word, line, MAX_STRING_CHARS);

//Instantise Static vars
int Shader_BaseFunc::TextureStage;
int Shader_BaseFunc::Flags;
BOOL Shader_BaseFunc::EnableTransparency;
MATRIX Shader_BaseFunc::TexCoordMultiply[8];
LPDIRECT3DTEXTURE9 Shader_BaseFunc::LightmapTexture;
DWORD Shader_BaseFunc::CurrentRS;
DWORD Shader_BaseFunc::CurrentTSS;		//TSS = Texture Stage State
float Shader_BaseFunc::ShaderTime;
DWORD Shader_BaseFunc::TFactor;
DWORD Shader_BaseFunc::ShaderRGBA;
int Shader_BaseFunc::OverbrightBits;

//Reference variables pointing to the current variables of the shader being Initialised or Run
//static int &TextureStage = Shader_BaseFunc::TextureStage;
//static int &Flags = Shader_BaseFunc::Flags;
static DWORD &ShaderRS = Shader_BaseFunc::CurrentRS;
static DWORD &ShaderTSS = Shader_BaseFunc::CurrentTSS;

//Reference variables for Run() only. These variables are State Variables
static BOOL &EnableTransparency = Shader_BaseFunc::EnableTransparency;
#define TC (Shader_BaseFunc::TexCoordMultiply[TextureStage])
/*
static int &TextureNum = c_Shader::TextureNum;
static BOOL &EnableTextureTransform = c_Shader::EnableTextureTransform;
extern D3DXVECTOR3 g_SpriteVertexData[3];
*/
//file-global variable for temporary storage of the current parameter
static char word[MAX_STRING_CHARS];

//***************************************************************************

float Shader_BaseFunc::WaveEvaluate(s_Wave *wave) {
	switch(wave->func) {
	case e_sin:
		return (wave->base + wave->amp* (float)sin(2*D3DX_PI*(wave->freq*ShaderTime +wave->phase)));
	case e_triangle:
		return (wave->base + wave->amp*TriangleWaveFunc(wave->freq*ShaderTime +wave->phase));
	case e_square:
		return (wave->base + wave->amp*SquareWaveFunc(wave->freq*ShaderTime +wave->phase));
	case e_sawtooth:
		return (wave->base + wave->amp*SawtoothWaveFunc(wave->freq*ShaderTime +wave->phase));
	case e_invsawtooth:
		return (wave->base + wave->amp*(1.0f-SawtoothWaveFunc(wave->freq*ShaderTime +wave->phase)));
	case e_noise:
		return (wave->base + wave->amp*(SquareWaveFunc(wave->freq*ShaderTime +wave->phase)*SquareWaveFunc(wave->freq*3*ShaderTime +wave->phase)));
	default:
		Error(ERR_SHADER, "Invalid Wave function");
		return 0;
	}
}

float Shader_BaseFunc::TriangleWaveFunc(float x) {
	x-=(float)floor(x);
	x*=2.0f;
	if(x>1.0f) x=2.0f-x;
	return x;
}

float Shader_BaseFunc::SquareWaveFunc(float x) {
	x-=(float)floor(x);
	return (x>0.5f)?1.0f:0.0f;
}

float Shader_BaseFunc::SawtoothWaveFunc(float x) {
	x-=(float)floor(x);
	return x;
}

//******************************************************

Shader_Map::Shader_Map(char *line) {
	//Initialise
	Texture = NULL;

	NEXTPARAM;
	ifcompare("$lightmap") { 
		type = lightmap; 
		return; 
	}
	ifcompare("$whiteimage") { type = whiteimage; return; }
	ifcompare("*white") { type = whiteimage; return; }
	ifcompare("$portaltexture") { type = portaltexture; return; }

	type = texture;
	DQstrcpy(TextureFilename, word, MAX_QPATH);
}

void Shader_Map::Unload() {
	if(type == texture)
		SafeRelease( Texture );
}

void Shader_Map::Initialise() {

	//if already initialised
	if(Texture) return;

	if(type == lightmap) {
		return;					//Lightmap textures loaded into D3D by BSP loader
	}

	if(type == whiteimage) {
		Texture = &DQRender.WhiteImageTexture;
		return;
	}

#ifdef _PORTALS
	if(type == portaltexture) {
		if(DQRender.NumRenderToTextures==MAX_RENDERTOTEXTURE) DQRender.NumRenderToTextures = 0;
		Texture = (c_Texture*)&DQRender.RenderToTexture[DQRender.NumRenderToTextures++];
		return;
	}
#endif

	if(type == texture) {
		Texture = DQRender.OpenTextureFile( TextureFilename, (Flags&ShaderFlag_NoMipmaps), OverbrightBits );
	}
}

void Shader_Map::Run() {
	if(type==lightmap) {
		ShaderTSS |= RS_TSS_TCI_1;		//use lightmap tex coords
		d3dcheckOK( DQDevice->SetTexture( TextureStage, Shader_BaseFunc::LightmapTexture ) );
	}
#ifdef _PORTALS
	else if(type == portaltexture) {
		if( !(((c_Render::s_RenderToTexture*)Texture)->bEnable) ) {	//Only render this once the texture is rendered
			d3dcheckOK( DQDevice->SetTexture( TextureStage, ((c_Render::s_RenderToTexture*)Texture)->RenderTarget ) );
		}
		else {
			d3dcheckOK( DQDevice->SetTexture( TextureStage, DQRender.NoShaderTexture) );
		}
	}
#endif
	else {
		d3dcheckOK( DQDevice->SetTexture( TextureStage, Texture->D3DTexture ) );
	}
}

//******************************************************


Shader_ClampMap::Shader_ClampMap(char *line) {
	NEXTPARAM;
	DQstrcpy(TextureFilename, word, MAX_QPATH);

	//Initialise
	Texture = NULL;
}

void Shader_ClampMap::Unload() {
	SafeRelease( Texture );
}

void Shader_ClampMap::Initialise() {

	if(Texture) return;

	Texture = DQRender.OpenTextureFile( TextureFilename, (Flags&ShaderFlag_NoMipmaps), OverbrightBits );
}

void Shader_ClampMap::Run() {
	ShaderTSS |= RS_TSS_CLAMP;

	d3dcheckOK( DQDevice->SetTexture( TextureStage, Texture->D3DTexture ) );
}

//******************************************************

Shader_AnimMap::Shader_AnimMap(char *line) {
	char *NextLine;

	//Initialise
	for(int i=0;i<8;++i) 
		Texture[i] = NULL;
	
	NEXTPARAM;
	Frequency = (float)atof(word);
	//AnimMap takes a variable number of parameters, up to 8, all on the same line
	NextLine = line + DQNextLine(line, MAX_STRING_CHARS);

	for(int i=0;i<8;++i) {
		NEXTPARAM;

		if(line>=NextLine) { 
			NumTextures = i; 
			return; 
		}
		DQstrcpy(TextureFilename[i], word, MAX_QPATH);
	}
	NumTextures = 8;
}

void Shader_AnimMap::Unload() {
	for(int i=0;i<NumTextures;++i) {
		SafeRelease( Texture[i] );
	}
}

void Shader_AnimMap::Initialise() {
	int i;

	if(Texture[0]) return;

	for(i=0;i<NumTextures;++i) {
		Texture[i] = DQRender.OpenTextureFile( TextureFilename[i], (Flags&ShaderFlag_NoMipmaps), OverbrightBits );
		if(!Texture[i]) break;
	}
	NumTextures = i;
}

void Shader_AnimMap::Run() {
	int i = (int)(ShaderTime*Frequency)%NumTextures;

	d3dcheckOK( DQDevice->SetTexture( TextureStage, Texture[i]->D3DTexture ) );
}

//******************************************************

Shader_tcGen::Shader_tcGen(char *line)
{
	data = NULL;
	NEXTPARAM;
	ifcompare("vector") {
		type = vector;
		data = DQNew(s_Vector);
#define data ((s_Vector *)data)
		NEXTPARAM;
		data->u.x = (float)atof(word);
		NEXTPARAM;
		data->u.y = (float)atof(word);
		NEXTPARAM;
		data->u.z = (float)atof(word);
		NEXTPARAM;
		data->v.x = (float)atof(word);
		NEXTPARAM;
		data->v.y = (float)atof(word);
		NEXTPARAM;
		data->v.z = (float)atof(word);
#undef data
	}
		
	ifcompare("base") { type = base; return; }
	ifcompare("lightmap") { type = lightmap; return; }
	ifcompare("environment") { type = environment; return; }
}

void Shader_tcGen::Unload() {
	DQDelete( data );
}

void Shader_tcGen::Run() {
//	MATRIX mat;
	register float val4;
	BOOL is2D;

	switch(type) {

	case base:
	case lightmap:
		return;

	case environment:
		if(!(ShaderTSS & RS_TSS_TTF)) {
			//Need to initialise TCM if it's not been used yet
			//Optimised version of below
			TC._11 = 0.5f;
			TC._22 = -0.5f;
			TC._41 = 0.5f;
			TC._42 = 0.5f;
			TC._12 = TC._13 = TC._14 = TC._21 = TC._23 = TC._24 = TC._31 = TC._32 = TC._33 = TC._34 = TC._43 = 0.0f;
			TC._44 = 1.0f;
		}
		else 
		{
			is2D = (ShaderTSS & RS_TSS_TTFC3)?FALSE:TRUE;

			//Transform to 3D Matrix Translation - translation is now the 4th not 3rd element
			if(is2D) {
				TC._41 = TC._31;
				TC._42 = TC._32;
				TC._31 = TC._32 = TC._33 = TC._43 = 0.0f;
				TC._44 = 1.0f;
			}
/*
			D3DXMatrixIdentity(&mat);
			mat._11 = 0.5f;//707106781f;
			mat._22 = -0.5f;//707106781f;
			mat._41 = 0.5f;
			mat._42 = 0.5f;
			TC = TC*mat;

			TC._13 = TC._23 = TC._33 = 0.0f;
			*/

			//(Optimised version of the above)
			val4 = TC._14;
			TC._11 = 0.5f*(TC._11+val4);
			TC._12 = 0.5f*(-TC._12+val4);
			TC._13 *= 0.5f;
			TC._14 *= 0.5f;
			val4 = TC._24;
			TC._21 = 0.5f*(TC._21+val4);
			TC._22 = 0.5f*(-TC._22+val4);
			TC._23 *= 0.5f;
			TC._24 *= 0.5f;
			val4 = TC._34;
			TC._31 = 0.0f;
			TC._32 = 0.0f;
			TC._33 = 0.0f;
			TC._34 = 0.0f;
			val4 = TC._44;
			TC._41 = 0.5f*(TC._41+val4);
			TC._42 = 0.5f*(-TC._42+val4);
			TC._43 = 1.0f;
			TC._44 = 1.0f;

		}
		ShaderTSS &= ~RS_TSS_TCI_1;
		ShaderTSS |= RS_TSS_TCI_CSRV;
		ShaderTSS |= RS_TSS_CLAMP;		//ie. not wrap
		ShaderTSS |= RS_TSS_MIRROR;		//override clamp - use mirror Tex Coords instead
		ShaderRS |= RS_NORMALIZENORMALS;
		ShaderTSS |= RS_TSS_TTF;
		ShaderTSS |= RS_TSS_TTFC3;
		return;

	case vector:
		CriticalError(ERR_SHADER, "tcgen vector Not implemented"); 
		return;

	default:
		CriticalError(ERR_SHADER, "**Invalid tcGen parameter**");
	};
	
}

//*********************************************************************

Shader_tcMod::Shader_tcMod(char *line):data(NULL)
{
	NEXTPARAM;
	ifcompare("rotate") {
		type = rotate;
		data = DQNew(s_Rotate);
#define data ((s_Rotate*)data)
		NEXTPARAM;
		data->rate = (float)atof(word);
#undef data
	}

	ifcompare("scale") {
		type = scale;
		data = DQNew(s_Scale);
#define data ((s_Scale*)data)
		NEXTPARAM;
		data->uScale = (float)atof(word);
		NEXTPARAM;
		data->vScale = (float)atof(word);
#undef data
	}

	ifcompare("scroll") {
		type = scroll;
		data = DQNew(s_Scroll);
#define data ((s_Scroll*)data)
		NEXTPARAM;
		data->uRate = (float)atof(word);
		NEXTPARAM;
		data->vRate = (float)atof(word);
#undef data
	}

	ifcompare("stretch") {
		type = stretch;
		data = DQNew(s_Stretch);
#define data ((s_Stretch*)data)
		NEXTPARAM;
		ifcompare("sin") { data->wave.func = e_sin; }
		ifcompare("triangle") { data->wave.func = e_triangle; }
		ifcompare("square") { data->wave.func = e_square; }
		ifcompare("sawtooth") { data->wave.func = e_sawtooth; }
		ifcompare("inversesawtooth") { data->wave.func = e_invsawtooth; }
		ifcompare("noise") { data->wave.func = e_noise; }
		NEXTPARAM;
		data->wave.base = (float)atof(word);
		NEXTPARAM;
		data->wave.amp = (float)atof(word);
		NEXTPARAM;
		data->wave.phase = (float)atof(word);
		NEXTPARAM;
		data->wave.freq = (float)atof(word);
#undef data
	}

	ifcompare("transform") {
		type = transform;
		data = DQNew(s_Transform);
#define data ((s_Transform*)data)
		NEXTPARAM;
		data->m00 = (float)atof(word);
		NEXTPARAM;
		data->m01 = (float)atof(word);
		NEXTPARAM;
		data->m10 = (float)atof(word);
		NEXTPARAM;
		data->m11 = (float)atof(word);
		NEXTPARAM;
		data->t0 = (float)atof(word);
		NEXTPARAM;
		data->t1 = (float)atof(word);
#undef data
	}

	ifcompare("turb") {
		type = turb;
		data = DQNew(s_Turb);
#define data ((s_Turb*)data)
		data->wave.func = e_sin;		//always sin
		NEXTPARAM;
		data->wave.base = (float)atof(word);
		NEXTPARAM;
		data->wave.amp = (float)atof(word);
		NEXTPARAM;
		data->wave.phase = (float)atof(word);
		NEXTPARAM;
		data->wave.freq = (float)atof(word);
#undef data
	}
}

void Shader_tcMod::Unload() {	DQDelete(data); }

void Shader_tcMod::Run() {
	MATRIX mat;
	float temp,temp2;

	D3DXMatrixIdentity( &mat );
	if(!(ShaderTSS & RS_TSS_TTF))
		D3DXMatrixIdentity( &TC );

	switch(type) {
	case rotate:		//Rotate about (0.5,0.5)
		temp = ((s_Rotate*)data)->rate *2*D3DX_PI/360.0f*ShaderTime;
		temp2 = (float)cos(temp);
		temp = (float)sin(temp);
		//Rotation matrix
		// { x' } = { cos t	   -sin t } { x }
		// { y' } = { sin t		cos t } { y }
		//NB. D3D uses the transverse of this matrix
		//Rotate about 0.5,0.5
		//  x' = x*cos t - y*sin t +0.5*(1-cos t + sin t)
		//  y' = x*sin t + y*cos t +0.5*(1-sin t + cos t)
		mat._11 = temp2;
		mat._12 = temp;
		mat._21 = -temp;
		mat._22 = temp2;
		if(! (ShaderTSS & RS_TSS_TTFC3) ) {		//if we aren't using 3D tex coords (ie. env. mapping)
			mat._31 = 0.5f*(1-temp2+temp);
			mat._32 = 0.5f*(1-temp-temp2);
		}
		else {
			mat._41 = 0.5f*(1-temp2+temp);
			mat._42 = 0.5f*(1-temp-temp2);
		}
		break;
	case scale:
		mat._11 = ((s_Scale*)data)->uScale;
		mat._22 = ((s_Scale*)data)->vScale;
		break;
	case scroll:
		if(! (ShaderTSS & RS_TSS_TTFC3) ) {
			mat._31 = ((s_Scroll*)data)->uRate*ShaderTime;
			mat._32 = ((s_Scroll*)data)->vRate*ShaderTime;
		}
		else {
			mat._41 = ((s_Scroll*)data)->uRate*ShaderTime;
			mat._42 = ((s_Scroll*)data)->vRate*ShaderTime;
		}
		break;
	case stretch:
		temp = 1.0f/WaveEvaluate(&((s_Stretch*)data)->wave);
		mat._11 = temp;
		mat._22 = temp;
		if(! (ShaderTSS & RS_TSS_TTFC3) ) {
			mat._31 = 0.5f*(1-temp);
			mat._32 = 0.5f*(1-temp);
		}
		else {
			mat._41 = 0.5f*(1-temp);
			mat._42 = 0.5f*(1-temp);
		}
		break;
	case transform:
		mat._11 = ((s_Transform*)data)->m00;
		mat._21 = ((s_Transform*)data)->m10;
		mat._12 = ((s_Transform*)data)->m01;
		mat._22 = ((s_Transform*)data)->m11;
		if(! (ShaderTSS & RS_TSS_TTFC3) ) {
			mat._31 = ((s_Transform*)data)->t0;
			mat._32 = ((s_Transform*)data)->t1;
		}
		else {
			mat._41 = ((s_Transform*)data)->t0;
			mat._42 = ((s_Transform*)data)->t1;
		}
		break;
	case turb:
		//Requires sin wave like addition to each vert separately
		// Not implemented
		break;
/*		//temp = WaveEvaluate(&((s_Turb*)data)->wave);
		temp = 2*D3DX_PI* ((s_Turb*)data)->wave.freq*ShaderTime;
		temp1 = ((s_Turb*)data)->wave.base + ((s_Turb*)data)->wave.amp*(float)sin(temp+ ((s_Turb*)data)->wave.phase );
		temp2 = ((s_Turb*)data)->wave.base + ((s_Turb*)data)->wave.amp*(float)sin(temp+ ((s_Turb*)data)->wave.phase +1);
		mat._11 = 0;
		mat._22 = 0;
		mat._13 = 1.0f-temp1;
		mat._23 = 1.0f-temp2;
		if(! (ShaderTSS & RS_TSS_TTFC3) ) {
			mat._31 = 0.5f*(temp1)+temp2;
			mat._32 = 0.5f*(temp2)+temp1;
		}
		else {
			mat._41 = 0.5f*(temp1)+temp2;
			mat._42 = 0.5f*(temp2)+temp1;
		}
		break;*/
	default:
		break;
	};
	TC = TC * mat;
	ShaderTSS |= RS_TSS_TTF;				//Enable texture transforms
}

//*********************************************************

Shader_rgbgen::Shader_rgbgen(char *line):wavedata(NULL) {

	NEXTPARAM;
	ifcompare("identityLighting") {	type = identityLighting; return;}
	ifcompare("identity") {	type = identity; return;}
	ifcompare("wave") {
		type = wave;
		wavedata = DQNew( s_Wave );
		NEXTPARAM;
		wavedata->func = (e_Wave)-1;
		ifcompare("sin") { wavedata->func = e_sin; }
		ifcompare("triangle") { wavedata->func = e_triangle; }
		ifcompare("square") { wavedata->func = e_square; }
		ifcompare("sawtooth") { wavedata->func = e_sawtooth; }
		ifcompare("inversesawtooth") { wavedata->func = e_invsawtooth; }
		ifcompare("noise") { wavedata->func = e_noise; }
		if(wavedata->func==-1) { 
			Error( ERR_MISC, "Invalid Shader rgbgen parameter" );
		}
		NEXTPARAM;
		wavedata->base = (float)atof(word);
		NEXTPARAM;
		wavedata->amp = (float)atof(word);
		NEXTPARAM;
		wavedata->phase = (float)atof(word);
		NEXTPARAM;
		wavedata->freq = (float)atof(word);
		return;
	}
	ifcompare("lightingDiffuse") { type = lightingDiffuse; return;}
	ifcompare("entity") { type = entity; return;}
	ifcompare("oneMinusEntity") {type = oneMinusEntity; return;}
	ifcompare("vertex") {type = vertex; return;}
	ifcompare("oneMinusVertex") {type = oneMinusVertex; return;}
	ifcompare("exactVertex") { type = exactVertex; return;}
	Error(ERR_SHADER, "rbdgen param error");
}

void Shader_rgbgen::Unload() {
	DQDelete(wavedata);
}

void Shader_rgbgen::Run() {
	float Colour;
	U8 U8Colour;
	switch(type) {/*
	case identity:
		TextureColour = 1.0f;
		ShaderRS &= ~RS_LIGHTING;
		ShaderRS &= ~RS_COLOR_DIFFUSE;
		break;
	case identityLighting:
		TextureColour = 1.0f;
		//Check this
		ShaderRS |= RS_LIGHTING;
		ShaderRS |= RS_COLOR_DIFFUSE;
		break;*/
/*	case lightingDiffuse:		//Overrides wave, etc
		//CHECK THIS
		TextureColour = 1.0f;
		ShaderRS |= RS_LIGHTING;
		ShaderRS |= RS_COLOR_DIFFUSE;
		break;*/
	case entity:
		TFactor = ShaderRGBA;
		break;
	case oneMinusEntity:
		TFactor = ~ShaderRGBA;
		break;
	case wave:					//Overrides identityLighting & lightingDiffuse
		Colour = ((float)(TFactor&0x000000FF))*min( 1.0f, max( 0.0f, WaveEvaluate(wavedata) ) );
		U8Colour = (U8)Colour;
		TFactor &= 0xFF000000;
		TFactor |= ( (U8Colour<<16) | (U8Colour<<8) | U8Colour );
		ShaderRS &= ~RS_LIGHTING;
		ShaderRS &= ~RS_COLOR_DIFFUSE;
		break;
/*	case exactVertex:
	case vertex:
		TextureColour = 1.0f;
		//d3dcheckOK( DQDevice->SetRenderState( D3DRS_LIGHTING, FALSE) );	//Check this
		ShaderRS &= ~RS_LIGHTING;		//check this
		ShaderRS |= RS_COLOR_DIFFUSE;
//		d3dcheckOK( DQDevice->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE) );	//always apply to stage 0
		break;
	case oneMinusVertex:
		TextureColour = 1.0f;
		ShaderRS &= ~RS_LIGHTING;		//check this
		ShaderRS |= RS_COLOR_DIFFUSE;
		ShaderTSS |= RS_TSS_COMPLEMENT_2;
//		d3dcheckOK( DQDevice->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE | D3DTA_COMPLEMENT) );
		break;*/
	default:
		Error(ERR_SHADER, "rgbgen : Parameter not implemented");
		break;
	};
}

//*********************************************************

Shader_blendFunc::Shader_blendFunc(char *line) {

	NEXTPARAM;
	Src = D3DBLEND_FORCE_DWORD;
	Dest = D3DBLEND_FORCE_DWORD;

	ifcompare("add") {
		Src = D3DBLEND_ONE;
		Dest = D3DBLEND_ONE;
		goto CheckMultipass;
	}
	ifcompare("gl_add") {
		Src = D3DBLEND_ONE;
		Dest = D3DBLEND_ONE;
		goto CheckMultipass;
	}
	ifcompare("filter") {
		Src = D3DBLEND_DESTCOLOR;
		Dest = D3DBLEND_ZERO;
		goto CheckMultipass;
	}
	ifcompare("blend") {
		Src = D3DBLEND_SRCALPHA;
		Dest = D3DBLEND_INVSRCALPHA;
		goto CheckMultipass;
	}
	ifcompare("GL_ONE") { Src = D3DBLEND_ONE; }
	ifcompare("GL_ZERO") { Src = D3DBLEND_ZERO; }
	ifcompare("GL_DST_COLOR") { Src = D3DBLEND_DESTCOLOR; }
	ifcompare("GL_ONE_MINUS_DST_COLOR") { Src = D3DBLEND_INVDESTCOLOR; }
	ifcompare("GL_SRC_ALPHA") { Src = D3DBLEND_SRCALPHA; }
	ifcompare("GL_ONE_MINUS_SRC_ALPHA") { Src = D3DBLEND_INVSRCALPHA; }
	if(Src == D3DBLEND_FORCE_DWORD) Error(ERR_SHADER, "Invalid blend parameter");

	NEXTPARAM;
	ifcompare("GL_ONE") { Dest = D3DBLEND_ONE; }
	ifcompare("GL_ZERO") { Dest = D3DBLEND_ZERO; }
	ifcompare("GL_SRC_COLOR") { Dest = D3DBLEND_SRCCOLOR; }
	ifcompare("GL_ONE_MINUS_SRC_COLOR") { Dest = D3DBLEND_INVSRCCOLOR; }
	ifcompare("GL_SRC_ALPHA") { Dest = D3DBLEND_SRCALPHA; }
	ifcompare("GL_ONE_MINUS_SRC_ALPHA") { Dest = D3DBLEND_INVSRCALPHA; }
	ifcompare("GL_DST_ALPHA") { Dest = D3DBLEND_DESTALPHA; }
	ifcompare("GL_ONE_MINUS_DST_ALPHA") { Dest = D3DBLEND_INVDESTALPHA; }
	if(Dest == D3DBLEND_FORCE_DWORD) Error(ERR_SHADER, "Invalid blend parameter");

CheckMultipass:

	//Blendfunc's own TSSFlags (not Shader_BaseFunc)
	BlendTSSFlags = 0;
	switch(Src) {
	case D3DBLEND_ONE:
		switch(Dest) {
		case D3DBLEND_ONE:
			BlendTSSFlags |= RS_TSS_COLOR_ADD;
			BlendTSSFlags |= RS_TSS_ALPHA_ADD;
			break;
		case D3DBLEND_ZERO:
			BlendTSSFlags |= RS_TSS_COLOR_SEL1;
			BlendTSSFlags |= RS_TSS_ALPHA_SEL1;
			break;
		case D3DBLEND_INVSRCCOLOR:
			BlendTSSFlags |= RS_TSS_COLOR_ADDSMOOTH;
			BlendTSSFlags |= RS_TSS_ALPHA_ADDSMOOTH;
			break;
		case D3DBLEND_SRCALPHA:
			BlendTSSFlags |= RS_TSS_MODALPHA_ADDCOLOR;
			break;
		case D3DBLEND_INVSRCALPHA:
			BlendTSSFlags |= RS_TSS_MODINVALPHA_ADDCOLOR;
			break;
		default:
			bMultipass = TRUE;
			return;
		};
		bMultipass = FALSE;
		return;
	case D3DBLEND_ZERO:
		switch(Dest) {
		case D3DBLEND_ONE:
			BlendTSSFlags |= RS_TSS_COLOR_SEL2;
			BlendTSSFlags |= RS_TSS_ALPHA_SEL2;
			break;
		case D3DBLEND_ZERO:
			BlendTSSFlags |= RS_TSS_COLOR_DISABLE;
			BlendTSSFlags |= RS_TSS_ALPHA_DISABLE;
			break;
		case D3DBLEND_SRCCOLOR:
			//D3DTOP_MODULATE
			break;
		case D3DBLEND_INVSRCCOLOR:
			BlendTSSFlags |= RS_TSS_COMPLEMENT_1;
			BlendTSSFlags |= RS_TSS_ALPHA_COMPLEMENT_1;
			break;
		default:
			bMultipass = TRUE;
			return;
		};
		bMultipass = FALSE;
		return;
	case D3DBLEND_DESTCOLOR:
		switch(Dest) {
		case D3DBLEND_ZERO:
			//D3DTOP_MODULATE
			break;
		case D3DBLEND_SRCCOLOR:
			BlendTSSFlags |= RS_TSS_COLOR_MODULATE2X | RS_TSS_ALPHA_MODULATE2X;
			break;
		case D3DBLEND_INVSRCCOLOR:
			BlendTSSFlags |= RS_TSS_COLOR_SEL2 | RS_TSS_ALPHA_SEL2;
			break;
		default:
			bMultipass = TRUE;
			return;
		};
		bMultipass = FALSE;
		return;
	case D3DBLEND_INVDESTCOLOR:
		switch(Dest) {
		case D3DBLEND_ONE:
			BlendTSSFlags |= RS_TSS_COLOR_ADDSMOOTH | RS_TSS_ALPHA_ADDSMOOTH;
			break;
		case D3DBLEND_ZERO:
			BlendTSSFlags |= RS_TSS_COMPLEMENT_2 | RS_TSS_ALPHA_COMPLEMENT_2;
			break;
		case D3DBLEND_SRCCOLOR:
			BlendTSSFlags |= RS_TSS_COLOR_SEL1 | RS_TSS_ALPHA_SEL1;
			break;
		default:
			bMultipass = TRUE;
			return;
		};
		bMultipass = FALSE;
		return;
	case D3DBLEND_SRCALPHA:
		switch(Dest) {
		case D3DBLEND_INVSRCALPHA:
			BlendTSSFlags |= RS_TSS_COLOR_BLENDTEXTUREALPHA | RS_TSS_ALPHA_BLENDTEXTUREALPHA;
			bMultipass = FALSE;
			return;
		default:
			bMultipass = TRUE;
			return;
		};
	default:
		bMultipass = TRUE;
	};
}

//*************************************************************

Shader_cull::Shader_cull(char *line) {
	NEXTPARAM;

	ifcompare("front") { type = front; return; }
	ifcompare("back") { type = back; return; }
	ifcompare("disable") { type = none; return; }
	ifcompare("none") { type = none; return; }
}

void Shader_cull::Run() {
	switch(type) {
	case front:
		ShaderRS |= RS_CULLFRONT;
		ShaderRS &= ~RS_CULLNONE;
//		d3dcheckOK( DQDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CW ) ); return;
	case back:
		ShaderRS &= ~RS_CULLFRONT;
		ShaderRS &= ~RS_CULLNONE;
//		d3dcheckOK( DQDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW ) ); return;
	case none:
		ShaderRS |= RS_CULLNONE;
		if(!(Flags&ShaderFlag_BSPShader)) ShaderRS |= RS_ZWRITEDISABLE;
//		d3dcheckOK( DQDevice->SetRenderState( D3DRS_ZWRITEENABLE, FALSE) );
//		d3dcheckOK( DQDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE ) ); return;
	default: break;
	};
}

//*************************************************************

Shader_alphaFunc::Shader_alphaFunc(char *line) {
	NEXTPARAM;

	ifcompare("GT0") { type = GT0; return; }
	ifcompare("LT128") { type = LT128; return; }
	ifcompare("GE128") { type = GE128; return; }
	Error(ERR_SHADER, "Invalid alphaFunc parameter");
}

void Shader_alphaFunc::Run() {
	switch(type) {
	case GT0:
		ShaderRS &= ~RS_ALPHAREF80;
		ShaderRS |= RS_ALPHATEST;
		ShaderRS &= ~RS_ALPHAFUNC;				//clear alpha func bits
		ShaderRS |= RS_ALPHAFUNC_GREATER;
//		d3dcheckOK( DQDevice->SetRenderState( D3DRS_ALPHAREF, 0x00000000 ) );
//		d3dcheckOK( DQDevice->SetRenderState( D3DRS_ALPHATESTENABLE, TRUE ) );
//		d3dcheckOK( DQDevice->SetRenderState( D3DRS_ALPHAFUNC, D3DCMP_GREATER ) );
		break;
	case LT128:
		ShaderRS |= RS_ALPHAREF80;
		ShaderRS |= RS_ALPHATEST;
		ShaderRS &= ~RS_ALPHAFUNC;				//clear alpha func bits
		ShaderRS |= RS_ALPHAFUNC_LESS;
//		d3dcheckOK( DQDevice->SetRenderState( D3DRS_ALPHAREF, 0x00000080 ) );
//		d3dcheckOK( DQDevice->SetRenderState( D3DRS_ALPHATESTENABLE, TRUE ) );
//		d3dcheckOK( DQDevice->SetRenderState( D3DRS_ALPHAFUNC, D3DCMP_LESS ) );
		break;
	case GE128:
		ShaderRS |= RS_ALPHAREF80;
		ShaderRS |= RS_ALPHATEST;
		ShaderRS &= ~RS_ALPHAFUNC;				//clear alpha func bits
		ShaderRS |= RS_ALPHAFUNC_GREATEREQUAL;
//		d3dcheckOK( DQDevice->SetRenderState( D3DRS_ALPHAREF, 0x00000080 ) );
//		d3dcheckOK( DQDevice->SetRenderState( D3DRS_ALPHATESTENABLE, TRUE ) );
//		d3dcheckOK( DQDevice->SetRenderState( D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL ) );
		break;
	default: break;
	};
}

//*************************************************************

Shader_depthFunc::Shader_depthFunc(char *line) {
	NEXTPARAM;
	ifcompare("lequal") { type = lequal; return; }
	ifcompare("equal") { type = equal; return; }
	Error(ERR_SHADER, "Invalid depthFunc parameter");
}

void Shader_depthFunc::Run() {
	switch(type) {
	case lequal:
//		d3dcheckOK( DQDevice->SetRenderState( D3DRS_ZFUNC, D3DCMP_LESSEQUAL ) );
		ShaderRS &= ~RS_ZFUNC_EQUAL;
		break;
	case equal:
		ShaderRS |= RS_ZFUNC_EQUAL;
//		d3dcheckOK( DQDevice->SetRenderState( D3DRS_ZFUNC, D3DCMP_EQUAL ) );
		break;
	default: break;
	};
}

//*************************************************************

//identical functionality as rgbgen, but with additional portal param
Shader_alphagen::Shader_alphagen(char *line):data(NULL) {
	NEXTPARAM;
	ifcompare("identityLighting") {	type = identityLighting; return; }
	ifcompare("identity") {	type = identity; return; }
	ifcompare("wave") {
		type = wave;
		data = DQNew( s_Wave );
#define data ((s_Wave*)data)
		NEXTPARAM;
		ifcompare("sin") { data->func = e_sin; }
		ifcompare("triangle") { data->func = e_triangle; }
		ifcompare("square") { data->func = e_square; }
		ifcompare("sawtooth") { data->func = e_sawtooth; }
		ifcompare("inversesawtooth") { data->func = e_invsawtooth; }
		ifcompare("noise") { data->func = e_noise; }
		NEXTPARAM;
		data->base = (float)atof(word);
		NEXTPARAM;
		data->amp = (float)atof(word);
		NEXTPARAM;
		data->phase = (float)atof(word);
		NEXTPARAM;
		data->freq = (float)atof(word);
#undef data
		return;
	}
	ifcompare("lightingDiffuse") { type = lightingDiffuse; return; }
	ifcompare("entity") { type = entity; return; }
	ifcompare("oneMinusEntity") { type = oneMinusEntity; return; }
	ifcompare("vertex") { type = vertex; return; }
	ifcompare("oneMinusVertex") { type = oneMinusVertex; return; }

	ifcompare("portal") { 
		type = portal; 
		data = DQNew( s_Portal );
#define data ((s_Portal*)data)
		NEXTPARAM;
		data->DistanceToTransparent = (float)atof(word);		//distance to transparent. The closer you get, the clearer the image is
		data->Distance = 0;
#undef data
		return; 
	}
	ifcompare("lightingSpecular") { type = lightingSpecular; return; }
	Error(ERR_SHADER, "alphagen param error");
}

void Shader_alphagen::Unload() {
	DQDelete(data);
}

void Shader_alphagen::Run() {
	float Alpha;
	U8 U8Alpha;
	switch(type) {
/*	case identity:
		TextureAlpha = 1.0f;
		ShaderRS &= ~RS_LIGHTING;
		ShaderRS &= ~RS_ALPHA_DIFFUSE;
		break;
	case identityLighting:
		TextureColour = 1.0f;
		//Check this
		ShaderRS &= ~RS_LIGHTING;
		ShaderRS &= ~RS_ALPHA_DIFFUSE;
		break;
	case lightingDiffuse:
		TextureAlpha = 1.0f;
		ShaderRS |= RS_LIGHTING;
		ShaderRS |= RS_ALPHA_DIFFUSE;
		break;
	case vertex:
		ShaderRS &= ~RS_LIGHTING;
		ShaderRS |= RS_ALPHA_DIFFUSE;
//		d3dcheckOK( DQDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_MODULATE) );
//		d3dcheckOK( DQDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE) );
		break;
	case oneMinusVertex:
		ShaderRS &= ~RS_LIGHTING;
		ShaderRS |= RS_ALPHA_DIFFUSE;
		ShaderTSS |= RS_TSS_ALPHA_COMPLEMENT_2;
//		d3dcheckOK( DQDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_MODULATE) );
//		d3dcheckOK( DQDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE | D3DTA_COMPLEMENT) );
		break;*/
	case entity:
		TFactor = ShaderRGBA;
		break;
	case oneMinusEntity:
		TFactor = ~ShaderRGBA;
		break;
	case wave:					//Overrides identityLighting & lightingDiffuse
		Alpha = ((float)(TFactor&0xFF000000))*WaveEvaluate((s_Wave*)data);
		U8Alpha = (U8)Alpha;
		TFactor &= 0x00FFFFFF;
		TFactor |= (U8Alpha<<24);
		ShaderRS &= ~RS_LIGHTING;
		ShaderRS &= ~RS_ALPHA_DIFFUSE;
		break;
	case lightingSpecular:
//		TFactor.a = 0.3f;				//HACK
		TFactor &= 0x00FFFFFF;
		TFactor |= (0x4C<<24);
		break;
	case portal:
		TFactor &= 0x00FFFFFF;
		Alpha = (((s_Portal*)data)->Distance-20.0f)/((s_Portal*)data)->DistanceToTransparent;
		Alpha = max( 0.0f, min( 1.0f, Alpha ) );
		U8Alpha = (U8)Alpha;
		TFactor |= (U8Alpha<<24);
		break;
	default:
		Error(ERR_SHADER, "Specified Alphagen not implemented");
		break;
	};
}

//*************************************************************


Shader_deformVertexes::Shader_deformVertexes(char *line):data(NULL) {
	NEXTPARAM;
	ifcompare("wave") {	
		type = wave;
		data = DQNew( s_deformWave );
#define data ((s_deformWave*)data)
		NEXTPARAM;
		data->div = (float)atof(word);
		NEXTPARAM;
		ifcompare("sin") { data->wave.func = e_sin; }
		ifcompare("triangle") { data->wave.func = e_triangle; }
		ifcompare("square") { data->wave.func = e_square; }
		ifcompare("sawtooth") { data->wave.func = e_sawtooth; }
		ifcompare("inversesawtooth") { data->wave.func = e_invsawtooth; }
		ifcompare("noise") { data->wave.func = e_noise; }
		NEXTPARAM;
		data->wave.base = (float)atof(word);
		NEXTPARAM;
		data->wave.amp = (float)atof(word);
		NEXTPARAM;
		data->wave.phase = (float)atof(word);
		NEXTPARAM;
		data->wave.freq = (float)atof(word);
#undef data
		return;
	}
	ifcompare("normal") {
		type = normal;
		data = DQNew( s_deformWave );
		//deformVertexes normal <amplitude> <freq>
#define data ((s_deformWave*)data)
		NEXTPARAM;
		data->wave.amp = (float)atof(word);
		NEXTPARAM;
		data->wave.freq = (float)atof(word);
		data->wave.phase = 0.0f;
		data->div = 0.0f;
		data->wave.func = e_sin;
		data->wave.base = 0.0f;
#undef data
		return; 
	}
	ifcompare("bulge") {
		type = bulge;
		data = DQNew( s_bulge );
#define data ((s_bulge*)data)
		NEXTPARAM;
		data->width = (float)atof(word);
		NEXTPARAM;
		data->height = (float)atof(word);
		NEXTPARAM;
		data->speed = (float)atof(word);
#undef data
		return;
	}
	ifcompare("move") {
		type = move;
		data = DQNew( s_move );
#define data ((s_move*)data)
		NEXTPARAM;
		data->Direction.x = (float)atof(word);
		NEXTPARAM;
		data->Direction.y = (float)atof(word);
		NEXTPARAM;
		data->Direction.z = (float)atof(word);
		NEXTPARAM;
		ifcompare("sin") { data->wave.func = e_sin; }
		ifcompare("triangle") { data->wave.func = e_triangle; }
		ifcompare("square") { data->wave.func = e_square; }
		ifcompare("sawtooth") { data->wave.func = e_sawtooth; }
		ifcompare("inversesawtooth") { data->wave.func = e_invsawtooth; }
		ifcompare("noise") { data->wave.func = e_noise; }
		NEXTPARAM;
		data->wave.base = (float)atof(word);
		NEXTPARAM;
		data->wave.amp = (float)atof(word);
		NEXTPARAM;
		data->wave.phase = (float)atof(word);
		NEXTPARAM;
		data->wave.freq = (float)atof(word);
#undef data
		return;
	}
	ifcompare("autosprite") {
		type = autosprite;
		data = DQNew( D3DXVECTOR3 );
		return;
	}
	ifcompare("autosprite2") {
		type = autosprite2;
		data = DQNew( D3DXVECTOR3 );
		return;
	}
	ifcompare("ProjectionShadow") {
		return;
	}
	ifcompare("text0") {
		return;
	}
	ifcompare("text1") {
		return;
	}
	Error(ERR_SHADER, "Invalid deformVertices parameter");
}

void Shader_deformVertexes::Unload() {
	DQDelete(data);
}

void Shader_deformVertexes::Run() {
}

//Run from DQRenderStack.UpdateDynamicVerts()
void Shader_deformVertexes::Deform( int NumVerts, D3DXVECTOR3 *Source, s_VertexStreamP *Dest, D3DXVECTOR3 *NormalArray )
{
	int i;
	float k, angfreq;
	D3DXVECTOR3 Base, Axis1, Axis2, Normal;

	switch(type) {
	case wave:
#define data ((s_deformWave*)data)
		Base = Source[0];
		Normal = NormalArray[0];
		if( Normal.x > 0.9f ) {	
			//Normal points mostly in x direction
			Axis1 = D3DXVECTOR3( 0,1.0f,0 );
		}
		else {
			Axis1 = D3DXVECTOR3( 0,0,1.0f );
		}
		D3DXVec3Cross( &Axis2, &Normal, &Axis1 );
		D3DXVec3Cross( &Axis1, &Normal, &Axis2 );

		//Assume div means distance for 1/2 a wavelength
		k = 1.0f / data->div;
		angfreq = 2*D3DX_PI*data->wave.freq;
		switch(data->wave.func) {
		case e_sin:
			k *= D3DX_PI;
			for(i=0; i<NumVerts; ++i, ++Dest, ++Source) {
				Dest->Pos = *Source + Normal * sin( k * D3DXVec3Dot(Source, &Axis1) + k * D3DXVec3Dot(Source, &Axis2) - angfreq*ShaderTime ) * data->wave.amp;
			}
			break;
		case e_triangle:
			for(i=0; i<NumVerts; ++i, ++Dest, ++Source) {
				Dest->Pos = *Source + Normal * TriangleWaveFunc( k * D3DXVec3Dot(Source, &Axis1) + k * D3DXVec3Dot(Source, &Axis2) - data->wave.freq*ShaderTime ) * data->wave.amp;
			}
			break;
		case e_square:
			for(i=0; i<NumVerts; ++i, ++Dest, ++Source) {
				Dest->Pos = *Source + Normal * SquareWaveFunc( k * D3DXVec3Dot(Source, &Axis1) + k * D3DXVec3Dot(Source, &Axis2) - data->wave.freq*ShaderTime ) * data->wave.amp;
			}
			break;
		case e_sawtooth:
			for(i=0; i<NumVerts; ++i, ++Dest, ++Source) {
				Dest->Pos = *Source + Normal * SawtoothWaveFunc( k * D3DXVec3Dot(Source, &Axis1) + k * D3DXVec3Dot(Source, &Axis2) - data->wave.freq*ShaderTime ) * data->wave.amp;
			}
			break;
		case e_invsawtooth:
			for(i=0; i<NumVerts; ++i, ++Dest, ++Source) {
				Dest->Pos = *Source + Normal * (1.0f-SawtoothWaveFunc( k * D3DXVec3Dot(Source, &Axis1) + k * D3DXVec3Dot(Source, &Axis2) - data->wave.freq*ShaderTime )) * data->wave.amp;
			}
			break;
		default:
			Error( ERR_RENDER, "Unsupported deformVertex wave function" );
		};
		break;
#undef data

	case normal:
#define data ((s_deformWave*)data)
		if(!NormalArray) return;
		angfreq = 2*D3DX_PI*data->wave.freq;

		for(i=0; i<NumVerts; ++i, ++Dest, ++Source, ++NormalArray) {
			Dest->Pos = *Source + *NormalArray * sin( angfreq*ShaderTime ) * data->wave.amp;
		}
		break;
#undef data

	case move:
#define data ((s_move*)data)
		angfreq = 2*D3DX_PI*data->wave.freq;
		for(i=0; i<NumVerts; ++i, ++Dest, ++Source) {
			Dest->Pos = *Source + data->Direction * sin( angfreq*ShaderTime ) * data->wave.amp;
		}

	default:
		break;
	};
}

//******************************************

void Shader_depthWrite::Run() {
//	d3dcheckOK( DQDevice->SetRenderState( D3DRS_ZWRITEENABLE, TRUE) );
	ShaderRS &= ~RS_ZWRITEDISABLE;
}

//******************************************

Shader_surfaceParam::Shader_surfaceParam(char *line)
{
	NEXTPARAM;
	ifcompare("alphaShadow") {	type = AlphaShadow;	return; }
	ifcompare("AreaPortal") { type = AreaPortal; return; }
	ifcompare("ClusterPortal") { type = ClusterPortal; return; }
	ifcompare("DoNotEnter") { type = Ignore; return; }
	ifcompare("Flesh") { type = Flesh; return; }
	ifcompare("Fog") { type = Fog; return; }
	ifcompare("Lava") { type = Lava; return; }
	ifcompare("MetalSteps") { type = MetalSteps; return; }
	ifcompare("NoDamage") { type = Ignore; return; }
	ifcompare("NoDLight") { type = NoDLight; return; }
	ifcompare("NoDraw") { type = NoDraw; return; }
	ifcompare("NoDrop") { type = Ignore; return; }
	ifcompare("NoImpact") { type = Ignore; return; }
	ifcompare("NoMarks") { type = Ignore; return; }
	ifcompare("NoLightmap") { type = NoLightmap; return; }
	ifcompare("NoSteps") { type = NoSteps; return; }
	ifcompare("NonSolid") { type = NonSolid; return; }
	ifcompare("Origin") { type = Origin; return; }
	ifcompare("PlayerClip") { type = PlayerClip; return; }
	ifcompare("Slick") { type = Slick; return; }
	ifcompare("Slime") { type = Slime; return; }
	ifcompare("Structural") { type = Structural; return; }
	ifcompare("Trans") { type = Ignore; return; }
	ifcompare("Water") { type = Water; return; }
	ifcompare("sky") { type = Sky; return; }
	ifcompare("Detail") { type = Ignore; return; }
	ifcompare("nomipmaps") { type = NoMipMaps; return; }
	Warning(ERR_SHADER, va("Invalid Surfaceparam parameter %s",word) );
}

//*************************************************************

//skyparms <farbox> <cloudheight> <nearbox>
Shader_Sky::Shader_Sky(char *line, int isObsoleteVersion)
{
	NEXTPARAM;
	DQstrcpy(farbox, word, MAX_QPATH);
	if(isObsoleteVersion==0) {
		NEXTPARAM;
		cloudheight = (float)atof(word);
		NEXTPARAM;
		DQstrcpy(nearbox, word, MAX_QPATH);
	}
	else {
		cloudheight = 128;
		nearbox[0] = '\0';
	}
}

void Shader_Sky::Unload() {}

void Shader_Sky::Initialise()
{
	char BaseFilename[MAX_QPATH];

	DQBSP.CloudHeight = cloudheight;

	if(DQBSP.SkyBoxFilename[0][0]) return;					//Only load once
	if(DQstrcmpi( farbox, "-", MAX_QPATH )==0) return;

	//Create side texture filenames
	DQstrcpy( BaseFilename, farbox, MAX_QPATH );
	DQStripGfxExtention( BaseFilename, MAX_QPATH );
	for(int i=0;i<6;++i) {
		DQstrcpy( DQBSP.SkyBoxFilename[i], BaseFilename, MAX_QPATH );
		switch(i) {
		case 0:	DQstrcat( DQBSP.SkyBoxFilename[i], "_up.tga", MAX_QPATH ); break;
		case 1: DQstrcat( DQBSP.SkyBoxFilename[i], "_bk.tga", MAX_QPATH ); break;
		case 2: DQstrcat( DQBSP.SkyBoxFilename[i], "_lf.tga", MAX_QPATH ); break;
		case 3: DQstrcat( DQBSP.SkyBoxFilename[i], "_ft.tga", MAX_QPATH ); break;
		case 4: DQstrcat( DQBSP.SkyBoxFilename[i], "_rt.tga", MAX_QPATH ); break;
		case 5: DQstrcat( DQBSP.SkyBoxFilename[i], "_dn.tga", MAX_QPATH ); break;
		}
	}
}

void Shader_Sky::Run() {}

//*************************************************************

Shader_Sort::Shader_Sort(char *line) {
	NEXTPARAM;
	ifcompare("portal") { value = 1; return; }
	ifcompare("sky") { value = 2; return; }
	ifcompare("opaque") { value = 3; return; }
	ifcompare("banner") { value = 6; return; }
	ifcompare("underwater") { value = 8; return; }
	ifcompare("additive") { value = 9; return; }
	ifcompare("nearest") { value = 16; return; }
	value = (int)atoi(word);

	if(value<1 || value>16) Error(ERR_SHADER, "Invalid Sort Parameter");
}

//*************************************************************

Shader_fogParams::Shader_fogParams(char *line)
{
	NEXTPARAM;
	ifcompare("(") {
		NEXTPARAM;
	}

	r = (float)atof(word) * 255.0f;
	NEXTPARAM;
	g = (float)atof(word) * 255.0f;
	NEXTPARAM;
	b = (float)atof(word) * 255.0f;
	NEXTPARAM;

	ifcompare(")") {
		NEXTPARAM;
	}

//	DistanceToOpaque = (float)atof(word);
	FogDensity = 3.0f / (float)atof(word);
}

void Shader_fogParams::Initialise()
{
	float Overbright = 1<<OverbrightBits;
	dwFogColour = D3DCOLOR_RGBA( (int)(r/Overbright), (int)(g/Overbright), (int)(b/Overbright), 0 );
}

void Shader_fogParams::Run()
{
	ShaderRS |= RS_FOGENABLE;

	d3dcheckOK( DQDevice->SetRenderState( D3DRS_FOGCOLOR, dwFogColour ) );

//	d3dcheckOK( DQDevice->SetRenderState( D3DRS_FOGEND, *((DWORD*)&DistanceToOpaque) ) );
	d3dcheckOK( DQDevice->SetRenderState( D3DRS_FOGDENSITY, *((DWORD*)&FogDensity) ) );
}

void Shader_fogParams::RunFogMultipass()
{
	DQRender.SetTFactor( 0 );
	d3dcheckOK( DQDevice->SetRenderState( D3DRS_FOGCOLOR, 0xFFFFFFFF ) );
	d3dcheckOK( DQDevice->SetRenderState( D3DRS_BLENDFACTOR, dwFogColour ) );
	d3dcheckOK( DQDevice->SetRenderState( D3DRS_FOGDENSITY, *((DWORD*)&FogDensity) ) );

	DQRender.SetBlendFuncs( D3DBLEND_BLENDFACTOR, D3DBLEND_INVSRCCOLOR );
	DQRender.UpdateRS(RS_ALPHABLENDENABLE | RS_ZWRITEDISABLE | RS_FOGENABLE);
	DQRender.UpdateTSS(0, 0, NULL);
	DQRender.DisableStage( 1 );
}
	

//*************************************************************
#undef ifcompare
#undef NEXTPARAM