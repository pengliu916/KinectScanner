#pragma once

#include <D3D11.h>
#include <DirectXMath.h>
#include "DXUT.h"
#include <iostream>

#include "header.h"

using namespace std;
class NormalGenerator
{
public:
	ID3D11VertexShader*				m_pPassVS;
	ID3D11PixelShader*				m_pPS;
	ID3D11GeometryShader*			m_pScreenQuadGS;
	ID3D11InputLayout*				m_pScreenQuadIL;
	ID3D11Buffer*					m_pScreenQuadVB; 

	ID3D11ShaderResourceView**		m_ppInputSRV;

	ID3D11Texture2D*				m_pOutTex;
	ID3D11RenderTargetView*			m_pOutRTV;
	ID3D11ShaderResourceView*		m_pOutSRV;

	D3D11_VIEWPORT					m_RTviewport;

	UINT			m_uRTWidth;
	UINT			m_uRTHeight;

	NormalGenerator(UINT width = D_W, UINT height = D_H)
	{
		m_uRTWidth=width;
		m_uRTHeight=height;
		m_ppInputSRV=NULL;
		m_pOutTex=NULL;
		m_pOutRTV=NULL;
		m_pOutSRV=NULL;
	}

	HRESULT Initial()
	{
		HRESULT hr=S_OK;
		return hr;
	}

