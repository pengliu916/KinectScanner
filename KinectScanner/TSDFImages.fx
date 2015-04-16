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
static const float4 g_cf4LightOffset = float4 (0.0f, 0.0f, 0.0f, 0.0f);
static const float4 g_cf4Ambient = float4 (0.1f, 0.1f, 0.1f, 1.0f);
static const float4 g_cf4LightAtte = float4 (1, 0, 0, 0);

static const float3 g_cf3InvVoxelReso = float3(1.f / VOXEL_NUM_X, 1.f / VOXEL_NUM_Y, 1.f / VOXEL_NUM_Z);
static const float3 g_cf3VoxelReso = float3(VOXEL_NUM_X, VOXEL_NUM_Y, VOXEL_NUM_Z);
static const float2 g_cf2DepthF = float2(F_X, -F_Y);
static const float2 g_cf2DepthC = float2(C_X, C_Y);

static const uint g_cuCellVolSliceStep = VOXEL_NUM_X * VOXEL_NUM_Y / CELLRATIO / CELLRATIO;
static const uint g_cuCellVolStripStep = VOXEL_NUM_X / CELLRATIO;

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
//--------------------------------------------------------------------------------------
// Const buffers
//--------------------------------------------------------------------------------------
cbuffer cbInit : register(b0)
{
	float cb_fTruncDist; // not referenced 
	float3 cb_f3VolHalfSize; //  = volumeRes * voxelSize / 2
	float cb_fStepLen; // step length in meters of ray casting algorithm
	float3 cb_f3InvVolSize; // = 1 / ( volumeRes * voxelSize )
	float3 cb_f3VolBBMin; // Volume bonding box front left bottom pos in world space
	bool cb_bKinectShade; // Whether shading from Kinect's point of view
	float3 cb_f3VolBBMax; // Volume bonding box back right top pos in world space
	float NIU1;
	float2 cb_f2RTReso;
	float2 NIU2;
};
cbuffer cbKinectUpdate : register(b1)
{
	float4 cb_f4KinectPos; // Kinect's pos in world space
	matrix cb_mInvKinectWorld; // Kinect's view transform matrix, the inversion of cb_mKinectWorld
	matrix cb_mKinectWorld; // Matrix which transform Kinect from ( 0, 0, 0 ) to its current pos and orientation
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
bool IntersectBox(Ray r, float3 BoxMin, float3 BoxMax, out float tnear, out float tfar)
{
	// compute intersection of ray with all six bbox planes
	float3 invR = 1.0 / r.d.xyz;
		float3 tbot = invR * (BoxMin.xyz - r.o.xyz);
		float3 ttop = invR * (BoxMax.xyz - r.o.xyz);

		// re-order intersections to find smallest and largest on each axis
		float3 tmin = min(ttop, tbot);
		float3 tmax = max(ttop, tbot);

		// find the largest tmin and the smallest tmax
		float2 t0 = max(tmin.xx, tmin.yz);
		tnear = max(t0.x, t0.y);
	t0 = min(tmax.xx, tmax.yz);
	tfar = min(t0.x, t0.y);

	return tnear <= tfar;
}

// 'uv' is f3 in texture space, variable 'value' contain the interpolated sample result
// the function will return false is any of neighbor sample point contian INVALID_VALUE
bool SampleDW(float3 uv, inout float2 value, int3 offset = int3(0, 0, 0))
{
	// count for the half pixel offset
	float3 fIdx = uv - g_cf3InvVoxelReso *0.5f;
		float neighbor[8];
	float2 sum = 0;
		// return Invalid value if the sample point is in voxel which contain invalid value;
		[unroll]for (int i = 0; i<8; i++){
		neighbor[i] = g_srvDepthWeight.SampleLevel(g_samLinear, fIdx, 0, cb_QuadrantOffset[i] + offset).x;
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
	float4 Depth : SV_TARGET0;
	float4 Normal : SV_TARGET1;
	float DepthOut : SV_Depth;
};

struct PS_Depth_OUT
{
	float4 RGBD : SV_Target;
	float Depth : SV_Depth;
};
//--------------------------------------------------------------------------------------
// Shaders
//--------------------------------------------------------------------------------------
/* Pass through VS */
GS_INPUT VS()
{
	GS_INPUT output = (GS_INPUT)0;
	return output;
}

/*GS for rendering the volume on screen ------------texVolume Read, no half pixel correction*/
[maxvertexcount(18)]
void ActiveCellAndSOGS(point GS_INPUT particles[1], uint primID : SV_PrimitiveID, inout TriangleStream<PS_INPUT> triStream)
{
	// calculate the idx for access the cell volume
	uint3 u3Idx;
	u3Idx.z = primID / g_cuCellVolSliceStep;
	u3Idx.y = (primID % g_cuCellVolSliceStep) / g_cuCellVolStripStep;
	u3Idx.x = primID % g_cuCellVolStripStep;

	// check whether the current cell is active, discard the cell if inactive
	int front = g_srvCellFront[u3Idx];
	int back = g_srvCellBack[u3Idx];

	if(back*front!=-1)return;
	//if(!all(D3DX_R16G16_UINT_to_UINT2(g_srvCell[u3Idx]))) return;

	float4 f4CellCenterOffset = float4((u3Idx - g_cf3VoxelReso * 0.5f / CELLRATIO + 0.5f) * VOXEL_SIZE * CELLRATIO,0.f);

	PS_INPUT output;
	float4 f4HalfCellSize = float4(VOXEL_SIZE*CELLRATIO, VOXEL_SIZE*CELLRATIO, VOXEL_SIZE*CELLRATIO,2.f)*0.5f;
		f4HalfCellSize.xyz*=1.3;
	output.Pos=float4(1.0f,1.0f,1.0f,1.0f)*f4HalfCellSize+f4CellCenterOffset;output.projPos=mul(output.Pos,cb_mKinectViewProj);triStream.Append(output);
	output.Pos=float4(1.0f,-1.0f,1.0f,1.0f)*f4HalfCellSize+f4CellCenterOffset;output.projPos=mul(output.Pos,cb_mKinectViewProj);triStream.Append(output);
	output.Pos=float4(1.0f,1.0f,-1.0f,1.0f)*f4HalfCellSize+f4CellCenterOffset;output.projPos=mul(output.Pos,cb_mKinectViewProj);triStream.Append(output);
	output.Pos=float4(1.0f,-1.0f,-1.0f,1.0f)*f4HalfCellSize+f4CellCenterOffset;output.projPos=mul(output.Pos,cb_mKinectViewProj);triStream.Append(output);
	output.Pos=float4(-1.0f,1.0f,-1.0f,1.0f)*f4HalfCellSize+f4CellCenterOffset;output.projPos=mul(output.Pos,cb_mKinectViewProj);triStream.Append(output);
	output.Pos=float4(-1.0f,-1.0f,-1.0f,1.0f)*f4HalfCellSize+f4CellCenterOffset;output.projPos=mul(output.Pos,cb_mKinectViewProj);triStream.Append(output);
	output.Pos=float4(-1.0f,1.0f,1.0f,1.0f)*f4HalfCellSize+f4CellCenterOffset;output.projPos=mul(output.Pos,cb_mKinectViewProj);triStream.Append(output);
	output.Pos=float4(-1.0f,-1.0f,1.0f,1.0f)*f4HalfCellSize+f4CellCenterOffset;output.projPos=mul(output.Pos,cb_mKinectViewProj);triStream.Append(output);
	output.Pos=float4(1.0f,1.0f,1.0f,1.0f)*f4HalfCellSize+f4CellCenterOffset;output.projPos=mul(output.Pos,cb_mKinectViewProj);triStream.Append(output);
	output.Pos=float4(1.0f,-1.0f,1.0f,1.0f)*f4HalfCellSize+f4CellCenterOffset;output.projPos=mul(output.Pos,cb_mKinectViewProj);triStream.Append(output);
	triStream.RestartStrip();

	output.Pos=float4(1.0f,1.0f,1.0f,1.0f)*f4HalfCellSize+f4CellCenterOffset;output.projPos=mul(output.Pos,cb_mKinectViewProj);triStream.Append(output);
	output.Pos=float4(1.0f,1.0f,-1.0f,1.0f)*f4HalfCellSize+f4CellCenterOffset;output.projPos=mul(output.Pos,cb_mKinectViewProj);triStream.Append(output);
	output.Pos=float4(-1.0f,1.0f,1.0f,1.0f)*f4HalfCellSize+f4CellCenterOffset;output.projPos=mul(output.Pos,cb_mKinectViewProj);triStream.Append(output);
	output.Pos=float4(-1.0f,1.0f,-1.0f,1.0f)*f4HalfCellSize+f4CellCenterOffset;output.projPos=mul(output.Pos,cb_mKinectViewProj);triStream.Append(output);
	triStream.RestartStrip();

	output.Pos=float4(1.0f,-1.0f,-1.0f,1.0f)*f4HalfCellSize+f4CellCenterOffset;output.projPos=mul(output.Pos,cb_mKinectViewProj);triStream.Append(output);
	output.Pos=float4(1.0f,-1.0f,1.0f,1.0f)*f4HalfCellSize+f4CellCenterOffset;output.projPos=mul(output.Pos,cb_mKinectViewProj);triStream.Append(output);
	output.Pos=float4(-1.0f,-1.0f,-1.0f,1.0f)*f4HalfCellSize+f4CellCenterOffset;output.projPos=mul(output.Pos,cb_mKinectViewProj);triStream.Append(output);
	output.Pos=float4(-1.0f,-1.0f,1.0f,1.0f)*f4HalfCellSize+f4CellCenterOffset;output.projPos=mul(output.Pos,cb_mKinectViewProj);triStream.Append(output);

}

/*GS for rendering the active cells*/
[maxvertexcount(24)]
void Debug_CellGS(point GS_INPUT particles[1], uint primID : SV_PrimitiveID, inout LineStream<PS_INPUT> lineStream)
{
	// calculate the idx for access the cell volume
	uint3 u3Idx;
	u3Idx.z = primID / g_cuCellVolSliceStep;
	u3Idx.y = (primID % g_cuCellVolSliceStep) / g_cuCellVolStripStep;
	u3Idx.x = primID % g_cuCellVolStripStep;

	// check whether the current cell is active, discard the cell if inactive
	int front = g_srvCellFront[u3Idx];
	int back = g_srvCellBack[u3Idx];

	if (back*front != -1)return;
	//if(!all(D3DX_R16G16_UINT_to_UINT2(g_srvCell[u3Idx]))) return;

	float4 f4CellCenterOffset = float4((u3Idx - g_cf3VoxelReso * 0.5f / CELLRATIO + 0.5f) * VOXEL_SIZE * CELLRATIO, 0.f);

	PS_INPUT output;
	float4 f4HalfCellSize = float4(VOXEL_SIZE*CELLRATIO, VOXEL_SIZE*CELLRATIO, VOXEL_SIZE*CELLRATIO, 2.f)*0.5f;
	output.Pos = float4(-1.0f, -1.0f, -1.0f, 1.0f)*f4HalfCellSize + f4CellCenterOffset; output.projPos = mul(output.Pos, cb_mKinectViewProj); lineStream.Append(output);
	output.Pos = float4(-1.0f, 1.0f, -1.0f, 1.0f)*f4HalfCellSize + f4CellCenterOffset; output.projPos = mul(output.Pos, cb_mKinectViewProj); lineStream.Append(output);
	output.Pos = float4(1.0f, 1.0f, -1.0f, 1.0f)*f4HalfCellSize + f4CellCenterOffset; output.projPos = mul(output.Pos, cb_mKinectViewProj); lineStream.Append(output);
	output.Pos = float4(1.0f, -1.0f, -1.0f, 1.0f)*f4HalfCellSize + f4CellCenterOffset; output.projPos = mul(output.Pos, cb_mKinectViewProj); lineStream.Append(output);
	output.Pos = float4(-1.0f, -1.0f, -1.0f, 1.0f)*f4HalfCellSize + f4CellCenterOffset; output.projPos = mul(output.Pos, cb_mKinectViewProj); lineStream.Append(output);
	lineStream.RestartStrip();
	output.Pos = float4(-1.0f, -1.0f, 1.0f, 1.0f)*f4HalfCellSize + f4CellCenterOffset; output.projPos = mul(output.Pos, cb_mKinectViewProj); lineStream.Append(output);
	output.Pos = float4(-1.0f, 1.0f, 1.0f, 1.0f)*f4HalfCellSize + f4CellCenterOffset; output.projPos = mul(output.Pos, cb_mKinectViewProj); lineStream.Append(output);
	output.Pos = float4(1.0f, 1.0f, 1.0f, 1.0f)*f4HalfCellSize + f4CellCenterOffset; output.projPos = mul(output.Pos, cb_mKinectViewProj); lineStream.Append(output);
	output.Pos = float4(1.0f, -1.0f, 1.0f, 1.0f)*f4HalfCellSize + f4CellCenterOffset; output.projPos = mul(output.Pos, cb_mKinectViewProj); lineStream.Append(output);
	output.Pos = float4(-1.0f, -1.0f, 1.0f, 1.0f)*f4HalfCellSize + f4CellCenterOffset; output.projPos = mul(output.Pos, cb_mKinectViewProj); lineStream.Append(output);
	lineStream.RestartStrip();

	output.Pos = float4(-1.0f, -1.0f, 1.0f, 1.0f)*f4HalfCellSize + f4CellCenterOffset; output.projPos = mul(output.Pos, cb_mKinectViewProj); lineStream.Append(output);
	output.Pos = float4(-1.0f, -1.0f, -1.0f, 1.0f)*f4HalfCellSize + f4CellCenterOffset; output.projPos = mul(output.Pos, cb_mKinectViewProj); lineStream.Append(output);
	lineStream.RestartStrip();
	output.Pos = float4(-1.0f, 1.0f, 1.0f, 1.0f)*f4HalfCellSize + f4CellCenterOffset; output.projPos = mul(output.Pos, cb_mKinectViewProj); lineStream.Append(output);
	output.Pos = float4(-1.0f, 1.0f, -1.0f, 1.0f)*f4HalfCellSize + f4CellCenterOffset; output.projPos = mul(output.Pos, cb_mKinectViewProj); lineStream.Append(output);
	lineStream.RestartStrip();
	output.Pos = float4(1.0f, 1.0f, 1.0f, 1.0f)*f4HalfCellSize + f4CellCenterOffset; output.projPos = mul(output.Pos, cb_mKinectViewProj); lineStream.Append(output);
	output.Pos = float4(1.0f, 1.0f, -1.0f, 1.0f)*f4HalfCellSize + f4CellCenterOffset; output.projPos = mul(output.Pos, cb_mKinectViewProj); lineStream.Append(output);
	lineStream.RestartStrip();
	output.Pos = float4(1.0f, -1.0f, 1.0f, 1.0f)*f4HalfCellSize + f4CellCenterOffset; output.projPos = mul(output.Pos, cb_mKinectViewProj); lineStream.Append(output);
	output.Pos = float4(1.0f, -1.0f, -1.0f, 1.0f)*f4HalfCellSize + f4CellCenterOffset; output.projPos = mul(output.Pos, cb_mKinectViewProj); lineStream.Append(output);
}

 /*GS for ray marching*/
[maxvertexcount(4)]
void RaymarchGS(point GS_INPUT particles[1], inout TriangleStream<MarchPS_INPUT> triStream)
{
	MarchPS_INPUT output;
	output.Pos = float4((float2(0,0) - g_cf2DepthC) / g_cf2DepthF, 1.f, 1.f);
	output.Pos = mul(output.Pos, cb_mKinectWorld);
	output.projPos = float4(-1.0f, 1.0f, 0.0f, 1.0f);
	output.Tex = float2(0.0f, 0.0f);
	triStream.Append(output);

	output.Pos = float4((float2(D_W, 0) - g_cf2DepthC) / g_cf2DepthF, 1.f, 1.f);
	output.Pos = mul(output.Pos, cb_mKinectWorld);
	output.projPos = float4(1.0f, 1.0f, 0.0f, 1.0f);
	output.Tex = float2(D_W, 0.0f);
	triStream.Append(output);

	output.Pos = float4((float2(0, D_H) - g_cf2DepthC) / g_cf2DepthF, 1.f, 1.f);
	output.Pos = mul(output.Pos, cb_mKinectWorld);
	output.projPos = float4(-1.0f, -1.0f, 0.0f, 1.0f);
	output.Tex = float2(0.0f, D_H);
	triStream.Append(output);

	output.Pos = float4((float2(D_W, D_H) - g_cf2DepthC) / g_cf2DepthF, 1.f, 1.f);
	output.Pos = mul(output.Pos, cb_mKinectWorld);
	output.projPos = float4(1.0f, -1.0f, 0.0f, 1.0f);
	output.Tex = float2(D_W,D_H);
	triStream.Append(output);
}

/*PS*/
float4 FarNearPS(PS_INPUT input) : SV_Target
{
	return float4(input.projPos.w, 1.f, input.projPos.w, input.projPos.w);
	//return float4(1,1,1,1);
}
/*PS for debug active cell */
PS_Depth_OUT Debug_CellPS(PS_INPUT input)
{
	PS_Depth_OUT output;
	output.RGBD = float4(0.133f,0.691f,0.297f,1.f)*0.8f;
	output.Depth = input.projPos.w/10.f;
	return output;
}

/* PS for extracting RGBZ, normal, shaded image from volume */
PS_3_OUT RaymarchPS(MarchPS_INPUT input)
{
	PS_3_OUT output;
	output.Depth = float4 ( 0, 0, 0, -1 );
	output.Normal = float4 ( 0, 0, 0, -1 );
	output.DepthOut = 1000.f;


	Ray eyeray;
	//world space
	eyeray.o = cb_f4KinectPos;
	eyeray.d = input.Pos - eyeray.o;

#if NEW_METHOD
	// calculate ray intersection with bounding box
	int3 i3Idx = int3(input.Tex,0);
	float4 resu = g_srvFarNear.Load(i3Idx);

	float2 f2NearFar = resu.ra;
	/*output.Depth = float4(1,1,1,1)*(f2NearFar.y - f2NearFar.x)*0.5f;
	if(resu.g > 0.2) output.Depth.b = 1;
	return output;*/

	//if(resu.g < 0.5) return output;
	f2NearFar = f2NearFar*length(eyeray.d.xyz);
#else
	// calculate ray intersection with bounding box
	float tnear, tfar;
	bool hit = IntersectBox(eyeray, cb_f3VolBBMin, cb_f3VolBBMax, tnear, tfar);
	if (!hit) return output;
	if (tnear < 0) tnear = 0;
	float2 f2NearFar = float2(tnear,tfar);
#endif

	eyeray.d = float4(normalize(eyeray.d.xyz),0);

	// calculate intersection points and convert to texture space
	float3 f3Porg = (eyeray.o.xyz + eyeray.d.xyz * f2NearFar.x) * cb_f3InvVolSize + 0.5f;
	float3 f3P = f3Porg;
	float3 f3P_pre;
	// ray length
	float t = f2NearFar.x;
	
	float3 f3Step = eyeray.d.xyz * cb_f3InvVolSize;

	// read the first value
	float2 f2IntersectResult = LoadDW(f3P);

	// initialize depth pre and cur
	float fDist_cur = f2IntersectResult.x*TRUNC_DIST;
	float fDist_pre = fDist_cur;

	// ray marching
	float fTstep = cb_fStepLen;
	[loop]while (t<f2NearFar.y){
		// recored previous footprint, and move one step in the volume
		f3P_pre = f3P;
		fDist_pre = fDist_cur;
		t += fTstep;
		f3P = f3Porg + (t - f2NearFar.x)*f3Step;
		// read depth and weight
		float2 f2DistWeight = LoadDW(f3P);
		fDist_cur = f2DistWeight.x * TRUNC_DIST;
		if (fDist_cur > INVALID_VALUE*TRUNC_DIST && fDist_pre > INVALID_VALUE*TRUNC_DIST && fDist_pre * fDist_cur < 0){
			// get sub voxel txCoord
			float3 f3VolUVW = f3P_pre + (f3P - f3P_pre) * fDist_pre / (fDist_pre - fDist_cur); 
			// get and compute f3Normal
			float2 temp0 = float2(0.0f, 0.0f);
			float2 temp1 = float2(0.0f, 0.0f);
			if (!SampleDW(f3VolUVW, temp0, int3(1, 0, 0))) return output;
			if (!SampleDW(f3VolUVW, temp1, int3(-1, 0, 0))) return output;
			float depth_dx = temp0.x - temp1.x;
			if (!SampleDW(f3VolUVW, temp0, int3(0, 1, 0))) return output;
			if (!SampleDW(f3VolUVW, temp1, int3(0, -1, 0))) return output;
			float depth_dy = temp0.x - temp1.x;
			if (!SampleDW(f3VolUVW, temp0, int3(0, 0, 1))) return output;
			if (!SampleDW(f3VolUVW, temp1, int3(0, 0, -1))) return output;
			float depth_dz = temp0.x - temp1.x;
			// the normal is calculated and stored in view space
			float3 f3Normal = normalize(float3 (depth_dx, depth_dy, depth_dz));

			float4 f4SurfacePos = float4((f3VolUVW - 0.5f)*2.f * cb_f3VolHalfSize,1);
			output.Normal = mul(float4(f3Normal, 0.f), cb_mInvKinectWorld);
			// get color (maybe later I will use color to guide ICP)
			float4 f4Col = g_srvColor.SampleLevel(g_samLinear, f3VolUVW, 0);

				// the light is calculated in view space, since the normal is in view space
				float4 f4LightPos = mul(g_cf4LightOffset + float4 ( 0.f, 0.f, 0.f, 1.f ), cb_mInvView); 
				float4 f4LightDir = f4LightPos - f4SurfacePos;
				float fLightDist = length ( f4LightDir );
				f4LightDir /= fLightDist;
				f4LightDir.w = clamp ( 1.0f / ( g_cf4LightAtte.x + 
									 g_cf4LightAtte.y * fLightDist + 
									 g_cf4LightAtte.z * fLightDist * fLightDist ), 0.f, 1.f );
				//float fNdotL = clamp(dot(f3Normal, f4LightDir.xyz), 0, 1);
				float fNdotL = abs(dot(f3Normal, f4LightDir.xyz));
				f4Col = f4Col * (f4LightDir.w * fNdotL + g_cf4Ambient);


			f4SurfacePos = mul(f4SurfacePos, cb_mInvKinectWorld);
			output.Normal = output.Normal* 0.5f + 0.5f;
			// output RGBD
			output.Depth = float4(f4Col.xyz, f4SurfacePos.z);
			output.DepthOut  = f4SurfacePos.z/10.f;
			return output;
		}
	}
	//output.RGBD = float4(0.05f, 0.05f, 0.05f, -1.f);
	return output;
}
