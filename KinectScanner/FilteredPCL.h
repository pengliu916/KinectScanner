#pragma once

#include <D3D11.h>
#include <DirectXMath.h>

#include "DXUT.h"
#include "DXUTcamera.h"
#include "SDKmisc.h"
#include "ScreenGrab.h"

#include "header.h"

#include "TransPCLs.h"
#include "RGBBilateralFilter.h"
#include "NormalGenerator.h"

#if USING_KINECT
#include "Scanner.h"

#endif

class FilteredPCL
{
public:
#if USING_KINECT
	ID3D11VertexShader*				m_pPassVS;
	ID3D11PixelShader*				m_pPS;
	ID3D11GeometryShader*			m_pScreenQuadGS;

	ID3D11InputLayout*				m_pScreenQuadIL;
	ID3D11Buffer*					m_pScreenQuadVB;

	ID3D11ShaderResourceView**		m_ppDepthSRV;
	ID3D11ShaderResourceView**		m_ppColorSRV;
	//For Texture output
	ID3D11Texture2D*				m_pOutTex;
	ID3D11RenderTargetView*			m_pOutRTV;

	D3D11_VIEWPORT					m_Viewport;

	Scanner							m_kinect;

#endif
	RGBBilateralFilter				m_bilateralFilter;
	NormalGenerator					m_normalGenerator;

	ID3D11ShaderResourceView*		m_pOutSRV;
	TransformedPointClould			m_TransformedPC;
	UINT							m_uRTwidth;
	UINT							m_uRTheight;
	bool			m_bShoot;
	bool			m_bUpdated;

	FilteredPCL(UINT width = DEPTH_WIDTH, UINT height = DEPTH_HEIGHT)
	{
		m_bShoot = false;
		m_bUpdated = false;

		m_uRTwidth = width;
		m_uRTheight = height;
	}

	HRESULT Initial()
	{
		HRESULT hr=S_OK;
#if USING_KINECT
		hr = m_kinect.InitialKinect();
#endif
		return hr;
	}

