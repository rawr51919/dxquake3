// DXQuake3 Source
// Copyright (c) 2003 Richard Geary (richard.geary@dial.pipex.com)
//

#ifndef __C_RENDERSTACK_H
#define __C_RENDERSTACK_H

#define MAX_SHADERS			5000

//#define MAX_STATICVERTS		300000
//#define MAX_DYNAMICVERTS	70000
//#define MAX_INDICES			700000
#define MAX_VBHANDLES		5000
#define MAX_IBHANDLES		2000
#define MAX_TM				2048

//#define _USE_STREAM_OFFSETS

struct s_RenderFeatureInfo {
	BOOL DrawAsSprite;
	float ShaderTime;
	DWORD ShaderRGBA;
	MATRIX *pTM;									//in s_RenderItem this points to one of the TMArray
	LPDIRECT3DTEXTURE9 LightmapTexture;		//NOT COMPARED
	BOOL bApplyLightVols;

	BOOL operator == (s_RenderFeatureInfo &other) {
		return ( (DrawAsSprite == other.DrawAsSprite) && (ShaderTime == other.ShaderTime) 
			&& (ShaderRGBA == other.ShaderRGBA) && (pTM == other.pTM) && (bApplyLightVols == other.bApplyLightVols) );
	}	
};

// Reference : Diesel Engine - Mathias Heyer(sonode@gmx.de) 
// (for the idea of a Render Stack)

class c_Render;
class c_RenderStack
{
public:

	friend c_Render;
	c_RenderStack();
	~c_RenderStack();
	void Initialise();
	void Clear();
	void SaveState();
	void ResetState();
	void Unload();
	void AllocateShaderHint( int ShaderHandle, int ItemCount );

	//Create Static Buffers
	int AddStaticVertices( D3DXVECTOR3 *PosArray, D3DXVECTOR2 *TexCoordArray1, D3DXVECTOR3 *NormalArray, DWORD *DiffuseArray, D3DXVECTOR2 *TexCoordArray2, int NumVerts );
	int AddStaticIndices( WORD *OffsetArray, int NumIndices );

	//Create Dynamic Buffers
	int NewDynamicVertexBufferHandle( int NumVerts, BOOL useNormals, BOOL useDiffuse, BOOL useTex2 );
	void UpdateDynamicVerts( int handle, int Offset, int NumVerts, D3DXVECTOR3 *PosArray, D3DXVECTOR2 *TexCoordArray1, D3DXVECTOR3 *NormalArray, DWORD *DiffuseArray, D3DXVECTOR2 *TexCoordArray2, int ShaderHandleHint );

	//Draw (add Primitives to stack)
	inline void DrawPrimitive( int VBhandle, int ShaderHandle, D3DPRIMITIVETYPE PrimType, unsigned int StartVert, int PrimCount, D3DXVECTOR3 ApproxPos, s_RenderFeatureInfo *ExtraFeatures ) { 	DrawIndexedPrimitive( VBhandle, 0, ShaderHandle, PrimType, StartVert, 0, PrimCount, ApproxPos, ExtraFeatures ); }
	void DrawIndexedPrimitive( int VBhandle, int IBhandle, int ShaderHandle, D3DPRIMITIVETYPE PrimType, unsigned int StartIndex, int BaseVertexOffset, int PrimCount, D3DXVECTOR3 ApproxPos, s_RenderFeatureInfo *ExtraFeatures  );
	
	inline void DrawBSPPrimitive( int VBhandle, int ShaderHandle, D3DPRIMITIVETYPE PrimType, unsigned int StartVert, int PrimCount, D3DXVECTOR3 ApproxPos, c_Texture *LightmapTexture ) { 	DrawBSPIndexedPrimitive( VBhandle, 0, ShaderHandle, PrimType, StartVert, 0, PrimCount, ApproxPos, LightmapTexture ); }
	void c_RenderStack::DrawBSPIndexedPrimitive( int VBhandle, int IBhandle, int ShaderHandle, D3DPRIMITIVETYPE PrimType, unsigned int StartIndex, int BaseVertexOffset, int PrimCount, D3DXVECTOR3 ApproxPos, c_Texture *_LightmapTexture  );

