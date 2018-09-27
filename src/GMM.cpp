#include "GMM.h"
#include <iostream>
#include <fstream>


//背景和前景各有一个对应的GMM（混合高斯模型）
GMM::GMM(Mat& _model)
{
    //一个像素的（唯一对应）高斯模型的参数个数或者说一个高斯模型的参数个数
    //一个像素RGB三个通道值，故3个均值，3*3个协方差，共用一个权值
    const int modelSize = 3/*mean*/ + 9/*covariance*/ + 1/*component weight*/;
    if (_model.empty())
    {
        //一个GMM共有componentsCount个高斯模型，一个高斯模型有modelSize个模型参数
        _model.create(1, modelSize*componentsCount, CV_64FC1);
        _model.setTo(Scalar(0));
    }
    else if ((_model.type() != CV_64FC1) || (_model.rows != 1) || (_model.cols != modelSize*componentsCount))
        CV_Error(CV_StsBadArg, "_model must have CV_64FC1 type, rows == 1 and cols == 13*componentsCount");

    model = _model;

    //注意这些模型参数的存储方式：先排完componentsCount个coefs，再3*componentsCount个mean。
    //再3*3*componentsCount个cov。
    coefs = model.ptr<double>(0);  //GMM的每个像素的高斯模型的权值变量起始存储指针
    mean = coefs + componentsCount; //均值变量起始存储指针
    cov = mean + 3 * componentsCount;  //协方差变量起始存储指针

    for (int ci = 0; ci < componentsCount; ci++)
        if (coefs[ci] > 0)
            //计算GMM中第ci个高斯模型的协方差的逆Inverse和行列式Determinant
            //为了后面计算每个像素属于该高斯模型的概率（也就是数据能量项）
            calcInverseCovAndDeterm(ci);
    for (int ci = 0; ci < componentsCount; ci++) {
        //res += coefs[ci] * (*this)(ci, color);
        double* m = mean + 3 * ci;
        double* c = cov + 9 * ci;
        Vec3d m1Color = { m[0] + arg_p1Cov*sqrt(c[0]),m[1] + arg_p1Cov*sqrt(c[4]),m[2] + arg_p1Cov* sqrt(c[8]) };
        p1Cov[ci] = (*this)(ci, m1Color);
        Vec3d m2Color = { m[0] + arg_p3Cov * sqrt(c[0]),m[1] + arg_p3Cov * sqrt(c[4]),m[2] + arg_p3Cov * sqrt(c[8]) };
        p3Cov[ci]=(*this)(ci, m2Color);
    }

}


//计算一个像素（由color=（B,G,R）三维double型向量来表示）属于这个GMM混合高斯模型的概率。
//也就是把这个像素像素属于componentsCount个高斯模型的概率与对应的权值相乘再相加，
//具体见论文的公式（10）。结果从res返回。
//这个相当于计算Gibbs能量的第一个能量项（取负后）。
double GMM::operator()(const Vec3d color) const
{
    double res = 0;
    double max = 0;
    for (int ci = 0; ci < componentsCount; ci++) {
        //res += coefs[ci] * (*this)(ci, color);
        res = (*this)(ci, color);
        max = max < res ? res : max;
    }
    return max;
}

//计算一个像素（由color=（B,G,R）三维double型向量来表示）属于第ci个高斯模型的概率。
//具体过程，即高阶的高斯密度模型计算式，具体见论文的公式（10）。结果从res返回
double GMM::operator()(int ci, const Vec3d color) const
{
    double res = 0;
    if (coefs[ci] > 0)
    {
        CV_Assert(covDeterms[ci] > std::numeric_limits<double>::epsilon());
        Vec3d diff = color;
        double* m = mean + 3 * ci;
        diff[0] -= m[0]; diff[1] -= m[1]; diff[2] -= m[2];
        double mult = diff[0] * (diff[0] * inverseCovs[ci][0][0] + diff[1] * inverseCovs[ci][1][0] + diff[2] * inverseCovs[ci][2][0])
            + diff[1] * (diff[0] * inverseCovs[ci][0][1] + diff[1] * inverseCovs[ci][1][1] + diff[2] * inverseCovs[ci][2][1])
            + diff[2] * (diff[0] * inverseCovs[ci][0][2] + diff[1] * inverseCovs[ci][1][2] + diff[2] * inverseCovs[ci][2][2]);
        res = 1.0f / sqrt(covDeterms[ci]) * exp(-0.5f*mult);
    }
    return res;
}

