#include <functional>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include "KinectSensor.h"

class OpenCVThread
{
public:
    bool m_bTerminated;
    KinectSensor*   m_pKinect;
    int m_uLowThreashold;
    int m_uHighThreashold;
    OpenCVThread(KinectSensor* _m_pKinect){ 
        m_pKinect = _m_pKinect; 
        m_bTerminated = false;
        cv::namedWindow("Color",cv::WINDOW_AUTOSIZE);
        /*
        cv::namedWindow("Canny",cv::WINDOW_AUTOSIZE);
        cv::createTrackbar("Low Threashold","Canny",&m_uLowThreashold,255);
        cv::createTrackbar("High Threashold","Canny",&m_uHighThreashold,255);*/
    };

    void Run(){
        while(!m_bTerminated){
            cv::Mat img_gry,img_canny;
            m_pKinect->UpdateColorMat();
            cv::imshow("Color",m_pKinect->m_matColor);
            /*cv::cvtColor(m_pKinect->m_matColor,img_gry,cv::COLOR_BGR2GRAY);
            cv::Canny(img_gry,img_canny,m_uLowThreashold,m_uHighThreashold,3,true);
            cv::imshow("Canny",img_canny);*/
            cv::waitKey(10);
        }
    }

    OpenCVThread(const OpenCVThread&) = delete;
};
