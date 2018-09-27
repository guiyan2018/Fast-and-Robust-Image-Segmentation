#include "Bilateral.h"
#include <iostream>
#include <fstream>

int Bilateral::xyStep=20;
int Bilateral::rgbStep=16;
double Bilateral::E2W=10.0;

Bilateral::Bilateral(Mat img) :
	imgSrc(img) {
    gridSize[1] = img.rows/xyStep;
    gridSize[2] = img.cols/xyStep;
    gridSize[3] = 256/rgbStep;
    gridSize[4] = 256/rgbStep;
    gridSize[5] = 256/rgbStep;
	initGrid();
}

Bilateral::Bilateral()
{
}

Bilateral::~Bilateral()
{
	grid.release();
	gridColor.release();
}

void Bilateral::InitGmms(Mat& mask)
{
	


	int tSize = 1;
	int xSize = imgSrc.rows;
	int ySize = imgSrc.cols;
	int point[6] = { 0,0,0,0,0,0 };

	std::vector<Vec3f> bgdSamples;    //从背景点存储背景颜色
	std::vector<Vec3f> fgdSamples;    //从前景点存储前景颜色


	for (int x = 0; x < xSize; x++)
	{
		for (int y = 0; y < ySize; y++)
		{
			if (mask.at<uchar>(x, y) == GC_BGD) {
				getGridPoint(0, Point(x, y), point, tSize, xSize, ySize);
				grid.at<Vec< int, 4 > >(point)[bgdSum] += 3;
			}
			else if (mask.at<uchar>(x, y) == GC_FGD/*GC_FGD*/) {
				getGridPoint(0, Point(x, y), point, tSize, xSize, ySize);
				grid.at<Vec< int, 4 > >(point)[fgdSum] += 3;
			}
			else if (mask.at<uchar>(x, y) == GC_PR_FGD/*GC_FGD*/) {
				getGridPoint(0, Point(x, y), point, tSize, xSize, ySize);
				grid.at<Vec< int, 4 > >(point)[fgdSum] += 1;
			}
			else if (mask.at<uchar>(x, y) == GC_PR_BGD/*GC_FGD*/) {
				getGridPoint(0, Point(x, y), point, tSize, xSize, ySize);
				grid.at<Vec< int, 4 > >(point)[bgdSum] += 1;
			}
		}
	}

	for (int t = 0; t < gridSize[0]; t++) {
		for (int x = 0; x < gridSize[1]; x++) {
			for (int y = 0; y < gridSize[2]; y++) {
				for (int r = 0; r < gridSize[3]; r++) {
					for (int g = 0; g < gridSize[4]; g++) {
						for (int b = 0; b < gridSize[5]; b++) {
							int point[6] = { t,x,y,r,g,b };
							int pixCount = grid.at<Vec< int, 4 > >(point)[pixSum];
							if (pixCount > 0) {
								int bgdcount = grid.at<Vec< int, 4 > >(point)[bgdSum];
								int fgdcount = grid.at<Vec< int, 4 > >(point)[fgdSum];
								if (bgdcount > 0) {
									Vec3f color = gridColor.at<Vec3f>(point);
									bgdSamples.push_back(color);
								}

								if (fgdcount > 0) {
									Vec3f color = gridColor.at<Vec3f>(point);
									fgdSamples.push_back(color);
								}
							}
						}
					}
				}
			}
		}
	}


	double _time = static_cast<double>(getTickCount());//计时

	GMM bgdGMM(bgModel), fgdGMM(fgModel);

	const int kMeansItCount = 10;  //迭代次数  
	const int kMeansType = KMEANS_PP_CENTERS; //Use kmeans++ center initialization by Arthur and Vassilvitskii  
	Mat bgdLabels, fgdLabels; //记录背景和前景的像素样本集中每个像素对应GMM的哪个高斯模型

							  //kmeans进行分类
	Mat _bgdSamples((int)bgdSamples.size(), 3, CV_32FC1, &bgdSamples[0][0]);
	kmeans(_bgdSamples, GMM::componentsCount, bgdLabels,
		TermCriteria(CV_TERMCRIT_ITER, kMeansItCount, 0.0), 0, kMeansType);
	Mat _fgdSamples((int)fgdSamples.size(), 3, CV_32FC1, &fgdSamples[0][0]);
	kmeans(_fgdSamples, GMM::componentsCount, fgdLabels,
		TermCriteria(CV_TERMCRIT_ITER, kMeansItCount, 0.0), 0, kMeansType);

	//分类后的结果用来训练GMMs（初始化）
	bgdGMM.initLearning();
	for (int i = 0; i < (int)bgdSamples.size(); i++)
		bgdGMM.addSample(bgdLabels.at<int>(i, 0), bgdSamples[i], 1);
	bgdGMM.endLearning();
	fgdGMM.initLearning();
	for (int i = 0; i < (int)fgdSamples.size(); i++)
		fgdGMM.addSample(fgdLabels.at<int>(i, 0), fgdSamples[i], 1);
	fgdGMM.endLearning();
	for (int times = 0; times < 3; times++)
	{
		//训练GMMs模型
		for (int i = 0; i < (int)bgdSamples.size(); i++) {
			Vec3d color = bgdSamples[i];
			bgdLabels.at<int>(i, 0) = bgdGMM.whichComponent(color);
		}

		for (int i = 0; i < (int)fgdSamples.size(); i++) {
			Vec3d color = fgdSamples[i];
			fgdLabels.at<int>(i, 0) = fgdGMM.whichComponent(color);
		}

		bgdGMM.initLearning();
		for (int i = 0; i < (int)bgdSamples.size(); i++)
			bgdGMM.addSample(bgdLabels.at<int>(i, 0), bgdSamples[i], 1);
		bgdGMM.endLearning();
		fgdGMM.initLearning();
		for (int i = 0; i < (int)fgdSamples.size(); i++)
			fgdGMM.addSample(fgdLabels.at<int>(i, 0), fgdSamples[i], 1);
		fgdGMM.endLearning();
	}


	std::vector<Vec3f> unSamples;    //错误分类点

	for (int i = 0; i < (int)bgdSamples.size(); i++) {
		Vec3d color = bgdSamples[i];
		double b = bgdGMM(color), f = fgdGMM(color);
		if (fgdGMM.bigThan1Cov(color) || (b < f)) {
			unSamples.push_back(color);
		}
	}
	for (int i = 0; i < (int)fgdSamples.size(); i++) {
		Vec3d color = fgdSamples[i];
		double b = bgdGMM(color), f = fgdGMM(color);
		if (bgdGMM.bigThan1Cov(color) || (b > f)) {
			unSamples.push_back(color);
		}
	}
	if (unSamples.size() < 10) {
		haveUnModel = false;
	}
	else {
		haveUnModel = true;
		GMM unGMM(unModel);
		Mat unLabels;
		Mat _unSamples((int)unSamples.size(), 3, CV_32FC1, &unSamples[0][0]);
		kmeans(_unSamples, GMM::componentsCount, unLabels,
			TermCriteria(CV_TERMCRIT_ITER, kMeansItCount, 0.0), 0, kMeansType);
		unGMM.initLearning();
		for (int i = 0; i < (int)unSamples.size(); i++)
			unGMM.addSample(unLabels.at<int>(i, 0), unSamples[i], 1);
		unGMM.endLearning();
		for (int times = 0; times < 3; times++)
		{
			//训练GMMs模型
			for (int i = 0; i < (int)unSamples.size(); i++) {
				Vec3d color = unSamples[i];
				unLabels.at<int>(i, 0) = unGMM.whichComponent(color);
			}
			unGMM.initLearning();
			for (int i = 0; i < (int)unSamples.size(); i++)
				unGMM.addSample(unLabels.at<int>(i, 0), unSamples[i], 1);
			unGMM.endLearning();
		}

	}


	_time = (static_cast<double>(getTickCount()) - _time) / getTickFrequency();
    //printf("高斯建模用时%f\n", _time);//显示时间
}

