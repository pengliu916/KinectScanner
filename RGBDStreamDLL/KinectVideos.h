#pragma once
#include <thread>
#include <mutex>
#include <Windows.h>
#include <Ole2.h>
#include "NuiApi.h"
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <d3d11.h>
#include "DXUT.h"
#include "IRGBDStreamForDirectX.h"
using namespace std;

#ifndef SKIP_FRAME_NUM
#define SKIP_FRAME_NUM 0
#endif

class KinectVideos : public IRGBDStreamForDirectX{
public:
    KinectVideos(bool);
	virtual HRESULT Initialize();
	virtual void GetColorReso( int& iColorWidth, int& iColorHeight );
	virtual void GetDepthReso( int& iDepthWidth, int& iDepthHeight );
	virtual void GetInfraredReso(int& iInfraredWidth, int& iInfraredHeight);
	virtual HRESULT CreateResource(ID3D11Device* pd3dDevice);
    virtual bool UpdateTextures( ID3D11DeviceContext* pd3dimmediateContext,
                               bool defaultReg, bool color, bool depth, bool infrared);
    virtual void Release();
    virtual LRESULT HandleMessages(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    virtual ID3D11ShaderResourceView** getColor_ppSRV();
    virtual ID3D11ShaderResourceView** getDepth_ppSRV();
	virtual ID3D11ShaderResourceView** getInfrared_ppSRV();
	virtual ID3D11ShaderResourceView** getRGBD_ppSRV();
	virtual ~KinectVideos();

private:
    void Play();// Work in other stread as video stream
    // Depth input video suppose to be 1C16U but coded as 3C8U, so need to decode
    void Mat8UC3toMat16UC1(cv::Mat& inMat, cv::Mat& outMat);
    HRESULT ProcessDepth(ID3D11DeviceContext* pd3dimmediateContext);
    HRESULT ProcessColor(ID3D11DeviceContext* pd3dimmediateContext);
	
	// Property of frame size
    UINT _depthWidth;
    UINT _depthHeight;
    UINT _colorWidth;
    UINT _colorHeight;
    // For open and play the videos
    cv::VideoCapture _colorCap;
    cv::VideoCapture _depthCap;
    // As frame buffer
    cv::Mat _matColor;
    cv::Mat _matDepth;
    cv::Mat _matDepthEncoded;
    // For output SRV for DirectX
    ID3D11Texture2D* _pColorTex;
    ID3D11ShaderResourceView* _pColorSRV;
    ID3D11Texture2D* _pDepthTex;
	ID3D11ShaderResourceView* _pDepthSRV;
    

	HRESULT CompileFormString(string code,
											 const D3D_SHADER_MACRO* pDefines,
											 LPCSTR pEntrypoint, LPCSTR pTarget,
											 UINT Flags1, UINT Flags2,
											 ID3DBlob** ppCode);
	// Generate shader code
	void GenShaderCode();
	// flag for shader compile
	const UINT					m_uCompileFlag;

	ID3D11VertexShader*				m_pPassVS;
	ID3D11PixelShader*				m_pPS;
	ID3D11GeometryShader*			m_pScreenQuadGS;
	ID3D11InputLayout*				m_pScreenQuadIL;
	ID3D11Buffer*					m_pScreenQuadVB;
	//For Texture output
	ID3D11Texture2D*				m_pRGBDTex;
	ID3D11ShaderResourceView*		m_pRGBDSRV;
	ID3D11RenderTargetView*			m_pRGBDRTV;
	D3D11_VIEWPORT					m_Viewport;

	// str for shader code
	std::string m_strShaderCode;



	bool _bActive;// Indicate the next frame is triggered or not
    bool _bPaused;// Indicate whether the playback is paused
    bool _bColorUpdated;// Indicate new color data is ready
    bool _bDepthUpdated;// Indicate new depth date is ready
    bool _bTerminate;// Flag as siginal to terminate the child thread
    bool _bThreadStart;// Indicate whether the playback thread start or not

    // Prevent corrupt data, so these are read&write mutex
    std::mutex _colorDataMutex;
    std::mutex _depthDataMutex;

    // inner thread
    std::thread _playbackThread;
};

KinectVideos::KinectVideos(bool active = true) : m_uCompileFlag(D3DCOMPILE_ENABLE_STRICTNESS){
	_bActive = active;
    _bPaused = true;
    _bTerminate = false;
    _bColorUpdated = false;
    _bDepthUpdated = false;
    _bThreadStart = false;
}


HRESULT KinectVideos::Initialize(){
    HRESULT hr = S_OK;
	//_colorCap.open("..\\Kinect2Recorder\\Kinect2Color.avi");
	_colorCap.open("..\\RGBDRecorder\\ColorChannel.avi");
	//_depthCap.open("..\\Kinect2Recorder\\Kinect2Depth.avi");
	_depthCap.open("..\\RGBDRecorder\\DepthChannel.avi");
    if( !_colorCap.isOpened() || !_depthCap.isOpened()) return (HRESULT)-1L;

    _depthWidth = (UINT)_depthCap.get(CV_CAP_PROP_FRAME_WIDTH);
    _depthHeight = (UINT)_depthCap.get(CV_CAP_PROP_FRAME_HEIGHT);
    _colorWidth = (UINT)_colorCap.get(CV_CAP_PROP_FRAME_WIDTH);
    _colorHeight = (UINT)_colorCap.get(CV_CAP_PROP_FRAME_HEIGHT);

    _matColor.create(_colorHeight,_colorWidth,CV_8UC3);
    _matDepth.create(_depthHeight,_depthWidth,CV_16UC1);
    _matDepthEncoded.create(_depthHeight,_depthWidth,CV_8UC3);

	for(int i=0; i<2; i++){
		_colorCap >> _matColor;
		_depthCap >> _matDepthEncoded;
	}
    return hr;
}

void KinectVideos::GetColorReso( int& iColorWidth, int& iColorHeight )
{
	iColorWidth = _colorWidth;
	iColorHeight = _colorHeight;
}

void KinectVideos::GetDepthReso( int& iDepthWidth, int& iDepthHeight )
{
	iDepthWidth = _depthWidth;
	iDepthHeight = _depthHeight;
}

void KinectVideos::GetInfraredReso(int& iInfraredWidth, int& iInfraredHeight)
{
	iInfraredWidth = _depthWidth;
	iInfraredHeight = _depthHeight;
}

HRESULT KinectVideos::CreateResource(ID3D11Device* pd3dDevice){
     HRESULT hr = S_OK;

	 GenShaderCode();
	 ID3DBlob* pVSBlob = NULL;
	 V_RETURN(CompileFormString(m_strShaderCode, nullptr, "VS", "vs_5_0", m_uCompileFlag, 0, &pVSBlob));
	 V_RETURN(pd3dDevice->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), NULL, &m_pPassVS));
	 DXUT_SetDebugName(m_pPassVS, "m_pPassVS");

	 ID3DBlob* pGSBlob = NULL;
	 V_RETURN(CompileFormString(m_strShaderCode, nullptr, "GS", "gs_5_0", m_uCompileFlag, 0, &pGSBlob));
	 V_RETURN(pd3dDevice->CreateGeometryShader(pGSBlob->GetBufferPointer(), pGSBlob->GetBufferSize(), NULL, &m_pScreenQuadGS));
	 DXUT_SetDebugName(m_pScreenQuadGS, "m_pScreenQuadGS");
	 pGSBlob->Release();

	 ID3DBlob* pPSBlob = NULL;
	 V_RETURN(CompileFormString(m_strShaderCode, nullptr, "PS", "ps_5_0", m_uCompileFlag, 0, &pPSBlob));
	 V_RETURN(pd3dDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), NULL, &m_pPS));
	 DXUT_SetDebugName(m_pPS, "m_pPS");
	 pPSBlob->Release();

	 D3D11_INPUT_ELEMENT_DESC layout[] = { { "POSITION", 0, DXGI_FORMAT_R16_SINT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 } };
	 V_RETURN(pd3dDevice->CreateInputLayout(layout, ARRAYSIZE(layout), pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), &m_pScreenQuadIL));
	 DXUT_SetDebugName(m_pScreenQuadIL, "m_pScreenQuadIL");
	 pVSBlob->Release();


	 // Create the vertex buffer
	 D3D11_BUFFER_DESC bd = { 0 };
	 bd.Usage = D3D11_USAGE_DEFAULT;
	 bd.ByteWidth = sizeof(short);
	 bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	 bd.CPUAccessFlags = 0;
	 V_RETURN(pd3dDevice->CreateBuffer(&bd, NULL, &m_pScreenQuadVB));
	 DXUT_SetDebugName(m_pScreenQuadVB, "m_pScreenQuadVB");

    // Create depth texture and its resource view
    D3D11_TEXTURE2D_DESC depthTexDesc = { 0 };
    depthTexDesc.Width = _depthWidth;
    depthTexDesc.Height = _depthHeight;
    depthTexDesc.MipLevels = 1;
    depthTexDesc.ArraySize = 1;
    depthTexDesc.Format = DXGI_FORMAT_R16_UINT;
    depthTexDesc.SampleDesc.Count = 1;
    depthTexDesc.SampleDesc.Quality = 0;
    depthTexDesc.Usage = D3D11_USAGE_DYNAMIC;
    depthTexDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    depthTexDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    depthTexDesc.MiscFlags = 0;
    V_RETURN( pd3dDevice->CreateTexture2D( &depthTexDesc, NULL, &_pDepthTex ));
    V_RETURN( pd3dDevice->CreateShaderResourceView( _pDepthTex, NULL, &_pDepthSRV ));

    // Create color texture and its resource view
    D3D11_TEXTURE2D_DESC colorTexDesc = { 0 };
    colorTexDesc.Width = _colorWidth;
    colorTexDesc.Height = _colorHeight;
    colorTexDesc.MipLevels = 1;
    colorTexDesc.ArraySize = 1;
    colorTexDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    colorTexDesc.SampleDesc.Count = 1;
    colorTexDesc.SampleDesc.Quality = 0;
    colorTexDesc.Usage = D3D11_USAGE_DYNAMIC;
    colorTexDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    colorTexDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    colorTexDesc.MiscFlags = 0;
    V_RETURN( pd3dDevice->CreateTexture2D( &colorTexDesc, NULL, &_pColorTex ));
    V_RETURN( pd3dDevice->CreateShaderResourceView( _pColorTex, NULL, &_pColorSRV ));


	// Create rendertarget resource

	D3D11_TEXTURE2D_DESC	RTtextureDesc = { 0 };
	RTtextureDesc.Width = _depthWidth;
	RTtextureDesc.Height = _depthHeight;
	RTtextureDesc.MipLevels = 1;
	RTtextureDesc.ArraySize = 1;
	RTtextureDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	RTtextureDesc.SampleDesc.Count = 1;
	RTtextureDesc.Usage = D3D11_USAGE_DEFAULT;
	RTtextureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	RTtextureDesc.CPUAccessFlags = 0;
	RTtextureDesc.MiscFlags = 0;
	V_RETURN(pd3dDevice->CreateTexture2D(&RTtextureDesc, NULL, &m_pRGBDTex));
	V_RETURN(pd3dDevice->CreateRenderTargetView(m_pRGBDTex, NULL, &m_pRGBDRTV));
	V_RETURN(pd3dDevice->CreateShaderResourceView(m_pRGBDTex, NULL, &m_pRGBDSRV));

	m_Viewport.Width = (float)_depthWidth;
	m_Viewport.Height = (float)_depthHeight;
	m_Viewport.MinDepth = 0.0f;
	m_Viewport.MaxDepth = 1.0f;
	m_Viewport.TopLeftX = 0;
	m_Viewport.TopLeftY = 0;

	if (!_bThreadStart){
		if (_bActive){
			_playbackThread = std::thread(std::bind(&KinectVideos::Play, this));
		}
		_bThreadStart = true;
		_bPaused = false;
	}

    return hr;
}

