#include "header.h"
//-------------------------------------------------
// The comments assume VOXEL_NUM_X=256
//-------------------------------------------------

// The linear sampler only used for reading the input volume data
SamplerState g_samLinear : register(s0);

// For building the HP base, and traversal. This is the input volume
Texture3D<float4> g_txColorVol : register(t0);
Texture3D<float2> g_txDensityVol : register(t1);

// For building the HistoPyramid, in the reduction process, serves as input
Texture3D<uint> g_txHPLayer : register(t2);

// For traveral HP, the below are 3D texture objects in decreasing reso order
// We don't need the pyramid's top (1^3 texture3D), so the pyramid of 3d texture
// objects include 2^3 reso to the very base original reso one.
Texture3D<uint> g_txHP0 : register(t3);// HP base level 0, contain MC cell case number
Texture3D<uint> g_txHP1 : register(t4);// HP level1, and as all listed below contains number of active cells of the previous level
#if VOXEL_NUM_X > 4
Texture3D<uint> g_txHP2 : register(t5);// HP level2
#endif
#if VOXEL_NUM_X > 8
Texture3D<uint> g_txHP3 : register(t6);// HP level3
#endif
#if VOXEL_NUM_X > 16
Texture3D<uint> g_txHP4 : register(t7);// HP level4
#endif
#if VOXEL_NUM_X > 32
Texture3D<uint> g_txHP5 : register(t8);// HP level5
#endif
#if VOXEL_NUM_X > 64
Texture3D<uint> g_txHP6 : register(t9);// HP level6
#endif
#if VOXEL_NUM_X > 128
Texture3D<uint> g_txHP7 : register(t10);// HP level7, the pyramid second Top, reso:2^3. We don't need the top
#endif
#if VOXEL_NUM_X > 256
Texture3D<uint> g_txHP8 : register(t11);
#endif
#if VOXEL_NUM_X > 512
Texture3D<uint> g_txHP9 : register(t12);
#endif

