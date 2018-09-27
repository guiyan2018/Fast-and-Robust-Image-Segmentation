#pragma once
#include <opencv2/imgproc.hpp>

#ifndef _GMM_H_
#define _GMM_H_

using namespace cv;
class GMM
{
public:
	static const int componentsCount = 5;
	GMM(Mat& _model);
	double operator()(const Vec3d color) const;
	double operator()(int ci, const Vec3d color) const;
	int whichComponent(const Vec3d color) const;

	void initLearning();
	void addSample(int ci, const Vec3d color, double weight);
	void endLearning();
	bool bigThan1Cov(const Vec3d color )const;
	bool smallThan2Cov(const Vec3d color)const;
private:
	void calcInverseCovAndDeterm(int ci);
	Mat model;
	double* coefs;
	double* mean;
	double* cov;

	double inverseCovs[componentsCount][3][3]; //协方差的逆矩阵  
	double covDeterms[componentsCount];  //协方差的行列式  

	double arg_p1Cov = 1;
	double p1Cov[componentsCount];
	double arg_p3Cov = 2;
	double p3Cov[componentsCount];

	double sums[componentsCount][3];
	double prods[componentsCount][3][3];
	double sampleCounts[componentsCount];
	double totalSampleCount;
};

#endif
