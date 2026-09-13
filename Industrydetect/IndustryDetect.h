#pragma once
// #pragma once：保证本头文件在同一次编译中只会被包含一次，避免“类型重复定义”

#include <QtWidgets/QMainWindow>  // QMainWindow：带菜单栏/工具栏/状态栏的主窗口基类
#include "ui_IndustryDetect.h"    // 由 IndustryDetect.ui 自动生成：里面有 btnOpen、tableResult 等

#include "core/InspectPipeline.h" // 检测流程类（本窗口里保留一份参数同步用）
#include "core/InspectWorker.h"   // 子线程里真正跑检测的“工人”
#include "output/InspectDb.h"     // 第7课：检测结果写 MySQL（窗口只调用，不写 SQL）
#include <opencv2/core.hpp>       // cv::Mat：OpenCV 图像类型
#include <QStringList>            // QStringList：字符串列表，用来存批量文件路径
#include <QVector>                // QVector：动态数组，用来存每张批量结果图

class QThread;  // 前置声明：头文件里只用 QThread*，不必 #include 完整定义

// =========================================================
// IndustryDetect：程序主窗口（只做人机交互）
//
// 能做的事：
//   1) 单张：打开图片 → 开始检测 → 左边显示结果（不写表格）
//   2) 批量：打开文件夹 → 批量检测 → 右边表格一行行记录
//   3) 点表格某一行 → 左边回看该张结果图
//   4) 左侧配方参数：调节 / 应用 / 恢复默认 / 退出时自动保存
//   5) 第7课：检测完成后把结果交给 InspectDb 入库（追溯）
//
// 不做的事：
//   OpenCV 算法细节（在 PatchDetector）
//   耗时计算（在子线程 InspectWorker 里）
//   SQL / 连接串细节（在 InspectDb 里）
// =========================================================
class IndustryDetect : public QMainWindow
{
    // Q_OBJECT：启用信号/槽。没有它，connect、自定义信号都会出问题
    Q_OBJECT

public:
    // 构造函数：创建窗口时调用一次
    // parent 默认 nullptr = 没有父窗口，这是顶层窗口
    IndustryDetect(QWidget *parent = nullptr);

    // 析构函数：窗口关闭时调用；这里会保存参数并安全结束工作线程
    ~IndustryDetect();

private slots:
    // ---------- 单张相关槽（由按钮点击触发）----------
    void onOpenImage();                         // “打开图片”
    void onDetect();                            // “开始检测”（只发任务，不卡界面）
    void onDetectFinished(InspectResult result); // 子线程算完后回调（单张/批量都进这里）

    // ---------- 批量相关槽 ----------
    void onOpenFolder();                         // “打开文件夹”：收集图片路径列表
    void onBatchDetect();                        // “批量检测”：按队列一张张发给 Worker
    void onTableRowClicked(int row, int column); // 点击表格某行：回看结果图

    // ---------- 参数配方相关槽（第 6 课）----------
    void onApplyParams();   // “应用参数”：把 Spin 上的值写进 m_params
    void onResetParams();   // “恢复默认”：回到 InspectParams 结构体默认值

private:
    // ---------- 内部工具函数（不是槽，只给本类自己调用）----------
    void showMat(const cv::Mat& mat);            // 把 Mat 显示到 labelImage
    void setupWorkerThread();                    // 创建线程+工人并 connect
    void setBusy(bool busy);                     // 忙/闲：禁用或恢复按钮
    void setupResultTable();                     // 初始化表格列标题等
    void startNextBatchItem();                   // 批量队列：取下一张发给 Worker
    void appendResultRow(const InspectResult& result); // 往表格追加一行结果

    // ---------- 参数配方工具（第 6 课）----------
    void paramsToUi(const InspectParams& p); // 把内存里的参数显示到 Spin 控件上
    void uiToParams();                       // 从 Spin 读到 m_params，并同步 pipeline
    void loadParams();                       // 从 QSettings（注册表/配置）读上次参数
    void saveParams();                       // 把当前参数写到 QSettings

    // ---------- 第7课：数据库 ----------
    // 把一次检测结果写入 MySQL；成功 true，失败 false（不打断检测）
    bool saveResultToDb(const InspectResult& result);

signals:
    // 自定义信号：主窗口 → 子线程 Worker
    // 跨线程时 Qt 会排队传递参数，最终调用 Worker::doInspect
    // image     : 图像副本（发出前应 clone）
    // imagePath : 文件路径
    // params    : 检测参数（阈值、面积等）
    void inspectRequested(cv::Mat image, QString imagePath, InspectParams params);

private:
    // ---------- 界面 ----------
    Ui::IndustryDetectClass ui; // 所有控件入口：ui.btnOpen、ui.tableResult ...

    // ---------- 当前单张图 ----------
    cv::Mat m_image;        // 当前打开/正在看的图（UI 线程使用）
    QString m_imagePath;    // 当前图路径
    InspectParams m_params; // 当前检测参数（配方）：检测前会发给 Worker

    // 主窗口保留的 pipeline（主要用于 setParams；真正异步检测用 Worker 内部那份）
    InspectPipeline m_pipeline;

    // ---------- 异步线程 ----------
    QThread* m_workerThread = nullptr; // 工作线程；nullptr=还没创建
    InspectWorker* m_worker = nullptr; // 跑在工作线程里的检测工人
    bool m_busy = false;               // true=正在检测（单张或批量），防止连点

    // ---------- 批量专用 ----------
    // m_batchPaths：待检测的全部文件完整路径，例如 D:/.../patches_1.jpg
    QStringList m_batchPaths;
    // m_batchIndex：当前处理到第几张（0 表示第一张）
    int m_batchIndex = 0;
    // m_batchMode：true=批量流程；false=单张流程（决定 finished 时要不要写表格）
    bool m_batchMode = false;
    // m_batchViewImages：与表格行一一对应的结果图，点行时可 showMat 回看
    QVector<cv::Mat> m_batchViewImages;

    // ---------- 第7课：数据库 ----------
    // 只负责“打开/插入”；SQL 细节封装在 InspectDb 内部
    InspectDb m_db;
};
