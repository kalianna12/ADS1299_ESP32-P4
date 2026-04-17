# SSVEP App 快速参考指南

## API 快速使用

### 基本使用
```cpp
#include "ssvep/SSVEPApp.hpp"

// 应用自动启动时会：
// 1. 初始化灰度查表
// 2. 创建四个闪烁方框
// 3. 启动FreeRTOS更新任务
```

### 反馈测试示例
```cpp
// 获取应用实例（需要从Phone管理器获取）
SSVEPApp* ssvep = ...;

// 模拟检测到10Hz脑电波
ssvep->setFeedback(SSVEP_FREQ_10HZ);
// 右上角方框边框将变为绿色并增加宽度
// 500ms后自动消失

// 手动清除反馈
ssvep->clearFeedback();
```

## 频率配置

| 频率(Hz) | 周期(ms) | 位置   | 内部枚举值 |
|---------|---------|--------|-----------|
| 8       | 125     | 左上   | SSVEP_FREQ_8HZ (0) |
| 10      | 100     | 右上   | SSVEP_FREQ_10HZ (1) |
| 12      | 83      | 左下   | SSVEP_FREQ_12HZ (2) |
| 16      | 62.5    | 右下   | SSVEP_FREQ_16HZ (3) |

## 性能参数

| 参数 | 值 |
|-----|-----|
| 更新周期 | 10ms |
| 灰度分辨率 | 8位 (0-255) |
| 时间精度 | 微秒级 |
| 灰度表大小 | 128采样点 |
| 内存占用 | <1KB (灰度表) |
| CPU占用 | <2% |

## 关键常数

```cpp
// 在SSVEPApp.hpp中定义
UPDATE_PERIOD_MS = 10              // 更新周期
FEEDBACK_DURATION_MS = 500         // 反馈持续时间
GRAYSCALE_TABLE_SIZE = 128         // 灰度表采样点数

// 频率定义（在SSVEPApp.cpp中）
FREQ_HZ[] = {8, 10, 12, 16}        // 频率数组
FREQ_PERIOD_MS[] = {125, 100, 83, 62}  // 周期数组
```

## 生命周期

```
应用启动 -> run()
    ├─ init() [初始化灰度表]
    ├─ 创建UI[四个方框 + 回显区]
    └─ 启动更新任务

运行中:
    ├─ pause() -> 停止更新
    ├─ resume() -> 继续更新
    └─ setFeedback() -> 显示反馈

应用关闭 -> close()
    ├─ 停止更新任务
    └─ 清理资源
```

## 灰度值计算

### 正弦波生成
```
gray_value = 128 + 127 * sin(2π * index / 128)
```

### 实时查询（支持插值）
```cpp
uint8_t gray = getGrayscaleValue(SSVEP_FREQ_10HZ, current_time_ms);
```

## 调试建议

### 启用日志
所有关键操作都有ESP_LOGI输出，标签为"SSVEPApp"

### 观察输出
```
I (xxx) SSVEPApp: Initializing grayscale tables...
I (xxx) SSVEPApp: Grayscale table for 8 Hz initialized (period: 125 ms)
I (xxx) SSVEPApp: Creating rectangles at four corners...
I (xxx) SSVEPApp: Update task started
I (xxx) SSVEPApp: Feedback detected: 10 Hz
```

### 性能监测
- 观察任务状态：`vTaskList()`
- 监控内存：`heap_caps_print_heap_info()`
- 检查帧率：可通过计算颜色变化次数

## 常见问题

### Q: 闪烁频率不准确？
A: 使用微秒级时间戳，精度应该±1%以内。如需更高精度，增加GRAYSCALE_TABLE_SIZE。

### Q: 反馈绿色边框不显示？
A: 确保调用了setFeedback()，且检查反馈是否已超时（500ms）。

### Q: 画面撕裂？
A: LVGL的缓冲机制已解决。如仍出现，检查屏幕刷新率设置。

### Q: CPU占用过高？
A: 正常情况<2%。如过高，检查是否有其他应用在运行。

## 集成到BCI系统

### 最小集成示例
```cpp
// 您的BCI检测代码
if (detect_frequency(signal, SSVEP_FREQ_10HZ)) {
    ssvep_app->setFeedback(SSVEP_FREQ_10HZ);
}
```

### 推荐集成步骤
1. 从Phone管理器获取SSVEPApp实例
2. 实现频率检测算法（FFT/Welch等）
3. 在检测回调中调用setFeedback()
4. 实现命令转换逻辑

---

**最后更新**: 2026年4月17日
**适用版本**: SSVEP App v1.0