	HRESULT CreateResource(ID3D11Device* pd3dDevice)
	{
		HRESULT hr=S_OK;

#if USING_KINECT
		V_RETURN(m_kinect.CreateResource(pd3dDevice));
		m_ppDepthSRV=&m_kinect.m_pDepthSRV;
		m_ppColorSRV=&m_kinect.m_pColorSRV;

		ID3D11DeviceContext* immediateContext;
		pd3dDevice->GetImmediateContext(&immediateContext);

		ID3DBlob* pVSBlob = NULL;
		V_RETURN(DXUTCompileFromFile(L"FilteredPCL.fx", nullptr, "VS", "vs_5_0", COMPILE_FLAG, 0, &pVSBlob));
		V_RETURN(pd3dDevice->CreateVertexShader(pVSBlob->GetBufferPointer(),pVSBlob->GetBufferSize(),NULL,&m_pPassVS));
		DXUT_SetDebugName(m_pPassVS,"m_pPassVS");

		ID3DBlob* pPSBlob = NULL;
		V_RETURN(DXUTCompileFromFile(L"FilteredPCL.fx", nullptr, "PS", "ps_5_0", COMPILE_FLAG, 0, &pPSBlob));
		V_RETURN(pd3dDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), NULL, &m_pPS));
		DXUT_SetDebugName(m_pPS, "m_pPS");
		pPSBlob->Release();

		ID3DBlob* pGSBlob = NULL;
		V_RETURN(DXUTCompileFromFile(L"FilteredPCL.fx", nullptr, "GS", "gs_5_0", COMPILE_FLAG, 0, &pGSBlob));
		V_RETURN(pd3dDevice->CreateGeometryShader(pGSBlob->GetBufferPointer(), pGSBlob->GetBufferSize(), NULL, &m_pScreenQuadGS));
		DXUT_SetDebugName(m_pScreenQuadGS, "m_pScreenQuadGS");
		pGSBlob->Release();

		D3D11_INPUT_ELEMENT_DESC layout[] = { { "POSITION", 0, DXGI_FORMAT_R16_SINT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 } };
		V_RETURN(pd3dDevice->CreateInputLayout(layout,ARRAYSIZE(layout),pVSBlob->GetBufferPointer(),pVSBlob->GetBufferSize(),&m_pScreenQuadIL));
		DXUT_SetDebugName(m_pScreenQuadIL, "m_pScreenQuadIL");
		pVSBlob->Release();

		// Create the vertex buffer
		D3D11_BUFFER_DESC bd = {0};
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.ByteWidth = sizeof(short);
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bd.CPUAccessFlags = 0;
		V_RETURN(pd3dDevice->CreateBuffer(&bd, NULL, &m_pScreenQuadVB));
		DXUT_SetDebugName(m_pScreenQuadVB, "m_pScreenQuadVB");

		// Create rendertarget resource

		D3D11_TEXTURE2D_DESC	RTtextureDesc = {0};
		RTtextureDesc.Width = m_uRTwidth;
		RTtextureDesc.Height = m_uRTheight;
		RTtextureDesc.MipLevels = 1;
		RTtextureDesc.ArraySize = 1;
		RTtextureDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		RTtextureDesc.SampleDesc.Count = 1;
		RTtextureDesc.Usage = D3D11_USAGE_DEFAULT;
		RTtextureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		RTtextureDesc.CPUAccessFlags = 0;
		RTtextureDesc.MiscFlags = 0;
		V_RETURN(pd3dDevice->CreateTexture2D(&RTtextureDesc, NULL, &m_pOutTex));

		D3D11_RENDER_TARGET_VIEW_DESC		RTViewDesc;
		ZeroMemory( &RTViewDesc, sizeof(RTViewDesc));
		RTViewDesc.Format = RTtextureDesc.Format;
		RTViewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		RTViewDesc.Texture2D.MipSlice = 0;
		V_RETURN(pd3dDevice->CreateRenderTargetView(m_pOutTex, &RTViewDesc,&m_pOutRTV));

		
		D3D11_SHADER_RESOURCE_VIEW_DESC RTshaderResourceDesc;
		RTshaderResourceDesc.Format=RTtextureDesc.Format;
		RTshaderResourceDesc.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2D;
		RTshaderResourceDesc.Texture2D.MostDetailedMip=0;
		RTshaderResourceDesc.Texture2D.MipLevels=1;
		V_RETURN(pd3dDevice->CreateShaderResourceView(m_pOutTex,&RTshaderResourceDesc,&m_pOutSRV));
		
		m_Viewport.Width = (float)m_uRTwidth ;
		m_Viewport.Height = (float)m_uRTheight;
		m_Viewport.MinDepth = 0.0f;
		m_Viewport.MaxDepth = 1.0f;
		m_Viewport.TopLeftX = 0;
		m_Viewport.TopLeftY = 0;


		// Set rasterizer state to disable backface culling
		D3D11_RASTERIZER_DESC rasterDesc;
		rasterDesc.FillMode = D3D11_FILL_SOLID;
		rasterDesc.CullMode = D3D11_CULL_NONE;
		rasterDesc.FrontCounterClockwise = true;
		rasterDesc.DepthBias = false;
		rasterDesc.DepthBiasClamp = 0;
		rasterDesc.SlopeScaledDepthBias = 0;
		rasterDesc.DepthClipEnable = false;
		rasterDesc.ScissorEnable = false;
		rasterDesc.MultisampleEnable = false;
		rasterDesc.AntialiasedLineEnable = false;

		ID3D11RasterizerState* pState = NULL;

		V_RETURN(pd3dDevice->CreateRasterizerState(&rasterDesc, &pState));

		immediateContext->RSSetState(pState);

		SAFE_RELEASE(pState);
		SAFE_RELEASE(immediateContext);

#else
		V_RETURN(D3DX11CreateShaderResourceViewFromFile( pd3dDevice, L"1TURN2.dds", NULL, NULL, &m_pOutSRV, NULL ));

