#include "IndustryDetect.h"       // 本窗口类声明
#include "core/ImageConverter.h"  // Mat → QImage 转换

#include <QFileDialog>   // 打开文件 / 选择文件夹对话框
#include <QMessageBox>   // 弹窗提示
#include <QPixmap>       // 显示到 QLabel 用
#include <QThread>       // 工作线程
#include <QMetaType>     // qRegisterMetaType
#include <QLabel>        // 状态栏欢迎语用的 QLabel
#include <QAbstractItemView> // 表格选择/编辑行为枚举
#include <QSettings>     // 第6课：把参数持久化到本地（Windows 下常在注册表）

#include <opencv2/imgcodecs.hpp> // cv::imread

// 批量处理用到的头文件
#include <QDir>              // 遍历文件夹
#include <QFileInfo>         // 取文件名、绝对路径
#include <QHeaderView>       // 表头：最后一列拉伸等
#include <QTableWidgetItem>  // 表格单元格内容

// =========================================================
// 构造函数：窗口创建时只执行一次
// =========================================================
IndustryDetect::IndustryDetect(QWidget *parent)
    : QMainWindow(parent)  // 先初始化父类主窗口
{
    // 根据 .ui 创建控件，并填到 ui.xxx 指针里
    ui.setupUi(this);

    // ---------- 注册元类型：自定义类型才能安全跨线程传信号 ----------
    qRegisterMetaType<InspectResult>("InspectResult");
    qRegisterMetaType<InspectParams>("InspectParams");
    qRegisterMetaType<cv::Mat>("cv::Mat");

    // ---------- 单张：按钮 → 槽 ----------
    connect(ui.btnOpen, &QPushButton::clicked,
            this, &IndustryDetect::onOpenImage);
    connect(ui.btnDetect, &QPushButton::clicked,
            this, &IndustryDetect::onDetect);

    // ---------- 批量：按钮/表格 → 槽 ----------
    connect(ui.btnOpenFolder, &QPushButton::clicked,
            this, &IndustryDetect::onOpenFolder);
    connect(ui.btnBatchDetect, &QPushButton::clicked,
            this, &IndustryDetect::onBatchDetect);
    // 点击表格某个单元格 → 回调里用 row 回看对应结果图
    connect(ui.tableResult, &QTableWidget::cellClicked,
            this, &IndustryDetect::onTableRowClicked);

    // ---------- 参数配方：应用 / 恢复默认 ----------
    connect(ui.btnApplyParams, &QPushButton::clicked,
            this, &IndustryDetect::onApplyParams);
    connect(ui.btnResetParams, &QPushButton::clicked,
            this, &IndustryDetect::onResetParams);

    // 设置表格列数、表头、不可编辑等
    setupResultTable();

    // ---------- 第6课：先读上次保存的参数，再显示到左侧 Spin ----------
    // 若从来没保存过，loadParams 内部会用默认值（5 / 100 / 80 / 20000）
    loadParams();
    paramsToUi(m_params);           // 内存 → 界面控件
    m_pipeline.setParams(m_params); // 同步给主窗口这份 pipeline（Worker 用 emit 时带的 m_params）

    // 创建子线程 + Worker，并 start
    setupWorkerThread();

    // ---------- 第7课：启动时尝试连接 MySQL（失败不崩程序，只提示）----------
    // 连接信息封装在 InspectDb::open() 里：127.0.0.1:3306 / industry_detect / root
    if (m_db.open()) {
        ui.labelStatus->setText(QStringLiteral("数据库已连接（ODBC → MySQL）"));
    } else {
        // 没连上也能继续检测/看图；只是不会入库
        ui.labelStatus->setText(
            QStringLiteral("数据库未连接：%1").arg(m_db.lastError()));
    }

    // 在主窗口底部状态栏加一句欢迎语（和 labelStatus 是两回事）
    QLabel *label = new QLabel(this);                 // parent=this，随窗口释放
    label->setText(QStringLiteral("欢迎使用IndustryDetect"));
    statusBar()->addWidget(label);                    // 放到状态栏左侧
}

