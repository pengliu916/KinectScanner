#include "header.h"
#include "D3DX_DXGIFormatConvert.inl"

//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
Texture3D<float2> g_srvDepthWeight : register(t0);
Texture3D<float4> g_srvColor : register(t1);
Texture3D<int> g_srvCellFront : register(t2);
Texture3D<int> g_srvCellBack : register(t3);

Texture2D<float4> g_srvFarNear : register(t4);

SamplerState g_samLinear : register(s0);

// Phong shading variable
static const float4 g_cf4LightOffset = float4 (0.f,0.f,0.f,0.f);
static const float4 g_cf4Ambient = float4 (0.1f,0.1f,0.1f,1.f);
static const float4 g_cf4LightAtte = float4 (1,0,0,0);

static const float3 g_cf3InvVoxelReso = float3(1.f / VOXEL_NUM_X,1.f / VOXEL_NUM_Y,1.f / VOXEL_NUM_Z);
static const float3 g_cf3VoxelReso = float3(VOXEL_NUM_X,VOXEL_NUM_Y,VOXEL_NUM_Z);
static const float2 g_cf2DepthF = float2(F_X,-F_Y);
static const float2 g_cf2DepthC = float2(C_X,C_Y);

static const uint g_cuCellVolSliceStep = VOXEL_NUM_X * VOXEL_NUM_Y / CELLRATIO / CELLRATIO;
static const uint g_cuCellVolStripStep = VOXEL_NUM_X / CELLRATIO;
static const float3 g_cf3HalfCellSize = float3(VOXEL_SIZE*CELLRATIO,VOXEL_SIZE*CELLRATIO,VOXEL_SIZE*CELLRATIO)*0.5f;

static const int3 cb_QuadrantOffset[8] =
{
	int3(0,0,0),
	int3(1,0,0),
	int3(0,1,0),
	int3(1,1,0),
	int3(0,0,1),
	int3(1,0,1),
	int3(0,1,1),
	int3(1,1,1),
};
//--------------------------------------------------------------------------------------
// Const buffers
//--------------------------------------------------------------------------------------
cbuffer cbInit : register(b0)
{
	float cb_fTruncDist;// not referenced 
	float3 cb_f3VolHalfSize;//  = volumeRes * voxelSize / 2
	float cb_fStepLen;// step length in meters of ray casting algorithm
	float3 cb_f3InvVolSize;// = 1 / ( volumeRes * voxelSize )
	float3 cb_f3VolBBMin;// Volume bonding box front left bottom pos in world space
	bool cb_bKinectShade;// Whether shading from Kinect's point of view
	float3 cb_f3VolBBMax;// Volume bonding box back right top pos in world space
	float NIU1;
	float2 cb_f2RTReso;
	float2 NIU2;
};
cbuffer cbKinectUpdate : register(b1)
{
	float4 cb_f4KinectPos;// Kinect's pos in world space
	matrix cb_mInvKinectWorld;// Kinect's view transform matrix,the inversion of cb_mKinectWorld
	matrix cb_mKinectWorld;// Matrix which transform Kinect from ( 0,0,0 ) to its current pos and orientation
	matrix cb_mKinectViewProj;
};
cbuffer cbFreeCamUpdate : register(b2)
{
	matrix cb_mInvView;
	float4 cb_f4ViewPos;
};

//--------------------------------------------------------------------------------------
// Utility structure
//--------------------------------------------------------------------------------------
struct Ray
{
	float4 o;
	float4 d;
};

struct MarchingResult
{
	float4 RGBD;
	float4 Normal;
};

