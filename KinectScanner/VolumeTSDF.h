// Need to be improved
#pragma once
#include <D3D11.h>
#include <DirectXMath.h>
#include "DXUT.h"
#include "DXUTcamera.h"
#include "SDKmisc.h"

#include "header.h"

#include "TransPCLs.h"

using namespace DirectX;

struct CB_VolumeTSDF_PerCall
{
	float halfWidth;
	float halfHeight;
	float halfDepth;
	float voxelSize;
	float truncDist;
	float maxWeight;
	int NotInUse0;
	int NotInUse1;
};

struct CB_VolumeTSDF_PerFrame
{
	XMMATRIX mInversedWorld_kinect;
};

class VolumeTSDF
{
public:
	ID3D11ComputeShader*			m_pCS;
	ID3D11ComputeShader*			m_pResetCS;
	ID3D11ComputeShader*			m_pRefreshCellCS;

	CB_VolumeTSDF_PerCall			m_CBperCall;
	ID3D11Buffer*					m_pCBperCall;

	CB_VolumeTSDF_PerFrame			m_CBperFrame;
	ID3D11Buffer*					m_pCBperFrame;
	// Volume for Depth and Weight data
	ID3D11Texture3D*				m_pDWVolumeTex;
	ID3D11ShaderResourceView*		m_pDWVolumeSRV;
	ID3D11UnorderedAccessView*		m_pDWVolumeUAV;
	// Volume for subVolume cell used for object-ordered empty space skipping
	ID3D11Texture3D*				m_pBrickVolTex[2];
	ID3D11ShaderResourceView*		m_pBrickVolSRV[2];
	ID3D11UnorderedAccessView*		m_pBrickVolUAV[2];
	// Volume for Color data
	ID3D11Texture3D*				m_pColVolumeTex;
	ID3D11ShaderResourceView*		m_pColVolumeSRV;
	ID3D11UnorderedAccessView*		m_pColVolumeUAV;

	TransformedPointClould*			m_pInputPC;

	bool							m_bResetVol;

	VolumeTSDF( float voxelSize, UINT width, UINT height, UINT depth )
	{
		m_CBperCall.voxelSize = voxelSize;
		m_CBperCall.halfWidth = width / 2.f;
		m_CBperCall.halfHeight = height / 2.f;
		m_CBperCall.halfDepth = depth / 2.f;
		m_CBperCall.truncDist = XMMax( 0.001f, 2.1f * voxelSize ); 
		m_CBperCall.maxWeight = MAX_WEIGHT;

		m_bResetVol = true;
	}

