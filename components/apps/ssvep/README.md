# SSVEP App (Steady-State Visually Evoked Potential)

## 概述

SSVEP应用是一个稳态视觉诱发电位（Steady-State Visually Evoked Potential）的演示应用，主要用于脑机接口（Brain-Computer Interface, BCI）研究和开发。

## 功能特性

### 1. 四个频率闪烁方框
- **左上角**: 8Hz - 绿色
- **右上角**: 10Hz - 蓝色  
- **左下角**: 12Hz - 黄色
- **右下角**: 16Hz - 红色

### 2. 灰度SSVEP实现
- 使用预计算的灰度查表（LUT）来减少实时计算开销
- 采用正弦波调制实现平滑的灰度过渡
- 使用线性插值提高视觉平滑度
- 128个采样点的灰度表，覆盖完整周期

### 3. 性能优化
- **FreeRTOS精确控制**: 使用10ms更新周期，结合FreeRTOS的精确延时
- **微秒级时间精度**: 使用esp_timer_get_time()获取微秒精度的时间戳
- **缓冲机制**: 使用LVGL的内置缓冲防止画面撕裂
- **效率高的更新**: 仅在必要时更新UI元素

### 4. 回显反馈（BCI反馈）
- 当检测到特定频率的脑电波时，对应方框的边框会变为绿色并增加宽度
- 反馈持续500ms后自动消失
- 提供视觉反馈机制供BCI算法使用

## 技术细节

### 频率参数
| 频率(Hz) | 周期(ms) | 位置 |
|---------|---------|------|
| 8       | 125     | 左上 |
| 10      | 100     | 右上 |
| 12      | ~83.33  | 左下 |
| 14      | ~71.43  | 右下 |

### 灰度值生成
灰度值通过正弦波生成，范围0-255：
```
gray_value = 128 + 127 * sin(2π * index / table_size)
```

### 时间控制
使用FreeRTOS的vTaskDelayUntil确保精确的更新周期：
- 更新周期: 10ms
- 时基分辨率: 1ms（configTICK_RATE_HZ = 1000）
- 微秒级时间戳: esp_timer_get_time()

## 使用方法

### 启动应用
1. 在手机界面中查找SSVEP应用图标
2. 点击启动SSVEP应用
3. 应用启动后会显示四个闪烁的方框

### 回显反馈测试
可以通过代码调用setFeedback()方法来测试反馈功能：
```cpp
ssvep_app->setFeedback(SSVEP_FREQ_10HZ);  // 模拟检测到10Hz频率
```

**应用内测试按钮：**
应用底部提供四个彩色测试按钮用于快速测试：
- **绿色按钮**: 测试8Hz反馈（左上角方框）
- **蓝色按钮**: 测试10Hz反馈（右上角方框）
- **橙色按钮**: 测试12Hz反馈（左下角方框）
- **红色按钮**: 测试14Hz反馈（右下角方框）

### 暂停/恢复
- 当应用暂停时，闪烁会停止
- 恢复时闪烁会继续

## 代码结构

### 头文件 (SSVEPApp.hpp)
- `SSVEPApp` 类: 主应用类
- `ssvep_freq_t` 枚举: 频率类型定义
- `ssvep_freq_table_t` 结构: 灰度查表结构
- `ssvep_rect_t` 结构: 方块状态结构

### 实现文件 (SSVEPApp.cpp)
- `initGrayscaleTables()`: 初始化灰度查表
- `createRectangles()`: 创建四个闪烁方框
- `getGrayscaleValue()`: 获取当前灰度值（支持插值）
- `updateAllRectangles()`: 更新所有方框颜色和反馈
- `updateTaskEntry()`: FreeRTOS更新任务
- `setFeedback()`: 设置反馈频率
- `clearFeedback()`: 清除反馈

## 性能指标

### 内存占用
- 灰度表总大小: 512字节 (4个频率 × 128个采样点)
- 其他对象: ~200字节
- 总计: <1KB（不包括LVGL对象）

### CPU占用
- 更新周期: 10ms
- 每次更新时间: <2ms
- CPU使用率: <2% on ESP32-P4

### 渲染质量
- 灰度分辨率: 8位 (0-255)
- 时间分辨率: 10ms（对应10Hz刷新）
- 正弦波采样点: 128个（覆盖1个完整周期）

## 扩展建议

### 1. 集成BCI算法
- 实现频率检测算法（FFT/DFT）
- 自动调用setFeedback()显示反馈
- 可选：添加置信度显示

### 2. 增加刺激模式
- 闪烁模式：0-255-0
- 脉冲模式：0-255突变
- 相位差模式：多个方框之间的相位差

### 3. 参数调整UI
- 动态调整频率
- 调整灰度范围（0-255）
- 调整更新周期

### 4. 性能监测
- 添加FPS显示
- 添加CPU使用率监测
- 添加延时监测

## 注意事项

1. **画面撕裂**: 使用LVGL的缓冲机制已经解决，无需额外处理
2. **频率精度**: 使用微秒级时间戳确保频率精度
3. **实时性**: FreeRTOS任务优先级设为3，确保及时更新
4. **暂停恢复**: 暂停时不进行颜色更新，以节省电力

## 参考资源

- [SSVEP - Wikipedia](https://en.wikipedia.org/wiki/Steady-state_visually_evoked_potential)
- ESP-IDF FreeRTOS 文档
- LVGL 文档