void Bilateral::initGrid() {
	double _time = static_cast<double>(getTickCount());

	Mat L(6, gridSize, CV_32SC(4), Scalar(0, 0, 0, -1));
	Mat C(6, gridSize, CV_32FC(3), Scalar::all(0));
	Mat P(6, gridSize, CV_32FC(3), Scalar::all(0));
	grid = L;gridColor = C;gridProbable = P;
	int tSize = 1;
	int xSize = imgSrc.rows;
	int ySize = imgSrc.cols;
	for (int t = 0; t < tSize; t++)
	{
		for (int x = 0; x < xSize; x++)
		{
			//#pragma omp parallel for
			for (int y = 0; y < ySize; y++)
			{
				int tNew = gridSize[0] * t / tSize;
				int xNew = gridSize[1] * x / xSize;
				int yNew = gridSize[2] * y / ySize;
				Vec3b color = (Vec3b)imgSrc.at<Vec3b>(x, y);
				int rNew = gridSize[3] * color[0] / 256;
				int gNew = gridSize[4] * color[1] / 256;
				int bNew = gridSize[5] * color[2] / 256;
				int point[6] = { tNew,xNew,yNew,rNew,gNew,bNew };
				int count = ++(grid.at<Vec< int, 4 > >(point)[pixSum]);
				Vec3f colorMeans = gridColor.at<Vec3f>(point);
				colorMeans[0] = colorMeans[0] * (count - 1.0) / (count + 0.0) + color[0] / (count + 0.0);
				colorMeans[1] = colorMeans[1] * (count - 1.0) / (count + 0.0) + color[1] / (count + 0.0);
				colorMeans[2] = colorMeans[2] * (count - 1.0) / (count + 0.0) + color[2] / (count + 0.0);
				gridColor.at<Vec3f>(point) = colorMeans;
			}
		}
	}



	_time = (static_cast<double>(getTickCount()) - _time) / getTickFrequency();
    //printf("构建grid用时%f\n", _time);//显示时间
}

