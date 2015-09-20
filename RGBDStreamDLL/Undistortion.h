/* Class for Image distortion correction.
 * Given img SRV and the camera distortion coefficients, 
 * it will generate the undistorted img srv
 * Right now it only support 5 distortion coefficients
 * k1, k2, k3, p1, p2
 * Requires DirectX11
 * author: Peng Liu <peng.liu916@gmail.com>
 * version: July 2014
 */
#pragma once

#ifdef RGBDSTREAMDLL_EXPORTS
#define RGBDSTREAMDLL_API __declspec(dllexport) 
#else
#define RGBDSTREAMDLL_API __declspec(dllimport) 
#endif

#include <d3d11.h>
#include <sstream>
#include "DXUT.h"

using namespace DirectX;
using namespace std;

class RGBDSTREAMDLL_API Undistortion
{
public:
	struct CB_once{
		XMFLOAT4 k;
		XMFLOAT2 p;
		XMFLOAT2 f;
		XMFLOAT2 c;
		XMUINT2 reso;
	};

	// flag for shader compile
	const UINT					m_uCompileFlag;

	// str for shade code
	string						m_strShader;

	ID3D11VertexShader*			m_pPassVS;
	ID3D11GeometryShader*		m_pQuadGS;
	ID3D11PixelShader*			m_pUndistortPS;
	ID3D11InputLayout*			m_pPassIL;
	ID3D11Buffer*				m_pPassVB;

	// Input data
	ID3D11ShaderResourceView**	m_ppInputSRV;
	UINT						m_uInputWidth;
	UINT						m_uInputHeight;

	// output data
	ID3D11Texture2D*			m_pOutTex;
	ID3D11ShaderResourceView*	m_pOutSRV;
	ID3D11RenderTargetView*		m_pOutRTV;

	// define the view port
	D3D11_VIEWPORT				m_RTviewport;

	// data send to GPU once
	ID3D11Buffer*				m_pCBonce;

	// indicate the input contain color or not
	bool						m_bHasColor;

	// utility function
	HRESULT CompileFormString(string code,
							  const D3D_SHADER_MACRO* pDefines,
							  LPCSTR pEntrypoint, LPCSTR pTarget,
							  UINT Flags1, UINT Flags2,
							  ID3DBlob** ppCode);
	void GenerateShaderStr(bool hasColor);

	Undistortion();
	// DXUT hook functions
	HRESULT CreateResource(ID3D11Device* pd3dDevice,
						   ID3D11ShaderResourceView** ppInputSRV,
						   UINT inputWidth, UINT inputHeight, bool hasColor,
						   XMFLOAT4 _k, XMFLOAT2 _p, XMFLOAT2 _f, XMFLOAT2 _c );
	HRESULT Resize(ID3D11Device* pd3dDevice, int width, int height);
	void Update(float fElapsedTime);
	void Render(ID3D11DeviceContext* pd3dImmediateContext);
	void Release();
	void Destory();

	~Undistortion();
};