//--------------------------------------------------------------------------------------
// Utility Functions
//--------------------------------------------------------------------------------------
bool IntersectBox(Ray r,float3 BoxMin,float3 BoxMax,out float tnear,out float tfar)
{
	// compute intersection of ray with all six bbox planes
	float3 invR = 1.0 / r.d.xyz;
		float3 tbot = invR * (BoxMin.xyz - r.o.xyz);
		float3 ttop = invR * (BoxMax.xyz - r.o.xyz);

		// re-order intersections to find smallest and largest on each axis
		float3 tmin = min(ttop,tbot);
		float3 tmax = max(ttop,tbot);

		// find the largest tmin and the smallest tmax
		float2 t0 = max(tmin.xx,tmin.yz);
		tnear = max(t0.x,t0.y);
	t0 = min(tmax.xx,tmax.yz);
	tfar = min(t0.x,t0.y);

	return tnear <= tfar;
}

// 'uv' is f3 in texture space,variable 'value' contain the interpolated sample result
// the function will return false is any of neighbor sample point contian INVALID_VALUE
bool SampleDW(float3 uv,inout float2 value,int3 offset = int3(0,0,0))
{
	// count for the half pixel offset
	float3 fIdx = uv - g_cf3InvVoxelReso *0.5f;
		float neighbor[8];
	float2 sum = 0;
		// return Invalid value if the sample point is in voxel which contain invalid value;
		[unroll]for (int i = 0;i<8;i++){
		neighbor[i] = g_srvDepthWeight.SampleLevel(g_samLinear,fIdx,0,cb_QuadrantOffset[i]+offset).x;
		if (neighbor[i] - INVALID_VALUE < EPSILON) return false;
		else sum += neighbor[i];
	}
	value = sum / 8.f;
	return true;
}

float2 LoadDW(float3 uv)
{
	uint3 idx = floor(uv * g_cf3VoxelReso);
	return g_srvDepthWeight[idx];
}

//--------------------------------------------------------------------------------------
// Shader I/O structure
//--------------------------------------------------------------------------------------
struct GS_INPUT{};

struct PS_INPUT
{
	float4	projPos : SV_POSITION;
	float4	Pos : TEXCOORD0;
};

struct MarchPS_INPUT
{
	float4 projPos: SV_Position;
	float4 Pos: NORMAL0;
	float2 Tex: TEXCOORD0;
};

struct PS_3_OUT
{
	float4 RGBD : SV_TARGET0;
	float4 Normal : SV_TARGET1;
	float DepthOut : SV_Depth;
};

struct PS_fDepth_OUT
{
	float4 RGBD : SV_Target;
	float Depth : SV_Depth;
};

//--------------------------------------------------------------------------------------
//  Vertex Shaders
//--------------------------------------------------------------------------------------
/* Pass through VS */
GS_INPUT VS()
{
	GS_INPUT output = (GS_INPUT)0;
	return output;
}