// =========================================================
// 搭建工作线程：工人 move 过去，用信号收发任务
// =========================================================
void IndustryDetect::setupWorkerThread()
{
    // 1. 创建线程对象；parent=this → 窗口销毁时 Qt 会处理这个 QThread 对象
    m_workerThread = new QThread(this);

    // 2. 创建工人；不要设 parent，否则不能顺利 moveToThread
    m_worker = new InspectWorker();

    // 3. 把工人“搬家”到子线程：之后它的槽在子线程执行
    m_worker->moveToThread(m_workerThread);

    // 4. 线程结束时自动删除工人，防止泄漏
    connect(m_workerThread, &QThread::finished,
            m_worker, &QObject::deleteLater);

    // 5. UI 发任务 → Worker::doInspect（跨线程自动排队）
    connect(this, &IndustryDetect::inspectRequested,
            m_worker, &InspectWorker::doInspect);

    // 6. Worker 完成 → UI::onDetectFinished（回到主线程改界面）
    connect(m_worker, &InspectWorker::finished,
            this, &IndustryDetect::onDetectFinished);

    // 7. 启动子线程事件循环（空闲时阻塞等待，不空转占满 CPU）
    m_workerThread->start();
}

// =========================================================
// 析构：先保存参数，再让子线程退出
// =========================================================
IndustryDetect::~IndustryDetect()
{
    // 退出前把当前配方写到本机，下次打开还能用
    saveParams();

    if (m_workerThread) {
        m_workerThread->quit();  // 请求结束事件循环
        m_workerThread->wait();  // 阻塞等到线程真正结束
    }
}

// =========================================================
// 忙状态：检测中禁用相关按钮，避免重复点击打乱队列
// =========================================================
void IndustryDetect::setBusy(bool busy)
{
    m_busy = busy;                           // 记下当前是否忙碌
    ui.btnDetect->setEnabled(!busy);         // !busy：忙时 false=禁用
    ui.btnOpen->setEnabled(!busy);
    ui.btnOpenFolder->setEnabled(!busy);
    ui.btnBatchDetect->setEnabled(!busy);
    // 检测进行中也不要改参数，避免和正在跑的任务混淆
    ui.btnApplyParams->setEnabled(!busy);
    ui.btnResetParams->setEnabled(!busy);
    ui.groupParams->setEnabled(!busy);
}

// =========================================================
// 显示：Mat → QImage → QPixmap → labelImage
// =========================================================
void IndustryDetect::showMat(const cv::Mat& mat)
{
    // 转成 Qt 图像
    const QImage img = ImageConverter::matToQImage(mat);
    // 空图不刷新，避免清掉当前显示
    if (img.isNull()) {
        return;
    }
    // 转成适合控件显示的 Pixmap
    const QPixmap pix = QPixmap::fromImage(img);
    // 按 label 大小缩放；KeepAspectRatio=不变形；Smooth=更平滑
    ui.labelImage->setPixmap(
        pix.scaled(ui.labelImage->size(),
                   Qt::KeepAspectRatio,
                   Qt::SmoothTransformation));
}

// =========================================================
// 第6课：把 InspectParams 显示到左侧 Spin 控件
// =========================================================
void IndustryDetect::paramsToUi(const InspectParams& p)
{
    ui.spinBlur->setValue(p.blurSize);                      // 模糊核（整数）
    ui.spinThresh->setValue(static_cast<int>(p.thresh));    // 阈值（Spin 是 int，转一下）
    ui.spinMinArea->setValue(p.minArea);                    // 最小面积（DoubleSpin）
    ui.spinMaxArea->setValue(p.maxArea);                    // 最大面积
}