// Phong shading variable
static const float4 light_offset = float4 (0.0f, 0.0f, 0.0f, 0.0f);
static const float4 ambient = float4 (0.1f, 0.1f, 0.1f, 1.0f);
static const float4 light_attenuation = float4 (1, 0, 0, 0);
//--------------------------------------------------------------------------------------
// Buffers
//--------------------------------------------------------------------------------------
cbuffer initial : register(b0){
	float4 cb_f4HPMCInfo;// # of cubes along x,y,z in .xyz conponents, .w is cube size
	float4 cb_f4VolInfo; // size of volume in object space;isolevel in .w conponent
};
cbuffer perFrame : register(b1){
	float4 cb_f4ViewPos;
	matrix cb_mWorldViewProj;
	matrix cb_mView;
};
cbuffer perReduction : register(b2){
	int4 cb_i4RTReso;
};
cbuffer cbImmutable
{
	static const int3 cb_QuadrantOffset[8] =
	{
		int3(0, 0, 0),
		int3(1, 0, 0),
		int3(0, 1, 0),
		int3(1, 1, 0),
		int3(0, 0, 1),
		int3(1, 0, 1),
		int3(0, 1, 1),
		int3(1, 1, 1),
	};
	static const float3 cb_quadPos[4] =
	{
		float3(-1, 1, 0),
		float3(1, 1, 0),
		float3(-1, -1, 0),
		float3(1, -1, 0),
	};
	static const float2 cb_quadTex[4] =
	{
		float2(0, 1),
		float2(1, 1),
		float2(0, 0),
		float2(1, 0),
	};
	static const float3 cb_halfCubeOffset[8] =
	{
		float3(-1, -1, -1),
		float3(-1, 1, -1),
		float3(1, 1, -1),
		float3(1, -1, -1),
		float3(-1, -1, 1),
		float3(-1, 1, 1),
		float3(1, 1, 1),
		float3(1, -1, 1)
	};
	// Return two endpoints idx, if given edge number
	static const int2 cb_edgeTable[12] =
	{
		{ 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 },
		{ 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 },
		{ 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 }
	};
	// Return # of polygons, if given case number
	static const int cb_casePolyTable[256] =
	{
		0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 2, 1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 3,
		1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 3, 2, 3, 3, 2, 3, 4, 4, 3, 3, 4, 4, 3, 4, 5, 5, 2,
		1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 3, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 4,
		2, 3, 3, 4, 3, 4, 2, 3, 3, 4, 4, 5, 4, 5, 3, 2, 3, 4, 4, 3, 4, 5, 3, 2, 4, 5, 5, 4, 5, 2, 4, 1,
		1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 3, 2, 3, 3, 4, 3, 4, 4, 5, 3, 2, 4, 3, 4, 3, 5, 2,
		2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 4, 3, 4, 4, 3, 4, 5, 5, 4, 4, 3, 5, 2, 5, 4, 2, 1,
		2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 2, 3, 3, 2, 3, 4, 4, 5, 4, 5, 5, 2, 4, 3, 5, 4, 3, 2, 4, 1,
		3, 4, 4, 5, 4, 5, 3, 4, 4, 5, 5, 2, 3, 4, 2, 1, 2, 3, 3, 2, 3, 4, 2, 1, 3, 2, 4, 1, 2, 1, 1, 0
	};
}
tbuffer tbImmutable
{
	// Return edge info for each vertex(5 at most), if given case number
	static const int3 cb_triTable[256][5] =
	{
		{ { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 0, 8, 3 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 0, 1, 9 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 1, 8, 3 }, { 9, 8, 1 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 1, 2, 10 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 0, 8, 3 }, { 1, 2, 10 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 9, 2, 10 }, { 0, 2, 9 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 2, 8, 3 }, { 2, 10, 8 }, { 10, 9, 8 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 3, 11, 2 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 0, 11, 2 }, { 8, 11, 0 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 1, 9, 0 }, { 2, 3, 11 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 1, 11, 2 }, { 1, 9, 11 }, { 9, 8, 11 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 3, 10, 1 }, { 11, 10, 3 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 0, 10, 1 }, { 0, 8, 10 }, { 8, 11, 10 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 3, 9, 0 }, { 3, 11, 9 }, { 11, 10, 9 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 9, 8, 10 }, { 10, 8, 11 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 4, 7, 8 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 4, 3, 0 }, { 7, 3, 4 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 0, 1, 9 }, { 8, 4, 7 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 4, 1, 9 }, { 4, 7, 1 }, { 7, 3, 1 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 1, 2, 10 }, { 8, 4, 7 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 3, 4, 7 }, { 3, 0, 4 }, { 1, 2, 10 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 9, 2, 10 }, { 9, 0, 2 }, { 8, 4, 7 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 2, 10, 9 }, { 2, 9, 7 }, { 2, 7, 3 }, { 7, 9, 4 }, { -1, -1, -1 } },
		{ { 8, 4, 7 }, { 3, 11, 2 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 11, 4, 7 }, { 11, 2, 4 }, { 2, 0, 4 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 9, 0, 1 }, { 8, 4, 7 }, { 2, 3, 11 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 4, 7, 11 }, { 9, 4, 11 }, { 9, 11, 2 }, { 9, 2, 1 }, { -1, -1, -1 } },
		{ { 3, 10, 1 }, { 3, 11, 10 }, { 7, 8, 4 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 1, 11, 10 }, { 1, 4, 11 }, { 1, 0, 4 }, { 7, 11, 4 }, { -1, -1, -1 } },
		{ { 4, 7, 8 }, { 9, 0, 11 }, { 9, 11, 10 }, { 11, 0, 3 }, { -1, -1, -1 } },
		{ { 4, 7, 11 }, { 4, 11, 9 }, { 9, 11, 10 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 9, 5, 4 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 9, 5, 4 }, { 0, 8, 3 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 0, 5, 4 }, { 1, 5, 0 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 8, 5, 4 }, { 8, 3, 5 }, { 3, 1, 5 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 1, 2, 10 }, { 9, 5, 4 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 3, 0, 8 }, { 1, 2, 10 }, { 4, 9, 5 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 5, 2, 10 }, { 5, 4, 2 }, { 4, 0, 2 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 2, 10, 5 }, { 3, 2, 5 }, { 3, 5, 4 }, { 3, 4, 8 }, { -1, -1, -1 } },
		{ { 9, 5, 4 }, { 2, 3, 11 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 0, 11, 2 }, { 0, 8, 11 }, { 4, 9, 5 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 0, 5, 4 }, { 0, 1, 5 }, { 2, 3, 11 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 2, 1, 5 }, { 2, 5, 8 }, { 2, 8, 11 }, { 4, 8, 5 }, { -1, -1, -1 } },
		{ { 10, 3, 11 }, { 10, 1, 3 }, { 9, 5, 4 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 4, 9, 5 }, { 0, 8, 1 }, { 8, 10, 1 }, { 8, 11, 10 }, { -1, -1, -1 } },
		{ { 5, 4, 0 }, { 5, 0, 11 }, { 5, 11, 10 }, { 11, 0, 3 }, { -1, -1, -1 } },
		{ { 5, 4, 8 }, { 5, 8, 10 }, { 10, 8, 11 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 9, 7, 8 }, { 5, 7, 9 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 9, 3, 0 }, { 9, 5, 3 }, { 5, 7, 3 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 0, 7, 8 }, { 0, 1, 7 }, { 1, 5, 7 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 1, 5, 3 }, { 3, 5, 7 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 9, 7, 8 }, { 9, 5, 7 }, { 10, 1, 2 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 10, 1, 2 }, { 9, 5, 0 }, { 5, 3, 0 }, { 5, 7, 3 }, { -1, -1, -1 } },
		{ { 8, 0, 2 }, { 8, 2, 5 }, { 8, 5, 7 }, { 10, 5, 2 }, { -1, -1, -1 } },
		{ { 2, 10, 5 }, { 2, 5, 3 }, { 3, 5, 7 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 7, 9, 5 }, { 7, 8, 9 }, { 3, 11, 2 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 9, 5, 7 }, { 9, 7, 2 }, { 9, 2, 0 }, { 2, 7, 11 }, { -1, -1, -1 } },
		{ { 2, 3, 11 }, { 0, 1, 8 }, { 1, 7, 8 }, { 1, 5, 7 }, { -1, -1, -1 } },
		{ { 11, 2, 1 }, { 11, 1, 7 }, { 7, 1, 5 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 9, 5, 8 }, { 8, 5, 7 }, { 10, 1, 3 }, { 10, 3, 11 }, { -1, -1, -1 } },
		{ { 5, 7, 0 }, { 5, 0, 9 }, { 7, 11, 0 }, { 1, 0, 10 }, { 11, 10, 0 } },
		{ { 11, 10, 0 }, { 11, 0, 3 }, { 10, 5, 0 }, { 8, 0, 7 }, { 5, 7, 0 } },
		{ { 11, 10, 5 }, { 7, 11, 5 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 10, 6, 5 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 0, 8, 3 }, { 5, 10, 6 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 9, 0, 1 }, { 5, 10, 6 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 1, 8, 3 }, { 1, 9, 8 }, { 5, 10, 6 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 1, 6, 5 }, { 2, 6, 1 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 1, 6, 5 }, { 1, 2, 6 }, { 3, 0, 8 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 9, 6, 5 }, { 9, 0, 6 }, { 0, 2, 6 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 5, 9, 8 }, { 5, 8, 2 }, { 5, 2, 6 }, { 3, 2, 8 }, { -1, -1, -1 } },
		{ { 2, 3, 11 }, { 10, 6, 5 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 11, 0, 8 }, { 11, 2, 0 }, { 10, 6, 5 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 0, 1, 9 }, { 2, 3, 11 }, { 5, 10, 6 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 5, 10, 6 }, { 1, 9, 2 }, { 9, 11, 2 }, { 9, 8, 11 }, { -1, -1, -1 } },
		{ { 6, 3, 11 }, { 6, 5, 3 }, { 5, 1, 3 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 0, 8, 11 }, { 0, 11, 5 }, { 0, 5, 1 }, { 5, 11, 6 }, { -1, -1, -1 } },
		{ { 3, 11, 6 }, { 0, 3, 6 }, { 0, 6, 5 }, { 0, 5, 9 }, { -1, -1, -1 } },
		{ { 6, 5, 9 }, { 6, 9, 11 }, { 11, 9, 8 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 5, 10, 6 }, { 4, 7, 8 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 4, 3, 0 }, { 4, 7, 3 }, { 6, 5, 10 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 1, 9, 0 }, { 5, 10, 6 }, { 8, 4, 7 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 10, 6, 5 }, { 1, 9, 7 }, { 1, 7, 3 }, { 7, 9, 4 }, { -1, -1, -1 } },
		{ { 6, 1, 2 }, { 6, 5, 1 }, { 4, 7, 8 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 1, 2, 5 }, { 5, 2, 6 }, { 3, 0, 4 }, { 3, 4, 7 }, { -1, -1, -1 } },
		{ { 8, 4, 7 }, { 9, 0, 5 }, { 0, 6, 5 }, { 0, 2, 6 }, { -1, -1, -1 } },
		{ { 7, 3, 9 }, { 7, 9, 4 }, { 3, 2, 9 }, { 5, 9, 6 }, { 2, 6, 9 } },
		{ { 3, 11, 2 }, { 7, 8, 4 }, { 10, 6, 5 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 5, 10, 6 }, { 4, 7, 2 }, { 4, 2, 0 }, { 2, 7, 11 }, { -1, -1, -1 } },
		{ { 0, 1, 9 }, { 4, 7, 8 }, { 2, 3, 11 }, { 5, 10, 6 }, { -1, -1, -1 } },
		{ { 9, 2, 1 }, { 9, 11, 2 }, { 9, 4, 11 }, { 7, 11, 4 }, { 5, 10, 6 } },
		{ { 8, 4, 7 }, { 3, 11, 5 }, { 3, 5, 1 }, { 5, 11, 6 }, { -1, -1, -1 } },
		{ { 5, 1, 11 }, { 5, 11, 6 }, { 1, 0, 11 }, { 7, 11, 4 }, { 0, 4, 11 } },
		{ { 0, 5, 9 }, { 0, 6, 5 }, { 0, 3, 6 }, { 11, 6, 3 }, { 8, 4, 7 } },
		{ { 6, 5, 9 }, { 6, 9, 11 }, { 4, 7, 9 }, { 7, 11, 9 }, { -1, -1, -1 } },
		{ { 10, 4, 9 }, { 6, 4, 10 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 4, 10, 6 }, { 4, 9, 10 }, { 0, 8, 3 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 10, 0, 1 }, { 10, 6, 0 }, { 6, 4, 0 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 8, 3, 1 }, { 8, 1, 6 }, { 8, 6, 4 }, { 6, 1, 10 }, { -1, -1, -1 } },
		{ { 1, 4, 9 }, { 1, 2, 4 }, { 2, 6, 4 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 3, 0, 8 }, { 1, 2, 9 }, { 2, 4, 9 }, { 2, 6, 4 }, { -1, -1, -1 } },
		{ { 0, 2, 4 }, { 4, 2, 6 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 8, 3, 2 }, { 8, 2, 4 }, { 4, 2, 6 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 10, 4, 9 }, { 10, 6, 4 }, { 11, 2, 3 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 0, 8, 2 }, { 2, 8, 11 }, { 4, 9, 10 }, { 4, 10, 6 }, { -1, -1, -1 } },
		{ { 3, 11, 2 }, { 0, 1, 6 }, { 0, 6, 4 }, { 6, 1, 10 }, { -1, -1, -1 } },
		{ { 6, 4, 1 }, { 6, 1, 10 }, { 4, 8, 1 }, { 2, 1, 11 }, { 8, 11, 1 } },
		{ { 9, 6, 4 }, { 9, 3, 6 }, { 9, 1, 3 }, { 11, 6, 3 }, { -1, -1, -1 } },
		{ { 8, 11, 1 }, { 8, 1, 0 }, { 11, 6, 1 }, { 9, 1, 4 }, { 6, 4, 1 } },
		{ { 3, 11, 6 }, { 3, 6, 0 }, { 0, 6, 4 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 6, 4, 8 }, { 11, 6, 8 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 7, 10, 6 }, { 7, 8, 10 }, { 8, 9, 10 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 0, 7, 3 }, { 0, 10, 7 }, { 0, 9, 10 }, { 6, 7, 10 }, { -1, -1, -1 } },
		{ { 10, 6, 7 }, { 1, 10, 7 }, { 1, 7, 8 }, { 1, 8, 0 }, { -1, -1, -1 } },
		{ { 10, 6, 7 }, { 10, 7, 1 }, { 1, 7, 3 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 1, 2, 6 }, { 1, 6, 8 }, { 1, 8, 9 }, { 8, 6, 7 }, { -1, -1, -1 } },
		{ { 2, 6, 9 }, { 2, 9, 1 }, { 6, 7, 9 }, { 0, 9, 3 }, { 7, 3, 9 } },
		{ { 7, 8, 0 }, { 7, 0, 6 }, { 6, 0, 2 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 7, 3, 2 }, { 6, 7, 2 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 2, 3, 11 }, { 10, 6, 8 }, { 10, 8, 9 }, { 8, 6, 7 }, { -1, -1, -1 } },
		{ { 2, 0, 7 }, { 2, 7, 11 }, { 0, 9, 7 }, { 6, 7, 10 }, { 9, 10, 7 } },
		{ { 1, 8, 0 }, { 1, 7, 8 }, { 1, 10, 7 }, { 6, 7, 10 }, { 2, 3, 11 } },
		{ { 11, 2, 1 }, { 11, 1, 7 }, { 10, 6, 1 }, { 6, 7, 1 }, { -1, -1, -1 } },
		{ { 8, 9, 6 }, { 8, 6, 7 }, { 9, 1, 6 }, { 11, 6, 3 }, { 1, 3, 6 } },
		{ { 0, 9, 1 }, { 11, 6, 7 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 7, 8, 0 }, { 7, 0, 6 }, { 3, 11, 0 }, { 11, 6, 0 }, { -1, -1, -1 } },
		{ { 7, 11, 6 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 7, 6, 11 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 3, 0, 8 }, { 11, 7, 6 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 0, 1, 9 }, { 11, 7, 6 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 8, 1, 9 }, { 8, 3, 1 }, { 11, 7, 6 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 10, 1, 2 }, { 6, 11, 7 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 1, 2, 10 }, { 3, 0, 8 }, { 6, 11, 7 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 2, 9, 0 }, { 2, 10, 9 }, { 6, 11, 7 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 6, 11, 7 }, { 2, 10, 3 }, { 10, 8, 3 }, { 10, 9, 8 }, { -1, -1, -1 } },
		{ { 7, 2, 3 }, { 6, 2, 7 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 7, 0, 8 }, { 7, 6, 0 }, { 6, 2, 0 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 2, 7, 6 }, { 2, 3, 7 }, { 0, 1, 9 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 1, 6, 2 }, { 1, 8, 6 }, { 1, 9, 8 }, { 8, 7, 6 }, { -1, -1, -1 } },
		{ { 10, 7, 6 }, { 10, 1, 7 }, { 1, 3, 7 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 10, 7, 6 }, { 1, 7, 10 }, { 1, 8, 7 }, { 1, 0, 8 }, { -1, -1, -1 } },
		{ { 0, 3, 7 }, { 0, 7, 10 }, { 0, 10, 9 }, { 6, 10, 7 }, { -1, -1, -1 } },
		{ { 7, 6, 10 }, { 7, 10, 8 }, { 8, 10, 9 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 6, 8, 4 }, { 11, 8, 6 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 3, 6, 11 }, { 3, 0, 6 }, { 0, 4, 6 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 8, 6, 11 }, { 8, 4, 6 }, { 9, 0, 1 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 9, 4, 6 }, { 9, 6, 3 }, { 9, 3, 1 }, { 11, 3, 6 }, { -1, -1, -1 } },
		{ { 6, 8, 4 }, { 6, 11, 8 }, { 2, 10, 1 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 1, 2, 10 }, { 3, 0, 11 }, { 0, 6, 11 }, { 0, 4, 6 }, { -1, -1, -1 } },
		{ { 4, 11, 8 }, { 4, 6, 11 }, { 0, 2, 9 }, { 2, 10, 9 }, { -1, -1, -1 } },
		{ { 10, 9, 3 }, { 10, 3, 2 }, { 9, 4, 3 }, { 11, 3, 6 }, { 4, 6, 3 } },
		{ { 8, 2, 3 }, { 8, 4, 2 }, { 4, 6, 2 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 0, 4, 2 }, { 4, 6, 2 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 1, 9, 0 }, { 2, 3, 4 }, { 2, 4, 6 }, { 4, 3, 8 }, { -1, -1, -1 } },
		{ { 1, 9, 4 }, { 1, 4, 2 }, { 2, 4, 6 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 8, 1, 3 }, { 8, 6, 1 }, { 8, 4, 6 }, { 6, 10, 1 }, { -1, -1, -1 } },
		{ { 10, 1, 0 }, { 10, 0, 6 }, { 6, 0, 4 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 4, 6, 3 }, { 4, 3, 8 }, { 6, 10, 3 }, { 0, 3, 9 }, { 10, 9, 3 } },
		{ { 10, 9, 4 }, { 6, 10, 4 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 4, 9, 5 }, { 7, 6, 11 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 0, 8, 3 }, { 4, 9, 5 }, { 11, 7, 6 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 5, 0, 1 }, { 5, 4, 0 }, { 7, 6, 11 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 11, 7, 6 }, { 8, 3, 4 }, { 3, 5, 4 }, { 3, 1, 5 }, { -1, -1, -1 } },
		{ { 9, 5, 4 }, { 10, 1, 2 }, { 7, 6, 11 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 6, 11, 7 }, { 1, 2, 10 }, { 0, 8, 3 }, { 4, 9, 5 }, { -1, -1, -1 } },
		{ { 7, 6, 11 }, { 5, 4, 10 }, { 4, 2, 10 }, { 4, 0, 2 }, { -1, -1, -1 } },
		{ { 3, 4, 8 }, { 3, 5, 4 }, { 3, 2, 5 }, { 10, 5, 2 }, { 11, 7, 6 } },
		{ { 7, 2, 3 }, { 7, 6, 2 }, { 5, 4, 9 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 9, 5, 4 }, { 0, 8, 6 }, { 0, 6, 2 }, { 6, 8, 7 }, { -1, -1, -1 } },
		{ { 3, 6, 2 }, { 3, 7, 6 }, { 1, 5, 0 }, { 5, 4, 0 }, { -1, -1, -1 } },
		{ { 6, 2, 8 }, { 6, 8, 7 }, { 2, 1, 8 }, { 4, 8, 5 }, { 1, 5, 8 } },
		{ { 9, 5, 4 }, { 10, 1, 6 }, { 1, 7, 6 }, { 1, 3, 7 }, { -1, -1, -1 } },
		{ { 1, 6, 10 }, { 1, 7, 6 }, { 1, 0, 7 }, { 8, 7, 0 }, { 9, 5, 4 } },
		{ { 4, 0, 10 }, { 4, 10, 5 }, { 0, 3, 10 }, { 6, 10, 7 }, { 3, 7, 10 } },
		{ { 7, 6, 10 }, { 7, 10, 8 }, { 5, 4, 10 }, { 4, 8, 10 }, { -1, -1, -1 } },
		{ { 6, 9, 5 }, { 6, 11, 9 }, { 11, 8, 9 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 3, 6, 11 }, { 0, 6, 3 }, { 0, 5, 6 }, { 0, 9, 5 }, { -1, -1, -1 } },
		{ { 0, 11, 8 }, { 0, 5, 11 }, { 0, 1, 5 }, { 5, 6, 11 }, { -1, -1, -1 } },
		{ { 6, 11, 3 }, { 6, 3, 5 }, { 5, 3, 1 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 1, 2, 10 }, { 9, 5, 11 }, { 9, 11, 8 }, { 11, 5, 6 }, { -1, -1, -1 } },
		{ { 0, 11, 3 }, { 0, 6, 11 }, { 0, 9, 6 }, { 5, 6, 9 }, { 1, 2, 10 } },
		{ { 11, 8, 5 }, { 11, 5, 6 }, { 8, 0, 5 }, { 10, 5, 2 }, { 0, 2, 5 } },
		{ { 6, 11, 3 }, { 6, 3, 5 }, { 2, 10, 3 }, { 10, 5, 3 }, { -1, -1, -1 } },
		{ { 5, 8, 9 }, { 5, 2, 8 }, { 5, 6, 2 }, { 3, 8, 2 }, { -1, -1, -1 } },
		{ { 9, 5, 6 }, { 9, 6, 0 }, { 0, 6, 2 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 1, 5, 8 }, { 1, 8, 0 }, { 5, 6, 8 }, { 3, 8, 2 }, { 6, 2, 8 } },
		{ { 1, 5, 6 }, { 2, 1, 6 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 1, 3, 6 }, { 1, 6, 10 }, { 3, 8, 6 }, { 5, 6, 9 }, { 8, 9, 6 } },
		{ { 10, 1, 0 }, { 10, 0, 6 }, { 9, 5, 0 }, { 5, 6, 0 }, { -1, -1, -1 } },
		{ { 0, 3, 8 }, { 5, 6, 10 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 10, 5, 6 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 11, 5, 10 }, { 7, 5, 11 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 11, 5, 10 }, { 11, 7, 5 }, { 8, 3, 0 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 5, 11, 7 }, { 5, 10, 11 }, { 1, 9, 0 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 10, 7, 5 }, { 10, 11, 7 }, { 9, 8, 1 }, { 8, 3, 1 }, { -1, -1, -1 } },
		{ { 11, 1, 2 }, { 11, 7, 1 }, { 7, 5, 1 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 0, 8, 3 }, { 1, 2, 7 }, { 1, 7, 5 }, { 7, 2, 11 }, { -1, -1, -1 } },
		{ { 9, 7, 5 }, { 9, 2, 7 }, { 9, 0, 2 }, { 2, 11, 7 }, { -1, -1, -1 } },
		{ { 7, 5, 2 }, { 7, 2, 11 }, { 5, 9, 2 }, { 3, 2, 8 }, { 9, 8, 2 } },
		{ { 2, 5, 10 }, { 2, 3, 5 }, { 3, 7, 5 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 8, 2, 0 }, { 8, 5, 2 }, { 8, 7, 5 }, { 10, 2, 5 }, { -1, -1, -1 } },
		{ { 9, 0, 1 }, { 5, 10, 3 }, { 5, 3, 7 }, { 3, 10, 2 }, { -1, -1, -1 } },
		{ { 9, 8, 2 }, { 9, 2, 1 }, { 8, 7, 2 }, { 10, 2, 5 }, { 7, 5, 2 } },
		{ { 1, 3, 5 }, { 3, 7, 5 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 0, 8, 7 }, { 0, 7, 1 }, { 1, 7, 5 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 9, 0, 3 }, { 9, 3, 5 }, { 5, 3, 7 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 9, 8, 7 }, { 5, 9, 7 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 5, 8, 4 }, { 5, 10, 8 }, { 10, 11, 8 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 5, 0, 4 }, { 5, 11, 0 }, { 5, 10, 11 }, { 11, 3, 0 }, { -1, -1, -1 } },
		{ { 0, 1, 9 }, { 8, 4, 10 }, { 8, 10, 11 }, { 10, 4, 5 }, { -1, -1, -1 } },
		{ { 10, 11, 4 }, { 10, 4, 5 }, { 11, 3, 4 }, { 9, 4, 1 }, { 3, 1, 4 } },
		{ { 2, 5, 1 }, { 2, 8, 5 }, { 2, 11, 8 }, { 4, 5, 8 }, { -1, -1, -1 } },
		{ { 0, 4, 11 }, { 0, 11, 3 }, { 4, 5, 11 }, { 2, 11, 1 }, { 5, 1, 11 } },
		{ { 0, 2, 5 }, { 0, 5, 9 }, { 2, 11, 5 }, { 4, 5, 8 }, { 11, 8, 5 } },
		{ { 9, 4, 5 }, { 2, 11, 3 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 2, 5, 10 }, { 3, 5, 2 }, { 3, 4, 5 }, { 3, 8, 4 }, { -1, -1, -1 } },
		{ { 5, 10, 2 }, { 5, 2, 4 }, { 4, 2, 0 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 3, 10, 2 }, { 3, 5, 10 }, { 3, 8, 5 }, { 4, 5, 8 }, { 0, 1, 9 } },
		{ { 5, 10, 2 }, { 5, 2, 4 }, { 1, 9, 2 }, { 9, 4, 2 }, { -1, -1, -1 } },
		{ { 8, 4, 5 }, { 8, 5, 3 }, { 3, 5, 1 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 0, 4, 5 }, { 1, 0, 5 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 8, 4, 5 }, { 8, 5, 3 }, { 9, 0, 5 }, { 0, 3, 5 }, { -1, -1, -1 } },
		{ { 9, 4, 5 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 4, 11, 7 }, { 4, 9, 11 }, { 9, 10, 11 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 0, 8, 3 }, { 4, 9, 7 }, { 9, 11, 7 }, { 9, 10, 11 }, { -1, -1, -1 } },
		{ { 1, 10, 11 }, { 1, 11, 4 }, { 1, 4, 0 }, { 7, 4, 11 }, { -1, -1, -1 } },
		{ { 3, 1, 4 }, { 3, 4, 8 }, { 1, 10, 4 }, { 7, 4, 11 }, { 10, 11, 4 } },
		{ { 4, 11, 7 }, { 9, 11, 4 }, { 9, 2, 11 }, { 9, 1, 2 }, { -1, -1, -1 } },
		{ { 9, 7, 4 }, { 9, 11, 7 }, { 9, 1, 11 }, { 2, 11, 1 }, { 0, 8, 3 } },
		{ { 11, 7, 4 }, { 11, 4, 2 }, { 2, 4, 0 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 11, 7, 4 }, { 11, 4, 2 }, { 8, 3, 4 }, { 3, 2, 4 }, { -1, -1, -1 } },
		{ { 2, 9, 10 }, { 2, 7, 9 }, { 2, 3, 7 }, { 7, 4, 9 }, { -1, -1, -1 } },
		{ { 9, 10, 7 }, { 9, 7, 4 }, { 10, 2, 7 }, { 8, 7, 0 }, { 2, 0, 7 } },
		{ { 3, 7, 10 }, { 3, 10, 2 }, { 7, 4, 10 }, { 1, 10, 0 }, { 4, 0, 10 } },
		{ { 1, 10, 2 }, { 8, 7, 4 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 4, 9, 1 }, { 4, 1, 7 }, { 7, 1, 3 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 4, 9, 1 }, { 4, 1, 7 }, { 0, 8, 1 }, { 8, 7, 1 }, { -1, -1, -1 } },
		{ { 4, 0, 3 }, { 7, 4, 3 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 4, 8, 7 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 9, 10, 8 }, { 10, 11, 8 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 3, 0, 9 }, { 3, 9, 11 }, { 11, 9, 10 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 0, 1, 10 }, { 0, 10, 8 }, { 8, 10, 11 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 3, 1, 10 }, { 11, 3, 10 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 1, 2, 11 }, { 1, 11, 9 }, { 9, 11, 8 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 3, 0, 9 }, { 3, 9, 11 }, { 1, 2, 9 }, { 2, 11, 9 }, { -1, -1, -1 } },
		{ { 0, 2, 11 }, { 8, 0, 11 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 3, 2, 11 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 2, 3, 8 }, { 2, 8, 10 }, { 10, 8, 9 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 9, 10, 2 }, { 0, 9, 2 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 2, 3, 8 }, { 2, 8, 10 }, { 0, 1, 8 }, { 1, 10, 8 }, { -1, -1, -1 } },
		{ { 1, 10, 2 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 1, 3, 8 }, { 9, 1, 8 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 0, 9, 1 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { 0, 3, 8 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 } },
		{ { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 } }
	};
};

//--------------------------------------------------------------------------------------
// Structures
//--------------------------------------------------------------------------------------
struct PassVS_OUT{
};

struct ShadingPS_IN{
	float4	Pos : SV_POSITION;
	float3	Nor : NORMAL0;
	float4	Col : COLOR0;
	float4	Pos_o : NORMAl1;
};

struct PosColGS_OUT{
	float4 SV_Pos : POSITION;
	float4 Col : COLOR0;
};

struct SliceGS_OUT{
	float4 SV_Pos : SV_POSITION;
	float4 VolCoord : NORMAL0;
	uint SliceIdx : SV_RenderTargetArrayIndex;
};

struct PS_3_OUT
{
	float4 Depth : SV_TARGET0;
	float4 Normal : SV_TARGET1;
	float4 Image : SV_TARGET2;
};

struct VertexInfo{
	float4 Field;
	float3 Pos;
	float3 Nor;
};

struct VertexInfoNoNor{
	float4 Field;
	float3 Pos;
};
//--------------------------------------------------------------------------------------
// Utility Funcs
//--------------------------------------------------------------------------------------
ShadingPS_IN CalIntersectionVertex(VertexInfo Data0, VertexInfo Data1){
	ShadingPS_IN output;
	float t = (cb_f4VolInfo.w - Data0.Field.w) / (Data1.Field.w - Data0.Field.w);
	output.Pos_o = float4(Data0.Pos + t * (Data1.Pos - Data0.Pos), 1);
	output.Pos = mul(output.Pos_o, cb_mWorldViewProj);
	//output.Pos_o = mul(output.Pos_o, cb_mView);
	output.Nor = normalize(Data0.Nor + t * (Data1.Nor - Data0.Nor));
	output.Col = float4(Data0.Field.xyz + t * (Data1.Field.xyz - Data0.Field.xyz), 1);
	return output;
}

PosColGS_OUT InterSec(VertexInfoNoNor Data0, VertexInfoNoNor Data1){
	PosColGS_OUT output;
	float t = (cb_f4VolInfo.w - Data0.Field.w) / (Data1.Field.w - Data0.Field.w);
	output.SV_Pos = float4(Data0.Pos + t * (Data1.Pos - Data0.Pos), 1);
	output.Col = float4(Data0.Field.xyz + t * (Data1.Field.xyz - Data0.Field.xyz), 1);
	return output;
}
float3 CalNormal(float3 txCoord){// Compute the normal from gradient
	float depth_dx = g_txDensityVol.SampleLevel(g_samLinear, txCoord, 0, int3 (1, 0, 0)).x -
		g_txDensityVol.SampleLevel(g_samLinear, txCoord, 0, int3 (-1, 0, 0)).x;
	float depth_dy = g_txDensityVol.SampleLevel(g_samLinear, txCoord, 0, int3 (0, -1, 0)).x -
		g_txDensityVol.SampleLevel(g_samLinear, txCoord, 0, int3 (0, 1, 0)).x;
	float depth_dz = g_txDensityVol.SampleLevel(g_samLinear, txCoord, 0, int3 (0, 0, 1)).x -
		g_txDensityVol.SampleLevel(g_samLinear, txCoord, 0, int3 (0, 0, -1)).x;
	return -normalize(float3 (depth_dx, depth_dy, depth_dz));
	//return -normalize(mul(float3 (depth_dx, depth_dy, depth_dz), cb_mView));
}

void PosInNextLevel(Texture3D<uint> txHPLevel, uint key_idx, inout uint4 p){// p.xyz is current pos, p.w is the sum
	int4 idx = int4(p.xyz * 2, 0);
	// Once we move to the next pyramid level, one texel in the previous level becomes 8 texels,
	// Here we get the corresponding 8 texels' value from the idx pos in previous level 
	uint4 frontNeighbor = uint4(txHPLevel.Load(idx, int3(0, 0, 0)), txHPLevel.Load(idx, int3(1, 0, 0)),
								txHPLevel.Load(idx, int3(0, 1, 0)), txHPLevel.Load(idx, int3(1, 1, 0)));
	uint4 backNeighbor = uint4(txHPLevel.Load(idx, int3(0, 0, 1)), txHPLevel.Load(idx, int3(1, 0, 1)),
							   txHPLevel.Load(idx, int3(0, 1, 1)), txHPLevel.Load(idx, int3(1, 1, 1)));

	// The following 2 uint4 variables works as the 8bit flag, which indicates whether
	// the local key value is greater or less than those value from the 8 texels
	uint4 frontFlag = uint4(0, 0, 0, 0);// indicate which quadrant the query belongs to
		uint4 backFlag = uint4(0, 0, 0, 0);// as the same as above

		// Here is how we calculate the local key
		uint localKey = key_idx - p.w;// substraction on key_idx is needed when moving p to the next HP level

	// The following code shows how we navigating in the current level 
	uint acc = frontNeighbor.x;
	frontFlag.x = acc < localKey;// flag the 'bit' if key value is larger than that quadrant sum
	acc += frontNeighbor.y;
	frontFlag.y = acc < localKey;
	acc += frontNeighbor.z;
	frontFlag.z = acc < localKey;
	acc += frontNeighbor.w;
	frontFlag.w = acc < localKey;
	acc += backNeighbor.x;
	backFlag.x = acc < localKey;
	acc += backNeighbor.y;
	backFlag.y = acc < localKey;
	acc += backNeighbor.z;
	backFlag.z = acc < localKey;
	acc += backNeighbor.w;
	//backFlag.w = 0; // now acc < key_idx must satisfied
	// now we sum the quadrants' whose accumulating value is less than the local key, 
	// and calculate the new p.w for generating new local key in the next level
	uint sum = dot(frontNeighbor, frontFlag) + dot(backNeighbor, backFlag);
	p.w += sum;
	// We calculate the p in this level
	uint offsetIdx = frontFlag.x + frontFlag.y + frontFlag.z + frontFlag.w + backFlag.x + backFlag.y + backFlag.z + backFlag.w;
	p.xyz = p.xyz * 2 + cb_QuadrantOffset[offsetIdx];
	return;
}

void PosInBaseLevel(Texture3D<uint> txHPBase, uint key_idx, inout uint4 p){// p.xyz is current pos, p.w is the sum
	int4 idx = int4(p.xyz * 2, 0);
		// Once we move to the next pyramid level, one texel in the previous level becomes 8 texels,
		// Here we get the corresponding 8 texels' value from the idx pos in previous level 
		uint4 frontNeighbor = uint4(txHPBase.Load(idx, int3(0, 0, 0)), txHPBase.Load(idx, int3(1, 0, 0)),
		txHPBase.Load(idx, int3(0, 1, 0)), txHPBase.Load(idx, int3(1, 1, 0)));
	uint4 backNeighbor = uint4(txHPBase.Load(idx, int3(0, 0, 1)), txHPBase.Load(idx, int3(1, 0, 1)),
							   txHPBase.Load(idx, int3(0, 1, 1)), txHPBase.Load(idx, int3(1, 1, 1)));
	frontNeighbor = clamp(frontNeighbor, 0, 1);
	backNeighbor = clamp(backNeighbor, 0, 1);
	// The following 2 uint4 variables works as the 8bit flag, which indicates whether
	// the local key value is greater or less than those value from the 8 texels
	uint4 frontFlag = uint4(0, 0, 0, 0);// indicate which quadrant the query belongs to
		uint4 backFlag = uint4(0, 0, 0, 0);// as the same as above

		// Here is how we calculate the local key
		uint localKey = key_idx - p.w;// substraction on key_idx is needed when moving p to the next HP level

	// The following code shows how we navigating in the current level 
	uint acc = frontNeighbor.x;
	frontFlag.x = acc < localKey;// flag the 'bit' if key value is larger than that quadrant sum
	acc += frontNeighbor.y;
	frontFlag.y = acc < localKey;
	acc += frontNeighbor.z;
	frontFlag.z = acc < localKey;
	acc += frontNeighbor.w;
	frontFlag.w = acc < localKey;
	acc += backNeighbor.x;
	backFlag.x = acc < localKey;
	acc += backNeighbor.y;
	backFlag.y = acc < localKey;
	acc += backNeighbor.z;
	backFlag.z = acc < localKey;
	acc += backNeighbor.w;
	//backFlag.w = 0; // now acc < key_idx must satisfied
	// now we sum the quadrants' whose accumulating value is less than the local key, 
	// and calculate the new p.w for generating new local key in the next level
	uint sum = dot(frontNeighbor, frontFlag) + dot(backNeighbor, backFlag);
	p.w += sum;
	// We calculate the p in this level
	uint offsetIdx = frontFlag.x + frontFlag.y + frontFlag.z + frontFlag.w + backFlag.x + backFlag.y + backFlag.z + backFlag.w;
	p.xyz = p.xyz * 2 + cb_QuadrantOffset[offsetIdx];
	return;
}
//--------------------------------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------------------------------
PassVS_OUT PassVS(uint vertexID : SV_VertexID){// Pass through VS
	PassVS_OUT output = (PassVS_OUT)0;
	return output;
}

//--------------------------------------------------------------------------------------
// Geometry Shader
//--------------------------------------------------------------------------------------
// GS for creating HP base volume, using normalized texCoord
[maxvertexcount(4)]
void VolSliceNorGS(point PassVS_OUT vertex[1], uint vertexID : SV_PrimitiveID, inout TriangleStream<SliceGS_OUT> triStream){
	SliceGS_OUT output;
	output.SV_Pos = float4(-1.0f, 1.0f, 0.0f, 1.0f);
	output.VolCoord = float4(0.0f, 0.0f, ((float)vertexID + 0.5) / cb_f4HPMCInfo.z, 0);// the half pix offset still exist when access texture3D(only z)
	output.SliceIdx = vertexID;
	triStream.Append(output);
	output.SV_Pos = float4(1.0f, 1.0f, 0.0f, 1.0f);
	output.VolCoord = float4(1.0f, 0.0f, ((float)vertexID + 0.5) / cb_f4HPMCInfo.z, 0);
	output.SliceIdx = vertexID;
	triStream.Append(output);
	output.SV_Pos = float4(-1.0f, -1.0f, 0.0f, 1.0f);
	output.VolCoord = float4(0.0f, 1.0f, ((float)vertexID + 0.5) / cb_f4HPMCInfo.z, 0);
	output.SliceIdx = vertexID;
	triStream.Append(output);
	output.SV_Pos = float4(1.0f, -1.0f, 0.0f, 1.0f);
	output.VolCoord = float4(1.0f, 1.0f, ((float)vertexID + 0.5) / cb_f4HPMCInfo.z, 0);
	output.SliceIdx = vertexID;
	triStream.Append(output);
}
// GS for creating HP mip level volume, using int non-normalized texCoord
[maxvertexcount(4)]
void VolSliceGS(point PassVS_OUT vertex[1], uint vertexID : SV_PrimitiveID, inout TriangleStream<SliceGS_OUT> triStream){
	// The half pixel offset trick in DX9 still exist, DX10,11 just pre deoffset for you after interpolation.
	// So if you want to do multiply on uv, first move back then do you multiply and after than offset half pixel!!
	// Here, since we do reduction which requires multiply by 2 in pixel shader. so we need to avoid the automatic 
	// half pixel offset, thus I offset half pixel on the opposite way.
	SliceGS_OUT output;
	output.SV_Pos = float4(-1.0f, 1.0f, 0.0f, 1.0f);
	//output.VolCoord = float4(0, 0, vertexID, 0);
	output.VolCoord = float4(0, 0, vertexID, 0);
	output.SliceIdx = vertexID;
	triStream.Append(output);
	output.SV_Pos = float4(1.0f, 1.0f, 0.0f, 1.0f);
	//output.VolCoord = float4(cb_i4RTReso.x, 0, vertexID, 0);
	output.VolCoord = float4(cb_i4RTReso.x, 0, vertexID, 0);
	output.SliceIdx = vertexID;
	triStream.Append(output);
	output.SV_Pos = float4(-1.0f, -1.0f, 0.0f, 1.0f);
	//output.VolCoord = float4(0, cb_i4RTReso.y, vertexID, 0);
	output.VolCoord = float4(0, cb_i4RTReso.y, vertexID, 0);
	output.SliceIdx = vertexID;
	triStream.Append(output);
	output.SV_Pos = float4(1.0f, -1.0f, 0.0f, 1.0f);
	//output.VolCoord = float4(cb_i4RTReso.x, cb_i4RTReso.y , vertexID, 0);
	output.VolCoord = float4(cb_i4RTReso.x, cb_i4RTReso.y, vertexID, 0);
	output.SliceIdx = vertexID;
	triStream.Append(output);
}
// GS for traversing the HP to generate MC case and output correspondent triangle
[maxvertexcount(15)]
void TraversalGS(point PassVS_OUT vertex[1], uint vertexID : SV_PrimitiveID, inout TriangleStream<ShadingPS_IN> triStream){
	uint4 p = uint4(0, 0, 0, 0);
		// now p is the idx of the pyramid top: 1^3 texture3d,
		// g_txHP9 is 2^3 texture3d object
#if VOXEL_NUM_X > 512
	PosInNextLevel(g_txHP9, vertexID, p);
#endif
#if VOXEL_NUM_X > 256
	PosInNextLevel(g_txHP8, vertexID, p);
#endif
#if VOXEL_NUM_X > 128
	PosInNextLevel(g_txHP7, vertexID, p);// now p is the idx of the pyramid 2th level: 2^3 texture3d
#endif
#if VOXEL_NUM_X > 64
	PosInNextLevel(g_txHP6, vertexID, p);// now p is the idx of the pyramid 3th level: 4^3 texture3d
#endif
#if VOXEL_NUM_X > 32
	PosInNextLevel(g_txHP5, vertexID, p);// now p is the idx of the pyramid 4th level: 8^3 texture3d
#endif
#if VOXEL_NUM_X > 16
	PosInNextLevel(g_txHP4, vertexID, p);// now p is the idx of the pyramid 5th level: 16^3 texture3d
#endif
#if VOXEL_NUM_X > 8
	PosInNextLevel(g_txHP3, vertexID, p);// now p is the idx of the pyramid 6th level: 32^3 texture3d
#endif
#if VOXEL_NUM_X > 4
	PosInNextLevel(g_txHP2, vertexID, p);// now p is the idx of the pyramid 7th level: 64^3 texture3d
#endif
	PosInNextLevel(g_txHP1, vertexID, p);// now p is the idx of the pyramid 8th level: 128^3 texture3d
	// Since in the base level the texel actully contain the caseID of that MC cube so 
	// we use another function to handle it
	PosInBaseLevel(g_txHP0, vertexID, p);// now p is the idx of the pyramid base level: 256^3 texture3d

	uint caseIdx = g_txHP0.Load(int4(p.xyz, 0));

	// Read the 8 corner of each MC cube
	float4 ept = float4(0,0,0,0);
	float4 fieldData[8] = { ept, ept, ept, ept, ept, ept, ept, ept} ;
	float3 fieldNormal[8];
	// p is int idx, so when convert it to normalized texture coordinate, we need to add half pixel in all dimensions
	float3 volTexCoord = float3(p.xyz + float3(0.5, 0.5, 0.5)) / cb_f4VolInfo.xyz;//-----------------------------------half pixel offset on z? attention!!!!!!!!!!!!!!!!
	float3 halfCube = 0.5f / cb_f4VolInfo.xyz;
	[unroll] for (int j = 0; j < 8; ++j){
		float3 idx = volTexCoord + halfCube * cb_halfCubeOffset[j];
		idx.y = 1.f - idx.y; // Convert from volume space to texture space
		fieldData[j].xyz = g_txColorVol.SampleLevel(g_samLinear, idx, 0).xyz;	// Color
		fieldData[j].w = g_txDensityVol.SampleLevel(g_samLinear, idx, 0).x * TRUNC_DIST; // Density
		if (abs(fieldData[j].w - INVALID_VALUE* TRUNC_DIST) < 1e-6) return;
		fieldNormal[j] = CalNormal(idx);
	}

	int polygonCount = cb_casePolyTable[caseIdx];// Find how many polygon need to be generated
	float3 pos = (p.xyz - 0.5*cb_f4VolInfo.xyz) * cb_f4HPMCInfo.w;

	VertexInfo v0, v1;
	int3 edges;
	int2 endPoints;
	for (int i = 0; i < polygonCount; ++i){
		edges = cb_triTable[caseIdx][i];
		endPoints = cb_edgeTable[edges.x];
		v0.Field = fieldData[endPoints.x];
		v0.Nor = fieldNormal[endPoints.x];
		v0.Pos = pos + cb_f4HPMCInfo.w * 0.5f * cb_halfCubeOffset[endPoints.x];
		v1.Field = fieldData[endPoints.y];
		v1.Nor = fieldNormal[endPoints.y];
		v1.Pos = pos + cb_f4HPMCInfo.w * 0.5f * cb_halfCubeOffset[endPoints.y];
		triStream.Append(CalIntersectionVertex(v0, v1));

		endPoints = cb_edgeTable[edges.y];
		v0.Field = fieldData[endPoints.x];
		v0.Nor = fieldNormal[endPoints.x];
		v0.Pos = pos + cb_f4HPMCInfo.w * 0.5f * cb_halfCubeOffset[endPoints.x];
		v1.Field = fieldData[endPoints.y];
		v1.Nor = fieldNormal[endPoints.y];
		v1.Pos = pos + cb_f4HPMCInfo.w * 0.5f * cb_halfCubeOffset[endPoints.y];
		triStream.Append(CalIntersectionVertex(v0, v1));

		endPoints = cb_edgeTable[edges.z];
		v0.Field = fieldData[endPoints.x];
		v0.Nor = fieldNormal[endPoints.x];
		v0.Pos = pos + cb_f4HPMCInfo.w * 0.5f * cb_halfCubeOffset[endPoints.x];
		v1.Field = fieldData[endPoints.y];
		v1.Nor = fieldNormal[endPoints.y];
		v1.Pos = pos + cb_f4HPMCInfo.w * 0.5f * cb_halfCubeOffset[endPoints.y];
		triStream.Append(CalIntersectionVertex(v0, v1));
		triStream.RestartStrip();
	}

}

// GS for traversing the HP to generate MC case and output correspondent triangle
[maxvertexcount(15)]
void TraversalAndOutGS(point PassVS_OUT vertex[1], uint vertexID : SV_PrimitiveID, inout TriangleStream<PosColGS_OUT> triStream){
	uint4 p = uint4(0, 0, 0, 0);
		// now p is the idx of the pyramid top: 1^3 texture3d,
		// g_txHP9 is 2^3 texture3d object
#if VOXEL_NUM_X > 512
		PosInNextLevel(g_txHP9, vertexID, p);
#endif
#if VOXEL_NUM_X > 256
	PosInNextLevel(g_txHP8, vertexID, p);
#endif
#if VOXEL_NUM_X > 128
	PosInNextLevel(g_txHP7, vertexID, p);// now p is the idx of the pyramid 2th level: 2^3 texture3d
#endif
#if VOXEL_NUM_X > 64
	PosInNextLevel(g_txHP6, vertexID, p);// now p is the idx of the pyramid 3th level: 4^3 texture3d
#endif
#if VOXEL_NUM_X > 32
	PosInNextLevel(g_txHP5, vertexID, p);// now p is the idx of the pyramid 4th level: 8^3 texture3d
#endif
#if VOXEL_NUM_X > 16
	PosInNextLevel(g_txHP4, vertexID, p);// now p is the idx of the pyramid 5th level: 16^3 texture3d
#endif
#if VOXEL_NUM_X > 8
	PosInNextLevel(g_txHP3, vertexID, p);// now p is the idx of the pyramid 6th level: 32^3 texture3d
#endif
#if VOXEL_NUM_X > 4
	PosInNextLevel(g_txHP2, vertexID, p);// now p is the idx of the pyramid 7th level: 64^3 texture3d
#endif
	PosInNextLevel(g_txHP1, vertexID, p);// now p is the idx of the pyramid 8th level: 128^3 texture3d
	// Since in the base level the texel actully contain the caseID of that MC cube so 
	// we use another function to handle it
	PosInBaseLevel(g_txHP0, vertexID, p);// now p is the idx of the pyramid base level: 256^3 texture3d

	uint caseIdx = g_txHP0.Load(int4(p.xyz, 0));

	// Read the 8 corner of each MC cube
	float4 ept = float4(0, 0, 0, 0);
	float4 fieldData[8] = { ept, ept, ept, ept, ept, ept, ept, ept };
	
	// p is int idx, so when convert it to normalized texture coordinate, we need to add half pixel in all dimensions
	float3 volTexCoord = float3(p.xyz + float3(0.5, 0.5, 0.5)) / cb_f4VolInfo.xyz;//-----------------------------------half pixel offset on z? attention!!!!!!!!!!!!!!!!
	float3 halfCube = 0.5f / cb_f4VolInfo.xyz;
	[unroll] for (int j = 0; j < 8; ++j){
		float3 idx = volTexCoord + halfCube * cb_halfCubeOffset[j];
		idx.y = 1.f - idx.y;// Convert from volume space to texture space
		fieldData[j].xyz = g_txColorVol.SampleLevel(g_samLinear, idx, 0).xyz;	// Color
		fieldData[j].w = g_txDensityVol.SampleLevel(g_samLinear, idx, 0).x* TRUNC_DIST; // Density
		if (abs(fieldData[j].w - INVALID_VALUE * TRUNC_DIST) < 1e-8) return;
	}

	int polygonCount = cb_casePolyTable[caseIdx];// Find how many polygon need to be generated
	float3 pos = (p.xyz - 0.5*cb_f4VolInfo.xyz) * cb_f4HPMCInfo.w;

	VertexInfoNoNor v0, v1;
	int3 edges;
	int2 endPoints;
	for (int i = 0; i < polygonCount; ++i){
		edges = cb_triTable[caseIdx][i];
		endPoints = cb_edgeTable[edges.x];
		v0.Field = fieldData[endPoints.x];
		v0.Pos = pos + cb_f4HPMCInfo.w * 0.5f * cb_halfCubeOffset[endPoints.x];
		v1.Field = fieldData[endPoints.y];
		v1.Pos = pos + cb_f4HPMCInfo.w * 0.5f * cb_halfCubeOffset[endPoints.y];
		triStream.Append(InterSec(v0, v1));

		endPoints = cb_edgeTable[edges.y];
		v0.Field = fieldData[endPoints.x];
		v0.Pos = pos + cb_f4HPMCInfo.w * 0.5f * cb_halfCubeOffset[endPoints.x];
		v1.Field = fieldData[endPoints.y];
		v1.Pos = pos + cb_f4HPMCInfo.w * 0.5f * cb_halfCubeOffset[endPoints.y];
		triStream.Append(InterSec(v0, v1));

		endPoints = cb_edgeTable[edges.z];
		v0.Field = fieldData[endPoints.x];
		v0.Pos = pos + cb_f4HPMCInfo.w * 0.5f * cb_halfCubeOffset[endPoints.x];
		v1.Field = fieldData[endPoints.y];
		v1.Pos = pos + cb_f4HPMCInfo.w * 0.5f * cb_halfCubeOffset[endPoints.y];
		triStream.Append(InterSec(v0, v1));
		triStream.RestartStrip();
	}
}



//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------
uint HPMCBasePS(SliceGS_OUT input) : SV_Target{
	float4 fieldData[8];				// To store the field data at each voxel's 8 corner point
	float3 halfCube = 0.5 / cb_f4VolInfo.xyz;
		[unroll]
	for (int i = 0; i < 8; ++i){
		float3 uv = input.VolCoord.xyz + halfCube * cb_halfCubeOffset[i];
		uv.y = 1.f - uv.y;
		fieldData[i].w = g_txDensityVol.Sample(g_samLinear, uv).x;
	}
	uint caseIdx = (uint(fieldData[7].w > cb_f4VolInfo.w) << 7) | (uint(fieldData[6].w > cb_f4VolInfo.w) << 6) |
		(uint(fieldData[5].w > cb_f4VolInfo.w) << 5) | (uint(fieldData[4].w > cb_f4VolInfo.w) << 4) |
		(uint(fieldData[3].w > cb_f4VolInfo.w) << 3) | (uint(fieldData[2].w > cb_f4VolInfo.w) << 2) |
		(uint(fieldData[1].w > cb_f4VolInfo.w) << 1) | (uint(fieldData[0].w > cb_f4VolInfo.w));
	if (caseIdx == 0 || caseIdx == 255) return 0;
	return caseIdx;
	//return (uint)1;
}

uint ReductionBasePS(SliceGS_OUT input) : SV_Target{
	uint sum = 0;
	[unroll]
	for (int i = 0; i < 8; i++){
		// Here if I don't to half pixel deoffset, then after multiplication, my uv will be incorrect!!
		int4 idx = (input.VolCoord - float4( 0.5, 0.5, 0, 0)) * 2 + float4(0.5, 0.5, 0.5, 0) + int4(cb_QuadrantOffset[i], 0);
			sum += g_txHPLayer.Load(idx) == 0 ? 0 : 1;
		//sum += g_txHPLayer.Load(int4(input.VolCoord * 2), cb_QuadrantOffset[i]) == 0 ? 1 : 1;
	}
	return sum;
}

uint ReductionPS(SliceGS_OUT input) : SV_Target{
	uint sum = 0;
	[unroll]
	for (int i = 0; i < 8; i++){
		// Here if I don't to half pixel deoffset, then after multiplication, my uv will be incorrect!!
		int4 idx = (input.VolCoord - float4(0.5, 0.5, 0, 0)) * 2 + float4(0.5, 0.5, 0.5, 0) + int4(cb_QuadrantOffset[i], 0);
			sum += g_txHPLayer.Load(idx);;
		//sum += g_txHPLayer.Load( int4(input.VolCoord*2), cb_QuadrantOffset[i] );
	}
	return sum;
}

PS_3_OUT GenerateRGBDPS(ShadingPS_IN input) : SV_Target
{
	PS_3_OUT output;
	
	// Shading
	float3 color = input.Col.rgb;

	float4 light_pos = light_offset + cb_f4ViewPos;

	// shading part
	float4 light_dir = light_pos - input.Pos_o;
	float light_dist = length(light_dir);
	light_dir /= light_dist;
	light_dir.w = clamp(1.0f / (light_attenuation.x +
						light_attenuation.y * light_dist +
						light_attenuation.z * light_dist * light_dist), 0, 1);
	float angleAttn = clamp(dot(-input.Nor, light_dir.xyz), 0, 1 );
	float3 col = color * light_dir.w * angleAttn + ambient;
	
	output.Normal = float4(input.Nor*0.5 + 0.5,1);
	output.Depth = float4(input.Col.rgb, input.Pos_o.z); 
	output.Image = float4(col, 1);
	return output;
}

float4 RenderPS(ShadingPS_IN input) : SV_Target
{
	// Shading
	float3 color = input.Col.rgb;

	float4 light_pos = light_offset + cb_f4ViewPos;

	// shading part
	float4 light_dir = light_pos - input.Pos_o;
	float light_dist = length(light_dir);
	light_dir /= light_dist;
	light_dir.w = clamp(1.0f / (light_attenuation.x +
						light_attenuation.y * light_dist +
						light_attenuation.z * light_dist * light_dist), 0, 1);
	float angleAttn = clamp(dot(-input.Nor, light_dir.xyz), 0, 1);
	float3 col = color * light_dir.w * angleAttn + ambient;

	return float4(col,1);
}