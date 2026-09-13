#include "PatchDetector.h"        // 本类声明

#include <opencv2/imgproc.hpp>    // 模糊、阈值、形态学、画矩形等
#include <opencv2/geometry.hpp>   // OpenCV5：contourArea / boundingRect 在这里
#include <vector>                 // std::vector

// 把外部传入的参数保存到成员变量里
void PatchDetector::setParams(const InspectParams& params)
{
    m_params = params;  // 赋值拷贝一份参数
}

// 核心检测函数
int PatchDetector::detect(const cv::Mat& src, cv::Mat& dst, std::vector<DefectBox>* defects)
{
    // 如果调用者传了 defects 指针，先清空旧数据，避免上次结果残留
    if (defects) {
        defects->clear();
    }

    // 原图是空的，没法检测，返回 0 个缺陷
    if (src.empty()) {
        return 0;
    }

    // ----- 准备一张“可画彩色框”的输出图 dst -----
    // 红框是 BGR 彩色，所以灰度图要先转成 3 通道
    if (src.channels() == 1) {
        // 单通道灰度 → BGR 三通道（看起来仍是灰的，但可以画彩色线）
        cv::cvtColor(src, dst, cv::COLOR_GRAY2BGR);
    } else {
        // 已经是彩色：克隆一份，避免直接改到原图 src
        dst = src.clone();
    }

    // ----- 得到灰度图 gray，供后续阈值使用 -----
    cv::Mat gray;  // 灰度图变量
    if (src.channels() == 1) {
        gray = src;  // 本来就是灰度，直接用（这里是共享数据头，后面只读一般没问题）
    } else {
        // 彩色转灰度
        cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
    }

    // ----- 高斯模糊：减弱钢材纹理噪声 -----
    int k = m_params.blurSize;  // 从参数取出模糊核大小
    if (k % 2 == 0) {
        ++k;  // 若是偶数，加 1 变成奇数（GaussianBlur 要求奇数）
    }
    cv::Mat blur;  // 模糊后的图
    // 对 gray 做高斯模糊，结果放 blur；Size(k,k) 是核大小；0 表示标准差由核大小自动算
    cv::GaussianBlur(gray, blur, cv::Size(k, k), 0);

    // ----- 阈值分割：把“可能是斑块”的区域变成白色 -----
    cv::Mat binary;  // 二值图（只有黑白）
    // THRESH_BINARY_INV：像素 < thresh 变成 255（白/前景），否则 0（黑/背景）
    // 因为斑块通常比背景更暗，所以用 INV
    cv::threshold(blur, binary, m_params.thresh, 255, cv::THRESH_BINARY_INV);

    // ----- 形态学开运算：先腐蚀再膨胀，去掉小白点噪声 -----
    // 创建一个 3x3 矩形结构元素（小刷子）
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3));
    // 对 binary 做开运算，结果写回 binary
    cv::morphologyEx(binary, binary, cv::MORPH_OPEN, kernel);

    // ----- 找轮廓：每个白色连通块变成一串点 -----
    std::vector<std::vector<cv::Point>> contours;  // 轮廓列表
    // RETR_EXTERNAL：只找最外层轮廓
    // CHAIN_APPROX_SIMPLE：压缩轮廓点，省内存
    cv::findContours(binary, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    // ----- 按面积过滤，并在 dst 上画红框 -----
    int defectCount = 0;  // 统计有效缺陷个数
    for (const auto& c : contours) {  // 遍历每一个轮廓 c
        // 计算该轮廓面积
        const double area = cv::contourArea(c);

        // 面积太小或太大：跳过（不当作缺陷）
        if (area < m_params.minArea || area > m_params.maxArea) {
            continue;  // 进入下一轮 for
        }

        // 用外接正矩形框住这个轮廓
        const cv::Rect box = cv::boundingRect(c);

        // 在输出图 dst 上画矩形
        // Scalar(0,0,255) 是 BGR 的红色；线宽 2
        cv::rectangle(dst, box, cv::Scalar(0, 0, 255), 2);

        // 如果调用者要缺陷列表，就把框信息存进去
        if (defects) {
            DefectBox d;          // 创建一个缺陷结构体
            d.x = box.x;          // 左上角 x
            d.y = box.y;          // 左上角 y
            d.w = box.width;      // 宽
            d.h = box.height;     // 高
            d.area = area;        // 轮廓面积
            defects->push_back(d); // 追加到数组末尾
        }

        ++defectCount;  // 有效缺陷数 +1
    }

    // 返回本次检出的缺陷总数
    return defectCount;
}
