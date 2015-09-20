/* Class for showing point cloud.
 * Given depth SRV, color SRV (optional), it will render
 * interactive point cloud
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
#include "DXUTcamera.h"	// CModelViewerCamera
//#include "SDKmisc.h"	// DXUTCompileFromFile

using namespace DirectX;
using namespace std;
class RGBDSTREAMDLL_API PointCloudVisualizer
{
public:
	struct CB_perCall{
		XMUINT2 reso;
		XMFLOAT2 fLength;
		XMFLOAT2 principalPt;
		XMFLOAT2 xz_offset;
	};
	struct CB_perFrame{
		XMMATRIX mWorldViewProj;
	};

	// flag for shader compile
	const UINT					m_uCompileFlag; 

	// str for shade code
	string						m_strShader;

	ID3D11VertexShader*			m_pPointCloudVS;
	ID3D11GeometryShader*		m_pPointCloudGS;
	ID3D11PixelShader*			m_pPointCloudPS;
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
	ID3D11Texture2D*			m_pOutputStencilTexture2D;
	ID3D11DepthStencilView*		m_pOutputStencilView;

	// define the view port
	D3D11_VIEWPORT				m_RTviewport;

	// interactive mouse interface
	CModelViewerCamera			m_Camera;

	// data send to GPU once
	ID3D11Buffer*				m_pCBperCall;

	// data send to GPU every frame
	ID3D11Buffer*				m_pCBperFrame;

	// Reso of output RTV
	UINT						m_uRTWidth;
	UINT						m_uRTHeight;

	// rough indicate of the radius of the object
	float						m_fObjectRadius;

	// utility function
	HRESULT CompileFormString(string code,
							const D3D_SHADER_MACRO* pDefines,
							LPCSTR pEntrypoint, LPCSTR pTarget,
							UINT Flags1, UINT Flags2,
							ID3DBlob** ppCode);
	void GenerateShaderStr(bool hasColor);

	PointCloudVisualizer();

	// DXUT hook functions
	HRESULT CreateResource(ID3D11Device* pd3dDevice, 
						   ID3D11ShaderResourceView** ppInputSRV, 
						   UINT inputWidth, UINT inputHeight, bool hasColor,
						   XMFLOAT2 fLength, XMFLOAT2 principalPt);
	HRESULT Resize(ID3D11Device* pd3dDevice, int width, int height);
	void Update( float fElapsedTime );
	void Render( ID3D11DeviceContext* pd3dImmediateContext );
	void Release();
	void Destory();
	LRESULT HandleMessages(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

	~PointCloudVisualizer();
};