	HRESULT CreateResource(ID3D11Device* pd3dDevice,
		ID3D11ShaderResourceView** ppInputTextureRV)
	{
		HRESULT hr=S_OK;

		ID3DBlob* pVSBlob = NULL;
		V_RETURN(DXUTCompileFromFile(L"NormalGenerator.fx", nullptr, "VS", "vs_5_0", COMPILE_FLAG, 0, &pVSBlob));
		V_RETURN(pd3dDevice->CreateVertexShader(pVSBlob->GetBufferPointer(),pVSBlob->GetBufferSize(),NULL,&m_pPassVS));
		DXUT_SetDebugName(m_pPassVS,"m_pPassVS");
		
		ID3DBlob* pGSBlob = NULL;
		V_RETURN(DXUTCompileFromFile(L"NormalGenerator.fx", nullptr, "GS", "gs_5_0", COMPILE_FLAG, 0, &pGSBlob));
		V_RETURN(pd3dDevice->CreateGeometryShader(pGSBlob->GetBufferPointer(), pGSBlob->GetBufferSize(), NULL, &m_pScreenQuadGS));
		DXUT_SetDebugName(m_pScreenQuadGS, "m_pScreenQuadGS");
		pGSBlob->Release();

		ID3DBlob* pPSBlob = NULL;
		V_RETURN(DXUTCompileFromFile(L"NormalGenerator.fx", nullptr, "PS", "ps_5_0", COMPILE_FLAG, 0, &pPSBlob));
		V_RETURN(pd3dDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), NULL, &m_pPS));
		DXUT_SetDebugName(m_pPS, "m_pPS");
		pPSBlob->Release();

		D3D11_INPUT_ELEMENT_DESC layout[] = { { "POSITION", 0, DXGI_FORMAT_R16_SINT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 } };
		V_RETURN(pd3dDevice->CreateInputLayout(layout,ARRAYSIZE(layout),pVSBlob->GetBufferPointer(),pVSBlob->GetBufferSize(),&m_pScreenQuadIL));
		DXUT_SetDebugName(m_pScreenQuadIL, "m_pScreenQuadIL");
		pVSBlob->Release();

		m_ppInputSRV = ppInputTextureRV;

		// Create the vertex buffer
		D3D11_BUFFER_DESC bd = {0};
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.ByteWidth = sizeof(short);
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bd.CPUAccessFlags = 0;

		V_RETURN(pd3dDevice->CreateBuffer(&bd, NULL, &m_pScreenQuadVB));
		DXUT_SetDebugName(m_pScreenQuadVB, "m_pScreenQuadVB");

		//Create rendertaget resource
		D3D11_TEXTURE2D_DESC	RTtextureDesc = {0};
		RTtextureDesc.Width=m_uRTWidth;
		RTtextureDesc.Height=m_uRTHeight;
		RTtextureDesc.MipLevels=1;
		RTtextureDesc.ArraySize=1;
		RTtextureDesc.Format=DXGI_FORMAT_R16G16B16A16_FLOAT;
		RTtextureDesc.SampleDesc.Count=1;
		RTtextureDesc.Usage=D3D11_USAGE_DEFAULT;
		RTtextureDesc.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
		RTtextureDesc.CPUAccessFlags=0;
		RTtextureDesc.MiscFlags=0;

		V_RETURN(pd3dDevice->CreateTexture2D(&RTtextureDesc,NULL,&m_pOutTex));

		D3D11_SHADER_RESOURCE_VIEW_DESC RTshaderResourceDesc;
		RTshaderResourceDesc.Format=RTtextureDesc.Format;
		RTshaderResourceDesc.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2D;
		RTshaderResourceDesc.Texture2D.MostDetailedMip=0;
		RTshaderResourceDesc.Texture2D.MipLevels=1;
		V_RETURN(pd3dDevice->CreateShaderResourceView(m_pOutTex,&RTshaderResourceDesc,&m_pOutSRV));

		D3D11_RENDER_TARGET_VIEW_DESC	RTviewDesc;
		RTviewDesc.Format=RTtextureDesc.Format;
		RTviewDesc.ViewDimension=D3D11_RTV_DIMENSION_TEXTURE2D;
		RTviewDesc.Texture2D.MipSlice=0;
		V_RETURN(pd3dDevice->CreateRenderTargetView(m_pOutTex,&RTviewDesc,&m_pOutRTV));

		m_RTviewport.Width=(float)m_uRTWidth;
		m_RTviewport.Height=(float)m_uRTHeight;
		m_RTviewport.MinDepth=0.0f;
		m_RTviewport.MaxDepth=1.0f;
		m_RTviewport.TopLeftX = 0;
		m_RTviewport.TopLeftY = 0;

		return hr;
	}

	~NormalGenerator()
	{

	}

	void Release()
	{
		SAFE_RELEASE(m_pPassVS);
		SAFE_RELEASE(m_pPS);
		SAFE_RELEASE(m_pScreenQuadGS);
		SAFE_RELEASE(m_pScreenQuadIL);
		SAFE_RELEASE(m_pScreenQuadVB);

		SAFE_RELEASE(m_pOutTex);
		SAFE_RELEASE(m_pOutRTV);
		SAFE_RELEASE(m_pOutSRV);

	}
	void SetupPipeline(ID3D11DeviceContext* pd3dImmediateContext)
	{
		pd3dImmediateContext->IASetInputLayout(m_pScreenQuadIL);
		pd3dImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
		UINT stride = 0;
		UINT offset = 0;
		pd3dImmediateContext->IASetVertexBuffers(0, 1, &m_pScreenQuadVB, &stride, &offset);
		pd3dImmediateContext->VSSetShader( m_pPassVS, NULL, 0 );
		pd3dImmediateContext->GSSetShader(m_pScreenQuadGS,NULL,0);	
		pd3dImmediateContext->RSSetViewports(1, &m_RTviewport);
	
	}

	void ProcessImage(ID3D11DeviceContext* pd3dImmediateContext)
	{
		pd3dImmediateContext->OMSetRenderTargets(1,&m_pOutRTV,NULL);
		pd3dImmediateContext->PSSetShader( m_pPS, NULL, 0 );
		pd3dImmediateContext->PSSetShaderResources(0, 1, m_ppInputSRV);

		pd3dImmediateContext->Draw(1,0);
		ID3D11RenderTargetView* nullRTVs = NULL;
		pd3dImmediateContext->OMSetRenderTargets(1, &nullRTVs, NULL);
	}

	LRESULT HandleMessages(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		return 0;
	}
};