	HRESULT CreateResource( ID3D11Device* pd3dDevice, TransformedPointClould* inputTCP )
	{
		HRESULT hr = S_OK;

		ID3DBlob* pCSBlob = NULL;
		V_RETURN(DXUTCompileFromFile(L"VolumeTSDF.hlsl", nullptr, "CS", "cs_5_0", COMPILE_FLAG, 0, &pCSBlob));
		V_RETURN(pd3dDevice->CreateComputeShader(pCSBlob->GetBufferPointer(), pCSBlob->GetBufferSize(), NULL, &m_pCS));
		DXUT_SetDebugName(m_pCS, "m_pCS");
		V_RETURN(DXUTCompileFromFile(L"VolumeTSDF.hlsl", nullptr, "ResetCS", "cs_5_0", COMPILE_FLAG, 0, &pCSBlob));
		V_RETURN(pd3dDevice->CreateComputeShader(pCSBlob->GetBufferPointer(), pCSBlob->GetBufferSize(), NULL, &m_pResetCS));
		DXUT_SetDebugName(m_pResetCS, "m_pResetCS");
		V_RETURN(DXUTCompileFromFile(L"VolumeTSDF.hlsl", nullptr, "RefreshCellCS", "cs_5_0", COMPILE_FLAG, 0, &pCSBlob));
		V_RETURN(pd3dDevice->CreateComputeShader(pCSBlob->GetBufferPointer(), pCSBlob->GetBufferSize(), NULL, &m_pRefreshCellCS));
		DXUT_SetDebugName(m_pRefreshCellCS, "m_pRefreshCellCS");
		pCSBlob->Release();

		D3D11_BUFFER_DESC bd;
		ZeroMemory( &bd, sizeof(bd) );
		// Create the constant buffers
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		bd.CPUAccessFlags = 0    ;
		bd.ByteWidth = sizeof(CB_VolumeTSDF_PerCall);
		V_RETURN(pd3dDevice->CreateBuffer( &bd, NULL, &m_pCBperCall ));
		DXUT_SetDebugName( m_pCBperCall, "m_pCBperCall");

		bd.ByteWidth = sizeof(CB_VolumeTSDF_PerFrame);
		V_RETURN(pd3dDevice->CreateBuffer( &bd, NULL, &m_pCBperFrame ));
		DXUT_SetDebugName( m_pCBperFrame, "m_pCBperFrame");


		// Create the texture
		D3D11_TEXTURE3D_DESC dstex;
		dstex.Width = (UINT)m_CBperCall.halfWidth * 2.f;
		dstex.Height = (UINT)m_CBperCall.halfHeight * 2.f;
		dstex.Depth = (UINT)m_CBperCall.halfDepth * 2.f;
		dstex.MipLevels = 1;
		dstex.Format = DXGI_FORMAT_R16G16_TYPELESS;// the texture file should have 32bit typeless format
		dstex.Usage = D3D11_USAGE_DEFAULT;
		dstex.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		dstex.CPUAccessFlags = 0;
		dstex.MiscFlags = 0;
		V_RETURN(pd3dDevice->CreateTexture3D(&dstex, NULL, &m_pDWVolumeTex));
		DXUT_SetDebugName(m_pDWVolumeTex, "m_pDWVolumeTex");
		dstex.Format = DXGI_FORMAT_R10G10B10A2_TYPELESS;// the texture file should have 32bit typeless format
		V_RETURN(pd3dDevice->CreateTexture3D(&dstex, NULL, &m_pColVolumeTex));
		DXUT_SetDebugName(m_pColVolumeTex, "m_pColVolumeTex");
		dstex.Width /= CELLRATIO;
		dstex.Height /= CELLRATIO;
		dstex.Depth /= CELLRATIO;
		dstex.Format = DXGI_FORMAT_R8_SINT;// the texture file should have 32bit typeless format
		V_RETURN(pd3dDevice->CreateTexture3D(&dstex, NULL, &m_pBrickVolTex[0]));
		DXUT_SetDebugName(m_pBrickVolTex[0], "m_pBrickVolTex[0]");
		V_RETURN(pd3dDevice->CreateTexture3D(&dstex, NULL, &m_pBrickVolTex[1]));
		DXUT_SetDebugName(m_pBrickVolTex[1], "m_pBrickVolTex[1]");

		// Create the resource view
		D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
		ZeroMemory(&SRVDesc, sizeof(SRVDesc));
		SRVDesc.Format = DXGI_FORMAT_R16G16_FLOAT;// the srv cannot have typeless formate, the format should match with the conversion func in pixel shader
		SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
		SRVDesc.Texture3D.MostDetailedMip = 0;
		SRVDesc.Texture3D.MipLevels = 1;
		V_RETURN(pd3dDevice->CreateShaderResourceView(m_pDWVolumeTex, &SRVDesc, &m_pDWVolumeSRV));
		DXUT_SetDebugName(m_pDWVolumeSRV, "m_pDWVolumeSRV");
		SRVDesc.Format = DXGI_FORMAT_R8_SINT;
		V_RETURN(pd3dDevice->CreateShaderResourceView(m_pBrickVolTex[0], &SRVDesc, &m_pBrickVolSRV[0]));
		DXUT_SetDebugName(m_pBrickVolSRV[0], "m_pBrickVolSRV[0]");
		V_RETURN(pd3dDevice->CreateShaderResourceView(m_pBrickVolTex[1], &SRVDesc, &m_pBrickVolSRV[1]));
		DXUT_SetDebugName(m_pBrickVolSRV[1], "m_pBrickVolSRV[1]");
		SRVDesc.Format = DXGI_FORMAT_R10G10B10A2_UNORM;// the srv cannot have typeless formate, the format should match with the conversion func in pixel shader
		V_RETURN(pd3dDevice->CreateShaderResourceView(m_pColVolumeTex, &SRVDesc, &m_pColVolumeSRV));
		DXUT_SetDebugName(m_pColVolumeSRV, "m_pColVolumeSRV");

		D3D11_UNORDERED_ACCESS_VIEW_DESC UAVDesc;
		ZeroMemory(&UAVDesc, sizeof(UAVDesc));
		UAVDesc.Format = DXGI_FORMAT_R32_UINT; // the UAV must have this format to allow simultaneous read and write
		UAVDesc.Texture3D.FirstWSlice = 0;
		UAVDesc.Texture3D.MipSlice = 0;
		UAVDesc.Texture3D.WSize = (UINT)m_CBperCall.halfDepth * 2.f;
		UAVDesc.ViewDimension = D3D11_UAV_DIMENSION::D3D11_UAV_DIMENSION_TEXTURE3D;
		V_RETURN(pd3dDevice->CreateUnorderedAccessView(m_pDWVolumeTex, &UAVDesc, &m_pDWVolumeUAV));
		DXUT_SetDebugName(m_pDWVolumeUAV, "m_pDWVolumeUAV");
		V_RETURN(pd3dDevice->CreateUnorderedAccessView(m_pColVolumeTex, &UAVDesc, &m_pColVolumeUAV));
		DXUT_SetDebugName(m_pColVolumeUAV, "m_pColVolumeUAV");
		UAVDesc.Format = DXGI_FORMAT_R8_SINT;
		UAVDesc.Texture3D.WSize = (UINT)m_CBperCall.halfDepth * 2.f / CELLRATIO;
		V_RETURN(pd3dDevice->CreateUnorderedAccessView(m_pBrickVolTex[0], &UAVDesc, &m_pBrickVolUAV[0]));
		DXUT_SetDebugName(m_pBrickVolUAV[0], "m_pBrickVolUAV[0]");
		V_RETURN(pd3dDevice->CreateUnorderedAccessView(m_pBrickVolTex[1], &UAVDesc, &m_pBrickVolUAV[1]));
		DXUT_SetDebugName(m_pBrickVolUAV[1], "m_pBrickVolUAV[1]");


		m_pInputPC = inputTCP;

		ID3D11DeviceContext* pd3dImmediateContext = DXUTGetD3D11DeviceContext();
		pd3dImmediateContext->UpdateSubresource( m_pCBperCall, 0, NULL, &m_CBperCall, 0, 0 );

		return hr;
	}
	void ClearVolume( ID3D11DeviceContext* pd3dImmediateContext )
	{
		DXUT_BeginPerfEvent(DXUT_PERFEVENTCOLOR, L"Clean Volume");
		// clear the render target
		pd3dImmediateContext->CSSetShader(m_pResetCS, NULL, 0);
		UINT initCounts = 0;
		ID3D11UnorderedAccessView* uavs[4] = { m_pDWVolumeUAV, m_pColVolumeUAV, m_pBrickVolUAV[0], m_pBrickVolUAV[1]};
		pd3dImmediateContext->CSSetUnorderedAccessViews(0, 4, uavs, &initCounts);
		pd3dImmediateContext->Dispatch(VOXEL_NUM_X / THREAD_X, VOXEL_NUM_Y / THREAD_Y, VOXEL_NUM_Z / THREAD_Z);

		pd3dImmediateContext->CSSetShader(m_pRefreshCellCS, NULL, 0);
		pd3dImmediateContext->Dispatch(VOXEL_NUM_X / CELLRATIO / THREAD_X, VOXEL_NUM_Y / CELLRATIO / THREAD_Y, VOXEL_NUM_Z / CELLRATIO / THREAD_Z);

		m_bResetVol = false;
		DXUT_EndPerfEvent();
	}

