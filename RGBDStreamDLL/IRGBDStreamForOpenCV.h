#pragma once

#ifdef RGBDSTREAMDLL_EXPORTS
#define RGBDSTREAMDLL_API __declspec(dllexport) 
#else
#define RGBDSTREAMDLL_API __declspec(dllimport) 
#endif

typedef long HRESULT;

namespace cv{
	class Mat;
}

class RGBDSTREAMDLL_API IRGBDStreamForOpenCV
{
public:
	virtual ~IRGBDStreamForOpenCV(){};
	virtual HRESULT Initialize()=0;
	virtual void GetColorReso( int& iColorWidth, int& iColorHeight ) = 0;
	virtual void GetDepthReso( int& iDepthWidth, int& iDepthHeight ) = 0;
	virtual void GetInfraredReso( int& iInfraredWidth, int& iInfraredHeight ) = 0;
	virtual bool UpdateMats(bool defaultReg=true, bool color=true, bool depth=true, bool infrared=true) = 0;
	virtual void GetColorMat(cv::Mat& out)=0;
	virtual void GetDepthMat(cv::Mat& out)=0;
	virtual void GetInfraredMat(cv::Mat& out)=0;
};

class RGBDSTREAMDLL_API OpenCVStreamFactory{
public:
	static IRGBDStreamForOpenCV* createFromKinect2();
	static IRGBDStreamForOpenCV* createFromKinect();
};