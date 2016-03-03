// DXQuake3 Source
// Copyright (c) 2003 Richard Geary (richard.geary@dial.pipex.com)
//
//
//	c_Shader definition
//

#ifndef __C_SHADERFUNCS
#define __C_SHADERFUNCS

class Shader_BaseFunc {
public:
	Shader_BaseFunc():Next(NULL) {}
	virtual ~Shader_BaseFunc() { DQDelete(Next); }
	virtual void Shader_BaseFunc::Run() {};
	virtual void Shader_BaseFunc::Initialise() {}
	virtual void Unload() {};

	//Static variables which hold data being currently worked on

	//These are variables changed by the Shader_ derived classes as they are run
	//using static vars like this saves a lot of needless passing variables
	static DWORD ShaderRGBA;
	static DWORD TFactor;
	static BOOL EnableTransparency;
	static MATRIX TexCoordMultiply[8];
	static LPDIRECT3DTEXTURE9 LightmapTexture;

	//These have to be set before running Initialise or Run
	static int TextureStage;
	static int Flags;
	static DWORD CurrentRS;		//Current Render State (anything set with device->SetRenderState)
	static DWORD CurrentTSS;		//Render state for current texture stage being worked on (loaded, initialised or run)
	static float ShaderTime;
	static int OverbrightBits;

	Shader_BaseFunc *Next;

protected:
	enum e_Wave { e_sin, e_triangle, e_square, e_sawtooth, e_invsawtooth, e_noise };
	struct s_Wave {
		e_Wave func;
		float base, amp, phase, freq;
	};
	float WaveEvaluate(s_Wave *wave);
	float TriangleWaveFunc(float x);
	float SquareWaveFunc(float x);
	float SawtoothWaveFunc(float x);
};

class Shader_Map : public Shader_BaseFunc {
public:
	Shader_Map::Shader_Map(char *line);
	virtual ~Shader_Map() { Unload(); }
	virtual void Shader_Map::Unload();
	virtual void Shader_Map::Initialise();
	virtual void Shader_Map::Run();

	char TextureFilename[MAX_QPATH];
	c_Texture *Texture;
	enum e_Type { texture, lightmap, whiteimage, portaltexture } type;
};

class Shader_ClampMap : public Shader_BaseFunc {
public:
	Shader_ClampMap::Shader_ClampMap (char *line);
	virtual ~Shader_ClampMap() { Unload(); }
	virtual void Shader_ClampMap ::Unload ();
	virtual void Shader_ClampMap ::Initialise();
	virtual void Shader_ClampMap ::Run();

	char TextureFilename[MAX_QPATH];
	c_Texture *Texture;
};

class Shader_AnimMap : public Shader_BaseFunc {
public:
	Shader_AnimMap(char *line);
	virtual ~Shader_AnimMap() { Unload(); }
	virtual void Unload();
	virtual void Run();
	virtual void Initialise();

	char TextureFilename[8][MAX_QPATH];
	c_Texture *Texture[8];
	float Frequency;
	int NumTextures;
};

class Shader_rgbgen :public Shader_BaseFunc {
public:
	Shader_rgbgen(char *line);
	virtual ~Shader_rgbgen() { Unload(); }
	virtual void Unload();
	virtual void Shader_rgbgen::Run();
	enum eType { identity, identityLighting, wave, entity, oneMinusEntity, vertex, oneMinusVertex, lightingDiffuse, exactVertex } type;
private:
	s_Wave *wavedata;
};

class Shader_tcMod :public Shader_BaseFunc {
public:
	Shader_tcMod(char *line);
	virtual ~Shader_tcMod() { Unload(); }
	virtual void Unload();
	virtual void Run();
private:
	enum eModType { rotate, scale, scroll, stretch, transform, turb } type;
	void *data;
	struct s_Rotate {
		float rate;
	};
	struct s_Scale {
		float uScale, vScale;
	};
	struct s_Scroll {
		float uRate, vRate;
	};
	struct s_Stretch {
		s_Wave wave;
	};
	struct s_Transform {
		float m00,m01,m10,m11,t0,t1;
	};
	struct s_Turb {
		s_Wave wave;	//always sin
	};
};

class Shader_tcGen :public Shader_BaseFunc {
public:
	Shader_tcGen(char *line);
	virtual ~Shader_tcGen() { Unload(); }
	virtual void Unload();
	virtual void Run();
	enum e_TCGen { base = 0, lightmap, environment, vector }; //base is default
	e_TCGen type;
private:
	struct s_Vector {
		D3DXVECTOR3 u,v;
	};
	void *data;
};

