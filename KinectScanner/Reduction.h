// Need to be improved
#pragma once
#define MAX_NUM_TEXTURE 10
#define MAX_NUM_INPUT 7
#include <D3D11.h>
#include <DirectXMath.h>
#include "DXUT.h"
#include "SDKmisc.h"

#include "header.h"

using namespace DirectX;

struct CB_Reduction_PerCall
{
	int texPairWidth;
	int texPairHeight;
	int txRTWidht;
	int txRTHeight;
};

class Reduction 
{
public:

	ID3D11VertexShader*				m_pPassVS;
	ID3D11PixelShader*				m_pPS;
	ID3D11GeometryShader*			m_pGS;
	ID3D11InputLayout*				m_pScreenQuadIL;
	ID3D11Buffer*					m_pScreenQuadVB; 

	ID3D11Buffer*					m_pCBperCall;
	CB_Reduction_PerCall			m_CBperCall;

	ID3D11Texture2D*				m_pTexReduction[MAX_NUM_TEXTURE][MAX_NUM_INPUT];    
	ID3D11ShaderResourceView*		m_pTexReductionSRV[MAX_NUM_TEXTURE][MAX_NUM_INPUT];
	ID3D11RenderTargetView*			m_pTexReductionRTV[MAX_NUM_TEXTURE][MAX_NUM_INPUT];

	ID3D11Texture2D*				m_pFinalTexForCPU[MAX_NUM_INPUT];

	UINT		m_uFinalTexIdx;
	UINT		m_uNumberOfInputs;
	UINT		m_uSourceTexWidth;
	UINT		m_uSourceTexHeight;

	XMFLOAT4	m_vfAnswer[MAX_NUM_INPUT];

	Reduction()
	{
		m_CBperCall.texPairHeight = 0;
		m_CBperCall.texPairWidth = 0;
		m_CBperCall.txRTHeight = 0;
		m_CBperCall.txRTWidht = 0;

		m_uFinalTexIdx = 0;
		m_uNumberOfInputs = 0;
		m_uSourceTexHeight = 0;
		m_uSourceTexWidth = 0;

		for(int i=0;i<MAX_NUM_INPUT;i++)
		{
			m_vfAnswer[i] = XMFLOAT4();
			for(int j=0;j<MAX_NUM_TEXTURE;j++)
			{
				m_pTexReductionSRV[j][i]=NULL;
			}
		}
	}

	HRESULT Initial()
	{
		HRESULT hr=S_OK;
		return hr;
	}

