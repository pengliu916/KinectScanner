/* Helper class for Kinect2, include functions to communicate
 * with DirectX and OpenCV.
 * Requires DirectX11 OpenCV 1.x
 * author Peng Liu <peng.liu916@gmail.com>
 */
#pragma once

#ifdef RGBDSTREAMDLL_EXPORTS
#define RGBDSTREAMDLL_API __declspec(dllexport) 
#else
#define RGBDSTREAMDLL_API __declspec(dllimport) 
#endif

#include <opencv2\core\core.hpp>
#include <d3d11.h>
#include "DXUT.h"
#include "SDKmisc.h"
#include <Kinect.h>
#include <iostream>
#include "IRGBDStreamForOpenCV.h"
#include "IRGBDStreamForDirectX.h"

#define COMPILE_FLAG D3DCOMPILE_ENABLE_STRICTNESS
using namespace std;

#define KINECT2_DEPTH_K "float4(8.5760834831004232e-002, -2.5095061439661859e-001, 7.7320261202870319e-002, 0)"
#define KINECT2_DEPTH_P "float2(-1.2027173455808845e-003, -3.0661909113997252e-004)"
#define KINECT2_DEPTH_F "float2(-3.6172484623525816e+002, -3.6121411187495357e+002)"
#define KINECT2_DEPTH_C "float2(2.5681568188916287e+002, 2.0554866916495337e+002)"

class RGBDSTREAMDLL_API Kinect2Sensor : public IRGBDStreamForOpenCV, public IRGBDStreamForDirectX{
	static const int        m_cDepthWidth = 512;
	static const int        m_cDepthHeight = 424;
	static const int        m_cColorWidth = 1920;
	static const int        m_cColorHeight = 1080;

public:
	// Current Kinect
	IKinectSensor*                      m_pKinect2Sensor;

	// Frame reader
	IMultiSourceFrameReader*            m_pMultiSourceFrameReader;
	ICoordinateMapper*                  m_pCoordinateMapper;
	ColorSpacePoint*                    m_pColorCoordinates;



	UINT16*                             m_pDepthBuffer;
	UINT                                m_uDepthBufferSize;
	BYTE*                               m_pColorBuffer;
	UINT                                m_uColorBufferSize;
	UINT16*                             m_pInfraredBuffer;
	UINT                                m_pInfraredBufferSize;

	bool                                m_bLongExposureInfrared;
	bool                                m_bUpdated;

	// For OpenCV Interface
	cv::Mat                             m_matColor;
	cv::Mat                             m_matDepth;
	cv::Mat                             m_matInfrared;

	// Color and depth resource for DirectX
	ID3D11Texture2D*                    m_pDepthTex;
	ID3D11ShaderResourceView*           m_pDepthSRV;

	ID3D11Texture2D*                    m_pColorTex;
	ID3D11ShaderResourceView*           m_pColorSRV;

	ID3D11Texture2D*                    m_pInfraredTex;
	ID3D11ShaderResourceView*           m_pInfraredSRV;

	ID3D11Texture2D*                    m_pColLookupForDetphTex;
	ID3D11ShaderResourceView*           m_pColLookupForDepthSRV;

	// DirectX resource for finalized RGBD texture
	ID3D11VertexShader*                 m_pPassVS;
	ID3D11GeometryShader*               m_pQuadGS;
	ID3D11PixelShader*                  m_pFinalizedRGBDPS;
	ID3D11InputLayout*                  m_pPassIL;
	ID3D11Buffer*                       m_pPassVB;
	ID3D11SamplerState*                 m_pNearestNeighborSS;
	
	// flag for shader compile
	const UINT					m_uCompileFlag;

	// output data
	ID3D11Texture2D*					m_pFinalizedRGBDTex;
	ID3D11ShaderResourceView*			m_pFinalizedRGBDSRV;
	ID3D11RenderTargetView*				m_pFinalizedRGBDRTV;

	D3D11_VIEWPORT                      m_RTviewport;

	ID3D11ShaderResourceView**          m_ppInputSRV;
	ID3D11ShaderResourceView**          m_ppNullSRV;

	string                              m_strShaderCode;

	Kinect2Sensor();
	virtual HRESULT Initialize();
	virtual void GetColorReso( int& iColorWidth, int& iColorHeight );
	virtual void GetDepthReso( int& iDepthWidth, int& iDepthHeight );
	virtual void GetInfraredReso(int& iInfraredWidth, int& iInfraredHeight);
	// For OpenCV
	HRESULT ProcessDepth(const UINT16* pDepthBuffer, int iDepthBufferSize);
	HRESULT ProcessColor(const BYTE* pColorBuffer, int iColorBufferSize);
	HRESULT ProcessInfrared(const UINT16* pInfraredBuffer, int iInfraredBufferSize);
	// For OpenCV interface
	virtual void GetColorMat( cv::Mat& out );
	virtual void GetDepthMat( cv::Mat& out );
	virtual void GetInfraredMat(cv::Mat& out);
	virtual bool UpdateMats(bool defaultReg, bool color, bool depth, bool infrared);

	// For DirectX
	HRESULT ProcessDepth( ID3D11DeviceContext* pd3dimmediateContext, const UINT16* pDepthBuffer, int iDepthBufferSize );
	HRESULT ProcessColor( ID3D11DeviceContext* pd3dimmediateContext, const BYTE* pColorBuffer, int iColorBufferSize );
	HRESULT ProcessInfrared( ID3D11DeviceContext* pd3dimmediateContext, const UINT16* pInfraredBuffer, int iInfraredBufferSize);
	HRESULT ProcessDepthColorMap(ID3D11DeviceContext* pd3dimmediatecontext, const ColorSpacePoint* pMapBuffer, int iMapBufferSize);

	// For DirectX interface
	virtual HRESULT CreateResource( ID3D11Device* );
	virtual bool UpdateTextures( ID3D11DeviceContext*,
								 bool defaultReg, bool color, bool depth, bool infrared);
	virtual void Release();
	virtual LRESULT HandleMessages( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam );
	virtual ID3D11ShaderResourceView** getColor_ppSRV();
	virtual ID3D11ShaderResourceView** getDepth_ppSRV();
	virtual ID3D11ShaderResourceView** getInfrared_ppSRV();

	virtual ~Kinect2Sensor();


	// Utility member and functions for DirectX, not in the IRGBDStreamForDirectX interface
	// DirectX resource for RGBD point cloud visualizer
	
	virtual ID3D11ShaderResourceView** getColorLookup_ppSRV();
	void GenShaderCode();
	HRESULT CompileFormString(string code,
					  const D3D_SHADER_MACRO* pDefines,
					  LPCSTR pEntrypoint, LPCSTR pTarget,
					  UINT Flags1, UINT Flags2,
					  ID3DBlob** ppCode);
	void ProcessFinalizedRGBD(ID3D11DeviceContext* pd3dimmediatecontext);
	//virtual void VisualizeRGBD( ID3D11DeviceContext* pd3dimmediatecontext );

};