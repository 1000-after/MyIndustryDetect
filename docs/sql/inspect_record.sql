-- IndustryDetect：检测结果追溯表
-- 库名：industry_detect
-- 在 Navicat「查询」中先选中库再执行，或取消下一行注释：

USE industry_detect;

CREATE TABLE IF NOT EXISTS inspect_record (
  id            BIGINT PRIMARY KEY AUTO_INCREMENT COMMENT '主键',
  image_path    VARCHAR(512)  NOT NULL COMMENT '图片完整路径',
  file_name     VARCHAR(255)  NOT NULL COMMENT '文件名',
  result        VARCHAR(16)   NOT NULL COMMENT 'OK/NG/FAIL',
  defect_count  INT           NOT NULL DEFAULT 0 COMMENT '缺陷个数',
  cost_ms       INT           NOT NULL DEFAULT 0 COMMENT '耗时毫秒',
  blur_size     INT           NULL COMMENT '当时模糊核',
  thresh        DOUBLE        NULL COMMENT '当时阈值',
  min_area      DOUBLE        NULL COMMENT '当时最小面积',
  max_area      DOUBLE        NULL COMMENT '当时最大面积',
  created_at    DATETIME      NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '入库时间'
) COMMENT='工业检测结果记录';