//--------------------------------------------------------------------------------------
//  Geometry Shaders
//--------------------------------------------------------------------------------------
/*GS for rendering the NearFar tex,expand cube geometries for all active cell*/
[maxvertexcount(18)]
void ActiveCellAndSOGS(point GS_INPUT particles[1],uint primID : SV_PrimitiveID,inout TriangleStream<PS_INPUT> triStream)
{
	// calculate the idx for access the cell volume
	uint3 u3Idx;
	u3Idx.z = primID / g_cuCellVolSliceStep;
	u3Idx.y = (primID % g_cuCellVolSliceStep) / g_cuCellVolStripStep;
	u3Idx.x = primID % g_cuCellVolStripStep;

	// check whether the current cell is active,discard the cell if inactive
	int front = g_srvCellFront[u3Idx];
	int back = g_srvCellBack[u3Idx];

	if(back*front!=-1)return;

	// calculate the offset vector to move (0,0,0,1) to current cell center
	float4 f4CellCenterOffset = float4((u3Idx - g_cf3VoxelReso * 0.5f / CELLRATIO+0.5f) * VOXEL_SIZE * CELLRATIO,0.f);

	PS_INPUT output;
	// get half cell size to expand the cell center point to full cell cube geometry
	float4 f4HalfCellSize = float4(g_cf3HalfCellSize,1.f);
	// first expand centered cube with edge length 2 to actual cell size in world space
	// then move to it's right pos in world space,and store the world space position in it's Pos component.
	// after that,transform into view space and then project into image plane,and store image space 4D coord in it's projPos component
	output.Pos=float4(1.f,1.f,1.f,1.f)*f4HalfCellSize+f4CellCenterOffset;output.projPos=mul(output.Pos,cb_mKinectViewProj);triStream.Append(output);
	output.Pos=float4(1.f,-1.f,1.f,1.f)*f4HalfCellSize+f4CellCenterOffset;output.projPos=mul(output.Pos,cb_mKinectViewProj);triStream.Append(output);
	output.Pos=float4(1.f,1.f,-1.f,1.f)*f4HalfCellSize+f4CellCenterOffset;output.projPos=mul(output.Pos,cb_mKinectViewProj);triStream.Append(output);
	output.Pos=float4(1.f,-1.f,-1.f,1.f)*f4HalfCellSize+f4CellCenterOffset;output.projPos=mul(output.Pos,cb_mKinectViewProj);triStream.Append(output);
	output.Pos=float4(-1.f,1.f,-1.f,1.f)*f4HalfCellSize+f4CellCenterOffset;output.projPos=mul(output.Pos,cb_mKinectViewProj);triStream.Append(output);
	output.Pos=float4(-1.f,-1.f,-1.f,1.f)*f4HalfCellSize+f4CellCenterOffset;output.projPos=mul(output.Pos,cb_mKinectViewProj);triStream.Append(output);
	output.Pos=float4(-1.f,1.f,1.f,1.f)*f4HalfCellSize+f4CellCenterOffset;output.projPos=mul(output.Pos,cb_mKinectViewProj);triStream.Append(output);
	output.Pos=float4(-1.f,-1.f,1.f,1.f)*f4HalfCellSize+f4CellCenterOffset;output.projPos=mul(output.Pos,cb_mKinectViewProj);triStream.Append(output);
	output.Pos=float4(1.f,1.f,1.f,1.f)*f4HalfCellSize+f4CellCenterOffset;output.projPos=mul(output.Pos,cb_mKinectViewProj);triStream.Append(output);
	output.Pos=float4(1.f,-1.f,1.f,1.f)*f4HalfCellSize+f4CellCenterOffset;output.projPos=mul(output.Pos,cb_mKinectViewProj);triStream.Append(output);
	triStream.RestartStrip();

	output.Pos=float4(1.f,1.f,1.f,1.f)*f4HalfCellSize+f4CellCenterOffset;output.projPos=mul(output.Pos,cb_mKinectViewProj);triStream.Append(output);
	output.Pos=float4(1.f,1.f,-1.f,1.f)*f4HalfCellSize+f4CellCenterOffset;output.projPos=mul(output.Pos,cb_mKinectViewProj);triStream.Append(output);
	output.Pos=float4(-1.f,1.f,1.f,1.f)*f4HalfCellSize+f4CellCenterOffset;output.projPos=mul(output.Pos,cb_mKinectViewProj);triStream.Append(output);
	output.Pos=float4(-1.f,1.f,-1.f,1.f)*f4HalfCellSize+f4CellCenterOffset;output.projPos=mul(output.Pos,cb_mKinectViewProj);triStream.Append(output);
	triStream.RestartStrip();

	output.Pos=float4(1.f,-1.f,-1.f,1.f)*f4HalfCellSize+f4CellCenterOffset;output.projPos=mul(output.Pos,cb_mKinectViewProj);triStream.Append(output);
	output.Pos=float4(1.f,-1.f,1.f,1.f)*f4HalfCellSize+f4CellCenterOffset;output.projPos=mul(output.Pos,cb_mKinectViewProj);triStream.Append(output);
	output.Pos=float4(-1.f,-1.f,-1.f,1.f)*f4HalfCellSize+f4CellCenterOffset;output.projPos=mul(output.Pos,cb_mKinectViewProj);triStream.Append(output);
	output.Pos=float4(-1.f,-1.f,1.f,1.f)*f4HalfCellSize+f4CellCenterOffset;output.projPos=mul(output.Pos,cb_mKinectViewProj);triStream.Append(output);

}