//返回这个像素最有可能属于GMM中的哪个高斯模型（概率最大的那个）
int GMM::whichComponent(const Vec3d color) const
{
    int k = 0;
    double max = 0;

    for (int ci = 0; ci < componentsCount; ci++)
    {
        double p = (*this)(ci, color);
        if (p > max)
        {
            k = ci;  //找到概率最大的那个，或者说计算结果最大的那个
            max = p;
        }
    }
    return k;
}

//GMM参数学习前的初始化，主要是对要求和的变量置零
void GMM::initLearning()
{
    for (int ci = 0; ci < componentsCount; ci++)
    {
        sums[ci][0] = sums[ci][1] = sums[ci][2] = 0;
        prods[ci][0][0] = prods[ci][0][1] = prods[ci][0][2] = 0;
        prods[ci][1][0] = prods[ci][1][1] = prods[ci][1][2] = 0;
        prods[ci][2][0] = prods[ci][2][1] = prods[ci][2][2] = 0;
        sampleCounts[ci] = 0;
    }
    totalSampleCount = 0;
}

//增加样本，即为前景或者背景GMM的第ci个高斯模型的像素集（这个像素集是来用估
//计计算这个高斯模型的参数的）增加样本像素。计算加入color这个像素后，像素集
//中所有像素的RGB三个通道的和sums（用来计算均值），还有它的prods（用来计算协方差），
//并且记录这个像素集的像素个数和总的像素个数（用来计算这个高斯模型的权值）。
void GMM::addSample(int ci, const Vec3d color, double weight)
{
    sums[ci][0] += color[0] * weight; sums[ci][1] += color[1] * weight; sums[ci][2] += color[2] * weight;
    prods[ci][0][0] += color[0] * color[0] * weight; prods[ci][0][1] += color[0] * color[1] * weight; prods[ci][0][2] += color[0] * color[2] * weight;
    prods[ci][1][0] += color[1] * color[0] * weight; prods[ci][1][1] += color[1] * color[1] * weight; prods[ci][1][2] += color[1] * color[2] * weight;
    prods[ci][2][0] += color[2] * color[0] * weight; prods[ci][2][1] += color[2] * color[1] * weight; prods[ci][2][2] += color[2] * color[2] * weight;
    sampleCounts[ci] += weight;
    totalSampleCount += weight;
}

//从图像数据中学习GMM的参数：每一个高斯分量的权值、均值和协方差矩阵；
//这里相当于论文中“Iterative minimisation”的step 2
void GMM::endLearning()
{
    const double variance = 0.01;
    for (int ci = 0; ci < componentsCount; ci++)
    {
        double n = sampleCounts[ci]; //第ci个高斯模型的样本像素个数
        if (n == 0)
            coefs[ci] = 0;
        else
        {
            //计算第ci个高斯模型的权值系数
            coefs[ci] = (double)n / totalSampleCount;

            //计算第ci个高斯模型的均值
            double* m = mean + 3 * ci;
            m[0] = sums[ci][0] / n; m[1] = sums[ci][1] / n; m[2] = sums[ci][2] / n;

            //计算第ci个高斯模型的协方差
            double* c = cov + 9 * ci;
            c[0] = prods[ci][0][0] / n - m[0] * m[0]; c[1] = prods[ci][0][1] / n - m[0] * m[1]; c[2] = prods[ci][0][2] / n - m[0] * m[2];
            c[3] = prods[ci][1][0] / n - m[1] * m[0]; c[4] = prods[ci][1][1] / n - m[1] * m[1]; c[5] = prods[ci][1][2] / n - m[1] * m[2];
            c[6] = prods[ci][2][0] / n - m[2] * m[0]; c[7] = prods[ci][2][1] / n - m[2] * m[1]; c[8] = prods[ci][2][2] / n - m[2] * m[2];

            //计算第ci个高斯模型的协方差的行列式
            double dtrm = c[0] * (c[4] * c[8] - c[5] * c[7]) - c[1] * (c[3] * c[8] - c[5] * c[6]) + c[2] * (c[3] * c[7] - c[4] * c[6]);
            if (dtrm <= std::numeric_limits<double>::epsilon())
            {
                //相当于如果行列式小于等于0，（对角线元素）增加白噪声，避免其变
                //为退化（降秩）协方差矩阵（不存在逆矩阵，但后面的计算需要计算逆矩阵）。
                // Adds the white noise to avoid singular covariance matrix.
                c[0] += variance;
                c[4] += variance;
                c[8] += variance;
            }

            //计算第ci个高斯模型的协方差的逆Inverse和行列式Determinant
            calcInverseCovAndDeterm(ci);
        }
    }
    for (int ci = 0; ci < componentsCount; ci++) {
        //res += coefs[ci] * (*this)(ci, color);
        double* m = mean + 3 * ci;
        double* c = cov + 9 * ci;
        Vec3d m1Color = { m[0] + arg_p1Cov*sqrt(c[0]),m[1] + arg_p1Cov*sqrt(c[4]),m[2] + arg_p1Cov*sqrt(c[8]) };
        p1Cov[ci] = (*this)(ci, m1Color);
        Vec3d m2Color = { m[0] + arg_p3Cov * sqrt(c[0]),m[1] + arg_p3Cov * sqrt(c[4]),m[2] + arg_p3Cov * sqrt(c[8]) };
        p3Cov[ci] = (*this)(ci, m2Color);
    }
}