	void inline SetOverrideShader( int ShaderHandle ) { OverrideShaderHandle = ShaderHandle; }

	BOOL bProcessingBillboards;
private:
	BOOL useStreamOffsets;
	void inline SetUseStreamOffsets(BOOL canUseStreamOffsets) { useStreamOffsets = canUseStreamOffsets; }	//only run by c_Render::Initialise

	int MaxStaticIndices, MaxStaticVerts, MaxDynamicVerts;

	//***only run by c_Render***
	void c_RenderStack::RenderToScreen();
	void c_RenderStack::SortStack();
	//**************************

	void inline LockStaticVB();
	void inline LockDynamicVB();
	void inline LockStaticIB(); 
	void inline LockDynamicIB();
	void inline UnlockBuffers();

	//***********
	enum eVertDecl { eVD_Unspecified = 0, eVD_P0T1, eVD_P0N1T2, eVD_P0D1T2, eVD_P0N1D2T3, eVD_P0T1T2, eVD_P0N1T2T3, eVD_P0D1T2T3, eVD_P0N1D2T3T4 };
	LPDIRECT3DVERTEXDECLARATION9 Decl_P0T1, Decl_P0N1T2, Decl_P0D1T2, Decl_P0N1D2T3, Decl_P0T1T2, Decl_P0N1T2T3, Decl_P0D1T2T3, Decl_P0N1D2T3T4;

	LPDIRECT3DVERTEXDECLARATION9 inline GetVertexDeclaration( eVertDecl Decl );
	eVertDecl inline c_RenderStack::GetVertexDeclaration( c_Shader *Shader );

	struct s_D3DStreams {
		int P;		//Pos
		int N;		//Normal
		int D;		//Diffuse
		int T1;		//Tex1
		int T2;		//Tex2
	};

	//=======================
	//*****Render Stack********
	//=======================
	// DrawPrimitive and DrawIndexedPrimitive both add Render Items into these structs.
	// The render items are grouped into identical shader groups
	// SortStack() then calculates the optimum draw order (Front->Back)

#define MAX_ITEMS								100000
#define DEFAULT_ITEMS_PER_GROUP		   20
#define MAX_GROUPS_PER_SORTEDSTACK 10000
#define MAX_SHADERGROUPS				  10000

	struct s_RenderItem {
		int VBhandle;
		int IBhandle;

		//Render Info
		int IndexOffset;			//relative to VB/IB Handle offset.
		int BaseVertexOffset;	//for Indexed rendering
		D3DPRIMITIVETYPE PrimitiveType;
		int PrimCount;
		D3DXVECTOR3 ApproxPos;		//World position given by DrawPrimitive call
	};

	struct s_ItemHeap
	{
		s_RenderItem Items[MAX_ITEMS];
		int NextItem;

		s_ItemHeap():NextItem(0) {};
		inline void Reset()
		{
			NextItem = 0;
		}
	} ItemHeap;

	//**************************************************

	//Render all items in this group using the same shader pass
	struct s_ShaderGroup {
		c_QuicksortArray SortedArray;

		int StackIndex;							//Index into the stack's SortedArray (before running quicksort)
		int CurrentFrame;						//Frame that this data applies to

		int ShaderHandle;
		eVertDecl VertexDeclaration;			//Filled by Sort

		//Extra Rendering Features
		s_RenderFeatureInfo RenderFeatures;

		//Inline Functions
		s_ShaderGroup():StackIndex(-1),CurrentFrame(-1) {}
		~s_ShaderGroup() {}

		inline void Clear()
		{
			SortedArray.NextElement = 0;
		}

		inline void RegisterNewItem(int ItemIndex)
		{
			if( SortedArray.NextElement == SortedArray.ArraySize ) {
				//Should only happen rarely
				//Since the groups are registered by ShaderHandle, hopefully the well used groups will only do this a couple of times at the start
				SortedArray.IncreaseSize( max( 10, SortedArray.ArraySize*2 ) );
#if _DEBUG
				DQPrint( va("Performance Warning : Creating new larger item, Shader %d   StackIndex %d   Size %d",ShaderHandle,StackIndex,SortedArray.ArraySize ) );
#endif
			}
			else {
				SortedArray.Array[SortedArray.NextElement++].index = ItemIndex;
			}
		}
	};

