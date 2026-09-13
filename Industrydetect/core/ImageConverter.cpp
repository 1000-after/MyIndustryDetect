#include "ImageConverter.h"     // 本类自己的声明
#include <opencv2/imgproc.hpp>  // 颜色转换 cvtColor、通道处理等

// 把 OpenCV 的 Mat 转成 Qt 的 QImage，供界面显示
QImage ImageConverter::matToQImage(const cv::Mat& mat)
{
    // 如果 Mat 里没有数据，直接返回空 QImage
    if (mat.empty()) {
        return {};  // {} 表示默认构造的空对象
    }

    // ---------- 情况1：8 位单通道灰度图（NEU 数据集常见）----------
    // CV_8UC1：每个像素 0~255，只有 1 个通道
    if (mat.type() == CV_8UC1) {
        // 用 Mat 的底层像素内存“包装”成 QImage
        // mat.data : 像素起始地址
        // mat.cols : 宽度
        // mat.rows : 高度
        // mat.step : 每一行字节数（考虑内存对齐）
        // Format_Grayscale8 : 告诉 Qt 这是灰度图
        // .copy() : 复制一份像素，避免 QImage 和 Mat 共用内存导致花屏
        return QImage(mat.data, mat.cols, mat.rows,
                      static_cast<int>(mat.step),
                      QImage::Format_Grayscale8).copy();
    }

    // ---------- 情况2：8 位三通道彩色图 ----------
    // CV_8UC3：BGR 三通道；Qt 显示要用 RGB，所以先转换颜色顺序
    if (mat.type() == CV_8UC3) {
        cv::Mat rgb;  // 用来存放转换后的 RGB 图
        // 把 BGR 转成 RGB，结果写入 rgb
        cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
        // 按 RGB888 格式做成 QImage，并 copy 保证安全
        return QImage(rgb.data, rgb.cols, rgb.rows,
                      static_cast<int>(rgb.step),
                      QImage::Format_RGB888).copy();
    }

    // ---------- 情况3：其他少见格式（兜底）----------
    // 先转成 8 位灰度，保证至少能显示出来
    cv::Mat gray;  // 存放最终灰度图
    if (mat.channels() == 1) {
        // 已经是单通道：只把位深转成 8U
        mat.convertTo(gray, CV_8U);
    } else {
        // 多通道：先转 8 位，再转成灰度
        cv::Mat bgr;  // 临时 8 位图
        mat.convertTo(bgr, CV_8U);
        cv::cvtColor(bgr, gray, cv::COLOR_BGR2GRAY);
    }
    // 灰度图转 QImage 并复制
    return QImage(gray.data, gray.cols, gray.rows,
                  static_cast<int>(gray.step),
                  QImage::Format_Grayscale8).copy();
}
