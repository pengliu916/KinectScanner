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
	XMMATRIX					mCurFrame;
	XMMATRIX					mPreFrame;
	XMMATRIX					mCurRotation;
	XMMATRIX					mPreRotation;
	XMVECTOR					vTranslation;
	XMVECTOR					vRotation;

	TransformedPointClould()
	{
		ppMeshRGBZTexSRV = NULL;
		mCurFrame = XMMatrixTranslation(X_OFFSET, Y_OFFSET, Z_OFFSET);
		mPreFrame = XMMatrixTranslation(X_OFFSET, Y_OFFSET, Z_OFFSET);
		mCurRotation = XMMatrixTranslation ( 0, 0, 0 );
		mPreRotation = XMMatrixTranslation ( 0, 0, 0 );
		vTranslation = XMLoadFloat3(&XMFLOAT3(X_OFFSET, Y_OFFSET, Z_OFFSET));
		vRotation = XMLoadFloat3 ( &XMFLOAT3 ( 0, 0, 0 ) );
	}
	void reset()
	{
		mCurFrame = XMMatrixTranslation(X_OFFSET, Y_OFFSET, Z_OFFSET);
		mPreFrame = XMMatrixTranslation(X_OFFSET, Y_OFFSET, Z_OFFSET);
		mCurRotation = XMMatrixTranslation ( 0, 0, 0 );
		mPreRotation = XMMatrixTranslation ( 0, 0, 0 );
		vTranslation = XMLoadFloat3(&XMFLOAT3(X_OFFSET, Y_OFFSET, Z_OFFSET));
		vRotation = XMLoadFloat3 ( &XMFLOAT3 ( 0, 0, 0 ) );
	}
};