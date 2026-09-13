# 架构说明（面试口述用）

## 分层

| 层 | 目录/类 | 职责 |
|----|---------|------|
| 界面 | `IndustryDetect` | 按钮、显示、表格、发任务、收结果 |
| 异步 | `InspectWorker` + `QThread` | 在子线程跑检测 |
| 流程 | `InspectPipeline` | 调算法、判 OK/NG、填 `InspectResult`、计时 |
| 算法 | `PatchDetector` | OpenCV：模糊→阈值→形态学→轮廓→面积过滤→画框 |
| 工具 | `ImageConverter` | `cv::Mat` → `QImage` |
| 数据 | `InspectDb` | ODBC 连接 MySQL，插入 `inspect_record` |
| 公共类型 | `InspectTypes.h` | `InspectParams` / `DefectBox` / `InspectResult` |

## 单张检测时序

```text
用户点击「开始检测」
  → UI：uiToParams()，clone 图像
  → emit inspectRequested(image, path, params)
  → Worker::doInspect（子线程）
  → Pipeline::run → PatchDetector
  → emit finished(InspectResult)
  → UI::onDetectFinished：showMat + InspectDb::insertRecord
```

## 批量检测

- `m_batchPaths` + `m_batchIndex` 组成队列
- 一张 `finished` 后再 `startNextBatchItem` 发下一张
- 表格与 `m_batchViewImages` 按行对应，点击行回看

## 为何这样拆

- **高内聚：** 算法只在 `vision/`，SQL 只在 `output/`
- **低耦合：** 换深度学习检测器时，优先替换 `PatchDetector` / Pipeline，UI 少动
- **可演示：** 异步 + 批量 + 入库，接近工业复检上位机叙事