	HRESULT CreateResource(ID3D11Device* pd3dDevice,
		ID3D11Texture2D** ppSourceTexture2D, UINT NumOfTex)
	{
		HRESULT hr=S_OK;

		m_uNumberOfInputs=NumOfTex;

		ID3DBlob* pVSBlob = NULL;
		V_RETURN(DXUTCompileFromFile(L"Reduction.fx", nullptr, "VS", "vs_5_0", COMPILE_FLAG, 0, &pVSBlob));
		V_RETURN(pd3dDevice->CreateVertexShader(pVSBlob->GetBufferPointer(),pVSBlob->GetBufferSize(),NULL,&m_pPassVS));
		DXUT_SetDebugName(m_pPassVS,"m_pPassVS");

		ID3DBlob* pGSBlob = NULL;
		V_RETURN(DXUTCompileFromFile(L"Reduction.fx", nullptr, "GS", "gs_5_0", COMPILE_FLAG, 0, &pGSBlob));
		V_RETURN(pd3dDevice->CreateGeometryShader(pGSBlob->GetBufferPointer(), pGSBlob->GetBufferSize(), NULL, &m_pGS));
		DXUT_SetDebugName( m_pGS, "m_pGS");
		pGSBlob->Release();

		ID3DBlob* pPSBlob = NULL;
		V_RETURN(DXUTCompileFromFile(L"Reduction.fx", nullptr, "PS", "ps_5_0", COMPILE_FLAG, 0, &pPSBlob));
		V_RETURN(pd3dDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), NULL, &m_pPS));
		DXUT_SetDebugName(m_pPS, "m_pPS");
		pPSBlob->Release();

		D3D11_INPUT_ELEMENT_DESC layout[] = { { "POSITION", 0, DXGI_FORMAT_R16_SINT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 } };
		V_RETURN(pd3dDevice->CreateInputLayout(layout,ARRAYSIZE(layout),pVSBlob->GetBufferPointer(),pVSBlob->GetBufferSize(),&m_pScreenQuadIL));
		pVSBlob->Release();
		DXUT_SetDebugName( m_pScreenQuadIL, "m_pScreenQuadIL");


		D3D11_BUFFER_DESC bd = {0};
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		bd.CPUAccessFlags = 0;
		bd.ByteWidth = sizeof(CB_Reduction_PerCall);
		V_RETURN(pd3dDevice->CreateBuffer( &bd, NULL, &m_pCBperCall ));
		DXUT_SetDebugName( m_pCBperCall, "Reduction_m_pCBperCall");

		// Create the vertex buffer
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.ByteWidth = sizeof(short);
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bd.CPUAccessFlags = 0;

		V_RETURN(pd3dDevice->CreateBuffer(&bd, NULL, &m_pScreenQuadVB));
		DXUT_SetDebugName( m_pScreenQuadVB, "Reduction_m_pScreenQuadVB");


		for(UINT j=0;j<m_uNumberOfInputs;j++)
		{
			m_pTexReduction[0][j] = ppSourceTexture2D[j];
			D3D11_TEXTURE2D_DESC txDesc = {0};
			m_pTexReduction[0][j]->GetDesc(&txDesc);


			D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
			SRVDesc.Format = txDesc.Format;
			SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			SRVDesc.Texture2D.MipLevels = 1;
			SRVDesc.Texture2D.MostDetailedMip = 0;
			V_RETURN( pd3dDevice->CreateShaderResourceView( m_pTexReduction[0][j], &SRVDesc, &m_pTexReductionSRV[0][j] ) );
			char temp[100];
			sprintf_s(temp,"Reduction_m_pTexReductionSRV_%d",j);
			DXUT_SetDebugName( m_pTexReduction[0][j], temp );

			m_uSourceTexWidth = txDesc.Width;
			m_uSourceTexHeight = txDesc.Height;
			int width = ceil(m_uSourceTexWidth/3.0f);
			int height = ceil(m_uSourceTexHeight/3.0f);

			int i=1;
			bool end=false;
			while(i<MAX_NUM_TEXTURE && !end )
			{
				if(width==1 && height==1)
					end=true;
				D3D11_TEXTURE2D_DESC tmdesc;
				ZeroMemory( &tmdesc, sizeof( D3D11_TEXTURE2D_DESC ) );
				tmdesc.ArraySize = 1;
				tmdesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
				tmdesc.Usage = D3D11_USAGE_DEFAULT;
				tmdesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
				tmdesc.Width = width;
				tmdesc.Height = height;
				tmdesc.MipLevels = 1;
				tmdesc.SampleDesc.Count = 1;
				V_RETURN( pd3dDevice->CreateTexture2D( &tmdesc, NULL, &m_pTexReduction[i][j] ) );
				sprintf_s(temp,"Reduction_m_pTexReduction_%d_%d",i,j);
				DXUT_SetDebugName( m_pTexReduction[i][j],temp );

				// Create the render target view
				D3D11_RENDER_TARGET_VIEW_DESC DescRT;
				DescRT.Format = tmdesc.Format;
				DescRT.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
				DescRT.Texture2D.MipSlice = 0;
				V_RETURN( pd3dDevice->CreateRenderTargetView( m_pTexReduction[i][j], &DescRT, &m_pTexReductionRTV[i][j] ) );
				sprintf_s(temp,"Reduction_m_pTexReductionRTV_%d_%d",i,j);
				DXUT_SetDebugName( m_pTexReductionRTV[i][j], temp );

				// Create the shader resource view
				D3D11_SHADER_RESOURCE_VIEW_DESC DescRV;
				DescRV.Format = tmdesc.Format;
				DescRV.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
				DescRV.Texture2D.MipLevels = 1;
				DescRV.Texture2D.MostDetailedMip = 0;
				V_RETURN( pd3dDevice->CreateShaderResourceView( m_pTexReduction[i][j], &DescRV, &m_pTexReductionSRV[i][j] ) );
				sprintf_s(temp,"Reduction_m_pTexReductionSRV_%d_%d",i,j);
				DXUT_SetDebugName( m_pTexReductionSRV[i][j], temp );

				width = ceil(width/3.0f);
				height = ceil(height/3.0f);
				m_uFinalTexIdx = i;
				i++;
			}

			D3D11_TEXTURE2D_DESC tmdesc;
			ZeroMemory( &tmdesc, sizeof( D3D11_TEXTURE2D_DESC ) );
			tmdesc.ArraySize = 1;
			tmdesc.BindFlags =0;
			tmdesc.Usage = D3D11_USAGE_STAGING;
			tmdesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
			tmdesc.Width = 1;
			tmdesc.Height = 1;
			tmdesc.MipLevels = 1;
			tmdesc.SampleDesc.Count = 1;
			tmdesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
			tmdesc.MiscFlags = 0;
			V_RETURN( pd3dDevice->CreateTexture2D( &tmdesc, NULL, &m_pFinalTexForCPU[j] ) );
			sprintf_s(temp,"Reduction_m_pFinalTexForCPU_%d",j);
			DXUT_SetDebugName( m_pFinalTexForCPU[j],temp );

			m_CBperCall.texPairWidth = m_uSourceTexWidth;
			m_CBperCall.texPairHeight = m_uSourceTexHeight;
		}
		return hr;
	}

	~Reduction()
	{

	}

	void Resize(ID3D11DeviceContext* pd3dimmediateContext,const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc = NULL)
	{

	}

	void Release()
	{
		SAFE_RELEASE(m_pPassVS);
		SAFE_RELEASE(m_pPS);
		SAFE_RELEASE(m_pGS);
		SAFE_RELEASE(m_pScreenQuadIL);
		SAFE_RELEASE(m_pScreenQuadVB);
		SAFE_RELEASE(m_pCBperCall);
		for(UINT j=0;j<m_uNumberOfInputs;j++)
		{
			SAFE_RELEASE(m_pFinalTexForCPU[j]);
			for(UINT i=0;i<=m_uFinalTexIdx;i++)
			{
				if(i!=0)
				{
					SAFE_RELEASE(m_pTexReduction[i][j]);
				}
				SAFE_RELEASE(m_pTexReductionSRV[i][j]);
				SAFE_RELEASE(m_pTexReductionRTV[i][j]);
			}
		}
	}

	void Update(float fElapsedTime)
	{
	}
	void Processing(ID3D11DeviceContext* pd3dImmediateContext)
	{
		pd3dImmediateContext->IASetInputLayout(m_pScreenQuadIL);
		pd3dImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
		UINT stride = 0;
		UINT offset = 0;
		pd3dImmediateContext->IASetVertexBuffers(0, 1, &m_pScreenQuadVB, &stride, &offset);
		pd3dImmediateContext->VSSetShader( m_pPassVS, NULL, 0 );
		pd3dImmediateContext->GSSetShader(m_pGS,NULL,0);	
		pd3dImmediateContext->PSSetShader( m_pPS, NULL, 0 );
		pd3dImmediateContext->GSSetConstantBuffers(0,1,&m_pCBperCall);

		UINT width=ceil(m_uSourceTexWidth/3.0f);
		UINT height=ceil(m_uSourceTexHeight/3.0f);

		// Setup the viewport to match the backbuffer
			D3D11_VIEWPORT vp;
			vp.Width = width;
			vp.Height = height;
			vp.MinDepth = 0.0f;
			vp.MaxDepth = 1.0f;
			vp.TopLeftX = 0;
			vp.TopLeftY = 0; 

		for(UINT i=0;i<m_uFinalTexIdx;i++)
		{
			vp.Width = width;
			vp.Height = height;

			pd3dImmediateContext->RSSetViewports( 1, &vp );
			m_CBperCall.txRTWidht=vp.Width;
			m_CBperCall.txRTHeight=vp.Height;
			pd3dImmediateContext->UpdateSubresource(m_pCBperCall,0,NULL,&m_CBperCall,0,0);
			for(int j=0;j<m_uNumberOfInputs;j++){
				pd3dImmediateContext->OMSetRenderTargets(1,&m_pTexReductionRTV[i+1][j],NULL);
				pd3dImmediateContext->PSSetShaderResources(0,1,&m_pTexReductionSRV[i][j]);
				pd3dImmediateContext->Draw(1,0);
			}
			width=ceil(width/3.0f);
			height=ceil(height/3.0f);
		}	

		for(UINT j=0;j<m_uNumberOfInputs;j++)
		{
			pd3dImmediateContext->CopyResource(m_pFinalTexForCPU[j],m_pTexReduction[m_uFinalTexIdx][j]);
			D3D11_MAPPED_SUBRESOURCE subresource;
			pd3dImmediateContext->Map(m_pFinalTexForCPU[j],D3D11CalcSubresource(0,0,1),D3D11_MAP_READ,0,&subresource);
			m_vfAnswer[j] = *reinterpret_cast<XMFLOAT4*>(subresource.pData);
			pd3dImmediateContext->Unmap(m_pFinalTexForCPU[j],D3D11CalcSubresource(0,0,1));
		}
	}
};