#endif	
		m_bilateralFilter.CreateResource(pd3dDevice,&m_pOutSRV);
		m_normalGenerator.CreateResource(pd3dDevice,&m_bilateralFilter.m_pOutSRV);

		m_TransformedPC.ppMeshRGBZTexSRV = &m_bilateralFilter.m_pOutSRV;
		m_TransformedPC.ppMeshRawRGBZTexSRV = &m_pOutSRV;
		m_TransformedPC.ppMeshNormalTexSRV = &m_normalGenerator.m_pOutSRV;
		m_TransformedPC.reset();
		
		return hr;
}

	void SetupPipeline(ID3D11DeviceContext* pd3dImmediateContext)
	{
#if USING_KINECT
		pd3dImmediateContext->IASetInputLayout(m_pScreenQuadIL);
		pd3dImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
		UINT stride = 0;
		UINT offset = 0;
		pd3dImmediateContext->IASetVertexBuffers(0, 1, &m_pScreenQuadVB, &stride, &offset);
		pd3dImmediateContext->VSSetShader( m_pPassVS, NULL, 0 );
		pd3dImmediateContext->GSSetShader(m_pScreenQuadGS,NULL,0);
		pd3dImmediateContext->PSSetShader( m_pPS, NULL, 0 );
		pd3dImmediateContext->PSSetShaderResources(0, 1, m_ppDepthSRV);
		pd3dImmediateContext->PSSetShaderResources(1, 1, m_ppColorSRV);
		pd3dImmediateContext->RSSetViewports(1, &m_Viewport);
		pd3dImmediateContext->OMSetRenderTargets(1,&m_pOutRTV,NULL);
#endif
	}

	void Render(ID3D11DeviceContext* pd3dimmediateContext)
	{
#if USING_KINECT
		m_kinect.UpdateDepthTexture(pd3dimmediateContext);
		m_bUpdated = m_kinect.m_bDepthReceived;

		if(m_bUpdated)
		{
			this->SetupPipeline(pd3dimmediateContext);
			pd3dimmediateContext->Draw(1, 0);

			ID3D11ShaderResourceView* ppSRVNULLs[4] = { NULL,NULL,NULL,NULL };
			pd3dimmediateContext->PSSetShaderResources( 0, 4, ppSRVNULLs );
			//pd3dimmediateContext->OMSetRenderTargets(1,&m_pOutRTV,NULL);
			//if( m_bShoot )
			//{
			//	m_bShoot = false;
			//	//D3DX11SaveTextureToFile(pd3dimmediateContext,m_pOutTex,D3DX11_IFF_DDS,L"Frame.dds");

			//}
			m_bilateralFilter.ProcessImage(pd3dimmediateContext);
			m_normalGenerator.ProcessImage(pd3dimmediateContext);
		}

#else
		m_bUpdated = true;
		m_bilateralFilter.ProcessImage(pd3dimmediateContext);
		m_normalGenerator.ProcessImage(pd3dimmediateContext);
#endif
	}

	void Release()
	{
#if USING_KINECT
		m_kinect.Release();
		SAFE_RELEASE(m_pOutTex);
		SAFE_RELEASE(m_pOutRTV);
		
		SAFE_RELEASE(m_pPassVS);
		SAFE_RELEASE(m_pPS);
		SAFE_RELEASE(m_pScreenQuadGS);
		SAFE_RELEASE(m_pScreenQuadIL);
		SAFE_RELEASE(m_pScreenQuadVB);
#endif
		m_normalGenerator.Release();
		m_bilateralFilter.Release();
		
		SAFE_RELEASE(m_pOutSRV);
	}

	LRESULT HandleMessages(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
#if USING_KINECT
		m_kinect.HandleMessages(hWnd,uMsg,wParam,lParam);
		m_bilateralFilter.HandleMessages(hWnd,uMsg,wParam,lParam);
#endif
		switch(uMsg)
		{
		case WM_KEYDOWN:
			{
				int nKey = static_cast<int>(wParam);

				/*if( nKey == 'F' )
				{
					m_bShoot = true;
				}*/

				if (nKey == 'S')
				{
					m_TransformedPC.mModelM_now = XMMatrixMultiply(m_TransformedPC.mModelM_now,XMMatrixTranslation(0,0,-SMALL_OFFSET / 5.0f));
				}
				else if (nKey == 'W')
				{
					m_TransformedPC.mModelM_now = XMMatrixMultiply(m_TransformedPC.mModelM_now,XMMatrixTranslation(0,0,SMALL_OFFSET / 5.0f));
				}
				else if (nKey == 'A')
				{
					m_TransformedPC.mModelM_now = XMMatrixMultiply(m_TransformedPC.mModelM_now,XMMatrixTranslation(-SMALL_OFFSET / 5.0f,0,0));
				}
				else if (nKey == 'D')
				{
					m_TransformedPC.mModelM_now = XMMatrixMultiply(m_TransformedPC.mModelM_now,XMMatrixTranslation(SMALL_OFFSET / 5.0f,0,0));
				}
				else if (nKey == 'Q')
				{
					m_TransformedPC.mModelM_now = XMMatrixMultiply(m_TransformedPC.mModelM_now,XMMatrixTranslation(0,SMALL_OFFSET / 5.0f,0));
				}
				else if (nKey == 'E')
				{
					m_TransformedPC.mModelM_now = XMMatrixMultiply(m_TransformedPC.mModelM_now,XMMatrixTranslation(0,-SMALL_OFFSET / 5.0f,0));
				}

				else if (nKey == 'Y')
				{
					m_TransformedPC.mModelM_now = XMMatrixMultiply(m_TransformedPC.mModelM_now,XMMatrixRotationRollPitchYaw(SMALL_ROTATE / 5.0f,0,0));
				}
				else if (nKey == 'H')
				{
					m_TransformedPC.mModelM_now = XMMatrixMultiply(m_TransformedPC.mModelM_now,XMMatrixRotationRollPitchYaw(-SMALL_ROTATE / 5.0f,0,0));
				}
				else if (nKey == 'G')
				{
					m_TransformedPC.mModelM_now = XMMatrixMultiply(m_TransformedPC.mModelM_now,XMMatrixRotationRollPitchYaw(0,SMALL_ROTATE / 5.0f,0));
				}
				else if (nKey == 'J')
				{
					m_TransformedPC.mModelM_now = XMMatrixMultiply(m_TransformedPC.mModelM_now,XMMatrixRotationRollPitchYaw(0,-SMALL_ROTATE / 5.0f,0));
				}
				else if (nKey == 'T')
				{
					m_TransformedPC.mModelM_now = XMMatrixMultiply(m_TransformedPC.mModelM_now,XMMatrixRotationRollPitchYaw(0,0,SMALL_ROTATE / 5.0f));
				}
				else if (nKey == 'U')
				{
					m_TransformedPC.mModelM_now = XMMatrixMultiply(m_TransformedPC.mModelM_now,XMMatrixRotationRollPitchYaw(0,0,-SMALL_ROTATE / 5.0f));
				}

				else if (nKey == 'R')
				{
					m_TransformedPC.reset();
				}
#if !USING_KINECT
				else if (nKey == 'R')
				{
					m_TransformedPC.reset();
					m_TransformedPC.mModelM_now._41 = 0.2f*rand()/RAND_MAX-0.1;
					m_TransformedPC.mModelM_now._42 = 0.2f*rand()/RAND_MAX-0.1;
					m_TransformedPC.mModelM_now._43 = 0.2f*rand()/RAND_MAX-0.1;

					m_TransformedPC.mModelM_now = XMMatrixMultiply(m_TransformedPC.mModelM_now,XMMatrixRotationY( 0.2f*rand()/RAND_MAX-0.1));
					m_TransformedPC.mModelM_now = XMMatrixMultiply(m_TransformedPC.mModelM_now,XMMatrixRotationX( 0.2f*rand()/RAND_MAX-0.1));
					m_TransformedPC.mModelM_now = XMMatrixMultiply(m_TransformedPC.mModelM_now,XMMatrixRotationZ( 0.2f*rand()/RAND_MAX-0.1));

					m_TransformedPC.mModelM_pre = m_TransformedPC.mModelM_now;
				}
#endif
				break;
			}
		}

		return 0;
	}
};