	struct s_ShaderGroupHeap {
		s_ShaderGroup Groups[MAX_SHADERGROUPS];		//index is ShaderHandle for 0 to MAX_SHADERS. After that, it is allocated to shadergroups who need extra groups
		int NextGroup;

		s_ShaderGroupHeap() { Reset(); }
		inline void Reset() {
			NextGroup = MAX_SHADERS;
		}

	} ShaderGroupHeap;

	//********************************************

	struct s_SortedStack {
		c_QuicksortArray SortedGroups;

		s_SortedStack(unsigned int size):SortedGroups(size) {}
		
		inline void RegisterNewGroup(int GroupIndex)
		{
			if( SortedGroups.NextElement >= MAX_GROUPS_PER_SORTEDSTACK ) {
				CriticalError( ERR_RENDER, "Insufficient groups in Sort Stack" );
			}

			SortedGroups.Array[SortedGroups.NextElement++].index = GroupIndex;
		}

	} OpaqueSortStack, BlendedSortStack, ConsoleSortStack, SpriteSortStack, SkyboxSortStack;
	//Opaque has SortVal <= 3 
	//Blended has 3<SortVal<15
	//Console has SortVal>=15

	void inline ResetSortStack( s_SortedStack *Stack )
	{
		const int max = Stack->SortedGroups.NextElement;
		register int i;
		for(i=0; i<max; ++i) {
			ShaderGroupHeap.Groups[ Stack->SortedGroups.Array[i].index ].Clear();
		}

		Stack->SortedGroups.NextElement = 0;
	}

	//************************

	int &CurrentFrame;
	s_RenderFeatureInfo NullRenderFeatures;
	MATRIX TMArray[MAX_TM];
	WORD NextTM;
	BOOL bUseVertexLighting;
	
	//Vertex & Index Buffers
	//**********************
	struct s_Buffer {
		LPDIRECT3DVERTEXBUFFER9 VB_P;
		LPDIRECT3DVERTEXBUFFER9 VB_N;
		LPDIRECT3DVERTEXBUFFER9 VB_D;
		LPDIRECT3DVERTEXBUFFER9 VB_T1;
		LPDIRECT3DVERTEXBUFFER9 VB_T2;
		BOOL isVBLocked;
		s_VertexStreamP *VertexP;
		s_VertexStreamN *VertexN;
		s_VertexStreamD *VertexD;
		s_VertexStreamT *VertexT1;
		s_VertexStreamT *VertexT2;
		s_D3DStreams NextVBPos;

		LPDIRECT3DINDEXBUFFER9 IndexBuffer;
		WORD *Index;
		int NextIBPos;
		BOOL isIBLocked;
	};
	s_Buffer StaticBuffer;
	s_Buffer DynamicBuffer;

	struct s_VertexBufferItem {
		s_D3DStreams FirstIndex;
		s_D3DStreams bUseStream;
		BOOL bUseDynamicBuffer;
		int NumVerts;
	};
	s_VertexBufferItem VBHandles[MAX_VBHANDLES];

	//if IBhandle<0, DynamicHandle = ~IBhandle else StaticHandle = IBhandle. If IBhandle==0, there is no index
	struct s_IndexBufferItem {
		int Offset;		//Offset into the IndexBuffer
		WORD MinIndex;	//Drawing Hint
		WORD MaxIndex;	//Drawing Hint
		int NumIndices;
	};
	s_IndexBufferItem StaticIBHandles[MAX_IBHANDLES],DynamicIBHandles[MAX_IBHANDLES];

	int NextVBHandle;
	int NextStaticIBHandle, NextDynamicIBHandle;
	int SavedVBHandles, SavedStaticIB;

	int OverrideShaderHandle;
};


#endif // __C_RENDERSTACK_H