/* RGB Depth sensor interface for DirectX 
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
	virtual void GetColorReso( int& iColorWidth, int& iColorHeight ) = 0;
	virtual void GetDepthReso( int& iDepthWidth, int& iDepthHeight ) = 0;
	virtual void GetInfraredReso(int& iInfraredWidth, int& iInfraredHeight) = 0;
	virtual bool UpdateTextures(ID3D11DeviceContext* pd3dimmediateContext,
                                 bool defaultReg=true , bool bColor=true, bool bDepth=true, bool bInfrared=true ) = 0;
    virtual void Release() = 0;
    virtual LRESULT HandleMessages(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)=0;
    virtual ID3D11ShaderResourceView** getColor_ppSRV() = 0;
    virtual ID3D11ShaderResourceView** getDepth_ppSRV() = 0;
	virtual ID3D11ShaderResourceView** getInfrared_ppSRV() = 0;
	virtual ID3D11ShaderResourceView** getRGBD_ppSRV(){return NULL;};
};

class RGBDSTREAMDLL_API DirectXStreamFactory{
public:
    static IRGBDStreamForDirectX* createFromKinect();
	static IRGBDStreamForDirectX* createFromKinect2();
	static IRGBDStreamForDirectX* createFromVideo();
	static IRGBDStreamForDirectX* createFromPassiveVideo();
};