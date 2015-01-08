#include <opencv2/core/core.hpp>
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/video/video.hpp"
#include <iostream>
#include "RGBDStreamDLL/IRGBDStreamForOpenCV.h"
//#include "KinectLib\KinectLib.h"
//#include "KinectForOpencv.h"

using namespace std;


#define FAILED(hr) (((HRESULT)(hr)) < 0)

void Mat16UC1toMat8UC3(cv::Mat& inMat, cv::Mat& outMat)
{
    outMat.create( inMat.size(), CV_8UC3 );
    for(  int y = 0; y < inMat.size().height; ++y )
    {
        cv::Vec3b* pColorRow = outMat.ptr<cv::Vec3b>( y );
        for(  int x = 0; x < inMat.size().width; ++x )
        {
            short value = *inMat.ptr<short>(y,x);
            pColorRow[x] = cv::Vec3b((value & 0xFF00)>>8,(value & 0x00FF), 0);
        }
    }
}

void Mat8UC3toMat8UC4(cv::Mat& inMat, cv::Mat& outMat)
{
    //outMat.create( inMat.size(), CV_8UC4 );
    for(  int y = 0; y < inMat.size().height; ++y )
    {
        cv::Vec4b* pColorRow = outMat.ptr<cv::Vec4b>( y );
        for(  int x = 0; x < inMat.size().width; ++x )
        {
            uchar* value = inMat.ptr<uchar>(y,x);
            pColorRow[x] = cv::Vec4b(value[0],value[1],value[2],0);
        }
    }
}

void Mat8UC3toMat16UC1(cv::Mat& inMat, cv::Mat& outMat)
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

int main()
{
    IRGBDStreamForOpenCV* kinect = OpenCVStreamFactory::createFromKinect2();
    cv::Mat matColor;
    cv::Mat matDepth;
	cv::Mat matInfrared;
    cv::Mat converted;
	cv::Mat matGammar;
	cv::Mat temp;

    if (FAILED(kinect->Initialize())){
        cout<<"Cannot initialize kinect."<<endl;
        exit(1);
    }

    cv::VideoWriter colorWriter;
    cv::VideoWriter depthWriter;

	int iColorWidth, iColorHeight;
	kinect->GetColorReso(iColorWidth,iColorHeight);

	int iDepthWidth, iDepthHeight;
	kinect->GetDepthReso(iDepthWidth,iDepthHeight);

	int iInfraredWidth, iInfraredHeight;
	kinect->GetInfraredReso( iInfraredWidth, iInfraredHeight );

    colorWriter.open("ColorChannel.avi",CV_FOURCC('L','A','G','S'),30,cv::Size(iColorWidth,iColorHeight));
    depthWriter.open("DepthChannel.avi",CV_FOURCC('L','A','G','S'),30,cv::Size(iDepthWidth,iDepthHeight));
    if(!colorWriter.isOpened()||!depthWriter.isOpened()){
        cout<<"Cannot open video file to write."<<endl;
        exit(1);
    }


    while('q'!=cv::waitKey(1)){
        if( kinect->UpdateMats()){
            kinect->GetDepthMat(matDepth);
            kinect->GetColorMat(matColor);
			kinect->GetInfraredMat(matInfrared);
            Mat16UC1toMat8UC3(matDepth, converted);
            cv::imshow( "ConvertedDepth", converted );
			cv::imshow( "KinectColor", matColor );

			matInfrared.convertTo(matInfrared,CV_32F);
			temp = matInfrared/65535.f;
			cv::pow(temp,0.32,temp);
			cv::imshow("KinectInfrared", temp);
			cv::imshow( "KinectDepth", matDepth );
            colorWriter << matColor;
            depthWriter << converted;
        }
    }
    //colorWriter.release();
    //depthWriter.release();

    cout<<"Play? y"<<endl;
    if('y'==cv::waitKey()){
        //Playback
        cv::VideoCapture colorCap("ColorChannel.avi");
        cv::VideoCapture depthCap("DepthChannel.avi");

        int frame,width,height;
        frame = (int)colorCap.get(CV_CAP_PROP_FRAME_COUNT);
        width = (int)colorCap.get(CV_CAP_PROP_FRAME_WIDTH);
        height = (int)colorCap.get(CV_CAP_PROP_FRAME_HEIGHT);
        cout<<"ColorVideo has "<<frame<<" frames@"<<width<<"x"<<height<<endl;


        frame = (int)depthCap.get(CV_CAP_PROP_FRAME_COUNT);
        width = (int)depthCap.get(CV_CAP_PROP_FRAME_WIDTH);
        height = (int)depthCap.get(CV_CAP_PROP_FRAME_HEIGHT);
        cout << "DepthVideo has " << frame << " frames@" << width << "x" << height << endl;

        cv::Mat color;
        cv::Mat depth;
        cv::Mat depthDecoded;
        cv::Mat color4C;

        color4C.create( cv::Size(width,height), CV_8UC4 );
        depthDecoded.create( cv::Size(width,height), CV_16UC1 );
        while(1){
            double t = (double)cv::getTickCount();
            colorCap>>color;
            depthCap>>depth;
            if(!color.data ||!depth.data) break;
            //cv::imshow("colorVideo", color);
            //cv::imshow("depthVideo", depth);
            //Mat8UC3toMat16UC1(depth,depthDecoded);
            Mat8UC3toMat8UC4(color,color4C);
            cv::imshow("colorC4",color4C);
            //cv::imshow("depthDecoded",depthDecoded);
            char input=cv::waitKey(1);
            if('q'==input) break;
            t = ((double)cv::getTickCount() - t) / cv::getTickFrequency();// elapsed time in ms
            std::cout << "I am working at " << 1.0/t << " FPS" << std::endl;
        }
    }

    cv::waitKey(0);
    return 0;
}
/*
cv::Mat rgbdMat;
rgbdReader>>rgbdMat;
if(rgbdMat.data){
D3D11_MAPPED_SUBRESOURCE resource;
pd3dImmediateContext->Map(g_pRGBDTex, 0, D3D11_MAP_WRITE_DISCARD, 0, &resource);
float* data = (float*)resource.pData;
UINT pos=0;

for (int i = 0; i < rgbdMat.rows; i++){
for (int j = 0; j < rgbdMat.cols; j++){
cv::Vec4f elms = rgbdMat.at<cv::Vec4f>(i, j);
data[pos++] = elms[0];
data[pos++] = elms[1];
data[pos++] = elms[2];
data[pos++] = elms[3];
}
}
pd3dImmediateContext->Unmap(g_pRGBDTex, 0);
}
*/