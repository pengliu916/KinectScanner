//--------------------------------------------------------------------------------------
// GPU algorithm for parallel reduction see 
// http://channel9.msdn.com/Blogs/gclassy/DirectCompute-Lecture-Series-210-GPU-Optimizations-and-Performance 
// for more detail
//--------------------------------------------------------------------------------------
#include "header.h"
//--------------------------------------------------------------------------------------
// Structures
//--------------------------------------------------------------------------------------
struct InterData
{
	float4 f4DataElm0;//cxcx,cxcy,cxcz,cxnx
	float4 f4DataElm1;//cxny,cxnz,cycy,cycz
	float4 f4DataElm2;//cynx,cyny,cynz,czcz
	float4 f4DataElm3;//cznx,czny,cznz,nxnx
	float4 f4DataElm4;//nxny,nxnz,nyny,nynz
	float4 f4DataElm5;//nznz,cxpqn,cypqn,czpqn
	float4 f4DataElm6;//nxpqn,nypqn,nzpqn,NumOfPair
};

RWStructuredBuffer<InterData> g_idata : register(u0);

groupshared InterData sdata[THREAD1D];

cbuffer cbPerFrame :register(b0){
	uint4 cb_u4ElmCount;
};
//--------------------------------------------------------------------------------------
// Compute Shader
//--------------------------------------------------------------------------------------
[numthreads(THREAD1D,1,1)]
void CS(uint3 threadIdx : SV_GroupThreadID, uint3 groupIdx : SV_GroupID)
{
	// each thread loads one element from global to share memory
	uint tid = threadIdx.x;
	uint i = groupIdx.x * THREAD1D*2 + threadIdx.x;
	if(i+THREAD1D<cb_u4ElmCount.x){
		sdata[tid].f4DataElm0 = g_idata[i].f4DataElm0 + g_idata[i + THREAD1D].f4DataElm0;
		sdata[tid].f4DataElm1 = g_idata[i].f4DataElm1 + g_idata[i + THREAD1D].f4DataElm1;
		sdata[tid].f4DataElm2 = g_idata[i].f4DataElm2 + g_idata[i + THREAD1D].f4DataElm2;
		sdata[tid].f4DataElm3 = g_idata[i].f4DataElm3 + g_idata[i + THREAD1D].f4DataElm3;
		sdata[tid].f4DataElm4 = g_idata[i].f4DataElm4 + g_idata[i + THREAD1D].f4DataElm4;
		sdata[tid].f4DataElm5 = g_idata[i].f4DataElm5 + g_idata[i + THREAD1D].f4DataElm5;
		sdata[tid].f4DataElm6 = g_idata[i].f4DataElm6 + g_idata[i + THREAD1D].f4DataElm6;
	} else if (i<cb_u4ElmCount.x){
		sdata[tid] = g_idata[i];
	} else{
		sdata[tid].f4DataElm0 = float4(0.f,0.f,0.f,0.f);
		sdata[tid].f4DataElm1 = float4(0.f, 0.f, 0.f, 0.f);
		sdata[tid].f4DataElm2 = float4(0.f, 0.f, 0.f, 0.f);
		sdata[tid].f4DataElm3 = float4(0.f, 0.f, 0.f, 0.f);
		sdata[tid].f4DataElm4 = float4(0.f, 0.f, 0.f, 0.f);
		sdata[tid].f4DataElm5 = float4(0.f, 0.f, 0.f, 0.f);
		sdata[tid].f4DataElm6 = float4(0.f, 0.f, 0.f, 0.f);
	}
	GroupMemoryBarrierWithGroupSync();

	// do reduction in shared memory
	for(uint s=THREAD1D/2;s>0;s>>=1){
		if(tid<s){
			sdata[tid].f4DataElm0 += sdata[tid + s].f4DataElm0;
			sdata[tid].f4DataElm1 += sdata[tid + s].f4DataElm1;
			sdata[tid].f4DataElm2 += sdata[tid + s].f4DataElm2;
			sdata[tid].f4DataElm3 += sdata[tid + s].f4DataElm3;
			sdata[tid].f4DataElm4 += sdata[tid + s].f4DataElm4;
			sdata[tid].f4DataElm5 += sdata[tid + s].f4DataElm5;
			sdata[tid].f4DataElm6 += sdata[tid + s].f4DataElm6;
		}
		GroupMemoryBarrierWithGroupSync();
	}
	if(tid==0) g_idata[groupIdx.x] = sdata[0];
}