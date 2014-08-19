// Need to be improved
#pragma once
#include <D3D11.h>
#include <DirectXMath.h>
#include "DXUT.h"
#include "SDKmisc.h"

#include "Reduction.h"
#include "TransPCLs.h"

#include "header.h"

using namespace DirectX;
struct CB_PoseEstimator_PerCall
{
	int txRTWidth;
	int txRTHeight;
	int NIU0;//Not in use
	int NIU1;
};
struct CB_PoseEstimator_PerFrame
{
	XMMATRIX mKinectMesh;
	XMMATRIX mTsdfMesh;
};

class PoseEstimator
{
public:

	ID3D11VertexShader*				m_pPassVS;
	ID3D11PixelShader*				m_pPS_I;
	ID3D11PixelShader*				m_pPS_II;
	ID3D11GeometryShader*			m_pGS;
	ID3D11InputLayout*				m_pScreenQuadIL;
	ID3D11Buffer*					m_pScreenQuadVB; 

	CB_PoseEstimator_PerCall		m_CBperCall;
	ID3D11Buffer*					m_pCBperCall;

	CB_PoseEstimator_PerFrame		m_CBperFrame;
	ID3D11Buffer*					m_pCBperFrame;

	D3D11_VIEWPORT					m_ViewPort;

	//For Intermediate Data
	ID3D11Texture2D*				m_pSumOfCoordTex[7];
	ID3D11RenderTargetView*			m_pSumOfCoordRTV[7];
	ID3D11ShaderResourceView*		m_pSumOfCoordSRV[7];

	Reduction						m_Reduction;

	//Input Data
	TransformedPointClould*			m_pTsdfTPC;
	TransformedPointClould*			m_pKinectTPC;

	UINT			m_uOutputTexSize_x;
	UINT			m_uOutputTexSize_y;

	bool			m_bSigularMatrix;
	bool			m_bBadDepthMap;


	Eigen::Matrix<float,6,6>		m_mA;
	Eigen::Matrix<float,6,1>		m_vB;
	Eigen::Matrix<float,6,1>		m_vX;

	XMVECTOR						m_vIncTran;
	XMVECTOR						m_vIncRotate;
	float							m_fAlpha;
	float							m_fBeta;
	float							m_fGamma;
	float							m_fXoffset;
	float							m_fYoffset;
	float							m_fZoffset;
	float							m_fNpairs;
	float							m_fPreNpairs;
	double							m_dDet;

	PoseEstimator(UINT _inputWidth, UINT _inputHeight)
	{
		m_mA = Eigen::MatrixXf::Identity(6,6);
		m_uOutputTexSize_x = _inputWidth;
		m_uOutputTexSize_y = _inputHeight;
		m_fPreNpairs = 1;
	}

	HRESULT Initial(TransformedPointClould* pTsdfTPC,
					TransformedPointClould* pKinectTPC)
	{
		HRESULT hr=S_OK;

		m_pTsdfTPC = pTsdfTPC;
		m_pKinectTPC = pKinectTPC;
		return hr;
	}

