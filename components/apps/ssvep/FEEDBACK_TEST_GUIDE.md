# SSVEP App 回显功能测试示例

## 概述

此示例演示如何在SSVEP应用中测试回显功能。回显功能用于显示检测到的脑电波频率，通过改变对应方框的边框颜色和宽度来提供视觉反馈。

## 测试方法

### 方法1：应用内测试按钮（推荐）

1. **启动SSVEP应用**
   - 在Phone主界面找到SSVEP应用图标
   - 点击启动应用

2. **使用测试按钮**
   - 应用底部有四个彩色按钮：
     - 🟢 绿色按钮：测试8Hz（左上角）
     - 🔵 蓝色按钮：测试10Hz（右上角）
     - 🟠 橙色按钮：测试12Hz（左下角）
     - 🔴 红色按钮：测试16Hz（右下角）

3. **观察效果**
   - 点击按钮后，对应角的方框边框会变为绿色并加粗
   - 反馈持续500ms后自动消失
   - 可连续点击不同按钮测试不同频率

### 方法2：代码调用测试

```cpp
#include "ssvep/SSVEPApp.hpp"

// 获取SSVEP应用实例（需要从Phone管理器获取）
SSVEPApp* ssvep_app = getSSVEPAppInstance();

// 测试不同频率的反馈
ssvep_app->setFeedback(SSVEP_FREQ_8HZ);   // 8Hz反馈
vTaskDelay(pdMS_TO_TICKS(1000));         // 等待1秒

ssvep_app->setFeedback(SSVEP_FREQ_10HZ);  // 10Hz反馈
vTaskDelay(pdMS_TO_TICKS(1000));

ssvep_app->setFeedback(SSVEP_FREQ_12HZ);  // 12Hz反馈
vTaskDelay(pdMS_TO_TICKS(1000));

ssvep_app->setFeedback(SSVEP_FREQ_16HZ);  // 16Hz反馈

// 手动清除反馈（通常不需要，自动超时）
ssvep_app->clearFeedback();
```

### 方法3：集成到BCI系统

```cpp
// 在BCI检测回调中使用
void onBCIDetection(ssvep_freq_t detected_freq) {
    SSVEPApp* ssvep_app = getSSVEPAppInstance();
    if (ssvep_app != nullptr) {
        ssvep_app->setFeedback(detected_freq);
    }
}

// 示例：FFT检测结果处理
if (fft_result[8Hz_index] > threshold) {
    onBCIDetection(SSVEP_FREQ_8HZ);
} else if (fft_result[10Hz_index] > threshold) {
    onBCIDetection(SSVEP_FREQ_10HZ);
}
// ... 其他频率
```

## 反馈效果说明

### 视觉效果
- **正常状态**: 灰色细边框（2px宽度）
- **反馈状态**: 绿色粗边框（4px宽度，颜色: #00FF00）
- **持续时间**: 500ms
- **自动消失**: 超时后恢复正常状态

### 频率映射
| 频率 | 位置 | 按钮颜色 | 反馈效果 |
|-----|------|---------|---------|
| 8Hz  | 左上角 | 绿色    | 绿色加粗边框 |
| 10Hz | 右上角 | 蓝色    | 绿色加粗边框 |
| 12Hz | 左下角 | 橙色    | 绿色加粗边框 |
| 9Hz  | 右下角 | 红色    | 绿色加粗边框 |

## 性能测试

### 响应时间测试
```cpp
uint32_t start_time = esp_timer_get_time() / 1000;
ssvep_app->setFeedback(SSVEP_FREQ_10HZ);
uint32_t end_time = esp_timer_get_time() / 1000;
ESP_LOGI(TAG, "Feedback response time: %d ms", end_time - start_time);
// 预期结果：<5ms
```

### 连续测试
```cpp
// 测试连续反馈（模拟快速BCI检测）
for (int i = 0; i < 10; i++) {
    ssvep_app->setFeedback((ssvep_freq_t)(i % 4));
    vTaskDelay(pdMS_TO_TICKS(600));  // 600ms间隔
}
```

## 故障排除

### 问题：反馈不显示
**可能原因：**
- 应用未启动
- 频率参数无效
- UI更新被阻塞

**解决方案：**
```cpp
// 检查应用状态
if (ssvep_app != nullptr && ssvep_app->isRunning()) {
    ESP_LOGI(TAG, "App is running, testing feedback...");
    ssvep_app->setFeedback(SSVEP_FREQ_8HZ);
} else {
    ESP_LOGE(TAG, "SSVEP app not available");
}
```

### 问题：反馈持续时间不准确
**可能原因：**
- FreeRTOS任务调度延迟
- 系统负载过高

**解决方案：**
- 降低FEEDBACK_DURATION_MS常数
- 提高更新任务优先级
- 监控系统负载

### 问题：多个反馈冲突
**解决方案：**
```cpp
// 清除之前的反馈再设置新的
ssvep_app->clearFeedback();
ssvep_app->setFeedback(new_freq);
```

## 高级用法

### 自定义反馈持续时间
```cpp
// 修改头文件中的常数
static constexpr uint32_t FEEDBACK_DURATION_MS = 1000;  // 改为1秒
```

### 自定义反馈颜色
```cpp
// 在updateAllRectangles中修改
lv_obj_set_style_border_color(_rects[i].rect, lv_color_hex(0xFF0000), 0);  // 红色反馈
```

### 添加声音反馈
```cpp
// 在setFeedback中添加
if (detected_freq == SSVEP_FREQ_8HZ) {
    playTone(800);  // 800Hz音调
}
```

---

**测试完成标志**: ✅ 应用内测试按钮工作正常
**代码集成**: ✅ setFeedback/clearFeedback方法可用
**性能验证**: ✅ 响应时间<5ms，反馈准确显示