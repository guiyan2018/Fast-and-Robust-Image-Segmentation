#include "mylabel.h"
#include <QLabel>
#include <QPoint>
#include <QColor>
#include <QImage>
#include <QMouseEvent>
#include <QMessageBox>
#include <QPainter>
#include <QPen>

myLabel::myLabel(QWidget *parent) :
    QLabel(parent)
{

}

void myLabel::setImg(QImage qImg){
    imgSrc = qImg;
    qImg=qImg.scaled(this->width(),this->height(),Qt::IgnoreAspectRatio,Qt::SmoothTransformation);
    img = qImg;
    mask.create(this->height(), this->width(), CV_8UC1);
    mask.setTo(cv::Scalar::all(10));
    imgOpened = true;
    bgStartMark = false;
    fgStartMark = false;
    bgMarked = false;
    fgMarked = false;
}

void myLabel::refreshPixmap(){
    this->setPixmap(QPixmap::fromImage(img));
}

//鼠标按下
void myLabel::mousePressEvent(QMouseEvent *e)
{
    if(imgOpened){
        if(fgStartMark){
//            QPoint point = e->pos();
//            img.setPixelColor(point,QColor::QColor(255,0,0));
//            mask.at<uchar>(e->y(),e->x())=1;
//            refreshPixmap();

            QPainter painter(&img);
            QPen pen;
            pen.setColor(QColor(Qt::red));
            pen.setWidth(4);
            painter.setPen(pen);
            painter.drawPoint(e->pos());
            cv::circle(mask,cv::Point(e->x(),e->y()),4,cv::Scalar(1));
            refreshPixmap();

        }else if(bgStartMark){
//            QPoint point = e->pos();
//            img.setPixelColor(point,QColor::QColor(0,255,0));
//            mask.at<uchar>(e->y(),e->x())=0;
//            refreshPixmap();
            QPainter painter(&img);
            QPen pen;
            pen.setColor(QColor(Qt::blue));
            pen.setWidth(4);
            painter.setPen(pen);
            painter.drawPoint(e->pos());
            cv::circle(mask,cv::Point(e->x(),e->y()),4,cv::Scalar(0));
            refreshPixmap();
        }
    }

}

//鼠标移动
void myLabel::mouseMoveEvent(QMouseEvent *e)
{
    if(imgOpened){
        if(fgStartMark){
//            QPoint point = e->pos();
//            img.setPixelColor(point,QColor::QColor(255,0,0));
//            mask.at<uchar>(e->y(),e->x())=1;
//            refreshPixmap();

            QPainter painter(&img);
            QPen pen;
            pen.setColor(QColor(Qt::red));
            pen.setWidth(4);
            painter.setPen(pen);
            painter.drawPoint(e->pos());
            cv::circle(mask,cv::Point(e->x(),e->y()),4,cv::Scalar(1));
            refreshPixmap();

        }else if(bgStartMark){
//            QPoint point = e->pos();
//            img.setPixelColor(point,QColor::QColor(0,255,0));
//            mask.at<uchar>(e->y(),e->x())=0;
//            refreshPixmap();
            QPainter painter(&img);
            QPen pen;
            pen.setColor(QColor(Qt::blue));
            pen.setWidth(4);
            painter.setPen(pen);
            painter.drawPoint(e->pos());
            cv::circle(mask,cv::Point(e->x(),e->y()),4,cv::Scalar(0));
            refreshPixmap();
        }
    }
}

//鼠标抬起
void myLabel::mouseReleaseEvent(QMouseEvent *e)
{
    if(imgOpened){
        if(fgStartMark){
            fgMarked = true;
        }else if(bgStartMark){
            bgMarked = true;
        }
    }
}

void myLabel::resizeEvent(QResizeEvent *e){
    if(imgOpened){
        img=imgSrc.scaled(this->width(),this->height(),Qt::IgnoreAspectRatio,Qt::SmoothTransformation);
        this->setPixmap(QPixmap::fromImage(img));
        mask.create(this->height(), this->width(), CV_8UC1);
        mask.setTo(cv::Scalar::all(10));
    }
}

//标记前景
void myLabel::markFg()
{
    if(imgOpened){
        fgStartMark = true;
        bgStartMark = false;
    }
}

//标记背景
void myLabel::markBg()
{
    if(imgOpened){
        bgStartMark = true;
        fgStartMark = false;
    }
}

bool myLabel::canRan(){
    if(imgOpened&&fgMarked&&bgMarked){
        return true;
    }
    return false;

}