void Bilateral::savePreImg(Mat& preSegImg) {
	int tSize = 1;
	int xSize = imgSrc.rows;
	int ySize = imgSrc.cols;
	Mat randColor(6, gridSize, CV_8UC3, Scalar::all(0));

	for (int t = 0; t < gridSize[0]; t++) {
		for (int x = 0; x < gridSize[1]; x++) {
			for (int y = 0; y < gridSize[2]; y++) {
				for (int r = 0; r < gridSize[3]; r++) {
					for (int g = 0; g < gridSize[4]; g++) {
						for (int b = 0; b < gridSize[5]; b++) {
							int point[6] = { t,x,y,r,g,b };
							if (grid.at<Vec< int, 4 > >(point)[pixSum] > 0) {
								randColor.at<Vec3b>(point) = { (uchar)(rand() % 255),(uchar)(rand() % 255),(uchar)(rand() % 255) };
							}
						}
					}
				}
			}
		}
	}

    preSegImg.create(imgSrc.rows, imgSrc.cols, CV_8UC3);
	for (int y = 0; y < ySize; y++)
	{
		for (int x = 0; x < xSize; x++)
		{
			Point p(x, y);
			int point[6] = { 0,0,0,0,0,0 };
			getGridPoint(0, p, point, tSize, xSize, ySize);
			Vec3b colorMeans = randColor.at<Vec3b>(point);
			preSegImg.at<Vec3b>(x, y) = colorMeans;
		}
	}
	randColor.release();
}

static double calcBeta(const Mat& img)
{
	double beta = 0;
	for (int y = 0; y < img.rows; y++)
	{
		for (int x = 0; x < img.cols; x++)
		{
			//计算四个方向邻域两像素的差别，也就是欧式距离或者说二阶范数  
			//（当所有像素都算完后，就相当于计算八邻域的像素差了）  
			Vec3d color = img.at<Vec3b>(y, x);
			if (x > 0) // left  >0的判断是为了避免在图像边界的时候还计算，导致越界  
			{
				Vec3d diff = color - (Vec3d)img.at<Vec3b>(y, x - 1);
				beta += diff.dot(diff);  //矩阵的点乘，也就是各个元素平方的和  
			}
			if (y > 0 && x > 0) // upleft  
			{
				Vec3d diff = color - (Vec3d)img.at<Vec3b>(y - 1, x - 1);
				beta += diff.dot(diff);
			}
			if (y > 0) // up  
			{
				Vec3d diff = color - (Vec3d)img.at<Vec3b>(y - 1, x);
				beta += diff.dot(diff);
			}
			if (y > 0 && x < img.cols - 1) // upright  
			{
				Vec3d diff = color - (Vec3d)img.at<Vec3b>(y - 1, x + 1);
				beta += diff.dot(diff);
			}
		}
	}
	if (beta <= std::numeric_limits<double>::epsilon())
		beta = 0;
	else
		beta = 1.f / (2 * beta / (4 * img.cols*img.rows - 3 * img.cols - 3 * img.rows + 2)); //论文公式（5）  

	return beta;
}

