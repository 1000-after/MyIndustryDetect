#pragma once
// 防止头文件被重复包含

#include <QObject>              // 要有信号槽，必须继承 QObject
#include "core/InspectTypes.h"  // InspectResult / InspectParams
#include <opencv2/core.hpp>     // cv::Mat

// =========================================================
// InspectWorker：检测“工人”
//
// 它会被 moveToThread 放到子线程里。
// doInspect 在子线程执行，算完用信号把结果发回主窗口。
//
// 铁律：这里面永远不要写 ui.xxx、QLabel、QMessageBox 等界面代码！
// =========================================================
class InspectWorker : public QObject
{
    Q_OBJECT  // 启用信号槽（Worker 也必须有）

public:
    // explicit：防止把指针莫名其妙隐式转换成 InspectWorker
    // parent：一般传 nullptr，因为要 moveToThread
    explicit InspectWorker(QObject* parent = nullptr);

public slots:
    // 槽函数：在子线程被调用
    // image     : 主窗口 clone 过来的图（工人自己的那份）
    // imagePath : 图片路径，原样写进结果
    // params    : 阈值、面积等参数
    void doInspect(cv::Mat image, QString imagePath, InspectParams params);

signals:
    // 检测正常结束：把完整结果发给主窗口
    void finished(InspectResult result);
    // 预留：以后出错可以用（现在还没用上）
    void failed(QString message);
};