	void Release()
	{
		SAFE_RELEASE(m_pCS);
		SAFE_RELEASE(m_pResetCS);
		SAFE_RELEASE(m_pRefreshCellCS);

		SAFE_RELEASE( m_pDWVolumeTex );
		SAFE_RELEASE( m_pDWVolumeSRV );
		SAFE_RELEASE( m_pDWVolumeUAV );

		SAFE_RELEASE( m_pColVolumeTex );
		SAFE_RELEASE( m_pColVolumeSRV );
		SAFE_RELEASE( m_pColVolumeUAV );

		SAFE_RELEASE(m_pBrickVolTex[0]);
		SAFE_RELEASE(m_pBrickVolSRV[0]);
		SAFE_RELEASE(m_pBrickVolUAV[0]);
		SAFE_RELEASE(m_pBrickVolTex[1]);
		SAFE_RELEASE(m_pBrickVolSRV[1]);
		SAFE_RELEASE(m_pBrickVolUAV[1]);

		SAFE_RELEASE( m_pCBperCall );
		SAFE_RELEASE( m_pCBperFrame );
	}

	void Integrate(ID3D11DeviceContext* pd3dImmediateContext)
	{
		DXUT_BeginPerfEvent(DXUT_PERFEVENTCOLOR, L"UpdateVolume");

		if(m_bResetVol ) ClearVolume(pd3dImmediateContext);

		ID3D11UnorderedAccessView* uavs[4] = { m_pDWVolumeUAV, m_pColVolumeUAV, m_pBrickVolUAV[0], m_pBrickVolUAV[1] };

		XMVECTOR t;
		m_CBperFrame.mInversedWorld_kinect = XMMatrixTranspose(XMMatrixInverse(&t,m_pInputPC->mCurFrame));
		pd3dImmediateContext->UpdateSubresource( m_pCBperFrame, 0, NULL, &m_CBperFrame, 0, 0 );

		pd3dImmediateContext->CSSetConstantBuffers( 0, 1, &m_pCBperCall );
		pd3dImmediateContext->CSSetConstantBuffers( 1, 1, &m_pCBperFrame );
		pd3dImmediateContext->CSSetShaderResources( 0, 1, m_pInputPC->ppMeshRawRGBZTexSRV );
		pd3dImmediateContext->CSSetShaderResources( 1, 1, m_pInputPC->ppMeshNormalTexSRV );
		UINT initCounts = 0;
		pd3dImmediateContext->CSSetUnorderedAccessViews(0, 4, uavs, &initCounts);
		pd3dImmediateContext->CSSetShader(m_pRefreshCellCS, NULL, 0);
		pd3dImmediateContext->Dispatch(VOXEL_NUM_X / CELLRATIO / THREAD_X, VOXEL_NUM_Y / CELLRATIO / THREAD_Y, VOXEL_NUM_Z / CELLRATIO / THREAD_Z);

		pd3dImmediateContext->CSSetShader(m_pCS, NULL, 0);
		pd3dImmediateContext->Dispatch(VOXEL_NUM_X / THREAD_X, VOXEL_NUM_Y / THREAD_Y, VOXEL_NUM_Z / THREAD_Z);

		ID3D11UnorderedAccessView* nulluavs[4] = { NULL, NULL, NULL, NULL };
		ID3D11ShaderResourceView* nullsrvs[2] = { NULL, NULL };
		pd3dImmediateContext->CSSetUnorderedAccessViews(0, 4, nulluavs, &initCounts);
		pd3dImmediateContext->CSSetShaderResources(0,2,nullsrvs);
		DXUT_EndPerfEvent();
	}

	LRESULT HandleMessages(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		switch(uMsg)
		{
		case WM_KEYDOWN:
			{
				int nKey = static_cast<int>(wParam);

				if (nKey == 'R')
				{
					m_bResetVol = true;
					m_pInputPC->reset();
				}
				break;
			}
		}

		return 0;
	}
};