void Bilateral::constructGCGraph(GCGraph<double>& graph) {
	double _time = static_cast<double>(getTickCount());

	double bata = calcBeta(imgSrc);
	int vtxCount = calculateVtxCount();  //顶点数，每一个像素是一个顶点  
	int edgeCount = 2 * 256 * vtxCount;  //边数，需要考虑图边界的边的缺失
	graph.create(vtxCount, edgeCount);
    int eCount = 0, eCount3 = 0;
	GMM bgdGMM(bgModel), fgdGMM(fgModel), unGMM(unModel);

	for (int t = 0; t < gridSize[0]; t++) {
		for (int x = 0; x < gridSize[1]; x++) {
			for (int y = 0; y < gridSize[2]; y++) {
				for (int r = 0; r < gridSize[3]; r++) {
					for (int g = 0; g < gridSize[4]; g++) {
						for (int b = 0; b < gridSize[5]; b++) {
							int point[6] = { t,x,y,r,g,b };
							int pixCount = grid.at<Vec< int, 4 > >(point)[pixSum];
							if (pixCount > 0) {
								int vtxIdx = graph.addVtx();//存在像素点映射就加顶点								
								//先验项
								grid.at<Vec< int, 4 > >(point)[vIdx] = vtxIdx;
								Vec3f color = gridColor.at<Vec3f>(point);
								double fromSource, toSink;
								double fSum = grid.at<Vec< int, 4 > >(point)[fgdSum];
								double bSum = grid.at<Vec< int, 4 > >(point)[bgdSum];

								//综合方法
								if ((bSum > 0) && fSum == 0) {
									fromSource = 0;
									toSink = 99999;
									gridProbable.at<Vec3f>(point)[0] = 0;
								}
								else if (bSum == 0 && (fSum > 0)) {
									fromSource = 99999;
									toSink = 0;
									gridProbable.at<Vec3f>(point)[0] = 1;
								}
								else {
									double bgd = bgdGMM(color);
									double fgd = fgdGMM(color);
									bool un;
									if (haveUnModel)
										un = unGMM.bigThan1Cov(color);
									else
										un = false;

									if (un||(bgdGMM.smallThan2Cov(color) && fgdGMM.smallThan2Cov(color))) {
										bgd = fgd = 0.5;
										eCount3++;
									}
									gridProbable.at<Vec3f>(point)[0] = fgd / (bgd + fgd);
									fromSource = -log(bgd)*(pixCount);
									toSink = -log(fgd)*(pixCount);
                                    //printf("%f\n", fromSource);//显示时间
								}
								
								graph.addTermWeights(vtxIdx, fromSource, toSink);

								//平滑项

								for (int xN = x; xN >= x - 1 && xN >= 0 && xN < gridSize[1]; xN--) {
									for (int yN = y; yN >= y - 1 && yN >= 0 && yN < gridSize[2]; yN--) {
										for (int rN = r; rN >= r - 1 && rN >= 0 && rN < gridSize[3]; rN--) {
											for (int gN = g; gN >= g - 1 && gN >= 0 && gN < gridSize[4]; gN--) {
												for (int bN = b; bN >= b - 1 && bN >= 0 && bN < gridSize[5]; bN--) {
													int pointN[6] = { 0,xN,yN,rN,gN,bN };
													int vtxIdxNew = grid.at<Vec< int, 4 > >(pointN)[vIdx];
													if (grid.at<Vec< int, 4 > >(pointN)[pixSum] > 0 && vtxIdxNew > 0 && vtxIdxNew != vtxIdx) {
														double num = sqrt(grid.at<Vec< int, 4 > >(point)[pixSum] * grid.at<Vec< int, 4 > >(pointN)[pixSum] + 1);
														Vec3d diff = (Vec3d)color - (Vec3d)gridColor.at<Vec3f>(pointN);
														double e = exp(-bata*diff.dot(diff));  //矩阵的点乘，也就是各个元素平方的和
                                                        double w = E2W * e * (num);
														//w = 14.;
														graph.addEdges(vtxIdx, vtxIdxNew, w, w);
														eCount++;
													}
												}
											}
										}
									}
								}

							}
						}
					}
				}

			}
		}
	}

	_time = (static_cast<double>(getTickCount()) - _time) / getTickFrequency();
    //printf("图割构图用时 %f\n", _time);//显示时间
    //printf("顶点的总数 %d\n", vtxCount);
    //printf("边的总数 %d\n", eCount);
    //printf("e3的总数 %d\n", eCount2);
    //printf("unWeight<0.5的总数 %d\n", eCount3);
}


