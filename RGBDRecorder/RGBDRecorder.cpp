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
            pColorRow[x] = ((short)value[0] << 8 | (short)value[1]);
        }
    }
}

int main()
{
    KinectSensor kinect;
    cv::Mat mat_cvt2RGB;
    cv::Mat mat_cvt2D16;

    kinect.InitialKinect();

    while('q'!=cv::waitKey(1)){
        kinect.UpdateMats( true );
        if( kinect.m_bUpdated ){
            cv::imshow( "KinectDepth", kinect.m_matDepth );
            Mat16UC1toMat8UC3( kinect.m_matDepth, mat_cvt2RGB );
            cv::imshow( "mat_cvt2RGBDepth", mat_cvt2RGB );
            Mat8UC3toMat16UC1( mat_cvt2RGB, mat_cvt2D16 );
            cv::imshow( "mat_cvt2D16", mat_cvt2D16 );
            cv::Mat diff = kinect.m_matDepth - mat_cvt2D16;

            double min=0,max=0;
            cv::Point minloc;
            cv::Point maxloc;
            cv::minMaxLoc( diff, &min, &max, &minloc, &maxloc );

            cout << "colorFrame:  min:" << min << "; max:" << max << endl;
            cv::circle( diff, maxloc, 8, cv::Scalar( 255, 255, 255 ), 3 );
            cv::circle( diff, minloc, 4, cv::Scalar( 255, 255, 0 ) );
            cv::imshow( "difference", diff);
        }
    }
    cv::waitKey(0);
    
    return 0;
}
