#pragma once
 
#include <D3D11.h>
#include"DXUT.h"
#include <DirectXMath.h>
#include "DXUTcamera.h"
#include "SDKmisc.h"
#include <iostream>

#include "header.h"

using namespace std;


class MultiTexturePresenter
{
public:
	ID3D11VertexShader*				m_pPassVS;
	ID3D11GeometryShader*			m_pScreenQuadGS;
	ID3D11PixelShader*				m_pTexPS;
	ID3D11InputLayout*				m_pScreenQuadIL;
	ID3D11Buffer*					m_pScreenQuadVB;

	ID3D11ShaderResourceView**		m_ppInputSRV[6];
	ID3D11ShaderResourceView*		m_pNullSRV[6];
	ID3D11SamplerState*				m_pSS;

	ID3D11Texture2D*				m_pOutTex;
	ID3D11RenderTargetView*			m_pOutRTV;
	ID3D11ShaderResourceView*		m_pOutSRV;

	D3D11_VIEWPORT					m_RTviewport;

	CModelViewerCamera*				m_pVCamera[6];

	UINT						m_uTextureWidth;
	UINT						m_uTextureHeight;
	UINT						m_uRTwidth;
	UINT						m_uRTheight;

	bool						m_bDirectToBackBuffer;
	UINT						m_uTextureNumber;
	UINT						m_uLayoutStyle;//1,2,4,6 subWindows

	MultiTexturePresenter(UINT numOfTexture=1,bool bRenderToBackbuffer = true,UINT width=640, UINT height=480)
	{
		m_bDirectToBackBuffer = bRenderToBackbuffer;
		m_uTextureWidth = width;
		m_uTextureHeight = height;
		m_uTextureNumber = numOfTexture;

		m_pNullSRV[0] = NULL;
		m_pNullSRV[1] = NULL;
		m_pNullSRV[2] = NULL;
		m_pNullSRV[3] = NULL;
		m_pNullSRV[4] = NULL;
		m_pNullSRV[5] = NULL;

	}