void KinectVideos::Release(){
    _bTerminate = true;
    SAFE_RELEASE( _pDepthTex );
    SAFE_RELEASE( _pDepthSRV );
    SAFE_RELEASE( _pColorTex );
    SAFE_RELEASE( _pColorSRV );


	SAFE_RELEASE(m_pRGBDTex);
	SAFE_RELEASE(m_pRGBDRTV);
	SAFE_RELEASE(m_pRGBDSRV);

	SAFE_RELEASE(m_pPassVS);
	SAFE_RELEASE(m_pPS);
	SAFE_RELEASE(m_pScreenQuadGS);
	SAFE_RELEASE(m_pScreenQuadIL);
	SAFE_RELEASE(m_pScreenQuadVB);

}

void KinectVideos::Play(){
    while( !_bTerminate ){
        if( !_bPaused ){
            {
                std::lock_guard<std::mutex> guard( _colorDataMutex );
                _colorCap >> _matColor;
                if( !_matColor.data){_bPaused = true; continue;} 
                else{
                    cv::imshow( "color", _matColor );
                    _bColorUpdated = true;
                }
            }
            {
                std::lock_guard<std::mutex> guard( _depthDataMutex );
                _depthCap >> _matDepthEncoded;
                if( !_matDepthEncoded.data ){ _bPaused = true; continue; } 
                else{
                    Mat8UC3toMat16UC1( _matDepthEncoded, _matDepth );
                    cv::imshow( "depth", _matDepth );
                    _bDepthUpdated = true;
                }
            }
        }
        char input = cv::waitKey(33);
    }
}

