#pragma once
// 防止头文件被重复包含

#include <QImage>            // Qt 的图像类型，后面要转成它再显示
#include <opencv2/core.hpp>  // OpenCV 的 cv::Mat

// =========================================================
// ImageConverter：图像格式转换工具类
// 只做一件事：把 OpenCV 的 Mat 转成 Qt 的 QImage
// 不做检测算法（算法在 PatchDetector 里）
// =========================================================
class ImageConverter
{
public:
    // static：不需要创建 ImageConverter 对象，直接 ImageConverter::matToQImage(...) 调用
    // 参数 mat：输入的 OpenCV 图像
    // 返回值：Qt 能用的 QImage；失败则返回空图
    static QImage matToQImage(const cv::Mat& mat);
};