// =========================================================
// 第6课：从左侧 Spin 读到 m_params，并同步 pipeline
// 注意：真正检测时 Worker 用的是 emit 时带过去的那份 m_params 拷贝
// =========================================================
void IndustryDetect::uiToParams()
{
    m_params.blurSize = ui.spinBlur->value();
    // 高斯模糊核边长必须是奇数（3/5/7...）；偶数则 +1
    if (m_params.blurSize % 2 == 0) {
        ++m_params.blurSize;
        ui.spinBlur->setValue(m_params.blurSize);  // 顺便把界面也改成奇数，避免误解
    }

    m_params.thresh = ui.spinThresh->value();
    m_params.minArea = ui.spinMinArea->value();
    m_params.maxArea = ui.spinMaxArea->value();

    // 主窗口这份 pipeline 也更新一下（异步检测主要靠 emit 的 params）
    m_pipeline.setParams(m_params);
}

// =========================================================
// 第6课：从本机配置读取上次参数
// QSettings("组织名","应用名")：Windows 下通常写进注册表
// value(键, 默认值)：没有存过就用默认值
// =========================================================
void IndustryDetect::loadParams()
{
    QSettings s(QStringLiteral("IndustryDetect"), QStringLiteral("Params"));
    m_params.blurSize = s.value(QStringLiteral("blurSize"), 5).toInt();
    m_params.thresh = s.value(QStringLiteral("thresh"), 100.0).toDouble();
    m_params.minArea = s.value(QStringLiteral("minArea"), 80.0).toDouble();
    m_params.maxArea = s.value(QStringLiteral("maxArea"), 20000.0).toDouble();

    // 防止读到非法偶数模糊核
    if (m_params.blurSize % 2 == 0) {
        ++m_params.blurSize;
    }
}

// =========================================================
// 第6课：把当前参数写到本机（先以界面为准再保存）
// =========================================================
void IndustryDetect::saveParams()
{
    uiToParams();  // 确保 m_params 和 Spin 一致

    QSettings s(QStringLiteral("IndustryDetect"), QStringLiteral("Params"));
    s.setValue(QStringLiteral("blurSize"), m_params.blurSize);
    s.setValue(QStringLiteral("thresh"), m_params.thresh);
    s.setValue(QStringLiteral("minArea"), m_params.minArea);
    s.setValue(QStringLiteral("maxArea"), m_params.maxArea);
}

// =========================================================
// 第6课：点击「应用参数」
// =========================================================
void IndustryDetect::onApplyParams()
{
    if (m_busy) {
        return;  // 检测中不允许改
    }
    uiToParams();
    ui.labelStatus->setText(
        QStringLiteral("参数已应用：blur=%1 thresh=%2 minArea=%3 maxArea=%4")
            .arg(m_params.blurSize)
            .arg(m_params.thresh)
            .arg(m_params.minArea)
            .arg(m_params.maxArea));
}

// =========================================================
// 第6课：点击「恢复默认」
// =========================================================
void IndustryDetect::onResetParams()
{
    if (m_busy) {
        return;
    }
    // InspectParams{} 使用结构体里写的默认成员初值
    m_params = InspectParams{};
    paramsToUi(m_params);
    m_pipeline.setParams(m_params);
    ui.labelStatus->setText(QStringLiteral("已恢复默认参数"));
}

