#ifndef MYLABEL_H
#define MYLABEL_H
#include<QLabel>
#include <QLabel>
#include <QPoint>
#include <QColor>
#include <QImage>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>

class myLabel : public QLabel
{
public:
    myLabel(QWidget *parent);
    //设定显示图像
    void setImg(QImage img);
    //刷新显示
    void refreshPixmap();
    //鼠标按下
    void mousePressEvent(QMouseEvent *e);
    //鼠标移动
    void mouseMoveEvent(QMouseEvent *e);
    //鼠标抬起
    void mouseReleaseEvent(QMouseEvent *e);
    //窗体大小改变
    void resizeEvent(QResizeEvent *e);
    //标记前景
    void markBg();
    //标记背景
    void markFg();
    //是否标记完成
    bool canRan();

    cv::Mat mask;
private:
    QImage img,imgSrc;
    bool imgOpened = false;
    bool bgStartMark = false;
    bool fgStartMark = false;
    bool bgMarked = false;
    bool fgMarked = false;
};

#endif // MYLABEL_H
