#pragma once
#include <iostream>
#include <fstream>
#include <D3D11.h>
#include "DXUT.h"
#include <DirectXMath.h>

#include "header.h"

using namespace DirectX;

class TransformedPointClould
{
public:
	ID3D11ShaderResourceView**	ppMeshRGBZTexSRV;
	ID3D11ShaderResourceView**	ppMeshRawRGBZTexSRV;
	ID3D11ShaderResourceView**	ppMeshNormalTexSRV;
	XMMATRIX					mModelM_now;
	XMMATRIX					mModelM_pre;
	XMMATRIX					mModelM_r_now;
	XMMATRIX					mModelM_r_pre;
	XMVECTOR					m_vTrans;
	XMVECTOR					m_vRotate;

	XMMATRIX					mModelT_target;
	XMMATRIX					mModelR_center;
	XMMATRIX					mModelT_center;
	XMFLOAT4					mModelV_center;

	TransformedPointClould()
	{
		ppMeshRGBZTexSRV = NULL;
		mModelM_now = XMMatrixTranslation(X_OFFSET, Y_OFFSET, Z_OFFSET);
		mModelM_pre = XMMatrixTranslation(X_OFFSET, Y_OFFSET, Z_OFFSET);
		mModelM_r_now = XMMatrixTranslation ( 0, 0, 0 );
		mModelM_r_pre = XMMatrixTranslation ( 0, 0, 0 );
		m_vTrans = XMLoadFloat3(&XMFLOAT3(X_OFFSET, Y_OFFSET, Z_OFFSET));
		m_vRotate = XMLoadFloat3 ( &XMFLOAT3 ( 0, 0, 0 ) );

		mModelT_center = XMMATRIX(XMMatrixIdentity());
		mModelT_target = XMMATRIX(XMMatrixIdentity());
	}
	void reset()
	{
		mModelM_now = XMMatrixTranslation(X_OFFSET, Y_OFFSET, Z_OFFSET);
		mModelM_pre = XMMatrixTranslation(X_OFFSET, Y_OFFSET, Z_OFFSET);
		mModelM_r_now = XMMatrixTranslation ( 0, 0, 0 );
		mModelM_r_pre = XMMatrixTranslation ( 0, 0, 0 );
		m_vTrans = XMLoadFloat3(&XMFLOAT3(X_OFFSET, Y_OFFSET, Z_OFFSET));
		m_vRotate = XMLoadFloat3 ( &XMFLOAT3 ( 0, 0, 0 ) );

		mModelR_center = XMMatrixIdentity();
		mModelT_target = XMMatrixIdentity();
		mModelT_center = XMMatrixIdentity();
	}
};