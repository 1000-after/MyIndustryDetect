#include "IndustryDetect.h"          // 引入主窗口类 IndustryDetect 的声明
#include <QtWidgets/QApplication>    // 引入 Qt 应用程序类（每个 GUI 程序必须有一个）

// 程序入口：操作系统从这里开始执行
int main(int argc, char *argv[])
{
    // 创建 Qt 应用程序对象；argc/argv 是命令行参数
    QApplication a(argc, argv);

    // 创建主窗口对象（我们的工业检测界面）
    IndustryDetect w;

    // 显示主窗口（不调用 show 的话窗口不会出现）
    w.show();

    // 进入事件循环：等待鼠标点击、键盘等事件，直到退出程序
    // 返回值通常是退出码
    return a.exec();
}