class Shader_blendFunc :public Shader_BaseFunc {
public:
	Shader_blendFunc(char *line);
	BOOL bMultipass;		//Multitexturing not possible - use another pass
	BOOL IsFilterStage() { return (Src==D3DBLEND_DESTCOLOR && Dest==D3DBLEND_ZERO)?TRUE:FALSE; }
	D3DBLEND Src, Dest;
	DWORD BlendTSSFlags;
};

class Shader_cull :public Shader_BaseFunc {
public:
	Shader_cull(char *line);
	virtual ~Shader_cull() { Unload(); }
	virtual void Unload() {};
	virtual void Run();
	enum eType { front, back, none } type;
};

class Shader_alphaFunc :public Shader_BaseFunc {
public:
	Shader_alphaFunc(char *line);
	virtual ~Shader_alphaFunc() { Unload(); }
	virtual void Unload() {};
	virtual void Run();
private:
	enum eType { GT0, LT128, GE128 } type;
};

class Shader_depthWrite :public Shader_BaseFunc {
public:
	Shader_depthWrite(char *line) {};
	virtual ~Shader_depthWrite() { Unload(); }
	virtual void Unload() {};
	virtual void Run();
};

class Shader_depthFunc :public Shader_BaseFunc {
public:
	Shader_depthFunc(char *line);
	virtual ~Shader_depthFunc() { Unload(); }
	virtual void Unload() {};
	virtual void Run();
private:
	enum eType { lequal, equal } type;
};

class Shader_alphagen :public Shader_BaseFunc {
public:
	Shader_alphagen(char *line);
	virtual ~Shader_alphagen() { Unload(); }
	virtual void Unload();
	virtual void Run();
	enum eType { identity, identityLighting, wave, entity, oneMinusEntity, vertex, oneMinusVertex, lightingDiffuse, portal, lightingSpecular } type;
	
	//for alphagen portal
	struct s_Portal {
		float DistanceToTransparent;
		float Distance;	//Set by RenderStack, this is the distance to the object being rendered
	};
	void *data;
};

class Shader_deformVertexes :public Shader_BaseFunc {
public:
	Shader_deformVertexes(char *line);
	virtual ~Shader_deformVertexes() { Unload(); }
	virtual void Unload();
	virtual void Run();
	void Deform( int NumVerts, D3DXVECTOR3 *Source, s_VertexStreamP *Dest, D3DXVECTOR3 *NormalSource );
	enum eType { wave, normal, bulge, move, autosprite, autosprite2 } type;
private:
	struct s_deformWave {
		float div;
		s_Wave wave;
	};
	struct s_bulge {
		float width, height, speed;
	};
	struct s_move {
		D3DXVECTOR3 Direction;
		s_Wave wave;
	};
	void *data;
};

class Shader_surfaceParam :public Shader_BaseFunc {
public:
	Shader_surfaceParam(char *line);
	virtual ~Shader_surfaceParam() { Unload(); }
	virtual void Run() {}
	enum e_SPType { Ignore = 0, AlphaShadow, AreaPortal, ClusterPortal, Flesh, Fog, Lava, MetalSteps,
		NoDLight, NoDraw, NoLightmap, NoSteps, NonSolid, Origin,
		PlayerClip, Slick, Slime, Structural, NoMipMaps, Water, Sky } type;
};

class Shader_Sort : public Shader_BaseFunc {
public:
	Shader_Sort(char *line);
	virtual ~Shader_Sort() {}
	int value;
};

class Shader_Sky :public Shader_BaseFunc {
public:
	Shader_Sky(char *line, int isObsoleteVersion);
	virtual ~Shader_Sky() { Unload(); }
	virtual void Unload();
	virtual void Initialise();
	virtual void Run();
	char farbox[MAX_QPATH];
	char nearbox[MAX_QPATH];
	int cloudheight;
};

class Shader_fogParams :public Shader_BaseFunc {
public:
	Shader_fogParams(char *line);
	virtual ~Shader_fogParams() {}
	virtual void Initialise();
	virtual void Run();
	void RunFogMultipass();

private:
	float r,g,b;
//	float DistanceToOpaque;
	DWORD dwFogColour;
	float FogDensity;
};

//*********************************************************************************

#endif //__C_SHADERFUNCS