void KinectVideos::Mat8UC3toMat16UC1(cv::Mat& inMat, cv::Mat& outMat)
{
    //outMat.create( inMat.size(), CV_16UC1 );
    for(  int y = 0; y < inMat.size().height; ++y )
    {
        short* pColorRow = outMat.ptr<short>( y );
        for(  int x = 0; x < inMat.size().width; ++x )
        {
            uchar* value = inMat.ptr<uchar>(y,x);
            pColorRow[x] = (short)(value[0]<<8 | value[1]);
        }
    }
}

HRESULT KinectVideos::ProcessColor( ID3D11DeviceContext* pd3dimmediateContext){
    HRESULT hr = S_OK;
    D3D11_MAPPED_SUBRESOURCE msT;
    std::lock_guard<std::mutex> guard(_colorDataMutex);
    if(!_bColorUpdated || !_matColor.data) return (HRESULT)-1;
    hr = pd3dimmediateContext->Map( _pColorTex, NULL, D3D11_MAP_WRITE_DISCARD, NULL, &msT);
    if(FAILED(hr)) return hr;
    // Loop over each row and column for the color
    for(UINT y=0; y<_colorHeight;++y){
        BYTE* pDest = ((BYTE*)msT.pData + msT.RowPitch*y);
        for(UINT x=0;x<_colorWidth;++x){
            uchar* value = _matColor.ptr<uchar>(y,x);
            pDest[0] = value[0];
            pDest[1] = value[1];
            pDest[2] = value[2];
            pDest[3] = 0;
            pDest+=4;
        }
    }
    pd3dimmediateContext->Unmap( _pColorTex,NULL);
    _bColorUpdated = false;
    return hr;
}

