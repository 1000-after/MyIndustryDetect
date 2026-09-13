#include "InspectPipeline.h"  // 本类声明

#include <QElapsedTimer>      // Qt 计时器，用来统计耗时毫秒

// 设置参数：自己存一份，同时告诉 detector
void InspectPipeline::setParams(const InspectParams& params)
{
    m_params = params;              // 保存到流程自己的成员
    m_detector.setParams(params);   // 同步给算法模块
}

// 执行一次：检测 → 判定 → 返回结果
InspectResult InspectPipeline::run(const cv::Mat& image, const QString& imagePath)
{
    InspectResult result;           // 先创建一个空结果对象
    result.imagePath = imagePath;   // 记录这张图的路径

    // 如果输入图是空的：直接返回（不算 NG，缺陷数为 0）
    if (image.empty()) {
        result.ok = true;           // 没有图就不判 NG
        result.defectCount = 0;     // 缺陷数 0
        return result;              // 提前结束
    }

    QElapsedTimer timer;            // 创建计时器
    timer.start();                  // 开始计时

    // ----- 步骤1：调用检测器 -----
    // 把画框图写到 result.viewImage
    // 把框列表写到 result.defects
    // 返回缺陷个数到 result.defectCount
    result.defectCount = m_detector.detect(image, result.viewImage, &result.defects);

    // ----- 步骤2：判定 OK/NG -----
    // 规则很简单：缺陷数 == 0 → OK，否则 NG
    result.ok = (result.defectCount == 0);

    // ----- 步骤3：记录耗时 -----
    result.costMs = timer.elapsed();  // 从 start 到现在过了多少毫秒

    // 把完整结果返回给 UI（或以后的数据库模块）
    return result;
}