// =========================================================
// 单张：打开一张图片（不写表格）
// =========================================================
void IndustryDetect::onOpenImage()
{
    // 正在检测时不允许换图
    if (m_busy) {
        return;
    }

    // 文件对话框默认目录：你的 patches 演示集
    const QString defaultDir =
        QStringLiteral("D:/QT/project/Industrydetect/datasets/patches_demo/images");

    // 让用户选一张图；取消则 path 为空
    const QString path = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("打开图片"),
        defaultDir,
        QStringLiteral("Images (*.jpg *.png *.bmp)"));

    if (path.isEmpty()) {
        return;  // 用户取消
    }

    // OpenCV 读图到成员变量
    m_image = cv::imread(path.toLocal8Bit().toStdString(), cv::IMREAD_UNCHANGED);
    if (m_image.empty()) {
        QMessageBox::warning(this, QStringLiteral("错误"),
                             QStringLiteral("OpenCV 无法打开图片：\n") + path);
        return;
    }

    m_imagePath = path;   // 记住路径，检测结果里会带上
    showMat(m_image);     // 左边显示原图
    // 状态提示（central 里的 labelStatus，不是底部 statusBar）
    ui.labelStatus->setText(
        QStringLiteral("已加载(OpenCV)：%1 | %2x%3 channels=%4")
            .arg(path)
            .arg(m_image.cols)
            .arg(m_image.rows)
            .arg(m_image.channels()));
}

// =========================================================
// 单张：开始检测（只发任务；结果不进表格）
// =========================================================
void IndustryDetect::onDetect()
{
    // 还没打开图
    if (m_image.empty()) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("请先打开一张图片"));
        return;
    }
    // 已在忙（单张或批量）则忽略
    if (m_busy) {
        return;
    }

    // 明确标记：这是单张模式，finished 时不要写表格
    m_batchMode = false;

    // 即使忘记点「应用参数」，检测前也按当前 Spin 取值
    uiToParams();

    setBusy(true);  // 按钮变灰
    ui.labelStatus->setText(QStringLiteral("检测中（后台线程）…"));

    // clone：子线程用副本，避免和 UI 的 m_image 抢同一块内存
    const cv::Mat imageCopy = m_image.clone();

    // 发到子线程；这里立刻返回，界面不卡
    // 第三个参数 m_params：工人线程里会按这份配方检测
    emit inspectRequested(imageCopy, m_imagePath, m_params);
}

// =========================================================
// 检测完成：单张和批量都走这里，用 m_batchMode 分支
// =========================================================
void IndustryDetect::onDetectFinished(InspectResult result)
{
    // 左边先显示本次结果图（单张/批量当前张都适用）
    showMat(result.viewImage);

    // ---------- 第7课：无论单张还是批量，都尝试把结果写入 MySQL ----------
    // saveResultToDb 内部失败会改 labelStatus；成功返回 true
    const bool saved = saveResultToDb(result);

    // ---------- 分支 A：单张模式 → 不写表 ----------
    if (!m_batchMode) {
        setBusy(false);  // 恢复按钮

        // 拼状态文字：检测结果 + 是否入库成功
        QString text;
        if (result.ok) {
            text = QStringLiteral("检测结果：OK | 耗时 %1 ms | 异步线程")
                       .arg(result.costMs);
        } else {
            text = QStringLiteral("检测结果：NG | 缺陷数=%1 | 耗时 %2 ms | 异步线程")
                       .arg(result.defectCount)
                       .arg(result.costMs);
        }
        // 只有入库成功才追加“已入库”；失败时 saveResultToDb 已经写过错误原因，
        // 这里仍覆盖为检测结果，并在末尾带一句提示，避免小白看不到 OK/NG
        if (saved) {
            text += QStringLiteral(" | 已入库");
        } else {
            text += QStringLiteral(" | 未入库（看上一句/检查 MySQL）");
        }
        ui.labelStatus->setText(text);
        return;  // 单张到此结束，绝不 appendResultRow
    }

    // ---------- 分支 B：批量模式 → 写表 + 发下一张 ----------
    appendResultRow(result);                              // 表格加一行
    m_batchViewImages.push_back(result.viewImage.clone()); // 存图供点击回看
    ++m_batchIndex;                                       // 指向下一张索引
    startNextBatchItem();                                 // 继续队列（或结束）
}

