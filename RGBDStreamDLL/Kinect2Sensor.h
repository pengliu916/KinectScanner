/* Helper class for Kinect2, include functions to communicate
 * with DirectX and OpenCV.
 * Requires DirectX11 OpenCV 1.x
 * author Peng Liu <peng.liu916@gmail.com>
 */
#pragma once
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

class Kinect2Sensor : public IRGBDStreamForOpenCV, public IRGBDStreamForDirectX{
	static const int        m_cDepthWidth = 512;
	static const int        m_cDepthHeight = 424;
	static const int        m_cColorWidth = 1920;
	static const int        m_cColorHeight = 1080;

	static const float      m_fDistorParams[5];

public:
	// Current Kinect
	IKinectSensor*                      m_pKinect2Sensor;
	ICoordinateMapper*                  m_pCoordinateMapper;

	// Frame reader
	IMultiSourceFrameReader*            m_pMultiSourceFrameReader;



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

	// DirectX resource for undistorted RGBD texture
	ID3D11VertexShader*                 m_pPassVS;
	ID3D11GeometryShader*               m_pQuadGS;
	ID3D11PixelShader*                  m_pTexPS;
	ID3D11InputLayout*                  m_pPassIL;
	ID3D11Buffer*                       m_pPassVB;
	ID3D11SamplerState*                 m_pNearestNeighborSS;
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
	HRESULT MapColorToDepth();
	// For OpenCV interface
	virtual void GetColorMat( cv::Mat& out );
	virtual void GetDepthMat( cv::Mat& out );
	virtual void GetInfraredMat(cv::Mat& out);
	virtual bool UpdateMats(bool defaultReg, bool color, bool depth, bool infrared);

	// For DirectX
	HRESULT ProcessDepth( ID3D11DeviceContext* pd3dimmediateContext, const UINT16* pDepthBuffer, int iDepthBufferSize );
	HRESULT ProcessColor( ID3D11DeviceContext* pd3dimmediateContext, const BYTE* pColorBuffer, int iColorBufferSize );
	HRESULT ProcessInfrared( ID3D11DeviceContext* pd3dimmediateContext, const UINT16* pInfraredBuffer, int iInfraredBufferSize);
	HRESULT MapColorToDepth( ID3D11DeviceContext* pd3dimmediateContext );
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
	// DirectX resource for undistortion
	ID3D11Texture2D*                    m_pUndsitortedRGBDTex;
	ID3D11ShaderResourceView*           m_pUndsitortedRGBDSRV;
	ID3D11RenderTargetView*             m_pUndsitortedRGBDRTV;
	// DirectX resource for RGBD point cloud visualizer
	//CModelViewerCamera					m_ModelViewCam;
	ID3D11Texture2D*					m_pPCVisualizerTex;
	ID3D11ShaderResourceView*			m_pPCVisualizerSRV;
	ID3D11RenderTargetView*				m_pPCVisualizerRTV;
	ID3D11VertexShader*					m_pPCVisualizerVS;
	ID3D11PixelShader*					m_pPCVIsualizerPS;
	ColorSpacePoint*                    m_pColorCoordinates;
	
	virtual ID3D11ShaderResourceView** getColorLookup_ppSRV();
	HRESULT ProcessDepthColorMap(ID3D11DeviceContext* pd3dimmediatecontext, const ColorSpacePoint* pMapBuffer, int iMapBufferSize);
	string GenShaderCode();
	virtual void ProcessUndistortedRGBD(ID3D11DeviceContext* pd3dimmediatecontext);
	//virtual void VisualizeRGBD( ID3D11DeviceContext* pd3dimmediatecontext );

};