//计算协方差的逆Inverse和行列式Determinant
void GMM::calcInverseCovAndDeterm(int ci)
{
    if (coefs[ci] > 0)
    {
        //取第ci个高斯模型的协方差的起始指针
        double *c = cov + 9 * ci;
        double dtrm =
            covDeterms[ci] = c[0] * (c[4] * c[8] - c[5] * c[7]) - c[1] * (c[3] * c[8] - c[5] * c[6])
            + c[2] * (c[3] * c[7] - c[4] * c[6]);

        //在C++中，每一种内置的数据类型都拥有不同的属性, 使用<limits>库可以获
        //得这些基本数据类型的数值属性。因为浮点算法的截断，所以使得，当a=2，
        //b=3时 10*a/b == 20/b不成立。那怎么办呢？
        //这个小正数（epsilon）常量就来了，小正数通常为可用给定数据类型的
        //大于1的最小值与1之差来表示。若dtrm结果不大于小正数，那么它几乎为零。
        //所以下式保证dtrm>0，即行列式的计算正确（协方差对称正定，故行列式大于0）。
        CV_Assert(dtrm > std::numeric_limits<double>::epsilon());
        //三阶方阵的求逆
        inverseCovs[ci][0][0] = (c[4] * c[8] - c[5] * c[7]) / dtrm;
        inverseCovs[ci][1][0] = -(c[3] * c[8] - c[5] * c[6]) / dtrm;
        inverseCovs[ci][2][0] = (c[3] * c[7] - c[4] * c[6]) / dtrm;
        inverseCovs[ci][0][1] = -(c[1] * c[8] - c[2] * c[7]) / dtrm;
        inverseCovs[ci][1][1] = (c[0] * c[8] - c[2] * c[6]) / dtrm;
        inverseCovs[ci][2][1] = -(c[0] * c[7] - c[1] * c[6]) / dtrm;
        inverseCovs[ci][0][2] = (c[1] * c[5] - c[2] * c[4]) / dtrm;
        inverseCovs[ci][1][2] = -(c[0] * c[5] - c[2] * c[3]) / dtrm;
        inverseCovs[ci][2][2] = (c[0] * c[4] - c[1] * c[3]) / dtrm;
    }
}

bool GMM::bigThan1Cov(const Vec3d color) const {
    for (int ci = 0; ci < componentsCount; ci++) {
        if ((*this)(ci, color) > p1Cov[ci]) {
            return true;
        }
    }
    return false;
}

bool GMM::smallThan2Cov(const Vec3d color) const {
    for (int ci = 0; ci < componentsCount; ci++) {
        if ((*this)(ci,color) >p3Cov[ci]) {
            return false;
        }
    }
    return true;
}
