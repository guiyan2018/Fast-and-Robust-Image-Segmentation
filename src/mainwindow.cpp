#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "Bilateral.h"
#include <iostream>
#include "opencv2/imgproc.hpp"
#include <opencv2/highgui/highgui.hpp>
#include <vector>
#include <string>
#include <QMessageBox>
#include <QFileDialog>
#include <QDebug>
#include <QTime>
#include <QDir>
#include <QFile>


QString fileName;
Bilateral bilateral;
Mat imgSrc,outputMask,lastImg;

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->lineEdit->setValidator(new QIntValidator(1, 500, this));
    ui->lineEdit_2->setValidator(new QIntValidator(1, 256, this));
    ui->lineEdit_4->setValidator(new QDoubleValidator(0.01, 100.0, 3, this));
    Bilateral::xyStep = ui->lineEdit->text().toInt();
    Bilateral::rgbStep = ui->lineEdit_2->text().toInt();
    Bilateral::E2W = ui->lineEdit_4->text().toDouble();
}

MainWindow::~MainWindow()
{
    delete ui;
}

//打开文件
void MainWindow::on_open_triggered()
{
    fileName = QFileDialog::getOpenFileName(
                    this, tr(u8"Open image"),
                    "./", tr("Image files(*.bmp *.jpg *.png);"));

    if(fileName.isEmpty())
    {
        QMessageBox mesg;
        mesg.warning(this,u8"warning",u8"Failed to open image!");
        return;
    }
    else
    {
        QImage img(fileName);
        //文件路径由QString转为char *，按系统编码方式，防止中文路径失败
        imgSrc=imread(fileName.toStdString());
        if(imgSrc.empty()){
            imgSrc=imread(fileName.toLocal8Bit().data());
        }
        if(img.isNull()||imgSrc.empty())
        {
            QMessageBox::information(this,u8"warning",u8"Failed to open file");
        }
        else
        {
            bilateral=Bilateral(imgSrc);
            ui->label->setImg(img);
            ui->label->refreshPixmap();
        }
    }
}


void MainWindow::on_save_triggered()
{
    QString dir = QFileDialog::getExistingDirectory(this, tr(u8"Save results"),
                                                     "./",
                                                     QFileDialog::ShowDirsOnly
                                                     | QFileDialog::DontResolveSymlinks);

    if(dir.isEmpty())
    {
        QMessageBox mesg;
        mesg.warning(this,u8"warning",u8"Failed to save result!");
        return;
    }
    else
    {
        imwrite(std::string(dir.toLocal8Bit().data())+"/result.bmp", lastImg);
        imwrite(std::string(dir.toLocal8Bit().data())+"/mask.bmp", outputMask*255);
    }
}

void MainWindow::on_action_triggered()
{
    ui->label->markBg();
}


void MainWindow::on_action_3_triggered()
{
    ui->label->markFg();
}

void MainWindow::on_run_triggered()
{
    //判断是否标记完成
    if(ui->label->canRan()){
        cv::Mat inputMask;
        //标记的mask重新调整大小为原图大小，用最邻近差值算法
        cv::resize(ui->label->mask,inputMask,imgSrc.size(),0,0,INTER_NEAREST);
        bilateral.InitGmms(inputMask);
        //获取提取结果
        bilateral.run(outputMask);
        //中值滤波获得更平滑结果
        Mat maskBlur;
        cv::medianBlur(outputMask, maskBlur, 3);
        outputMask = maskBlur;
        //最后提取结果
        lastImg.create(imgSrc.size(), CV_8UC3);
        lastImg.setTo(cv::Scalar(81, 249, 182));
        imgSrc.copyTo(lastImg, outputMask);
        //显示结果，缩放至合适比例
        Mat imgShow;
        if(imgSrc.cols>1600||imgSrc.rows>900){
            double s = 1600.0/imgSrc.cols;
            if(imgSrc.rows*s>900)
                s=900.0/imgSrc.rows;
            cv::resize(lastImg,imgShow,cv::Size(0,0),s,s,INTER_LINEAR);
        }else{
            imgShow = lastImg;
        }
        imshow("result", imgShow);
    }else{
        QMessageBox mesg;
        mesg.warning(this,u8"warning",u8"Please run after marking!");
        return;
    }
}

void MainWindow::on_action_4_triggered()
{
    //打开文件了才可以重置
    if(fileName.isEmpty())
    {
        QMessageBox mesg;
        mesg.warning(this,u8"warning",u8"Reset marke failed!");
        return;
    }
    else
    {
        QImage img(fileName);
        imgSrc=imread(fileName.toStdString());
        if(imgSrc.empty()){
            imgSrc=imread(fileName.toLocal8Bit().data());
        }
        if(img.isNull()||imgSrc.empty())
        {
            QMessageBox::information(this,u8"warning",u8"Failed to open file");
        }
        else
        {
            bilateral=Bilateral(imgSrc);
            ui->label->setImg(img);
            ui->label->refreshPixmap();
        }
    }
}

void MainWindow::on_pushButton_clicked()
{
    //参数有效范围限定，其中a*b<16可使得网格不至于过多，内存过大
    int a = ui->lineEdit->text().toInt();
    if(a<1||a>500){
        a=50;
        ui->lineEdit->setText(QString::number(a));
    }
    Bilateral::xyStep = a;

    int b = ui->lineEdit_2->text().toInt();
    if(b<1||b>256||a*b<16){
        b=256;
        ui->lineEdit_2->setText(QString::number(b));
    }
    Bilateral::rgbStep = b;

    double c = ui->lineEdit_4->text().toDouble();
    if(c<0.01||c>100.0){
        c=1;
        ui->lineEdit_4->setText(QString::number(c));
    }
    Bilateral::E2W = c;

    //修改参数后重新构建双边网格
    if(!imgSrc.empty())
    {
        bilateral=Bilateral(imgSrc);
    }
}
