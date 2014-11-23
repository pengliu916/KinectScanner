// Need to be improved
#pragma once
#include <D3D11.h>
#include <DirectXMath.h>
#include "DXUT.h"
#include "SDKmisc.h"

#include "header.h"

using namespace DirectX;

struct InterData
{
	XMFLOAT4 f4Data0;//cxcx,cxcy,cxcz,cxnx
	XMFLOAT4 f4Data1;//cxny,cxnz,cycy,cycz
	XMFLOAT4 f4Data2;//cynx,cyny,cynz,czcz
	XMFLOAT4 f4Data3;//cznx,czny,cznz,nxnx
	XMFLOAT4 f4Data4;//nxny,nxnz,nyny,nynz
	XMFLOAT4 f4Data5;//nznz,cxpqn,cypqn,czpqn
	XMFLOAT4 f4Data6;//nxpqn,nypqn,nzpqn,NumOfPair
};

struct CB_PerFrame
{
	XMUINT4 u4ElmCount;
};

class Reduction 
{
public:
	// compute shader for reduction
	ID3D11ComputeShader*	m_pCS;
	// constant buffer for holding element count
	CB_PerFrame				m_CBperFrame;
	ID3D11Buffer*			m_pCBperFrame;
	// CPU readable buffer for final result
	ID3D11Buffer*			m_pSumForCPU;
	// variable for final result
	InterData				m_structSum;

	Reduction()
	{
	}

	HRESULT CreateResource(ID3D11Device* pd3dDevice)
	{
		HRESULT hr=S_OK;
		
		ID3DBlob* pCSBlob = NULL;
		V_RETURN(DXUTCompileFromFile(L"Reduction.fx", nullptr, "CS", "cs_5_0", COMPILE_FLAG, 0, &pCSBlob));
		V_RETURN(pd3dDevice->CreateComputeShader(pCSBlob->GetBufferPointer(), pCSBlob->GetBufferSize(), NULL, &m_pCS));
		DXUT_SetDebugName(m_pCS, "m_pCS");

		// create constant buffer
		D3D11_BUFFER_DESC bd = { 0 };
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		bd.CPUAccessFlags = 0;
		bd.ByteWidth = sizeof(CB_PerFrame);
		V_RETURN(pd3dDevice->CreateBuffer(&bd, NULL, &m_pCBperFrame));
		DXUT_SetDebugName(m_pCBperFrame, "m_pCBperFrame");

		// create CPU readable buffer
		D3D11_BUFFER_DESC BUFDesc;
		ZeroMemory(&BUFDesc, sizeof(BUFDesc));
		BUFDesc.BindFlags = 0;
		BUFDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		BUFDesc.MiscFlags = 0;
		BUFDesc.StructureByteStride = sizeof(InterData);
		BUFDesc.Usage = D3D11_USAGE_STAGING;
		BUFDesc.ByteWidth = sizeof(InterData);
		V_RETURN(pd3dDevice->CreateBuffer(&BUFDesc, nullptr, &m_pSumForCPU));
		DXUT_SetDebugName(m_pSumForCPU, "m_pSumForCPU");
	
		return hr;
	}

	~Reduction()
	{

	}

	void Release()
	{
		SAFE_RELEASE(m_pCS);
		SAFE_RELEASE(m_pSumForCPU);
		SAFE_RELEASE(m_pCBperFrame);
	}

	void Processing(ID3D11DeviceContext* pd3dImmediateContext, 
					ID3D11UnorderedAccessView* uav[], 
					ID3D11Buffer* buf[],UINT elementCount)
	{
		DXUT_BeginPerfEvent(DXUT_PERFEVENTCOLOR, L"Reduction");
		pd3dImmediateContext->CSSetShader( m_pCS, NULL, 0 );
		UINT initCount = 0;
		pd3dImmediateContext->CSSetUnorderedAccessViews(0,8,uav,&initCount);
		pd3dImmediateContext->CSSetConstantBuffers(0,1,&m_pCBperFrame);

		UINT uThreadGroup = elementCount;
		do{
			m_CBperFrame.u4ElmCount.x = uThreadGroup;
			pd3dImmediateContext->UpdateSubresource(m_pCBperFrame, 0, NULL, &m_CBperFrame, 0, 0);
			uThreadGroup = ceil((float)uThreadGroup / THREAD1D /2);
			pd3dImmediateContext->Dispatch(uThreadGroup, 1, 1);
		}while(uThreadGroup>1);

		ID3D11UnorderedAccessView* ppUAViewNULL[8] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
		pd3dImmediateContext->CSSetUnorderedAccessViews(0, 8, ppUAViewNULL, &initCount);

		D3D11_BOX region;
		region.front = 0;
		region.top = 0;
		region.back = 1;
		region.bottom = 1;
		region.left = 0;
		region.right = sizeof(XMFLOAT4);
		for(int i=0;i<7;i++){
			pd3dImmediateContext->CopySubresourceRegion(m_pSumForCPU,0,i*sizeof(XMFLOAT4),0,0,buf[i],0,&region);
		}
		D3D11_MAPPED_SUBRESOURCE subresource;
		pd3dImmediateContext->Map(m_pSumForCPU, D3D11CalcSubresource(0, 0, 1), D3D11_MAP_READ, 0, &subresource);
		m_structSum = *reinterpret_cast<InterData*>(subresource.pData);
		pd3dImmediateContext->Unmap(m_pSumForCPU, D3D11CalcSubresource(0, 0, 1));

		DXUT_EndPerfEvent();
	}
};