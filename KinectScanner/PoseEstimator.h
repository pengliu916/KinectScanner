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
struct CB_PoseEstimator_PerFrame
{
	XMMATRIX mKinect;
	XMINT2 uUAVDim;
	UINT uStepLen;
	UINT NIU;
};

enum class AccessSize{
	fullSize = 0,
	halfSize = 1,
	quaterSize = 2,
};

class PoseEstimator
{
public:
	ID3D11ComputeShader*			m_pCS;

	CB_PoseEstimator_PerFrame		m_CBperFrame;
	ID3D11Buffer*					m_pCBperFrame;

	//For Intermediate Data
	ID3D11Buffer*					m_pInterDataBuf[3][7];
	ID3D11UnorderedAccessView*		m_pInterDataUAV[3][7];
	ID3D11ShaderResourceView*		m_pInterDataSRV[3][7];

	Reduction						m_Reduction;

	//Input Data
	TransformedPointClould*			m_pTsdfTPC;
	TransformedPointClould*			m_pKinectTPC;

	UINT			m_uDim_x;
	UINT			m_uDim_y;

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
		m_uDim_x = _inputWidth;
		m_uDim_y = _inputHeight;
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

		ID3DBlob* pCSBlob = NULL;
		V_RETURN(DXUTCompileFromFile(L"PoseEstimator.fx", nullptr, "CS", "cs_5_0", COMPILE_FLAG, 0, &pCSBlob));
		V_RETURN(pd3dDevice->CreateComputeShader(pCSBlob->GetBufferPointer(),pCSBlob->GetBufferSize(),NULL,&m_pCS));
		DXUT_SetDebugName(m_pCS,"m_pCS");

		// create constant buffer
		D3D11_BUFFER_DESC bd = {0};
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		bd.CPUAccessFlags = 0;
		bd.ByteWidth = sizeof(CB_PoseEstimator_PerFrame);
		V_RETURN(pd3dDevice->CreateBuffer( &bd, NULL, &m_pCBperFrame ));
		DXUT_SetDebugName( m_pCBperFrame, "m_pCBperFrame");

		// create InterData buffer
		D3D11_BUFFER_DESC BUFDesc;
		ZeroMemory(&BUFDesc, sizeof(BUFDesc));
		BUFDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		BUFDesc.CPUAccessFlags = 0;
		BUFDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		BUFDesc.StructureByteStride = sizeof(XMFLOAT4);
		BUFDesc.Usage = D3D11_USAGE_DEFAULT;
		BUFDesc.ByteWidth = m_uDim_x * m_uDim_y * sizeof(XMFLOAT4);
		for(int i=0;i<7;i++){
			V_RETURN(pd3dDevice->CreateBuffer(&BUFDesc, nullptr, &m_pInterDataBuf[0][i]));
			//DXUT_SetDebugName(m_pInterDataBuf[0][i], "m_pInterDataBuf[0]");
		}
		BUFDesc.ByteWidth = (m_uDim_x / 2) * (m_uDim_y / 2) * sizeof(XMFLOAT4);
		for (int i = 0; i < 7; i++){
			V_RETURN(pd3dDevice->CreateBuffer(&BUFDesc, nullptr, &m_pInterDataBuf[1][i]));
			//DXUT_SetDebugName(m_pInterDataBuf[1][i], "m_pInterDataBuf[1]");
		}
		BUFDesc.ByteWidth = (m_uDim_x / 4) * (m_uDim_y / 4) * sizeof(XMFLOAT4); 
		for (int i = 0; i < 7; i++){
			V_RETURN(pd3dDevice->CreateBuffer(&BUFDesc, nullptr, &m_pInterDataBuf[2][i]));
			//DXUT_SetDebugName(m_pInterDataBuf[2][i], "m_pInterDataBuf[2]");
		}

