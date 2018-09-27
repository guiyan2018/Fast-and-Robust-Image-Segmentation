#pragma once
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>
#include "GMM.h"
#include "gcgraph.hpp"

#ifndef _BILATERAL_H_
#define _BILATERAL_H_

using namespace cv;

enum girdIndex {
	pixSum = 0,  //像素点数
	fgdSum = 1,  //前景点数（邻近插值）
	bgdSum = 2,  //背景
	vIdx = 3,   //顶点标签
};

class Bilateral
{
public:
    Mat imgSrc_;	 //输入图片数据
	Mat bgModel, fgModel, unModel;	//前背景高斯模型
	Mat grid,gridColor, gridProbable;	//升维，平均取点，得到的grid。6维数组，保存顶点值与邻近像素点总数。
	bool haveUnModel;//unModel是否存在
    static int xyStep;
    static int rgbStep;
    static double E2W;
	int gridSize[6] = { 1,20,30,16,16,16 };	//grid各个维度的大小,按顺序来为：t,x,y,r,g,b。
	//int gridSize[6] = { 1,40,50,1,1,1 };	
public:
	Bilateral(Mat img);
    Bilateral();
	~Bilateral();
	void InitGmms(Mat& );
	void run(Mat& );
	void getGmmProMask(Mat& mask);
    void savePreImg(Mat& preSegImg);
private:
	void initGrid();
	void constructGCGraph(GCGraph<double>& graph);
	int calculateVtxCount();
	void estimateSegmentation(GCGraph<double>&, Mat& );
	void getGridPoint(int , const Point , int *, int , int , int );
	void getGridPoint(int , const Point , std::vector<int>& , int , int , int );
};

#endif