int Bilateral::calculateVtxCount() {
	int count = 0;
	for (int t = 0; t < gridSize[0]; t++)
	{
		for (int x = 0; x < gridSize[1]; x++)
		{
			for (int y = 0; y < gridSize[2]; y++)
			{
				for (int r = 0; r < gridSize[3]; r++)
				{
					for (int g = 0; g < gridSize[4]; g++)
					{
						for (int b = 0; b < gridSize[5]; b++)
						{
							int point[6] = { t,x,y,r,g,b };
							if (grid.at<Vec< int, 4 > >(point)[pixSum] > 0) {
								count++;
							}
						}
					}
				}
			}
		}
	}
	return count;
}


void Bilateral::estimateSegmentation(GCGraph<double>& graph, Mat& mask) {
	double _time = static_cast<double>(getTickCount());
	graph.maxFlow();//最大流图割
	_time = (static_cast<double>(getTickCount()) - _time) / getTickFrequency();
    //printf("图割分割用时 %f\n", _time);//显示时间

	double _time2 = static_cast<double>(getTickCount());
	int tSize = 1;
	int xSize = imgSrc.rows;
	int ySize = imgSrc.cols;
	for (int t = 0; t < tSize; t++)
	{
		for (int y = 0; y < ySize; y++)
		{
			for (int x = 0; x < xSize; x++)
			{
				Point p(x, y);
				int point[6] = { 0,0,0,0,0,0 };
				getGridPoint(t, p, point, tSize, xSize, ySize);
				int vertex = grid.at<Vec< int, 4 > >(point)[vIdx];
				if (graph.inSourceSegment(vertex))
					mask.at<uchar>(p.x, p.y) = 1;
				else
					mask.at<uchar>(p.x, p.y) = 0;
			}
		}
	}

	_time2 = (static_cast<double>(getTickCount()) - _time2) / getTickFrequency();
    //printf("grid结果传递mask用时 %f\n", _time2);//显示时间
}

void Bilateral::getGridPoint(int index, const Point p, int *point, int tSize, int xSize, int ySize) {
	point[0] = gridSize[0] * index / tSize;
	point[1] = gridSize[1] * p.x / xSize;
	point[2] = gridSize[2] * p.y / ySize;
	Vec3b color = (Vec3b)imgSrc.at<Vec3b>(p.x, p.y);
	point[3] = gridSize[3] * color[0] / 256;
	point[4] = gridSize[4] * color[1] / 256;
	point[5] = gridSize[5] * color[2] / 256;
}

void Bilateral::getGridPoint(int index, const Point p, std::vector<int>& point, int tSize, int xSize, int ySize) {
	point[0] = gridSize[0] * index / tSize;
	point[1] = gridSize[1] * p.y / xSize;
    point[2] = gridSize[2] * p.x / ySize;
	Vec3b color = (Vec3b)imgSrc.at<Vec3b>(p);
	point[3] = gridSize[3] * color[0] / 256;
	point[4] = gridSize[4] * color[1] / 256;
	point[5] = gridSize[5] * color[2] / 256;
}


void Bilateral::getGmmProMask(Mat& mask) {
	mask = Mat::zeros(imgSrc.rows, imgSrc.cols, CV_8UC1);
	int xSize = imgSrc.rows;
	int ySize = imgSrc.cols;
	for (int y = 0; y < ySize; y++)
	{
		for (int x = 0; x < xSize; x++)
		{
			Point p(x, y);
			int point[6] = { 0,0,0,0,0,0 };
			getGridPoint(0, p, point, 1, xSize, ySize);
			float probable = gridProbable.at<Vec3f>(point)[0];
			mask.at<uchar>(p.x, p.y) = (uchar)(probable * 255);
		}
	}

}

void Bilateral::run(Mat& mask) {

	mask.create(imgSrc.rows, imgSrc.cols, CV_8UC1);

	GCGraph<double> graph;//图割

	constructGCGraph(graph);
	estimateSegmentation(graph, mask);
}