/*GS for rendering the active cells as cube wireframe*/
[maxvertexcount(24)]
void Debug_CellGS(point GS_INPUT particles[1],uint primID : SV_PrimitiveID,inout LineStream<PS_INPUT> lineStream)
{
	// calculate the idx for access the cell volume
	uint3 u3Idx;
	u3Idx.z = primID / g_cuCellVolSliceStep;
	u3Idx.y = (primID % g_cuCellVolSliceStep) / g_cuCellVolStripStep;
	u3Idx.x = primID % g_cuCellVolStripStep;

	// check whether the current cell is active,discard the cell if inactive
	int front = g_srvCellFront[u3Idx];
	int back = g_srvCellBack[u3Idx];

	if (back*front != -1)return;

	// calculate the offset vector to move (0,0,0,1) to current cell center
	float4 f4CellCenterOffset = float4((u3Idx - g_cf3VoxelReso * 0.5f / CELLRATIO+0.5f) * VOXEL_SIZE * CELLRATIO,0.f);

	PS_INPUT output;
	// get half cell size to expand the cell center point to full cell cube geometry
	float4 f4HalfCellSize = float4(g_cf3HalfCellSize,1.f);
	// first expand centered cube with edge length 2 to actual cell size in world space
	// then move to it's right pos in world space,and store the world space position in it's Pos component.
	// after that,transform into view space and then project into image plane,and store image space 4D coord in it's projPos component
	output.Pos = float4(-1.f,-1.f,-1.f,1.f)*f4HalfCellSize+f4CellCenterOffset;output.projPos = mul(output.Pos,cb_mKinectViewProj);lineStream.Append(output);
	output.Pos = float4(-1.f,1.f,-1.f,1.f)*f4HalfCellSize+f4CellCenterOffset;output.projPos = mul(output.Pos,cb_mKinectViewProj);lineStream.Append(output);
	output.Pos = float4(1.f,1.f,-1.f,1.f)*f4HalfCellSize+f4CellCenterOffset;output.projPos = mul(output.Pos,cb_mKinectViewProj);lineStream.Append(output);
	output.Pos = float4(1.f,-1.f,-1.f,1.f)*f4HalfCellSize+f4CellCenterOffset;output.projPos = mul(output.Pos,cb_mKinectViewProj);lineStream.Append(output);
	output.Pos = float4(-1.f,-1.f,-1.f,1.f)*f4HalfCellSize+f4CellCenterOffset;output.projPos = mul(output.Pos,cb_mKinectViewProj);lineStream.Append(output);
	lineStream.RestartStrip();
	output.Pos = float4(-1.f,-1.f,1.f,1.f)*f4HalfCellSize+f4CellCenterOffset;output.projPos = mul(output.Pos,cb_mKinectViewProj);lineStream.Append(output);
	output.Pos = float4(-1.f,1.f,1.f,1.f)*f4HalfCellSize+f4CellCenterOffset;output.projPos = mul(output.Pos,cb_mKinectViewProj);lineStream.Append(output);
	output.Pos = float4(1.f,1.f,1.f,1.f)*f4HalfCellSize+f4CellCenterOffset;output.projPos = mul(output.Pos,cb_mKinectViewProj);lineStream.Append(output);
	output.Pos = float4(1.f,-1.f,1.f,1.f)*f4HalfCellSize+f4CellCenterOffset;output.projPos = mul(output.Pos,cb_mKinectViewProj);lineStream.Append(output);
	output.Pos = float4(-1.f,-1.f,1.f,1.f)*f4HalfCellSize+f4CellCenterOffset;output.projPos = mul(output.Pos,cb_mKinectViewProj);lineStream.Append(output);
	lineStream.RestartStrip();

	output.Pos = float4(-1.f,-1.f,1.f,1.f)*f4HalfCellSize+f4CellCenterOffset;output.projPos = mul(output.Pos,cb_mKinectViewProj);lineStream.Append(output);
	output.Pos = float4(-1.f,-1.f,-1.f,1.f)*f4HalfCellSize+f4CellCenterOffset;output.projPos = mul(output.Pos,cb_mKinectViewProj);lineStream.Append(output);
	lineStream.RestartStrip();
	output.Pos = float4(-1.f,1.f,1.f,1.f)*f4HalfCellSize+f4CellCenterOffset;output.projPos = mul(output.Pos,cb_mKinectViewProj);lineStream.Append(output);
	output.Pos = float4(-1.f,1.f,-1.f,1.f)*f4HalfCellSize+f4CellCenterOffset;output.projPos = mul(output.Pos,cb_mKinectViewProj);lineStream.Append(output);
	lineStream.RestartStrip();
	output.Pos = float4(1.f,1.f,1.f,1.f)*f4HalfCellSize+f4CellCenterOffset;output.projPos = mul(output.Pos,cb_mKinectViewProj);lineStream.Append(output);
	output.Pos = float4(1.f,1.f,-1.f,1.f)*f4HalfCellSize+f4CellCenterOffset;output.projPos = mul(output.Pos,cb_mKinectViewProj);lineStream.Append(output);
	lineStream.RestartStrip();
	output.Pos = float4(1.f,-1.f,1.f,1.f)*f4HalfCellSize+f4CellCenterOffset;output.projPos = mul(output.Pos,cb_mKinectViewProj);lineStream.Append(output);
	output.Pos = float4(1.f,-1.f,-1.f,1.f)*f4HalfCellSize+f4CellCenterOffset;output.projPos = mul(output.Pos,cb_mKinectViewProj);lineStream.Append(output);
}

 /*GS for ray marching,this will generate a fullscreen quad represent the projection plan of Kinect sensor in world space*/