// =========================================================
// 第7课：把一次检测结果交给 InspectDb 入库
//
// 返回值：
//   true  = 插入成功
//   false = 未连接或 SQL 失败（不打断检测/批量）
//
// 注意：
//   - 窗口自己不写 SQL（低耦合）
//   - 用当前 m_params，表示“这次检测实际用的配方”
// =========================================================
bool IndustryDetect::saveResultToDb(const InspectResult& result)
{
    // 没连上就尝试再 open 一次（例如启动时 MySQL 还没开，后来开了）
    if (!m_db.isOpen()) {
        if (!m_db.open()) {
            // 批量时短暂提示一下，下一张进度仍会覆盖
            ui.labelStatus->setText(
                QStringLiteral("入库跳过（库未连接）：%1").arg(m_db.lastError()));
            return false;
        }
    }

    if (!m_db.insertRecord(result, m_params)) {
        ui.labelStatus->setText(
            QStringLiteral("入库失败：%1").arg(m_db.lastError()));
        return false;
    }

    return true;
}

// =========================================================
// 初始化右边结果表（程序启动时调用一次）
// =========================================================
void IndustryDetect::setupResultTable()
{
    // 4 列：文件名 / OK·NG / 缺陷数 / 耗时
    ui.tableResult->setColumnCount(4);
    // 设置表头文字
    ui.tableResult->setHorizontalHeaderLabels(
        {QStringLiteral("文件名"), QStringLiteral("结果"),
         QStringLiteral("缺陷数"), QStringLiteral("耗时ms")});
    // 最后一列随表格变宽而拉伸
    ui.tableResult->horizontalHeader()->setStretchLastSection(true);
    // 点击时整行选中，而不是只选一个格子
    ui.tableResult->setSelectionBehavior(QAbstractItemView::SelectRows);
    // 禁止用户双击改表格内容
    ui.tableResult->setEditTriggers(QAbstractItemView::NoEditTriggers);
    // 初始 0 行
    ui.tableResult->setRowCount(0);
}

// =========================================================
// 批量：选择文件夹，只收集路径，不立刻检测
// =========================================================
void IndustryDetect::onOpenFolder()
{
    // 检测进行中不允许重选文件夹
    if (m_busy) {
        return;
    }

    // 弹出“选择文件夹”对话框
    const QString dir = QFileDialog::getExistingDirectory(
        this,
        QStringLiteral("选择图片文件夹"),
        QStringLiteral("D:/QT/project/Industrydetect/datasets/patches_demo/images"));

    // 用户取消
    if (dir.isEmpty()) {
        return;
    }

    // 只接受这些后缀
    QStringList filters;
    filters << "*.jpg" << "*.jpeg" << "*.png" << "*.bmp";

    QDir d(dir);  // 指向该目录
    // 列出符合后缀的文件，按名字排序
    const QFileInfoList files = d.entryInfoList(filters, QDir::Files, QDir::Name);

    // 清空旧列表，重新装路径
    m_batchPaths.clear();
    for (const QFileInfo& fi : files) {
        // absoluteFilePath = 带盘符的完整路径
        m_batchPaths << fi.absoluteFilePath();
    }

    // 清空旧表格和旧结果图缓存
    ui.tableResult->setRowCount(0);
    m_batchViewImages.clear();
    m_batchIndex = 0;

    // 提示：还没开始检，等用户点“批量检测”
    ui.labelStatus->setText(
        QStringLiteral("已选择文件夹：%1 | 图片 %2 张（请点批量检测）")
            .arg(dir)
            .arg(m_batchPaths.size()));
}

// =========================================================
// 批量：开始按队列检测（结果会写入表格）
// =========================================================
void IndustryDetect::onBatchDetect()
{
    // 已在忙则忽略
    if (m_busy) {
        return;
    }
    // 还没选文件夹 / 文件夹里没图
    if (m_batchPaths.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("请先打开文件夹"));
        return;
    }

    // 批量开始前也同步一次界面参数
    uiToParams();

    m_batchMode = true;                 // 标记：后续 finished 要写表
    m_batchIndex = 0;                   // 从第 0 张开始
    ui.tableResult->setRowCount(0);     // 清空旧结果行
    m_batchViewImages.clear();          // 清空旧回看图

    setBusy(true);                      // 禁用按钮
    startNextBatchItem();               // 发出第一张任务
}

