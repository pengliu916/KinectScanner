#pragma once

#ifdef RGBDSTREAMDLL_EXPORTS
#define RGBDSTREAMDLL_API __declspec(dllexport) 
#else
#define RGBDSTREAMDLL_API __declspec(dllimport) 
#endif
typedef long HRESULT;
struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11ShaderResourceView;

class RGBDSTREAMDLL_API IRGBDStreamForDirectX
{
public:
    virtual ~IRGBDStreamForDirectX(){}
    virtual HRESULT Initialize()=0;
    virtual HRESULT CreateResource(ID3D11Device* pd3dDevice) = 0;
    virtual bool UpdateTextures( ID3D11DeviceContext* pd3dimmediateContext, 
                                 bool defaultReg=true , bool bColor=true, bool bDepth=true ) = 0;
    virtual void Release() = 0;
    virtual LRESULT HandleMessages(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)=0;
    virtual ID3D11ShaderResourceView** getColor_ppSRV() = 0;
    virtual ID3D11ShaderResourceView** getDepth_ppSRV() = 0;
};

class RGBDSTREAMDLL_API DirectXStreamFactory{
public:
    static IRGBDStreamForDirectX* create();
};