HRESULT KinectVideos::ProcessDepth( ID3D11DeviceContext* pd3dimmediateContext){
    HRESULT hr = S_OK;
    D3D11_MAPPED_SUBRESOURCE msT;
    std::lock_guard<std::mutex> guard(_depthDataMutex);
    if(!_bDepthUpdated || !_matDepth.data) return (HRESULT)-1;
    hr = pd3dimmediateContext->Map( _pDepthTex,NULL, D3D11_MAP_WRITE_DISCARD,NULL,&msT);
    if(FAILED(hr)) return hr;
    memcpy(msT.pData,_matDepth.data,sizeof(short)*_depthHeight*_depthWidth);
    pd3dimmediateContext->Unmap( _pDepthTex,NULL);
    _bDepthUpdated = false;
    return hr;
}

bool KinectVideos::UpdateTextures( ID3D11DeviceContext* pd3dimmediateContext,
                               bool defaultReg, bool color, bool depth, bool infrared){
    
	if (_bPaused) return false;
	if (_bActive){
		if (color){
			if (FAILED(ProcessColor(pd3dimmediateContext))){ return false; }
		}
		if (depth){
			if (FAILED(ProcessDepth(pd3dimmediateContext))){ return false; }
		}
	}else{
		_colorCap >> _matColor;
		if (!_matColor.data){ _bPaused = true; } else{
			cv::imshow("color", _matColor);
			_bColorUpdated = true;
		}
		_depthCap >> _matDepthEncoded;
		if (!_matDepthEncoded.data){ _bPaused = true;} else{
			Mat8UC3toMat16UC1(_matDepthEncoded, _matDepth);
			cv::imshow("depth", _matDepth);
			_bDepthUpdated = true;
		}
		if (color){
			if (FAILED(ProcessColor(pd3dimmediateContext))){ return false; }
		}
		if (depth){
			if (FAILED(ProcessDepth(pd3dimmediateContext))){ return false; }
		}
	}


	pd3dimmediateContext->IASetInputLayout(m_pScreenQuadIL);
	pd3dimmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
	UINT stride = 0;
	UINT offset = 0;
	pd3dimmediateContext->IASetVertexBuffers(0, 1, &m_pScreenQuadVB, &stride, &offset);
	pd3dimmediateContext->VSSetShader(m_pPassVS, NULL, 0);
	pd3dimmediateContext->GSSetShader(m_pScreenQuadGS, NULL, 0);
	pd3dimmediateContext->PSSetShader(m_pPS, NULL, 0);
	pd3dimmediateContext->OMSetRenderTargets(1, &m_pRGBDRTV, NULL);
	pd3dimmediateContext->PSSetShaderResources(0, 1, &_pColorSRV);
	pd3dimmediateContext->PSSetShaderResources(1, 1, &_pDepthSRV);
	pd3dimmediateContext->RSSetViewports(1, &m_Viewport);
	pd3dimmediateContext->Draw(1, 0);
	return true;
}

