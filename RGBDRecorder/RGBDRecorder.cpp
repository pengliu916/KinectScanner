#include <opencv2/core/core.hpp>
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/video/video.hpp"
#include <iostream>
#include <windows.h>
#include "KinectForOpencv.h"

using namespace std;

void Mat16UC1toMat8UC3(cv::Mat& inMat, cv::Mat& outMat)
{
    outMat.create( inMat.size(), CV_8UC3 );
    for( LONG y = 0; y < inMat.size().height; ++y )
    {
        cv::Vec3b* pColorRow = outMat.ptr<cv::Vec3b>( y );
        for( LONG x = 0; x < inMat.size().width; ++x )
        {
            short value = *inMat.ptr<short>(y,x);
            if(y==100 && x==100){
                short ans = ((BYTE)((value & 0xFF00)>>8))*256+ ((BYTE)(value & 0x00FF));
            }
            pColorRow[x] = cv::Vec3b((value & 0xFF00)>>8,(value & 0x00FF), 0);
        }
    }
}

void Mat8UC3toMat16UC1(cv::Mat& inMat, cv::Mat& outMat)
{
    outMat.create( inMat.size(), CV_16UC1 );
    for( LONG y = 0; y < inMat.size().height; ++y )
    {
        short* pColorRow = outMat.ptr<short>( y );
        for( LONG x = 0; x < inMat.size().width; ++x )
        {
            uchar* value = inMat.ptr<uchar>(y,x);
            pColorRow[x] = (short)(value[0]<<8 | value[1]);
        }
    }
}

int main()
{
    KinectSensor kinect;
    LONG framecount=0;
    cv::Mat firstColorFrame;
    cv::Mat firstDepthFrame;
    cv::Mat converted;

    kinect.InitialKinect();/*
    cv::namedWindow("KinectColor", cv::WINDOW_AUTOSIZE);
    cv::namedWindow("KinectDepth",cv::WINDOW_AUTOSIZE);

    cv::namedWindow("ConvertedDepth",cv::WINDOW_AUTOSIZE);
*/
    cv::namedWindow("firstColorFrame", cv::WINDOW_AUTOSIZE);
    cv::namedWindow("firstDepthFrame", cv::WINDOW_AUTOSIZE);

    cv::VideoWriter colorWriter;
    cv::VideoWriter depthWriter;

    colorWriter.open("ColorChannel.avi",CV_FOURCC('L','A','G','S'),30,cv::Size(640,480));
    depthWriter.open("DepthChannel.avi",CV_FOURCC('L','A','G','S'),30,cv::Size(640,480));
    if(!colorWriter.isOpened()||!depthWriter.isOpened()){
        cout<<"Cannot open video file to write."<<endl;
        exit(1);
    }


    while('q'!=cv::waitKey(1)){
        kinect.UpdateMats( true );
        if( kinect.m_bUpdated ){
            if( framecount == 0 ){
                firstColorFrame = kinect.m_matColor;
                firstDepthFrame = kinect.m_matDepth;
                cv::imshow( "firstColorFrame", firstColorFrame );
                cv::imshow( "firstDepthFrame", firstDepthFrame );
            }
            Mat16UC1toMat8UC3( kinect.m_matDepth, converted );
            cv::imshow( "ConvertedDepth", converted );
            cv::imshow( "KinectColor", kinect.m_matColor );
            colorWriter << kinect.m_matColor;
            depthWriter << converted;
            //depthWriter << kinect.m_matDepth;
            framecount++;
        }
        //double min, max;
        //cv::minMaxLoc(kinect.m_matDepth,&min,&max);
        //cout<<"min:"<<min<<"; max:"<<max<<endl;
    }
    colorWriter.release();
    depthWriter.release();

    cv::VideoCapture colorCap("ColorChannel.avi");
    cv::VideoCapture depthCap("DepthChannel.avi");

    //depthCap.get(CV_CAP_PROP_FORMAT);
    cv::Mat firstColorCaptured;
    cv::Mat firstDepthCaptured;
    cv::Mat convertedC;
    colorCap>>firstColorCaptured;
    depthCap>>firstDepthCaptured;

    Mat8UC3toMat16UC1(firstDepthCaptured,convertedC);
    
    cv::imshow("VColor",firstColorCaptured);

    cv::Mat diff = convertedC - firstDepthFrame;
    double min=0,max=0;
    cv::Point minloc;
    cv::Point maxloc;
    cv::minMaxLoc(diff,&min,&max,&minloc,&maxloc);

    cout<<"colorFrame:  min:"<<min<<"; max:"<<max<<endl;
    cv::circle(firstDepthCaptured,maxloc,8,cv::Scalar(255,255,255),3);
    cv::circle(firstDepthCaptured,minloc,4,cv::Scalar(255,255,0));
    cv::imshow("VDepth",diff);
    /*
    diff = firstDepthCaptured - firstDepthFrame;
    cv::minMaxLoc(diff,&min,&max);
    cout<<"depthFrame:  min:"<<min<<"; max:"<<max<<endl;*/

    cv::waitKey(0);



    return 0;
}
