#pragma once
// 防止头文件被重复包含

#include "core/InspectTypes.h"       // InspectParams / InspectResult
#include "vision/PatchDetector.h"    // 具体的斑块检测器

// =========================================================
// InspectPipeline：工业视觉“流程引擎”
// 固定顺序：检测 → 判定 OK/NG → 填充 InspectResult
// UI 只调用 run()，不要在窗口类里直接写算法细节
// =========================================================
class InspectPipeline
{
public:
    // 设置整条流程使用的参数，并同步给内部 detector
    void setParams(const InspectParams& params);

    // 读取当前参数（小函数直接写在头文件里）
    InspectParams params() const { return m_params; }

    // 跑一次完整检测
    // image     : 输入图（一般是界面里当前打开的 m_image）
    // imagePath : 图片路径，只写入结果，方便以后入库；可省略（默认空）
    // 返回值    : 统一结果 InspectResult
    InspectResult run(const cv::Mat& image, const QString& imagePath = {});

private:
    InspectParams m_params;    // 当前配方参数
    PatchDetector m_detector;  // 真正做斑块检测的算法对象
};