[maxvertexcount(4)]
void RaymarchGS(point GS_INPUT particles[1],inout TriangleStream<MarchPS_INPUT> triStream)
{
	MarchPS_INPUT output;
	output.Pos = float4((float2(0,0) - g_cf2DepthC) / g_cf2DepthF,1.f,1.f);// view space 3D pos
	output.Pos = mul(output.Pos,cb_mKinectWorld);// world space 3D pos
	output.projPos = float4(-1.f,1.f,0.f,1.f);
	output.Tex = float2(0.f,0.f);
	triStream.Append(output);

	output.Pos = float4((float2(D_W,0) - g_cf2DepthC) / g_cf2DepthF,1.f,1.f);// view space 3D pos
	output.Pos = mul(output.Pos,cb_mKinectWorld);// world space 3D pos
	output.projPos = float4(1.f,1.f,0.f,1.f);
	output.Tex = float2(D_W,0.f);
	triStream.Append(output);

	output.Pos = float4((float2(0,D_H) - g_cf2DepthC) / g_cf2DepthF,1.f,1.f);// view space 3D pos
	output.Pos = mul(output.Pos,cb_mKinectWorld);// world space 3D pos
	output.projPos = float4(-1.f,-1.f,0.f,1.f);
	output.Tex = float2(0.f,D_H);
	triStream.Append(output);

	output.Pos = float4((float2(D_W,D_H) - g_cf2DepthC) / g_cf2DepthF,1.f,1.f);// view space 3D pos
	output.Pos = mul(output.Pos,cb_mKinectWorld);// world space 3D pos
	output.projPos = float4(1.f,-1.f,0.f,1.f);
	output.Tex = float2(D_W,D_H);
	triStream.Append(output);
}

