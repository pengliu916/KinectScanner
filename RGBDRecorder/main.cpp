#include <opencv2/core/core.hpp>
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/highgui/highgui.hpp"
#include <iostream>
#include <iomanip>
#include <DXUT.h>

using namespace cv;
using namespace std;
using namespace DirectX;

int mains()
{
    double mA[] = {16135.0664,	-1870.88147,	-4344.42822,	1437.27563,		-20658.4531,	7014.40332,
                   -1870.88147,	32692.4121,		-600.876343,	25744.6367,		1159.05212,		-21790.1270,
                   -4344.42822,	-600.876343,	13766.6963,		-3130.63599,	11871.9063,		-2596.32764,
                   1437.27563,	25744.6367,		-3130.63599,	40920.0469,		-4533.84326,	-20610.6523,
                   -20658.4531,	1159.05212,		11871.9063,		-4533.84326,	61019.2109,		-17875.2363,
                   7014.40332,	-21790.1270,	-2596.32764,	-20610.6523,	-17875.2363,	54660.6484};
    double mB[] = {-0.247963190,	1.79207921,	0.960133314,	2.07745194,	-0.0872344971,	1.47376990};
    double mX[] = {-2.50336361e-005,	6.14388046e-005,	9.47578665e-005,	6.14705059e-005,	-9.04476735e-007,	8.20503774e-005};

    Eigen::Matrix<double,6,6> m_mA = Eigen::Map<Eigen::MatrixXd>(mA,6,6);
	Eigen::Matrix<double,6,1> m_vB = Eigen::Map<Eigen::MatrixXd>(mB,6,1);
	Eigen::Matrix<double,6,1> m_vX;

    cv::Matx<double,6,6> cv_mA(mA);
    cv::Matx<double,6,1> cv_mB(mB);
    cv::Matx<double,6,1> cv_mX;

    double dDet;
    double cv_t1 = 0.0, cv_t2 = 0.0;
    double freq = cv::getTickFrequency()/1000.0f;

    cout.setf(ios::fixed);

    cv_t1 = (double)cv::getTickCount();
    cv_mX = cv_mA.solve(cv_mB,cv::DECOMP_LU);
    cv_t2 = (cv::getTickCount()-cv_t1)/freq;

    cout<<"OpenCV DECOMP_LU takes:"<<setprecision(15)<<cv_t2<<"ms"<<endl;
    cout<<cv_mX<<endl;

    cv_t1 = (double)cv::getTickCount();
    dDet = m_mA.determinant();
    m_vX = m_mA.llt().solve(m_vB).cast<double>();
    cv_t2 = (cv::getTickCount()-cv_t1)/freq;

    cout<<"Eigen takes:"<<setprecision(15)<<cv_t2<<"ms"<<endl;
    cout<<m_vX<<endl;

    cv_t1 = (double)cv::getTickCount();
    cv_mX = cv_mA.solve(cv_mB,cv::DECOMP_NORMAL);
    cv_t2 = (cv::getTickCount()-cv_t1)/freq;

    cout<<"OpenCV DECOMP_NORMAL takes:"<<setprecision(15)<<cv_t2<<"ms"<<endl;
    cout<<cv_mX<<endl;

    cv_t1 = (double)cv::getTickCount();
    cv_mX = cv_mA.solve(cv_mB,cv::DECOMP_CHOLESKY);
    cv_t2 = (cv::getTickCount()-cv_t1)/freq;

    cout<<"OpenCV DECOMP_CHOLESKY takes:"<<setprecision(15)<<cv_t2<<"ms"<<endl;
    cout<<cv_mX<<endl;

    cv_t1 = (double)cv::getTickCount();
    cv_mX = cv_mA.solve(cv_mB,cv::DECOMP_EIG);
    cv_t2 = (cv::getTickCount()-cv_t1)/freq;

    cout<<"OpenCV DECOMP_EIG takes:"<<setprecision(15)<<cv_t2<<"ms"<<endl;
    cout<<cv_mX<<endl;

    cv_t1 = (double)cv::getTickCount();
    cv_mX = cv_mA.solve(cv_mB,cv::DECOMP_QR);
    cv_t2 = (cv::getTickCount()-cv_t1)/freq;

    cout<<"OpenCV DECOMP_QR takes:"<<setprecision(15)<<cv_t2<<"ms"<<endl;
    cout<<cv_mX<<endl;

    cv_t1 = (double)cv::getTickCount();
    cv_mX = cv_mA.solve(cv_mB,cv::DECOMP_SVD);
    cv_t2 = (cv::getTickCount()-cv_t1)/freq;

    cout<<"OpenCV DECOMP_SVD takes:"<<setprecision(15)<<cv_t2<<"ms"<<endl;
    cout<<cv_mX<<endl;
    return 0;
}