LRESULT KinectVideos::HandleMessages(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    UNREFERENCED_PARAMETER(hWnd);

    switch (uMsg)
    {
    case WM_KEYDOWN:
        int nKey = static_cast< int >(wParam);

        if (nKey == 'P'){
            this->_bPaused = !this->_bPaused;
        }
        break;
    }
    return 0;
}

ID3D11ShaderResourceView** KinectVideos::getColor_ppSRV(){
    return &_pColorSRV;
}

ID3D11ShaderResourceView** KinectVideos::getDepth_ppSRV(){
    return &_pDepthSRV;
}

ID3D11ShaderResourceView** KinectVideos::getInfrared_ppSRV(){
	return &_pDepthSRV;
}





ID3D11ShaderResourceView** KinectVideos::getRGBD_ppSRV(){
	return &m_pRGBDSRV;
}

HRESULT KinectVideos::CompileFormString(string code,
										 const D3D_SHADER_MACRO* pDefines,
										 LPCSTR pEntrypoint, LPCSTR pTarget,
										 UINT Flags1, UINT Flags2,
										 ID3DBlob** ppCode){
	HRESULT hr;
#if defined( DEBUG ) || defined( _DEBUG )
	// Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
	// Setting this flag improves the shader debugging experience, but still allows 
	// the shaders to be optimized and to run exactly the way they will run in 
	// the release configuration of this program.
	Flags1 |= D3DCOMPILE_DEBUG;
#endif

	ID3DBlob* pErrorBlob = nullptr;
	hr = D3DCompile(code.c_str(), code.size(), NULL, NULL, D3D_COMPILE_STANDARD_FILE_INCLUDE, pEntrypoint, pTarget, Flags1, Flags2, ppCode, &pErrorBlob);
#pragma warning( suppress : 6102 )
	if (pErrorBlob)
	{
		OutputDebugStringA(reinterpret_cast<const char*>(pErrorBlob->GetBufferPointer()));
		pErrorBlob->Release();
	}

	return hr;
};

