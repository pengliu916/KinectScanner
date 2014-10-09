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
	int	halfWidth;
	int halfHeight;
	int halfDepth;
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
	ID3D11VertexShader*				m_pPassVS;
	ID3D11PixelShader*				m_pPS;
	ID3D11GeometryShader*			m_pGS;

	ID3D11InputLayout*				m_pScreenQuadIL;
	ID3D11Buffer*					m_pScreenQuadVB;


	CB_VolumeTSDF_PerCall			m_CBperCall;
	ID3D11Buffer*					m_pCBperCall;

	CB_VolumeTSDF_PerFrame			m_CBperFrame;
	ID3D11Buffer*					m_pCBperFrame;
	// Volume for Depth and Weight data
	ID3D11Texture3D*				m_pDWVolumeTex;
	ID3D11ShaderResourceView*		m_pDWVolumeSRV;
	ID3D11UnorderedAccessView*		m_pDWVolumeUAV;
	ID3D11RenderTargetView*			m_pDWVolumeRTV;// for easy volume reset
	// Volume for Color data
	ID3D11Texture3D*				m_pColVolumeTex;
	ID3D11ShaderResourceView*		m_pColVolumeSRV;
	ID3D11UnorderedAccessView*		m_pColVolumeUAV;
	ID3D11RenderTargetView*			m_pColVolumeRTV;// for easy volume reset

	//dummy rendertarget is needed, and its x and y dimension should be the same as the UAV resource
	ID3D11Texture3D*				m_pUAVDescmmyTex;
	ID3D11RenderTargetView*			m_pUAVDescmmyRTV;

	D3D11_VIEWPORT					m_cViewport;

	TransformedPointClould*			m_pInputPC;

	bool							m_bResetVol;

	VolumeTSDF( float voxelSize, UINT width, UINT height, UINT depth )
	{
		m_CBperCall.voxelSize = voxelSize;
		m_CBperCall.halfWidth = width / 2;
		m_CBperCall.halfHeight = height / 2;
		m_CBperCall.halfDepth = depth / 2;
		m_CBperCall.truncDist = XMMax( 0.001f, 2.1f * voxelSize ); 
		m_CBperCall.maxWeight = MAX_WEIGHT;

		m_bResetVol = true;
	}

	HRESULT CreateResource( ID3D11Device* pd3dDevice, TransformedPointClould* inputTCP )
	{
		HRESULT hr = S_OK;

		ID3DBlob* pVSBlob = NULL;
		V_RETURN(DXUTCompileFromFile(L"VolumeTSDF.fx", nullptr, "VS", "vs_5_0", COMPILE_FLAG, 0, &pVSBlob));
		V_RETURN(pd3dDevice->CreateVertexShader(pVSBlob->GetBufferPointer(),pVSBlob->GetBufferSize(),NULL,&m_pPassVS));
		DXUT_SetDebugName(m_pPassVS,"m_pPassVS");

		ID3DBlob* pPSBlob = NULL;
		V_RETURN(DXUTCompileFromFile(L"VolumeTSDF.fx", nullptr, "PS", "ps_5_0", COMPILE_FLAG, 0, &pPSBlob));
		V_RETURN(pd3dDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), NULL, &m_pPS));
		DXUT_SetDebugName(m_pPS, "m_pPS");
		pPSBlob->Release();

		ID3DBlob* pGSBlob = NULL;
		V_RETURN(DXUTCompileFromFile(L"VolumeTSDF.fx", nullptr, "GS", "gs_5_0", COMPILE_FLAG, 0, &pGSBlob));
		V_RETURN(pd3dDevice->CreateGeometryShader(pGSBlob->GetBufferPointer(), pGSBlob->GetBufferSize(), NULL, &m_pGS));
		DXUT_SetDebugName(m_pGS, "m_pGS");
		pGSBlob->Release();

		D3D11_INPUT_ELEMENT_DESC inputLayout[]=
		{{ "POSITION", 0, DXGI_FORMAT_R16_SINT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0}};
		V_RETURN(pd3dDevice->CreateInputLayout(inputLayout,ARRAYSIZE(inputLayout),pVSBlob->GetBufferPointer(),pVSBlob->GetBufferSize(),&m_pScreenQuadIL));
		DXUT_SetDebugName(m_pScreenQuadIL, "m_pScreenQuadIL");
		pVSBlob->Release();

		D3D11_BUFFER_DESC bd;
		ZeroMemory( &bd, sizeof(bd) );
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.ByteWidth = sizeof( short ) * m_CBperCall.halfDepth * 2;
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bd.CPUAccessFlags = 0;
		V_RETURN(pd3dDevice->CreateBuffer(&bd,NULL,&m_pScreenQuadVB));
		DXUT_SetDebugName(m_pScreenQuadVB, "m_pScreenQuadVB");

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
		D3D11_TEXTURE3D_DESC TEXDesc;
		TEXDesc.Width = m_CBperCall.halfWidth * 2;
		TEXDesc.Height = m_CBperCall.halfHeight * 2;
		TEXDesc.Depth = m_CBperCall.halfDepth * 2;
		TEXDesc.MipLevels = 1;
		TEXDesc.Format = DXGI_FORMAT_R16G16_TYPELESS;// the texture file should have 32bit typeless format
		TEXDesc.Usage = D3D11_USAGE_DEFAULT;
		TEXDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_RENDER_TARGET;
		TEXDesc.CPUAccessFlags = 0;
		TEXDesc.MiscFlags = 0;
		V_RETURN( pd3dDevice->CreateTexture3D( &TEXDesc, NULL, &m_pDWVolumeTex ) );
		DXUT_SetDebugName(m_pDWVolumeTex, "m_pDWVolumeTex");
		TEXDesc.Format = DXGI_FORMAT_R8G8B8A8_TYPELESS;
		V_RETURN( pd3dDevice->CreateTexture3D( &TEXDesc, NULL, &m_pColVolumeTex ) );
		DXUT_SetDebugName(m_pColVolumeTex, "m_pColVolumeTex");

		// Create the resource view
		D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
		ZeroMemory( &SRVDesc, sizeof( SRVDesc ) );
		SRVDesc.Format = DXGI_FORMAT_R16G16_FLOAT;// the srv cannot have typeless formate, the format should match with the conversion func in pixel shader
		SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
		SRVDesc.Texture3D.MostDetailedMip = 0;
		SRVDesc.Texture3D.MipLevels = 1;
		V_RETURN( pd3dDevice->CreateShaderResourceView( m_pDWVolumeTex, &SRVDesc,&m_pDWVolumeSRV ) );
		DXUT_SetDebugName(m_pDWVolumeSRV, "m_pDWVolumeSRV");
		SRVDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		V_RETURN( pd3dDevice->CreateShaderResourceView( m_pColVolumeTex, &SRVDesc,&m_pColVolumeSRV ) );
		DXUT_SetDebugName(m_pColVolumeSRV, "m_pColVolumeSRV");

		D3D11_UNORDERED_ACCESS_VIEW_DESC UAVDesc;
		ZeroMemory( &UAVDesc, sizeof( UAVDesc ) );
		UAVDesc.Format = DXGI_FORMAT_R32_UINT; // the UAV must have this format to allow simultaneous read and write
		UAVDesc.Texture3D.FirstWSlice = 0;
		UAVDesc.Texture3D.MipSlice = 0;
		UAVDesc.Texture3D.WSize = m_CBperCall.halfDepth * 2;
		UAVDesc.ViewDimension=D3D11_UAV_DIMENSION::D3D11_UAV_DIMENSION_TEXTURE3D;
		V_RETURN( pd3dDevice->CreateUnorderedAccessView(m_pDWVolumeTex,&UAVDesc,&m_pDWVolumeUAV));
		DXUT_SetDebugName(m_pDWVolumeUAV, "m_pDWVolumeUAV");
		V_RETURN(pd3dDevice->CreateUnorderedAccessView(m_pColVolumeTex, &UAVDesc, &m_pColVolumeUAV));
		DXUT_SetDebugName(m_pColVolumeUAV, "m_pColVolumeUAV");

		// Create the render target views
		D3D11_RENDER_TARGET_VIEW_DESC RTVDesc;
		ZeroMemory( &RTVDesc, sizeof( RTVDesc ) );
		RTVDesc.Format = DXGI_FORMAT_R16G16_FLOAT;
		RTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE3D;
		RTVDesc.Texture3D.MipSlice = 0;
		RTVDesc.Texture3D.FirstWSlice = 0;
		RTVDesc.Texture3D.WSize = m_CBperCall.halfDepth * 2;
		V_RETURN( pd3dDevice->CreateRenderTargetView( m_pDWVolumeTex, &RTVDesc, &m_pDWVolumeRTV ) );
		DXUT_SetDebugName(m_pDWVolumeRTV, "m_pDWVolumeRTV");
		RTVDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		V_RETURN( pd3dDevice->CreateRenderTargetView( m_pColVolumeTex, &RTVDesc, &m_pColVolumeRTV ) );
		DXUT_SetDebugName(m_pColVolumeRTV, "m_pColVolumeRTV");

		// Create the dummy volume tex and its RTV
		TEXDesc.Width = m_CBperCall.halfWidth * 2;
		TEXDesc.Height =  m_CBperCall.halfHeight * 2;
		TEXDesc.Depth = 1;// the dummy texture only need the same x y dimension as UAV resource, so set z to 1 to save memory
		TEXDesc.MipLevels = 1;
		TEXDesc.Format = DXGI_FORMAT_A8_UNORM;// choose one that saves memory, but not all format work
		TEXDesc.Usage = D3D11_USAGE_DEFAULT;
		TEXDesc.BindFlags = D3D11_BIND_RENDER_TARGET;
		TEXDesc.CPUAccessFlags = 0;
		TEXDesc.MiscFlags = 0;
		V_RETURN( pd3dDevice->CreateTexture3D( &TEXDesc, NULL, &m_pUAVDescmmyTex ) );
		DXUT_SetDebugName(m_pUAVDescmmyTex, "m_pUAVDescmmyTex");

		// Create the render target views
		RTVDesc.Format = TEXDesc.Format;
		RTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE3D;
		RTVDesc.Texture3D.MipSlice = 0;
		RTVDesc.Texture3D.FirstWSlice = 0;
		RTVDesc.Texture3D.WSize = 1;
		V_RETURN( pd3dDevice->CreateRenderTargetView( m_pUAVDescmmyTex, &RTVDesc, &m_pUAVDescmmyRTV ) );
		DXUT_SetDebugName(m_pUAVDescmmyRTV, "m_pUAVDescmmyRTV");

		m_cViewport.TopLeftX = 0;
		m_cViewport.TopLeftY = 0;
		m_cViewport.Width = (float)m_CBperCall.halfWidth * 2;
		m_cViewport.Height = (float)m_CBperCall.halfHeight * 2;
		m_cViewport.MinDepth = 0.0f;
		m_cViewport.MaxDepth = 1.0f;

		m_pInputPC = inputTCP;

		ID3D11DeviceContext* pd3dImmediateContext = DXUTGetD3D11DeviceContext();
		pd3dImmediateContext->UpdateSubresource( m_pCBperCall, 0, NULL, &m_CBperCall, 0, 0 );

		return hr;
	}
	void ClearVolume( ID3D11DeviceContext* pd3dImmediateContext )
	{
		DXUT_BeginPerfEvent(DXUT_PERFEVENTCOLOR, L"Clean Volume");
		// clear the render target
		float ClearColor[2] = { INVALID_VALUE,0.0f };
		pd3dImmediateContext->ClearRenderTargetView( m_pDWVolumeRTV, ClearColor );

		float ClearColor_color[4] = { 0.0f,0.0f,0.0f,0.0f };
		pd3dImmediateContext->ClearRenderTargetView( m_pColVolumeRTV, ClearColor_color );

		m_bResetVol = false;
		DXUT_EndPerfEvent();
	}

	void Release()
	{
		SAFE_RELEASE( m_pPassVS );
		SAFE_RELEASE( m_pGS );
		SAFE_RELEASE( m_pPS );

		SAFE_RELEASE( m_pScreenQuadIL);
		SAFE_RELEASE( m_pScreenQuadVB);
		
		SAFE_RELEASE( m_pDWVolumeTex );
		SAFE_RELEASE( m_pDWVolumeSRV );
		SAFE_RELEASE( m_pDWVolumeUAV );
		SAFE_RELEASE( m_pDWVolumeRTV );

		SAFE_RELEASE( m_pColVolumeTex );
		SAFE_RELEASE( m_pColVolumeSRV );
		SAFE_RELEASE( m_pColVolumeUAV );
		SAFE_RELEASE( m_pColVolumeRTV );

		SAFE_RELEASE( m_pUAVDescmmyTex );
		SAFE_RELEASE( m_pUAVDescmmyRTV );

		SAFE_RELEASE( m_pCBperCall );
		SAFE_RELEASE( m_pCBperFrame );
	}

	void Integrate(ID3D11DeviceContext* pd3dImmediateContext)
	{
		DXUT_BeginPerfEvent(DXUT_PERFEVENTCOLOR, L"UpdateVolume");

		if(m_bResetVol ) ClearVolume(pd3dImmediateContext);

		ID3D11UnorderedAccessView* uavs[2] = { m_pDWVolumeUAV, m_pColVolumeUAV };
		pd3dImmediateContext->RSSetViewports( 1, &m_cViewport );
		pd3dImmediateContext->OMSetRenderTargetsAndUnorderedAccessViews( 1, &m_pUAVDescmmyRTV, NULL, 1, 2, uavs, 0 );
		pd3dImmediateContext->IASetInputLayout( m_pScreenQuadIL );
		pd3dImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);

		UINT stride = sizeof( short ); UINT offset = 0;
		pd3dImmediateContext->IASetVertexBuffers( 0, 1, &m_pScreenQuadVB, &stride, &offset );
		
		XMVECTOR t;
		m_CBperFrame.mInversedWorld_kinect = XMMatrixTranspose(XMMatrixInverse(&t,m_pInputPC->mCurFrame));
		pd3dImmediateContext->UpdateSubresource( m_pCBperFrame, 0, NULL, &m_CBperFrame, 0, 0 );

		pd3dImmediateContext->VSSetShader( m_pPassVS, NULL, 0 );
		pd3dImmediateContext->GSSetShader( m_pGS ,NULL,0);
		pd3dImmediateContext->GSSetConstantBuffers( 0, 1, &m_pCBperCall );
		pd3dImmediateContext->PSSetShader( m_pPS, NULL, 0 );
		pd3dImmediateContext->PSSetConstantBuffers( 0, 1, &m_pCBperCall );
		pd3dImmediateContext->PSSetConstantBuffers( 1, 1, &m_pCBperFrame );
		pd3dImmediateContext->PSSetShaderResources( 2, 1, m_pInputPC->ppMeshRawRGBZTexSRV );
		pd3dImmediateContext->PSSetShaderResources( 3, 1, m_pInputPC->ppMeshNormalTexSRV );
		pd3dImmediateContext->Draw(m_CBperCall.halfDepth * 2, 0);

		ID3D11ShaderResourceView* ppSRVNULL[4] = { NULL,NULL,NULL,NULL };
		pd3dImmediateContext->PSSetShaderResources( 0, 4, ppSRVNULL);
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