#include "tcpserver.h"
#include "ui_tcpserver.h"

#include<QTcpServer>
#include<QTcpSocket>
#include<QMessageBox>
#include<QTime>
#include<QDataStream>
#include<QFileDialog>

tcpserver::tcpserver(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::tcpserver)
{
    ui->setupUi(this);
    tcpPort=6666;
    tcpServer=new QTcpServer(this);

    connect(tcpServer,SIGNAL(newConnection()),this,SLOT(sendMessage()));
    initServer();
    qDebug()<<"成功打开tcpserve";

}

tcpserver::~tcpserver()
{
    delete ui;
}

//初始化
void tcpserver::initServer()
{
    paylocalSize=64*1024;
    totalBytes=0;
    bytesWritten=0;
    bytesToWrite=0;
    ui->serverStatuslabel->setText(tr("请选择要输入的文件"));
    ui->progressBar->reset();
    //设置按钮状态
    ui->serverOpenBtn->setEnabled(true);
    ui->serverSendBtn->setEnabled(false);
    tcpServer->close();
}

//发送数据
void tcpserver::sendMessage()
{
    ui->serverSendBtn->setEnabled(false);
    qDebug()<<"发送文件";
    clientConnection = tcpServer->nextPendingConnection();
    connect(clientConnection,SIGNAL(bytesWritten(qint64)),
            this,SLOT(updateClientProgress(qint64)));
    ui->serverStatuslabel->setText(tr("开始传送文件 %1 ！").arg(theFileName));

    //打开文件
    localFile = new QFile(fileName);
    if(!localFile->open((QFile::ReadOnly)))
    {
        QMessageBox::warning(this,tr("应用程序"),tr("无法读取文件 %1:\n %2")
                             .arg(fileName).arg(localFile->errorString()));
        return;
    }
    totalBytes = localFile->size();

    QDataStream sendOut(&outBlock,QIODevice::WriteOnly);
    sendOut.setVersion(QDataStream::Qt_5_4);
    time.start();   //启动计时器
    QString currentFile = fileName.right(fileName.size()
                                         - fileName.lastIndexOf('/')-1);
    sendOut << qint64(0) << qint64(0) << currentFile;
    totalBytes += outBlock.size();
    sendOut.device()->seek(0);
    sendOut << totalBytes << qint64((outBlock.size() - sizeof(qint64) * 2));
    bytesToWrite = totalBytes - clientConnection->write(outBlock);
    outBlock.resize(0);
    qDebug()<<"发送文件中";
}

//更新进度条
void tcpserver::updateClientProgress(qint64 numBytes)
{
    qApp->processEvents();   //使界面不会冻结
    bytesWritten += (int)numBytes;
    if(bytesWritten > 0) {
        outBlock = localFile->read(qMin(bytesToWrite,paylocalSize));
        bytesToWrite -= (int)clientConnection->write(outBlock);
        outBlock.resize(0);
    }
    else
    {
        localFile->close();
    }
    ui->progressBar->setMaximum(totalBytes);
    ui->progressBar->setValue(bytesWritten);

    float userTime = time.elapsed();   //获取耗费的时间
    double speed = bytesWritten / userTime;
    ui->serverStatuslabel->setText(tr("已发送 %1MB（%2MB/s)"
                                   "\n共%3MB 已用时：%4秒\n 估计剩余时间:%5秒")
            .arg(bytesWritten / (1024*1024))
            .arg(speed * 1000 / (1024*1024),0,'f',2)
            .arg(totalBytes / (1024*1024))
                                   .arg(userTime/1000,0,'f',0));

    if(bytesToWrite == totalBytes) {
        localFile->close();
        tcpServer->close();
        ui->serverStatuslabel->setText(tr("传送文件 %1 成功")
                                       .arg(theFileName));
    }

}

void tcpserver::on_serverOpenBtn_clicked()
{

    //文件对话框
    fileName = QFileDialog::getOpenFileName(this);
    if(!fileName.isEmpty())
    {
        theFileName = fileName.right(fileName.size() - fileName.lastIndexOf('/') - 1);
        ui->serverStatuslabel->setText(tr("要传送的文件为：%1").arg(theFileName));
        ui->serverSendBtn->setEnabled(true);
        ui->serverOpenBtn->setEnabled(false);
    }
}

void tcpserver::on_serverSendBtn_clicked()
{

    if(!tcpServer->listen(QHostAddress::Any,tcpPort))
    {
        qDebug() << tcpServer->errorString();
        close();
        return;
    }
    ui->serverStatuslabel->setText(tr("等待对方接收... ..."));
    ui->serverSendBtn->setEnabled(false);
    emit sendFileName(theFileName);
}

void tcpserver::on_serverCloseBtn_clicked()
{
    if(tcpServer->isListening())
    {
        tcpServer->close();
        if(localFile->isOpen())
            localFile->close();
        clientConnection->abort();
    }
    close();

}

//接收端拒绝接收
void tcpserver::refused()
{
    //关闭文件传输服务
    tcpServer->close();
    ui->serverStatuslabel->setText(tr("对方拒绝接收"));
}

//关闭事件
void tcpserver::closeEvent(QCloseEvent *)
{
    on_serverCloseBtn_clicked();

}
