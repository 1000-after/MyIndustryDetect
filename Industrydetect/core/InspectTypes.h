#pragma once
// #pragma once：防止本头文件被重复包含导致“类型重定义”

#include <QString>           // Qt 字符串
#include <vector>            // 动态数组 std::vector
#include <opencv2/core.hpp>  // cv::Mat
#include <QMetaType>         // Q_DECLARE_METATYPE 宏所在头文件

// =========================================================
// InspectParams：检测参数（配方）
// 以后可以放到界面滑条上调节
// =========================================================
struct InspectParams
{
    int blurSize = 5;          // 高斯模糊核大小（必须奇数）
    double thresh = 100.0;     // 二值化阈值
    double minArea = 80.0;     // 小于该面积的轮廓当噪声
    double maxArea = 20000.0;  // 大于该面积的轮廓忽略
};

// =========================================================
// DefectBox：一个缺陷框（像素单位）
// =========================================================
struct DefectBox
{
    int x = 0;          // 左上角 x
    int y = 0;          // 左上角 y
    int w = 0;          // 宽
    int h = 0;          // 高
    double area = 0.0;  // 轮廓面积
};

// =========================================================
// InspectResult：一次检测的完整结果
// UI / 线程信号 / 以后数据库 都用这份结构
// =========================================================
struct InspectResult
{
    bool ok = true;                 // true=OK，false=NG
    int defectCount = 0;            // 缺陷个数
    qint64 costMs = 0;              // 耗时（毫秒）
    QString imagePath;              // 图片路径
    cv::Mat viewImage;              // 画了红框的图
    std::vector<DefectBox> defects; // 缺陷列表
};

// =========================================================
// 下面两行非常重要（和线程有关）！
//
// Q_DECLARE_METATYPE(类型)：
//   告诉 Qt：“这个自定义结构体允许放进 QVariant / 跨线程信号里传递”
//
// 只有声明还不够，还要在运行时 qRegisterMetaType<...>() 注册一次
// （我们在 IndustryDetect 构造函数里做了注册）
// =========================================================
Q_DECLARE_METATYPE(InspectResult)
Q_DECLARE_METATYPE(InspectParams)
