#include <windows.h>
#include "opencv2/calib3d/calib3d.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/highgui/highgui.hpp"
#include <iostream>
#include "RGBDStreamDLL/IRGBDStreamForOpenCV.h"
#include <math.h>

using namespace std;
using namespace cv;

#define FAILED(hr) (((HRESULT)(hr)) < 0)

bool FindOrCreateDirectory( const wchar_t* pszPath )
{
	WIN32_FIND_DATA fd;
	HANDLE hFind = ::FindFirstFile( pszPath, &fd );
	while( hFind != INVALID_HANDLE_VALUE ){
		if( fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
			return true;
	}

	if( !::CreateDirectory( pszPath, NULL ) ){
		char szDir[MAX_PATH];
		sprintf_s( szDir, sizeof( szDir ), "Failed to create folder [%s]", pszPath );
		cout<<szDir<<endl;
		return false;
	}
	return true;
}

bool readStringList( const string& filename, vector<string>& l )
{
	l.resize( 0 );
	FileStorage fs( filename, FileStorage::READ );
	if( !fs.isOpened() )
		return false;
	FileNode n = fs.getFirstTopLevelNode();
	if( n.type() != FileNode::SEQ )
		return false;
	FileNodeIterator it = n.begin(), it_end = n.end();
	for( ; it != it_end; ++it )
		l.push_back( ( string )*it );
	return true;
}

int main()
{
	vector<string> imageList;
	vector<Mat> imgForCalib;
	vector<Point2f> corners;

	char userInput='y';
	string folderName;
	string imglistName;

	imshow("Empty",cv::Mat(320,240,CV_16UC1));
	cout<<"Capture New Images for Calibration? ( y/n ) :";
	userInput = waitKey(0);

	if( userInput == 'y' ){
		IRGBDStreamForOpenCV* kinect = OpenCVStreamFactory::createFromKinect2();
		cv::Mat matInfrared;

		bool patternFound = false;


		if( FAILED( kinect->Initialize() ) ){
			cout << "Cannot initialize kinect." << endl;
			exit( 1 );
		}

		bool chessboard = true;
		cout<<"chessboard pattern?(y/n):";
		userInput =waitKey(0);
		if(userInput=='n') chessboard = false;
		cout<<"\n 'a' for accept image, 'q' for exiting image aquaring stage"<<endl;

		int iInfraredWidth, iInfraredHeight;
		kinect->GetInfraredReso( iInfraredWidth, iInfraredHeight );

		cv::Mat infrared8U;
		cv::Mat infraredWithPattern;
		unsigned long long int frameCount = 0;

		while( 'q' != ( userInput = cv::waitKey( 1 ) ) ){
			if( patternFound && 'a' == userInput ){
				imgForCalib.push_back( infrared8U );
				cout << "frame " << frameCount << " added" << endl;
			}
			if( kinect->UpdateMats() ){
				frameCount++;
				//cv::Mat color;
				//kinect->GetColorMat(color);
				//color.resize(960,540);
				//bool patternFound = findChessboardCorners( color, Size(9,6), corners,CALIB_CB_ADAPTIVE_THRESH);
				//drawChessboardCorners(color,Size(9,6),Mat(corners),patternFound);
				//imshow("ColorWithPattern",color);


				kinect->GetInfraredMat( matInfrared );
				matInfrared.convertTo( matInfrared, CV_32F );
				infrared8U = matInfrared / 65535.f;
				cv::pow( infrared8U, 0.32, infrared8U );
				infrared8U = infrared8U * 256;
				infrared8U.convertTo( infrared8U, CV_8UC1 );

				if (chessboard){
					patternFound = findChessboardCorners(infrared8U, Size(9, 6), corners,
														  CALIB_CB_ADAPTIVE_THRESH | CV_CALIB_CB_NORMALIZE_IMAGE);
					if (patternFound) cornerSubPix(infrared8U, corners, Size(11, 11), Size(-1, -1),
													 TermCriteria(CV_TERMCRIT_EPS + CV_TERMCRIT_ITER, 30, 0.1));
					cvtColor(infrared8U, infraredWithPattern, CV_GRAY2RGB);
					drawChessboardCorners(infraredWithPattern, Size(9, 6), Mat(corners), patternFound);
				}else{
					patternFound = findCirclesGrid(infrared8U, Size(4, 11), corners, CALIB_CB_ASYMMETRIC_GRID);
					cvtColor(infrared8U, infraredWithPattern, CV_GRAY2RGB);
					drawChessboardCorners(infraredWithPattern, Size(4, 11), Mat(corners), patternFound);
				}
				imshow( "WithPattern", infraredWithPattern );
			}
		}

		cout<<"Image acquisition finished. Saving images to folder? and create yaml file (y/n): ";
		userInput = cv::waitKey(0);
		if(userInput =='y'){
			cout<<endl<<"please type in the folder name:";
			cin>>folderName;
			cout<<endl<<"please type in the imglist name:";
			cin>>imglistName;
			wstringstream wFolderPath;
			wstring wFolderName(folderName.begin(),folderName.end());
			wFolderPath<<"./"<<wFolderName;
			if(!FindOrCreateDirectory(wFolderPath.str().c_str())) return 1;
			FileStorage fs(imglistName,FileStorage::WRITE);
			fs<<"images"<<"[";
			for(int i=0;i<imgForCalib.size();i++){
				stringstream buff;
				buff<<"./"<<folderName<<"/"<<i<<".jpg";
				string filename = buff.str();
				imwrite(filename,imgForCalib[i]);
				fs<<filename;
			}
			fs<<"]";
		}
	}else{
		cout<<endl<<"please type in image list file name:";
		cin>>imglistName;
		if(!readStringList(imglistName,imageList)){
			cout<<"Cannot read image list"<<endl;
			return 1;
		}
		cv::Mat tempImg;
		for( int i=0; i<imageList.size(); i++ )
		{
			tempImg = imread(imageList[i]);

			imshow( "image", tempImg );

			char key = ( char )waitKey( 100 );
		}
	}
	
    return 0;
}