// =========================================================
// 批量队列核心：取当前索引的图 → 发给 Worker
// 若已全部完成 → 收尾
// =========================================================
void IndustryDetect::startNextBatchItem()
{
    // 索引越界 = 全部做完
    if (m_batchIndex >= m_batchPaths.size()) {
        m_batchMode = false;   // 退出批量模式
        setBusy(false);        // 恢复按钮
        ui.labelStatus->setText(
            QStringLiteral("批量完成：共 %1 张").arg(m_batchPaths.size()));
        return;
    }

    // 当前要处理的路径
    const QString path = m_batchPaths[m_batchIndex];

    // 显示进度：第几张 / 总共几张 / 文件名
    ui.labelStatus->setText(
        QStringLiteral("批量检测中 %1/%2 : %3")
            .arg(m_batchIndex + 1)                 // 给人看从 1 开始
            .arg(m_batchPaths.size())
            .arg(QFileInfo(path).fileName()));     // 只显示文件名，不显示整路径

    // 读图
    cv::Mat img = cv::imread(path.toLocal8Bit().toStdString(), cv::IMREAD_UNCHANGED);
    if (img.empty()) {
        // 读失败：表格仍记一行，缺陷数用 -1 表示失败
        InspectResult bad;
        bad.ok = false;
        bad.defectCount = -1;
        bad.costMs = 0;
        bad.imagePath = path;
        appendResultRow(bad);
        m_batchViewImages.push_back(cv::Mat());  // 占位：空图，点击行不会显示
        ++m_batchIndex;
        startNextBatchItem();  // 递归/继续下一张（仍在主线程调度，很快）
        return;
    }

    // 更新“当前图”概念，方便和单张逻辑一致
    m_image = img;
    m_imagePath = path;

    // 发给子线程检测（clone 后再发）；带上当前配方 m_params
    emit inspectRequested(img.clone(), path, m_params);
    // 注意：这里不 ++m_batchIndex
    // 等 onDetectFinished 成功处理完再 ++，避免和异步完成顺序乱掉
}

// =========================================================
// 往 tableResult 追加一行（仅批量成功路径会调用；单张不调用）
// =========================================================
void IndustryDetect::appendResultRow(const InspectResult& result)
{
    // 当前有多少行，新行就插在这个下标
    const int row = ui.tableResult->rowCount();
    ui.tableResult->insertRow(row);

    // 只要文件名，不要完整路径
    const QString name = QFileInfo(result.imagePath).fileName();

    // defectCount < 0 表示读图失败；否则根据 ok 显示 OK/NG
    const QString okNg = (result.defectCount < 0)
                             ? QStringLiteral("读图失败")
                             : (result.ok ? QStringLiteral("OK")
                                          : QStringLiteral("NG"));

    // 第 0~3 列写入单元格（new 的 item 由表格接管内存）
    ui.tableResult->setItem(row, 0, new QTableWidgetItem(name));
    ui.tableResult->setItem(row, 1, new QTableWidgetItem(okNg));
    ui.tableResult->setItem(row, 2, new QTableWidgetItem(QString::number(result.defectCount)));
    ui.tableResult->setItem(row, 3, new QTableWidgetItem(QString::number(result.costMs)));
}

// =========================================================
// 点击表格某一行 → 左边显示对应结果图
// =========================================================
void IndustryDetect::onTableRowClicked(int row, int column)
{
    Q_UNUSED(column);  // 本函数不用列号，避免编译器“未使用参数”警告

    // 行号非法
    if (row < 0 || row >= m_batchViewImages.size()) {
        return;
    }
    // 该行是读图失败占位，没有可显示的图
    if (m_batchViewImages[row].empty()) {
        return;
    }
    // 回看
    showMat(m_batchViewImages[row]);
}