void KinectVideos::GenShaderCode(){
	std::stringstream shaderCode;
	shaderCode << "Texture2D<float4> txColor : register( t0 );\n";
	shaderCode << "Texture2D<uint> txDepth : register( t1 );\n";
	shaderCode << "static const float2 reso =float2(" << _depthWidth << "," << _depthHeight << ");\n";
	shaderCode << "struct GS_INPUT{};\n";
	shaderCode << "struct PS_INPUT{   \n";
	shaderCode << "	float4 Pos : SV_POSITION;\n";
	shaderCode << "	float2 Tex : TEXCOORD0;   \n";
	shaderCode << "};\n";
	shaderCode << "//--------------------------------------------------------------------------------------\n";
	shaderCode << "// Pass Through Vertex Shader, used in pass 0, pass 1 and pass 2\n";
	shaderCode << "//--------------------------------------------------------------------------------------\n";
	shaderCode << "GS_INPUT VS(){   \n";
	shaderCode << "	GS_INPUT output = (GS_INPUT)0;   \n";
	shaderCode << "	return output;   \n";
	shaderCode << "}\n";
	shaderCode << "//--------------------------------------------------------------------------------------\n";
	shaderCode << "// Quad Geometry Shader, used in pass 0 and pass 2\n";
	shaderCode << "//--------------------------------------------------------------------------------------\n";
	shaderCode << "[maxvertexcount(4)]\n";
	shaderCode << "void GS(point GS_INPUT particles[1], inout TriangleStream<PS_INPUT> triStream){   \n";
	shaderCode << "	PS_INPUT output;\n";
	shaderCode << "	output.Pos = float4(-1.0f, 1.0f, 0.01f, 1.0f);   \n";
	shaderCode << "	output.Tex = float2(0,0);\n";
	shaderCode << "	triStream.Append(output);\n";
	shaderCode << "	output.Pos = float4(1.0f, 1.0f, 0.01f, 1.0f);   \n";
	shaderCode << "	output.Tex = float2(reso.x,0);   \n";
	shaderCode << "	triStream.Append(output);\n";
	shaderCode << "	output.Pos = float4(-1.0f, -1.0f, 0.01f, 1.0f);   \n";
	shaderCode << "	output.Tex = float2(0, reso.y);   \n";
	shaderCode << "	triStream.Append(output);\n";
	shaderCode << "	output.Pos = float4(1.0f, -1.0f, 0.01f, 1.0f);   \n";
	shaderCode << "	output.Tex = reso;\n";
	shaderCode << "	triStream.Append(output);\n";
	shaderCode << "}\n";
	shaderCode << "//--------------------------------------------------------------------------------------\n";
	shaderCode << "// Undistorted Merge Pixel Shader, \n";
	shaderCode << "// generate undistorted RGBD texture, used in pass 0\n";
	shaderCode << "//--------------------------------------------------------------------------------------\n";
	shaderCode << "float4 PS(PS_INPUT input) : SV_Target{ \n";
	shaderCode << "	float4 col = txColor.Load(int3(input.Tex,0));\n";
	shaderCode << " col.w = txDepth.Load( int3(input.Tex,0) ) / 1000.f;\n";
	shaderCode << " return col;\n";
	shaderCode << "}\n";
	m_strShaderCode = shaderCode.str();
}



KinectVideos::~KinectVideos(){
    if(_playbackThread.joinable()) _playbackThread.join();
}

IRGBDStreamForDirectX* DirectXStreamFactory::createFromVideo(){
    return new KinectVideos();
}

IRGBDStreamForDirectX* DirectXStreamFactory::createFromPassiveVideo(){
	return new KinectVideos(false);
}