/* Class for Depth Color registration.
 * Given undistorted color and depth srv,
 * and the essential matrix E
 * it will generate the "color contamination free" registrated rgbd srv
 * Requires DirectX11
 * author: Peng Liu <peng.liu916@gmail.com>
 * version: Augest 2014
 */
#pragma once
#ifdef RGBDSTREAMDLL_EXPORTS
#define RGBDSTREAMDLL_API __declspec(dllexport) 
#else
#define RGBDSTREAMDLL_API __declspec(dllimport) 
#endif

#include <d3d11.h>
#include "DXUT.h"

using namespace DirectX;
using namespace std;

class RGBDSTREAMDLL_API DepthColorRegistration
{
public:
	struct CB_once{
		// focal length, principal pt and reso for master cam
		XMFLOAT2 f0;
		XMFLOAT2 c0;
		XMUINT2 reso0;
		// focal length, principal pt and reso for slave cam
		XMFLOAT2 f1;
		XMFLOAT2 c1;
		XMUINT2 reso1;
		// essential mat
		XMMATRIX e;
	};

	// flag for shader compile
	const UINT					m_uCompileFlag;

	// str for shade code
	string						m_strShader;

	ID3D11VertexShader*			m_pPassVS;
	ID3D11GeometryShader*		m_pQuadGS;
	ID3D11PixelShader*			m_pRegistrationPS;
	ID3D11InputLayout*			m_pPassIL;
	ID3D11Buffer*				m_pPassVB;

	// Input data
	ID3D11ShaderResourceView**	m_ppInDepthSRV;
	UINT						m_uInDepthWidth;
	UINT						m_uInDepthHeight;
	ID3D11ShaderResourceView**	m_ppInColorSRV;
	UINT						m_uInColorWidth;
	UINT						m_uInColorHeight;

	// output data
	ID3D11Texture2D*			m_pOutTex;
	ID3D11ShaderResourceView*	m_pOutSRV;
	ID3D11RenderTargetView*		m_pOutRTV;

	// define the view port
	D3D11_VIEWPORT				m_RTviewport;

	// data send to GPU once
	ID3D11Buffer*				m_pCBonce;

	// utility function
	HRESULT CompileFormString(string code,
							  const D3D_SHADER_MACRO* pDefines,
							  LPCSTR pEntrypoint, LPCSTR pTarget,
							  UINT Flags1, UINT Flags2,
							  ID3DBlob** ppCode);
	void GenerateShaderStr();

	DepthColorRegistration();
	// DXUT hook functions
	HRESULT CreateResource(ID3D11Device* pd3dDevice,
						   ID3D11ShaderResourceView** ppDepthSRV,XMUINT2 _reso0, XMFLOAT2 _f0, XMFLOAT2 _c0,
						   ID3D11ShaderResourceView** ppColorSRV,XMUINT2 _reso1, XMFLOAT2 _f1, XMFLOAT2 _c1,
						   XMMATRIX e);
	HRESULT Resize(ID3D11Device* pd3dDevice, int width, int height);
	void Update(float fElapsedTime);
	void Render(ID3D11DeviceContext* pd3dImmediateContext);
	void Release();
	void Destory();

	~DepthColorRegistration();
};