	HRESULT CreateResource(ID3D11Device* pd3dDevice)
	{
		HRESULT hr=S_OK;

		m_CBperCall.txRTWidth = m_uOutputTexSize_x;
		m_CBperCall.txRTHeight = m_uOutputTexSize_y;


		ID3DBlob* pVSBlob = NULL;
		V_RETURN(DXUTCompileFromFile(L"PoseEstimator.fx", nullptr, "VS", "vs_5_0", COMPILE_FLAG, 0, &pVSBlob));
		V_RETURN(pd3dDevice->CreateVertexShader(pVSBlob->GetBufferPointer(),pVSBlob->GetBufferSize(),NULL,&m_pPassVS));
		DXUT_SetDebugName(m_pPassVS,"m_pPassVS");

		ID3DBlob* pGSBlob = NULL;
		V_RETURN(DXUTCompileFromFile(L"PoseEstimator.fx", nullptr, "GS", "gs_5_0", COMPILE_FLAG, 0, &pGSBlob));
		V_RETURN(pd3dDevice->CreateGeometryShader(pGSBlob->GetBufferPointer(), pGSBlob->GetBufferSize(), NULL, &m_pGS));
		DXUT_SetDebugName(m_pGS, "m_pGS");
		pGSBlob->Release();

		ID3DBlob* pPSBlob = NULL;
		V_RETURN(DXUTCompileFromFile(L"PoseEstimator.fx", nullptr, "PS_I", "ps_5_0", COMPILE_FLAG, 0, &pPSBlob));
		V_RETURN(pd3dDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), NULL, &m_pPS_I));
		DXUT_SetDebugName(m_pPS_I, "m_pPS_I");
		V_RETURN(DXUTCompileFromFile(L"PoseEstimator.fx", nullptr, "PS_II", "ps_5_0", COMPILE_FLAG, 0, &pPSBlob));
		V_RETURN(pd3dDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), NULL, &m_pPS_II));
		DXUT_SetDebugName(m_pPS_II, "m_pPS_II");
		pPSBlob->Release();

		D3D11_INPUT_ELEMENT_DESC layout[] = { { "POSITION", 0, DXGI_FORMAT_R16_SINT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 } };
		V_RETURN(pd3dDevice->CreateInputLayout(layout,ARRAYSIZE(layout),pVSBlob->GetBufferPointer(),pVSBlob->GetBufferSize(),&m_pScreenQuadIL));
		pVSBlob->Release();
		DXUT_SetDebugName( m_pScreenQuadIL, "m_pScreenQuadIL");


		D3D11_BUFFER_DESC bd = {0};
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		bd.CPUAccessFlags = 0;
		bd.ByteWidth = sizeof(CB_PoseEstimator_PerCall);
		V_RETURN(pd3dDevice->CreateBuffer( &bd, NULL, &m_pCBperCall ));
		DXUT_SetDebugName( m_pCBperCall, "m_pCBperCall");

		bd.ByteWidth = sizeof(CB_PoseEstimator_PerFrame);
		V_RETURN(pd3dDevice->CreateBuffer( &bd, NULL, &m_pCBperFrame ));
		DXUT_SetDebugName( m_pCBperFrame, "m_pCBperFrame");

		// Create the vertex buffer
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.ByteWidth = sizeof(short);
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bd.CPUAccessFlags = 0;

		V_RETURN(pd3dDevice->CreateBuffer(&bd, NULL, &m_pScreenQuadVB));
		DXUT_SetDebugName( m_pScreenQuadVB, "m_pScreenQuadVB");


		D3D11_TEXTURE2D_DESC tmdesc;
		ZeroMemory( &tmdesc, sizeof( D3D11_TEXTURE2D_DESC ) );
		tmdesc.ArraySize = 1;
		tmdesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		tmdesc.Usage = D3D11_USAGE_DEFAULT;
		tmdesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		tmdesc.Width = m_uOutputTexSize_x;
		tmdesc.Height = m_uOutputTexSize_y;
		tmdesc.MipLevels = 1;
		tmdesc.SampleDesc.Count = 1;

		char temp[100];
		for(int i=0;i<7;i++){
			V_RETURN( pd3dDevice->CreateTexture2D( &tmdesc, NULL, &m_pSumOfCoordTex[i] ) );
			sprintf_s(temp,"PoseEstimator_m_pSumOfCoordTex_%d",i);
			DXUT_SetDebugName( m_pSumOfCoordTex[0], temp);
		}

		// Create the render target view
		D3D11_RENDER_TARGET_VIEW_DESC DescRT;
		DescRT.Format = tmdesc.Format;
		DescRT.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		DescRT.Texture2D.MipSlice = 0;

		for(int i=0;i<7;i++){
			V_RETURN( pd3dDevice->CreateRenderTargetView( m_pSumOfCoordTex[i], &DescRT, &m_pSumOfCoordRTV[i] ) );
			sprintf_s(temp,"PoseEstimator_m_pSumOfCoordRTV_%d",i);
			DXUT_SetDebugName( m_pSumOfCoordRTV[i],temp );
		}

		D3D11_SHADER_RESOURCE_VIEW_DESC nSRVDesc;
		nSRVDesc.Format = tmdesc.Format;
		nSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		nSRVDesc.Texture2D.MipLevels = 1;
		nSRVDesc.Texture2D.MostDetailedMip = 0;
		for(int i=0;i<7;i++){
			V_RETURN( pd3dDevice->CreateShaderResourceView( m_pSumOfCoordTex[0], &nSRVDesc, &m_pSumOfCoordSRV[i] ) );
			sprintf_s(temp,"PoseEstimator_m_pSumOfCoordSRV_%d",i);
			DXUT_SetDebugName( m_pSumOfCoordSRV[i],temp );
		}

		m_Reduction.CreateResource(pd3dDevice, m_pSumOfCoordTex,7);

		m_ViewPort.Width = m_uOutputTexSize_x;
		m_ViewPort.Height = m_uOutputTexSize_y;
		m_ViewPort.MinDepth = 0.0f;
		m_ViewPort.MaxDepth = 1.0f;
		m_ViewPort.TopLeftX = 0;
		m_ViewPort.TopLeftY = 0; 

		return hr;
	}

	~PoseEstimator()
	{

	}

	void Resize(ID3D11DeviceContext* pd3dimmediateContext,const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc = NULL)
	{
		pd3dimmediateContext->UpdateSubresource(m_pCBperCall,0,NULL,&m_CBperCall,0,0);
	}

	void Release()
	{
		m_Reduction.Release();
		SAFE_RELEASE(m_pPassVS);
		SAFE_RELEASE(m_pPS_I);
		SAFE_RELEASE(m_pPS_II);
		SAFE_RELEASE(m_pGS);
		SAFE_RELEASE(m_pScreenQuadIL);
		SAFE_RELEASE(m_pScreenQuadVB);
		SAFE_RELEASE(m_pCBperCall);
		SAFE_RELEASE(m_pCBperFrame);

		for(int i=0;i<7;i++){
			SAFE_RELEASE(m_pSumOfCoordTex[i]);
			SAFE_RELEASE(m_pSumOfCoordRTV[i]);
			SAFE_RELEASE(m_pSumOfCoordSRV[i]);
		}
	}

	bool Processing(ID3D11DeviceContext* pd3dImmediateContext)
	{
		
		pd3dImmediateContext->IASetInputLayout(m_pScreenQuadIL);
		pd3dImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
		UINT stride = 0;
		UINT offset = 0;
		pd3dImmediateContext->IASetVertexBuffers(0, 1, &m_pScreenQuadVB, &stride, &offset);
		pd3dImmediateContext->VSSetShader( m_pPassVS, NULL, 0 );
		pd3dImmediateContext->RSSetViewports( 1, &m_ViewPort );
		pd3dImmediateContext->PSSetConstantBuffers(1,1,&m_pCBperFrame);
		pd3dImmediateContext->GSSetConstantBuffers(0,1,&m_pCBperCall);
		//Every frame overwrite the whole texture, so no need to clear rendertarget
		pd3dImmediateContext->GSSetShader(m_pGS,NULL,0);	
		pd3dImmediateContext->PSSetShader( m_pPS_I, NULL, 0 );
		
		m_CBperFrame.mKinectMesh = XMMatrixTranspose ( m_pKinectTPC->mModelM_now );
		m_CBperFrame.mTsdfMesh = XMMatrixTranspose ( m_pTsdfTPC->mModelM_now );
		pd3dImmediateContext->UpdateSubresource(m_pCBperFrame,0,NULL,&m_CBperFrame,0,0);

		pd3dImmediateContext->OMSetRenderTargets(4,&m_pSumOfCoordRTV[0],NULL);
		pd3dImmediateContext->PSSetShaderResources(0, 1, m_pKinectTPC->ppMeshRGBZTexSRV);
		pd3dImmediateContext->PSSetShaderResources(1, 1, m_pTsdfTPC->ppMeshRGBZTexSRV);
		pd3dImmediateContext->PSSetShaderResources(2, 1, m_pKinectTPC->ppMeshNormalTexSRV);
		pd3dImmediateContext->PSSetShaderResources(3, 1, m_pTsdfTPC->ppMeshNormalTexSRV);
		pd3dImmediateContext->Draw(1,0);
		pd3dImmediateContext->PSSetShader(m_pPS_II,NULL,0);
		pd3dImmediateContext->OMSetRenderTargets(3,&m_pSumOfCoordRTV[4],NULL);
		pd3dImmediateContext->Draw(1,0);


		ID3D11ShaderResourceView* ppSRVNULLs[4] = { NULL,NULL,NULL,NULL};
		pd3dImmediateContext->PSSetShaderResources( 0, 4, ppSRVNULLs );

		m_Reduction.Processing(pd3dImmediateContext);

		m_mA(0,0) = m_Reduction.m_vfAnswer[0].x;
		m_mA(1,0) = m_mA(0,1) = m_Reduction.m_vfAnswer[0].y;
		m_mA(2,0) = m_mA(0,2) = m_Reduction.m_vfAnswer[0].z;
		m_mA(3,0) = m_mA(0,3) = m_Reduction.m_vfAnswer[0].w;
		m_mA(4,0) = m_mA(0,4) = m_Reduction.m_vfAnswer[1].x;
		m_mA(5,0) = m_mA(0,5) = m_Reduction.m_vfAnswer[1].y;
		m_mA(1,1) = m_Reduction.m_vfAnswer[1].z;
		m_mA(2,1) = m_mA(1,2) = m_Reduction.m_vfAnswer[1].w;
		m_mA(3,1) = m_mA(1,3) = m_Reduction.m_vfAnswer[2].x;
		m_mA(4,1) = m_mA(1,4) = m_Reduction.m_vfAnswer[2].y;
		m_mA(5,1) = m_mA(1,5) = m_Reduction.m_vfAnswer[2].z;
		m_mA(2,2) = m_Reduction.m_vfAnswer[2].w;
		m_mA(3,2) = m_mA(2,3) = m_Reduction.m_vfAnswer[3].x;
		m_mA(4,2) = m_mA(2,4) = m_Reduction.m_vfAnswer[3].y;
		m_mA(5,2) = m_mA(2,5) = m_Reduction.m_vfAnswer[3].z;
		m_mA(3,3) = m_Reduction.m_vfAnswer[3].w;
		m_mA(4,3) = m_mA(3,4) = m_Reduction.m_vfAnswer[4].x;
		m_mA(5,3) = m_mA(3,5) = m_Reduction.m_vfAnswer[4].y;
		m_mA(4,4) = m_Reduction.m_vfAnswer[4].z;
		m_mA(5,4) = m_mA(4,5) = m_Reduction.m_vfAnswer[4].w;
		m_mA(5,5) = m_Reduction.m_vfAnswer[5].x;
		 
		m_vB(0,0) = -m_Reduction.m_vfAnswer[5].y;
		m_vB(1,0) = -m_Reduction.m_vfAnswer[5].z;
		m_vB(2,0) = -m_Reduction.m_vfAnswer[5].w;
		m_vB(3,0) = -m_Reduction.m_vfAnswer[6].x;
		m_vB(4,0) = -m_Reduction.m_vfAnswer[6].y;
		m_vB(5,0) = -m_Reduction.m_vfAnswer[6].z;

		m_fPreNpairs = m_fNpairs;
		m_fNpairs = m_Reduction.m_vfAnswer[6].w;

		/*if( m_fNpairs / m_fPreNpairs < 0.7 )
		{
			m_bBadDepthMap = true;
			return false;
		}*/

		m_bBadDepthMap = false;


		m_dDet = m_mA.determinant();
	
		if(fabs(m_dDet) < 1e-15){
		//if(m_dDet < 1e-6){
			m_bSigularMatrix = true;
			return false;
		}
		m_bSigularMatrix = false;
		m_vX = m_mA.llt().solve(m_vB).cast<float>();

		m_vIncRotate = XMLoadFloat3 ( &XMFLOAT3 ( m_vX(0), m_vX(1), m_vX(2) ) );
		m_vIncTran = XMLoadFloat3 ( &XMFLOAT3 ( m_vX(3), m_vX(4), m_vX(5) ) );
		m_fAlpha = m_vX(0);
		m_fBeta = m_vX(1);
		m_fGamma = m_vX(2);
		m_fXoffset = m_vX(3);
		m_fYoffset = m_vX(4);
		m_fZoffset = m_vX(5);

	/*	if(m_fAlpha > SMALL_ROTATE ||
			m_fBeta > SMALL_ROTATE ||
			m_fGamma > SMALL_ROTATE )
		{
			m_mRinc = XMMatrixIdentity();
			m_mTinc = XMMatrixIdentity();
			return false;
		}*/

		return true;

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
					m_vIncRotate = XMLoadFloat3 ( &XMFLOAT3 ( 0, 0, 0 ) );
					m_vIncTran = XMLoadFloat3 ( &XMFLOAT3 ( 0, 0, 0 ) );

				}
				break;
			}
		}

		return 0;
	}
};