		// Create the resource view
		D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
		ZeroMemory(&SRVDesc, sizeof(SRVDesc));
		SRVDesc.Format = DXGI_FORMAT_UNKNOWN;
		SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		SRVDesc.Buffer.FirstElement = 0;
		SRVDesc.Buffer.NumElements = m_uDim_x * m_uDim_y * sizeof(XMFLOAT4) / BUFDesc.StructureByteStride;
		for (int i = 0; i < 7; i++){
			V_RETURN(pd3dDevice->CreateShaderResourceView(m_pInterDataBuf[0][i], &SRVDesc, &m_pInterDataSRV[0][i]));
			//DXUT_SetDebugName(m_pInterDataSRV[0], "m_pInterDataSRV[0]");
		}
		SRVDesc.Buffer.NumElements = (m_uDim_x / 2) * (m_uDim_y / 2) * sizeof(XMFLOAT4) / BUFDesc.StructureByteStride;
		for (int i = 0; i < 7; i++){
			V_RETURN(pd3dDevice->CreateShaderResourceView(m_pInterDataBuf[1][i], &SRVDesc, &m_pInterDataSRV[1][i]));
			//DXUT_SetDebugName(m_pInterDataSRV[1], "m_pInterDataSRV[1]");
		}
		SRVDesc.Buffer.NumElements = (m_uDim_x / 4) * (m_uDim_y / 4) * sizeof(XMFLOAT4) / BUFDesc.StructureByteStride;
		for (int i = 0; i < 7; i++){
			V_RETURN(pd3dDevice->CreateShaderResourceView(m_pInterDataBuf[2][i], &SRVDesc, &m_pInterDataSRV[2][i]));
			//DXUT_SetDebugName(m_pInterDataSRV[2], "m_pInterDataSRV[2]");
		}

		D3D11_UNORDERED_ACCESS_VIEW_DESC UAVDesc;
		ZeroMemory(&UAVDesc, sizeof(UAVDesc));
		UAVDesc.Format = DXGI_FORMAT_UNKNOWN;
		UAVDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		UAVDesc.Buffer.FirstElement = 0;
		UAVDesc.Buffer.NumElements = m_uDim_x * m_uDim_y * sizeof(XMFLOAT4) / BUFDesc.StructureByteStride;
		for (int i = 0; i < 7; i++){
			V_RETURN(pd3dDevice->CreateUnorderedAccessView(m_pInterDataBuf[0][i], &UAVDesc, &m_pInterDataUAV[0][i]));
			//DXUT_SetDebugName(m_pInterDataSRV[0], "m_pInterDataSRV[0]");
		}
		UAVDesc.Buffer.NumElements = (m_uDim_x / 2) * (m_uDim_y / 2) * sizeof(XMFLOAT4) / BUFDesc.StructureByteStride;
		for (int i = 0; i < 7; i++){
			V_RETURN(pd3dDevice->CreateUnorderedAccessView(m_pInterDataBuf[1][i], &UAVDesc, &m_pInterDataUAV[1][i]));
			//XUT_SetDebugName(m_pInterDataSRV[1], "m_pInterDataSRV[1]");
		}
		UAVDesc.Buffer.NumElements = (m_uDim_x / 4) * (m_uDim_y / 4) * sizeof(XMFLOAT4) / BUFDesc.StructureByteStride;
		for (int i = 0; i < 7; i++){
			V_RETURN(pd3dDevice->CreateUnorderedAccessView(m_pInterDataBuf[2][i], &UAVDesc, &m_pInterDataUAV[2][i]));
			//DXUT_SetDebugName(m_pInterDataSRV[2], "m_pInterDataSRV[2]");
		}

