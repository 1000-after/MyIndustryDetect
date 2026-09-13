#include "InspectWorker.h"         // 本类声明
#include "core/InspectPipeline.h"  // 真正的检测流程：run()

// 构造函数：目前没什么要初始化的，交给 QObject 即可
InspectWorker::InspectWorker(QObject* parent)
    : QObject(parent)  // 保存父对象指针（通常是 nullptr）
{
    // 空函数体：工人创建时不做检测，等 doInspect 被调用
}

// =========================================================
// 子线程入口：收到一张图后开始检测
// 谁调用它？
//   主窗口 emit inspectRequested(...) 
//   → Qt 排队到本线程
//   → 自动调用这个 doInspect
// =========================================================
void InspectWorker::doInspect(cv::Mat image, QString imagePath, InspectParams params)
{
    // ---------- 情况 A：图是空的 ----------
    if (image.empty()) {
        InspectResult empty;           // 做一个空结果
        empty.ok = true;               // 没有图，先当 OK（也可改成失败策略）
        empty.defectCount = 0;         // 缺陷数 0
        empty.imagePath = imagePath;   // 路径照样带回去
        emit finished(empty);          // 发信号通知 UI
        return;                        // 结束，后面不跑了
    }

    // ---------- 情况 B：正常检测 ----------
    // 在子线程里创建自己的 Pipeline
    // 为什么不共用主窗口的 m_pipeline？
    //   因为两个线程同时用一个对象不安全
    InspectPipeline pipeline;

    // 把 UI 传来的参数设置进去
    pipeline.setParams(params);

    // 跑完整流程：检测 → 判定 OK/NG → 得到 InspectResult
    // 这行可能比较耗时，但发生在子线程，所以主界面还能动
    InspectResult result = pipeline.run(image, imagePath);

    // 结果图再 clone 一次，保证发回主线程的是独立数据
    if (!result.viewImage.empty()) {
        result.viewImage = result.viewImage.clone();
    }

    // 发出完成信号：主窗口的 onDetectFinished 会收到
    emit finished(result);
}