	HRESULT Initial()
	{
		return S_OK;
	}
	void ModifyDeviceSettings(DXUTDeviceSettings* pDeviceSettings)
	{
		DXGI_SWAP_CHAIN_DESC sd;
		ZeroMemory( &sd, sizeof( sd ) );
		int widthNum;
		int heightNum;
		switch (m_uTextureNumber)
		{
		case 1:
			widthNum=1;heightNum=1;break;
		case 2:
			widthNum=2;heightNum=1;break;
		case 3:
			widthNum=2;heightNum=2;break;
		case 4:
			widthNum=2;heightNum=2;break;
		case 5:
			widthNum=3;heightNum=2;break;
		case 6:
			widthNum=3;heightNum=2;break;
		default:
			widthNum=1;heightNum=1;
		}

		sd.BufferCount = 1;
		sd.BufferDesc.Width = m_uTextureWidth*widthNum;
		sd.BufferDesc.Height = m_uTextureHeight*heightNum;
		sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		//sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		sd.BufferDesc.RefreshRate.Numerator = 60;
		sd.BufferDesc.RefreshRate.Denominator = 1;
		sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		sd.OutputWindow = pDeviceSettings->d3d11.sd.OutputWindow;
		sd.SampleDesc.Count = 1;
		sd.SampleDesc.Quality = 0;
		sd.Windowed = TRUE;
		pDeviceSettings->d3d11.sd=sd;
	}
	HRESULT CreateResource(ID3D11Device* pd3dDevice,
		ID3D11ShaderResourceView** ppInputSRV1,
		ID3D11ShaderResourceView** ppInputSRV2 = NULL,
		ID3D11ShaderResourceView** ppInputSRV3 = NULL,
		ID3D11ShaderResourceView** ppInputSRV4 = NULL,
		ID3D11ShaderResourceView** ppInputSRV5 = NULL,
		ID3D11ShaderResourceView** ppInputSRV6 = NULL)
	{
		HRESULT hr=S_OK;
		string GSname="GS_";

		m_ppInputSRV[0] = ppInputSRV1;
		m_ppInputSRV[1] = ppInputSRV2;
		m_ppInputSRV[2] = ppInputSRV3;
		m_ppInputSRV[3] = ppInputSRV4;
		m_ppInputSRV[4] = ppInputSRV5;
		m_ppInputSRV[5] = ppInputSRV6;

		m_uTextureNumber = 0;

		for(int i=0 ;i<=5;i++)
			if(m_ppInputSRV[i])
				m_uTextureNumber++;


		
		if( m_uTextureNumber <= 1 )
		{
			GSname = "GS_1";
			m_uRTwidth = m_uTextureWidth;
			m_uRTheight = m_uTextureHeight;
		}
		else if( m_uTextureNumber >= 3 && m_uTextureNumber<5)
		{
			GSname = "GS_4";
			m_uRTwidth = 2 * m_uTextureWidth;
			m_uRTheight = 2 * m_uTextureHeight;
		}
		else if( m_uTextureNumber >= 5 )
		{
			GSname = "GS_6";
			m_uRTwidth = 3 * m_uTextureWidth;
			m_uRTheight = 2 * m_uTextureHeight;
		}
		else
		{
			GSname = "GS_2";
			m_uRTwidth = 2 * m_uTextureWidth;
			m_uRTheight = m_uTextureHeight;
		}

		ID3DBlob* pVSBlob = NULL;
		wstring filename = L"MultiTexturePresenter.fx";

		V_RETURN(DXUTCompileFromFile(filename.c_str(), nullptr, "VS", "vs_5_0", COMPILE_FLAG, 0, &pVSBlob));
		V_RETURN(pd3dDevice->CreateVertexShader(pVSBlob->GetBufferPointer(),pVSBlob->GetBufferSize(),NULL,&m_pPassVS));
		DXUT_SetDebugName(m_pPassVS,"m_pPassVS");

		ID3DBlob* pGSBlob = NULL;
		V_RETURN(DXUTCompileFromFile(filename.c_str(), nullptr, GSname.c_str(), "gs_5_0", COMPILE_FLAG, 0, &pGSBlob));
		V_RETURN(pd3dDevice->CreateGeometryShader(pGSBlob->GetBufferPointer(),pGSBlob->GetBufferSize(),NULL,&m_pScreenQuadGS));
		DXUT_SetDebugName(m_pScreenQuadGS,"m_pScreenQuadGS");
		pGSBlob->Release();

		ID3DBlob* pPSBlob = NULL;
		V_RETURN(DXUTCompileFromFile(filename.c_str(), nullptr, "PS", "ps_5_0", COMPILE_FLAG, 0, &pPSBlob));
		V_RETURN(pd3dDevice->CreatePixelShader(pPSBlob->GetBufferPointer(),pPSBlob->GetBufferSize(),NULL,&m_pTexPS));
		DXUT_SetDebugName(m_pTexPS,"m_pTexPS");
		pPSBlob->Release();

		D3D11_INPUT_ELEMENT_DESC layout[] = { { "POSITION", 0, DXGI_FORMAT_R16_SINT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 } };
		V_RETURN(pd3dDevice->CreateInputLayout(layout,ARRAYSIZE(layout),pVSBlob->GetBufferPointer(),pVSBlob->GetBufferSize(),&m_pScreenQuadIL));
		DXUT_SetDebugName(m_pScreenQuadIL,"m_pScreenQuadIL");
		pVSBlob->Release();

		// Create the vertex buffer
		D3D11_BUFFER_DESC bd = {0};
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.ByteWidth = sizeof(short);
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bd.CPUAccessFlags = 0;
		V_RETURN(pd3dDevice->CreateBuffer(&bd, NULL, &m_pScreenQuadVB));
		DXUT_SetDebugName(m_pScreenQuadVB,"m_pScreenQuadVB");

		// Create rendertarget resource
		if( !m_bDirectToBackBuffer )
		{
			D3D11_TEXTURE2D_DESC	RTtextureDesc = {0};
			RTtextureDesc.Width = m_uRTwidth;
			RTtextureDesc.Height = m_uRTheight;
			RTtextureDesc.MipLevels = 1;
			RTtextureDesc.ArraySize = 1;
			RTtextureDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
			//RTtextureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
			
			RTtextureDesc.SampleDesc.Count = 1;
			RTtextureDesc.SampleDesc.Quality = 0;
			RTtextureDesc.Usage = D3D11_USAGE_DEFAULT;
			RTtextureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
			RTtextureDesc.CPUAccessFlags = 0;
			RTtextureDesc.MiscFlags = 0;
			V_RETURN(pd3dDevice->CreateTexture2D(&RTtextureDesc, NULL, &m_pOutTex));
			DXUT_SetDebugName(m_pOutTex,"m_pOutTex");

			D3D11_RENDER_TARGET_VIEW_DESC		RTViewDesc;
			ZeroMemory( &RTViewDesc, sizeof(RTViewDesc));
			RTViewDesc.Format = RTtextureDesc.Format;
			RTViewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
			RTViewDesc.Texture2D.MipSlice = 0;
			V_RETURN(pd3dDevice->CreateRenderTargetView(m_pOutTex, &RTViewDesc,&m_pOutRTV));
			DXUT_SetDebugName(m_pOutRTV,"m_pOutRTV");

			D3D11_SHADER_RESOURCE_VIEW_DESC		SRViewDesc;
			ZeroMemory( &SRViewDesc, sizeof(SRViewDesc));
			SRViewDesc.Format = RTtextureDesc.Format;
			SRViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			SRViewDesc.Texture2D.MostDetailedMip = 0;
			SRViewDesc.Texture2D.MipLevels = 1;
			V_RETURN(pd3dDevice->CreateShaderResourceView(m_pOutTex, &SRViewDesc, &m_pOutSRV));
			DXUT_SetDebugName(m_pOutSRV,"m_pOutSRV");
		}

		m_RTviewport.Width = (float)m_uRTwidth;
		m_RTviewport.Height = (float)m_uRTheight;
		m_RTviewport.MinDepth = 0.0f;
		m_RTviewport.MaxDepth = 1.0f;
		m_RTviewport.TopLeftX = 0;
		m_RTviewport.TopLeftY = 0;

		// Create the sample state
		D3D11_SAMPLER_DESC sampDesc;
		ZeroMemory( &sampDesc, sizeof(sampDesc) );
		sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
		sampDesc.MinLOD = 0;
		sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
		V_RETURN(pd3dDevice->CreateSamplerState(&sampDesc, &m_pSS ));
		DXUT_SetDebugName(m_pSS,"m_pSS");
		
		return hr;
	}

