#include <windows.h>
#include <math.h>
#include <iostream>
#include <vector>
#include "opencv2/calib3d/calib3d.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "RGBDStreamDLL/IRGBDStreamForOpenCV.h"
#include "Calibration.h"

#define IMG_ACQUISITION 1
#define IMG_SAVING 1
#define IMG_LOADING 1
#define IMG_DSIPLAY 1
#define CALIBRATION 1


const string strImgFolder = "Kinect2CircleGridImgs_home";

using namespace std;
using namespace cv;

int main()
{
	vector<vector<Mat>> imgList;
#if IMG_ACQUISITION
	imshow( "Sensor0", cv::Mat( 320, 240, CV_16UC1 ) );
	IRGBDStreamForOpenCV* kinect = OpenCVStreamFactory::createFromKinect();
	if( FAILED( kinect->Initialize() ) ){
		cout << "Cannot initialize kinect." << endl;
		exit( 1 );
	}
	IRGBDStreamForOpenCV* kinect2 = OpenCVStreamFactory::createFromKinect2();
	if( FAILED( kinect2->Initialize() ) ){
		cout << "Cannot initialize kinect2." << endl;
		exit( 1 );
	}

	imgList = Calibration::ImagesAcquisition( 2,[&kinect2,&kinect]( vector<Mat>& imgs )->bool{
		bool succeed;
		succeed = kinect2->UpdateMats();
		Mat img;
		kinect2->GetInfraredMat( img );
		img.convertTo( img, CV_32F );
		img = img / 65535.f;
		cv::pow( img, 0.32, img );
		img = img * 256;
		img.convertTo( img, CV_8UC1 );
		imgs.push_back( img );
		kinect2->GetColorMat( img );
		imgs.push_back( img );
/*
		succeed = kinect->UpdateMats(false);
		kinect->GetColorMat(img);
		imgs.push_back(img);*/
		return succeed;
	} );
#endif
	
#if IMG_SAVING	
	// Save images
	Calibration::SaveImages( imgList, strImgFolder );
#endif

#if IMG_LOADING
	// Load images
	imgList = Calibration::LoadImages( strImgFolder + "\\syncimgList.yml");
#endif

#if CALIBRATION
	// Calibration
	Calibration::BundleCalibration( strImgFolder + "\\calibration_result.yml", imgList);
#endif

#if IMG_DSIPLAY
	Calibration::DisplayImages(imgList);
#endif
	return 0;
}
