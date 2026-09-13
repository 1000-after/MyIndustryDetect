#pragma once
// 防止头文件被重复包含

#include "core/InspectTypes.h"  // 使用 InspectParams、DefectBox 等公共类型
#include <opencv2/core.hpp>     // 使用 cv::Mat

// =========================================================
// PatchDetector：斑块（patches）检测算法类
// 传统视觉：模糊 → 阈值 → 轮廓 → 画框
// 以后如果换成深度学习，只要新写一个 Detector，Pipeline 仍可复用
// =========================================================
class PatchDetector
{
public:
    // 设置检测参数（阈值、面积范围等）
    void setParams(const InspectParams& params);

    // 执行检测
    // src     : 输入原图（不会在原图上乱画，结果画到 dst）
    // dst     : 输出“画了红框的图”
    // defects : 可选；如果传入非空指针，就把每个缺陷框写进这个数组
    // 返回值  : 缺陷个数
    int detect(const cv::Mat& src, cv::Mat& dst, std::vector<DefectBox>* defects = nullptr);

private:
    InspectParams m_params;  // 当前使用的检测参数（成员变量）
};
