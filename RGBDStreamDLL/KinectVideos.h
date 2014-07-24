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

class KinectVideos : public IRGBDStreamForDirectX{
public:
    KinectVideos();
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

KinectVideos::KinectVideos(){
    _bPaused = true;
    _bTerminate = false;
    _bColorUpdated = false;
    _bDepthUpdated = false;
    _bThreadStart = false;
}

HRESULT KinectVideos::Initialize(){
    HRESULT hr = S_OK;
    _colorCap.open("..\\RGBDRecorder\\ColorChannel.avi");
    _depthCap.open("..\\RGBDRecorder\\DepthChannel.avi");
    if( !_colorCap.isOpened() || !_depthCap.isOpened()) return (HRESULT)-1L;

    _depthWidth = (UINT)_depthCap.get(CV_CAP_PROP_FRAME_WIDTH);
    _depthHeight = (UINT)_depthCap.get(CV_CAP_PROP_FRAME_HEIGHT);
    _colorWidth = (UINT)_colorCap.get(CV_CAP_PROP_FRAME_WIDTH);
    _colorHeight = (UINT)_colorCap.get(CV_CAP_PROP_FRAME_HEIGHT);

    _matColor.create(_colorHeight,_colorWidth,CV_8UC3);
    _matDepth.create(_depthHeight,_depthWidth,CV_16UC1);
    _matDepthEncoded.create(_depthHeight,_depthWidth,CV_8UC3);
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
    // Create depth texture and its resource view
    D3D11_TEXTURE2D_DESC depthTexDesc = { 0 };
    depthTexDesc.Width = _depthWidth;
    depthTexDesc.Height = _depthHeight;
    depthTexDesc.MipLevels = 1;
    depthTexDesc.ArraySize = 1;
    depthTexDesc.Format = DXGI_FORMAT_R16_SINT;
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

    if(!_bThreadStart){
        _playbackThread = std::thread(std::bind(&KinectVideos::Play,this));
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
    
    if(_bPaused) return false;
    if(color){
        if(FAILED(ProcessColor(pd3dimmediateContext))){return false;}
    }
    if(depth){
        if(FAILED(ProcessDepth(pd3dimmediateContext))){return false;}
    }
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

KinectVideos::~KinectVideos(){
    if(_playbackThread.joinable()) _playbackThread.join();
}

IRGBDStreamForDirectX* DirectXStreamFactory::createFromVideo(){
    return new KinectVideos();
}