		V_RETURN(m_Reduction.CreateResource(pd3dDevice));
		return hr;
	}

	~PoseEstimator()
	{

	}

	void Resize(ID3D11DeviceContext* pd3dimmediateContext,const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc = NULL)
	{
	}

	void Release()
	{
		m_Reduction.Release();
		SAFE_RELEASE(m_pCS);
		SAFE_RELEASE(m_pCBperFrame);
		for(int i=0;i<3;i++)
			for(int j=0;j<7;j++){
				SAFE_RELEASE(m_pInterDataBuf[i][j]);
				SAFE_RELEASE(m_pInterDataSRV[i][j]);
				SAFE_RELEASE(m_pInterDataUAV[i][j]);
			}
	}

	bool Processing(ID3D11DeviceContext* pd3dImmediateContext, AccessSize Asize = AccessSize::fullSize;)
	{
		DXUT_BeginPerfEvent(DXUT_PERFEVENTCOLOR, L"PoseEstimat Computing");

		UINT uStepSize = pow(2,(int)Asize);
		pd3dImmediateContext->CSSetConstantBuffers(0, 1, &m_pCBperFrame);
		pd3dImmediateContext->CSSetShader( m_pCS, NULL, 0 );
		
		m_CBperFrame.mKinect = XMMatrixTranspose ( m_pKinectTPC->mCurFrame );
		m_CBperFrame.uStepLen = uStepSize;
		m_CBperFrame.uUAVDim.x = m_uDim_x / uStepSize;
		m_CBperFrame.uUAVDim.y = m_uDim_y / uStepSize;
		pd3dImmediateContext->UpdateSubresource(m_pCBperFrame,0,NULL,&m_CBperFrame,0,0);

		UINT initCounts = 0;
		pd3dImmediateContext->CSSetUnorderedAccessViews(0,7,m_pInterDataUAV[(int)Asize], &initCounts);
		ID3D11ShaderResourceView* srvs[4] = {*m_pKinectTPC->ppMeshRGBZTexSRV, *m_pTsdfTPC->ppMeshRGBZTexSRV,*m_pKinectTPC->ppMeshNormalTexSRV,*m_pTsdfTPC->ppMeshNormalTexSRV};
		pd3dImmediateContext->CSSetShaderResources(0, 4, srvs);

		pd3dImmediateContext->Dispatch((UINT)ceil((float)m_CBperFrame.uUAVDim.x / uStepSize / THREAD2D_X), (UINT)ceil((float)m_CBperFrame.uUAVDim.y/uStepSize / THREAD2D_Y), 1);

		ID3D11ShaderResourceView* ppSRVNULLs[4] = { NULL,NULL,NULL,NULL};
		pd3dImmediateContext->CSSetShaderResources( 0, 4, ppSRVNULLs );

		ID3D11UnorderedAccessView* ppUAViewNULL[7] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL };
		pd3dImmediateContext->CSSetUnorderedAccessViews(0, 7, ppUAViewNULL, &initCounts);

		m_Reduction.Processing(pd3dImmediateContext,
							   m_pInterDataUAV[(int)Asize],
							   m_pInterDataBuf[(int)Asize],
							   m_CBperFrame.uUAVDim.x*m_CBperFrame.uUAVDim.y);

		m_mA(0,0) = m_Reduction.m_structSum.f4Data0.x;
		m_mA(1,0) = m_mA(0,1) = m_Reduction.m_structSum.f4Data0.y;
		m_mA(2,0) = m_mA(0,2) = m_Reduction.m_structSum.f4Data0.z;
		m_mA(3,0) = m_mA(0,3) = m_Reduction.m_structSum.f4Data0.w;
		m_mA(4,0) = m_mA(0,4) = m_Reduction.m_structSum.f4Data1.x;
		m_mA(5,0) = m_mA(0,5) = m_Reduction.m_structSum.f4Data1.y;
		m_mA(1,1) = m_Reduction.m_structSum.f4Data1.z;
		m_mA(2,1) = m_mA(1,2) = m_Reduction.m_structSum.f4Data1.w;
		m_mA(3,1) = m_mA(1,3) = m_Reduction.m_structSum.f4Data2.x;
		m_mA(4,1) = m_mA(1,4) = m_Reduction.m_structSum.f4Data2.y;
		m_mA(5,1) = m_mA(1,5) = m_Reduction.m_structSum.f4Data2.z;
		m_mA(2,2) = m_Reduction.m_structSum.f4Data2.w;
		m_mA(3,2) = m_mA(2,3) = m_Reduction.m_structSum.f4Data3.x;
		m_mA(4,2) = m_mA(2,4) = m_Reduction.m_structSum.f4Data3.y;
		m_mA(5,2) = m_mA(2,5) = m_Reduction.m_structSum.f4Data3.z;
		m_mA(3,3) = m_Reduction.m_structSum.f4Data3.w;
		m_mA(4,3) = m_mA(3,4) = m_Reduction.m_structSum.f4Data4.x;
		m_mA(5,3) = m_mA(3,5) = m_Reduction.m_structSum.f4Data4.y;
		m_mA(4,4) = m_Reduction.m_structSum.f4Data4.z;
		m_mA(5,4) = m_mA(4,5) = m_Reduction.m_structSum.f4Data4.w;
		m_mA(5,5) = m_Reduction.m_structSum.f4Data5.x;
		 
		m_vB(0,0) = -m_Reduction.m_structSum.f4Data5.y;
		m_vB(1,0) = -m_Reduction.m_structSum.f4Data5.z;
		m_vB(2,0) = -m_Reduction.m_structSum.f4Data5.w;
		m_vB(3,0) = -m_Reduction.m_structSum.f4Data6.x;
		m_vB(4,0) = -m_Reduction.m_structSum.f4Data6.y;
		m_vB(5,0) = -m_Reduction.m_structSum.f4Data6.z;

		m_fPreNpairs = m_fNpairs;
		m_fNpairs = m_Reduction.m_structSum.f4Data6.w;

		/*if( m_fNpairs / m_fPreNpairs < 0.7 )
		{
			m_bBadDepthMap = true;
			return false;
		}*/

		m_bBadDepthMap = false;


		//m_dDet = m_mA.determinant();
	
		if(fabs(m_dDet) < 1e-15){
		//if(m_dDet < 1e-6){
			m_bSigularMatrix = true;
			return false;
		}
		m_bSigularMatrix = false;
		//m_vX = m_mA.llt().solve(m_vB).cast<float>();

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
		DXUT_EndPerfEvent();

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