//--------------------------------------------------------------------------------------
//  Pixel Shaders
//--------------------------------------------------------------------------------------
/*PS for rendering the NearFar Texture for raymarching*/
//[Warning] keep this PS short since during alpha blending, each pixel will be shaded multiple times
float4 FarNearPS(PS_INPUT input) : SV_Target
{
	// each pixel only output it's pos(view space!!)'s z component 
	// as output for red channel: for smallest depth
	// as output for alpha channel: for largest depth
	// this is achieved by using alpha blending which treat color and alpha value differently
	// during alpha blending, output merge only keep the smallest value for color component
	// while OM only keep the largest value for alpha component
	return float4(input.projPos.w,1.f,input.projPos.w,input.projPos.w);
}
/*PS for debug active cell, this will render each active cell as hollow cube wireframe */
PS_fDepth_OUT Debug_CellPS(PS_INPUT input)
{
	PS_fDepth_OUT output;
	output.RGBD = float4(0.133f,0.691f,0.297f,1.f)*0.8f;
	output.Depth = input.projPos.w/10.f; // specifically output to depth buffer for correctly depth culling
	return output;
}

/* PS for extracting RGBZ,normal,shaded image from volume */
PS_3_OUT RaymarchPS(MarchPS_INPUT input)
{
	PS_3_OUT output;
	// initial the default value for no surface intersection
	output.RGBD = float4 ( 0,0,0,-1 );
	output.Normal = float4 ( 0,0,0,-1 );
	output.DepthOut = 1000.f;

	// read the raymarching start t(tNear) and end t(tFar) from the precomputed FarNear tex
	int3 i3Idx = int3(input.Tex,0);
	float4 resu = g_srvFarNear.Load(i3Idx);// now resu hold depth near and depth far in view space

	float3 nearPos = float3((input.Tex - float2(D_W, D_H)*0.5f) / float2(F_X, F_Y) * resu.r, resu.r);
	float3 farPos = float3((input.Tex - float2(D_W, D_H)*0.5f) / float2(F_X, F_Y) * resu.a, resu.a);
	
	resu.ra = sqrt(float2(dot(nearPos,nearPos),dot(farPos,farPos)));//now resu hold t near and t far in view space

	float2 f2NearFar = resu.ra;
	// for debug purpose
	/*output.RGBD = float4(1,1,1,1)*(f2NearFar.y - f2NearFar.x)*0.5f;
	if(resu.g > 0.2) output.RGBD.b = 1;
	output.DepthOut = 0;
	return output;*/

	//if(resu.g < 0.5) return output;



	// initial the pixel ray's data structure in world space
	Ray eyeray;
	eyeray.o = cb_f4KinectPos;//world space
	eyeray.d = input.Pos - eyeray.o;
	eyeray.d = float4(normalize(eyeray.d.xyz),0);//world space

	// calculate ray intersection with bounding box
	float tnear,tfar;
	// eyeray.o + eyeray.d*tnear is the near intersection point in the world space
	// eyeray.o + eyeray.d*tfar is the far intersection point in the world space
	bool hit = IntersectBox(eyeray,cb_f3VolBBMin,cb_f3VolBBMax,tnear,tfar);
	if (!hit) return output;
	if (tnear < 0) tnear = 0;
	//f2NearFar = float2(tnear,tfar);



	// calculate intersection points and convert to texture space
	/* if eyeray.o is in range [-halfVolSize,halfVolSize]
	 * then f3Porg should be in range [(0,0,0),(1,1,1)]
	 * is valid pos in texture space*/
	float3 f3Porg = eyeray.o.xyz * cb_f3InvVolSize+0.5f;
	float3 f3P = f3Porg;
	float3 f3P_pre;

	// to get near intersection point P = eyeray.o + t*eyeray.d
	// t is the distance from eyeray.o in world/view space(not in texture space!!)
	// since eyeray.d is unit vector;
	float t = f2NearFar.x;
	


	// convert normalized step vector into texture space
	float3 f3EyerayDirInTspace = eyeray.d.xyz * cb_f3InvVolSize;

	// read the first value: x: dist/TRUNC_DIST
	float2 f2IntersectResult = LoadDW(f3P);

	// initialize depth pre and cur(dist to nearest surface)
	float fDist_cur = f2IntersectResult.x*TRUNC_DIST;
	float fDist_pre = fDist_cur;

	// distance for each step in view/world space
	float fStepDistance = cb_fStepLen;
	// ray marching
	[loop]while (t<f2NearFar.y){
		// recored previous footprint,and move one step in the volume
		f3P_pre = f3P;
		fDist_pre = fDist_cur;
		// move one step further
		t += fStepDistance;
		/* the following equation should be equale to
		 * f3P = eyeray.o*cb_f3InvVolSize + 0.5f + t*eyeray.d*cb_f3InvVolSize
		 *     = (eyeray.o + t*eyeray.d)*cb_f3InvVolSize + 0.5f
		 * which represents current sample point in texture space*/
		f3P = f3Porg+t*f3EyerayDirInTspace;
		// read depth and weight
		float2 f2DistWeight = LoadDW(f3P);
		// distance to nearest surface for current sample
		fDist_cur = f2DistWeight.x * TRUNC_DIST;
		/* to detect surface we need two continuous sample along the same eye ray
		 * surface only exist if and only if the current sample has negative distance 
		 * to the nearest surface, while the previous sample has postive distance to 
		 * the nearest surface, or vice versa. So we will find surface if 
		 * fDist_pre * fDist_cur < 0; But the TSDF volume also contain unknown voxel
		 * which has distance value as INVALID_VALUE(which is -1 by default), so this
		 * may cause false postive surface signal. Thus we require both sample must not
		 * hold INVALID_VALUE:
		 * fDist_cur > INVALID_VALUE*TRUNC_DIST
		 * fDist_pre > INVALID_VALUE*TRUNC_DIST*/
		if (fDist_cur > INVALID_VALUE*TRUNC_DIST && fDist_pre > INVALID_VALUE*TRUNC_DIST && fDist_pre * fDist_cur < 0){
			/* base on the two distances to the nearest surface, we can do interpolation
			 * to get the usb voxel position of the surface point, the following using
			 * pos in texture space*/
			float3 f3VolUVW = f3P_pre+(f3P - f3P_pre) * fDist_pre / (fDist_pre - fDist_cur);
			/* get and compute f3Normal, we need direvitive in three dimensions*/
			float2 f2Temp0 = float2(0.f,0.f);
			float2 f2Temp1 = float2(0.f,0.f);
			if (!SampleDW(f3VolUVW,f2Temp0,int3(1,0,0))) return output;
			if (!SampleDW(f3VolUVW,f2Temp1,int3(-1,0,0))) return output;
			float fDepth_dx = f2Temp0.x - f2Temp1.x;
			if (!SampleDW(f3VolUVW,f2Temp0,int3(0,1,0))) return output;
			if (!SampleDW(f3VolUVW,f2Temp1,int3(0,-1,0))) return output;
			float fDepth_dy = f2Temp0.x - f2Temp1.x;
			if (!SampleDW(f3VolUVW,f2Temp0,int3(0,0,1))) return output;
			if (!SampleDW(f3VolUVW,f2Temp1,int3(0,0,-1))) return output;
			float fDepth_dz = f2Temp0.x - f2Temp1.x;
			// the following normal vector is in world space now
			float3 f3Normal = normalize(float3 (fDepth_dx,fDepth_dy,fDepth_dz));
			// convert the sub voxel surface intersection point into world space
			float4 f4SurfacePos = float4((f3VolUVW - 0.5f)*2.f * cb_f3VolHalfSize,1);
//[Bug Here?] convert the normal vector from world space into view space
			output.Normal = mul(float4(f3Normal,0.f),cb_mInvKinectWorld);
			// get color (maybe later I will use color to guide ICP)
			float4 f4Col = g_srvColor.SampleLevel(g_samLinear,f3VolUVW,0);
			// convert the sub voxel surface intersection point into view space to get the depth
			f4SurfacePos = mul(f4SurfacePos,cb_mInvKinectWorld);
			output.Normal = output.Normal* 0.5f+0.5f;
			// output RGBD
			output.RGBD = float4(f4Col.xyz, f4SurfacePos.z);
			output.DepthOut  = f4SurfacePos.z/10.f;
			return output;
		}
	}
	//output.RGBD = float4(0.05f,0.55f,0.05f,-1.f);
	return output;
}