	void LinkVCamera(CModelViewerCamera* pVC1= NULL,
		CModelViewerCamera* pVC2= NULL, 
		CModelViewerCamera* pVC3= NULL, 
		CModelViewerCamera* pVC4= NULL, 
		CModelViewerCamera* pVC5= NULL, 
		CModelViewerCamera* pVC6= NULL)
	{
		m_pVCamera[0] = pVC1;
		m_pVCamera[1] = pVC2;
		m_pVCamera[2] = pVC3;
		m_pVCamera[3] = pVC4;
		m_pVCamera[4] = pVC5;
		m_pVCamera[5] = pVC6;
	}

	void SetupPipeline(ID3D11DeviceContext* pd3dImmediateContext)
	{
		pd3dImmediateContext->OMSetRenderTargets(1,&m_pOutRTV,NULL);
		pd3dImmediateContext->IASetInputLayout(m_pScreenQuadIL);
		pd3dImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
		UINT stride = 0;
		UINT offset = 0;
		pd3dImmediateContext->IASetVertexBuffers(0, 1, &m_pScreenQuadVB, &stride, &offset);
		pd3dImmediateContext->VSSetShader( m_pPassVS, NULL, 0 );
		pd3dImmediateContext->GSSetShader(m_pScreenQuadGS,NULL,0);
		pd3dImmediateContext->PSSetShader( m_pTexPS, NULL, 0 );
		ID3D11ShaderResourceView* pSRV[6];
		for( int i = 0; i < m_uTextureNumber; ++i ) pSRV[i] = *m_ppInputSRV[i];
		pd3dImmediateContext->PSSetShaderResources(0, m_uTextureNumber, pSRV);
		pd3dImmediateContext->PSSetSamplers(0,1,&m_pSS);
		pd3dImmediateContext->RSSetViewports(1, &m_RTviewport);

		float ClearColor[4] = { 0.0f, 0.0f, 1.0f, 0.0f };
		pd3dImmediateContext->ClearRenderTargetView(m_pOutRTV,ClearColor);
	}

	void Resize()
	{
		if(m_uTextureNumber<=4)
		{
			for(int i =0 ;i<4;i++)
			{
				if(m_pVCamera[i]!=NULL)
				{
					RECT rc;
					rc.top=i/2*m_uTextureHeight;
					rc.bottom=(i/2+1)*m_uTextureHeight;
					rc.left=i%2*m_uTextureWidth;
					rc.right=(i%2+1)*m_uTextureWidth;
					m_pVCamera[i]->SetDragRect(rc);
				}
			}
		}
		else
		{
			for(int i =0 ;i<6;i++)
			{
				if(m_pVCamera[i]!=NULL)
				{
					RECT rc;
					rc.top=i/3*m_uTextureHeight;
					rc.bottom=(i/3+1)*m_uTextureHeight;
					rc.left=i%3*m_uTextureWidth;
					rc.right=(i%3+1)*m_uTextureWidth;
					m_pVCamera[i]->SetDragRect(rc);
				}
			}
		}
		if(m_bDirectToBackBuffer)
		{
			m_pOutRTV = DXUTGetD3D11RenderTargetView();
		}
	}

	void Update()
	{

	}

	void Render(ID3D11DeviceContext* pd3dImmediateContext)
	{
		this->SetupPipeline(pd3dImmediateContext);
		pd3dImmediateContext->Draw(m_uTextureNumber,0);

		pd3dImmediateContext->PSSetShaderResources(0,m_uTextureNumber,m_pNullSRV);

	}

	void Release()
	{
		SAFE_RELEASE(m_pPassVS);
		SAFE_RELEASE(m_pTexPS);
		SAFE_RELEASE(m_pScreenQuadGS);
		SAFE_RELEASE(m_pScreenQuadIL);
		SAFE_RELEASE(m_pScreenQuadVB);
		SAFE_RELEASE(m_pSS);
		if(!m_bDirectToBackBuffer)
		{
			SAFE_RELEASE(m_pOutTex);
			SAFE_RELEASE(m_pOutRTV);
			SAFE_RELEASE(m_pOutSRV);
		}
	}

	LRESULT HandleMessages(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		UNREFERENCED_PARAMETER(lParam);
		UNREFERENCED_PARAMETER(hWnd);

		return 0;
	}
};