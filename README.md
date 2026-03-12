# 宠物居家场景可视化系统

基于 C++ Qt 和 YOLOv8 的宠物居家场景 AI 可视化监控系统。支持本地视频加载、实时宠物检测与行为识别、运动轨迹可视化、行为统计图表和异常告警。

## 功能模块

| 模块 | 说明 |
|------|------|
| 视频播放 | 支持 .mp4/.avi/.mkv/.mov/.flv 格式，定时器逐帧播放 |
| 宠物检测 | YOLOv8 ONNX 模型实时检测猫/狗，绘制检测框和标签 |
| 行为识别 | 基于运动特征规则分类：睡眠、静坐、走动、奔跑、进食、拆家 |
| 运动轨迹 | 跨帧 IoU 跟踪，实时绘制宠物活动轨迹 |
| 行为统计 | QtCharts 饼图展示行为分布，折线图展示活动量趋势 |
| 异常告警 | 长时间消失/拆家行为触发界面告警横幅 |

## 环境要求

- **编译器**: 支持 C++17 的编译器 (MSVC 2019+, GCC 8+, Clang 10+)
- **Qt**: 5.15+ 或 6.x (需包含 Widgets 和 Charts 模块)
- **OpenCV**: 4.x (需包含 DNN 模块)
- **CMake**: 3.16+

## 依赖安装

### Windows (推荐使用 vcpkg)

```bash
# 安装 OpenCV
vcpkg install opencv4[dnn]:x64-windows

# Qt 通过 Qt Online Installer 安装，确保勾选 Qt Charts
```

### Ubuntu / Debian

```bash
sudo apt install qtbase5-dev libqt5charts5-dev
sudo apt install libopencv-dev
```

## 准备 YOLOv8 ONNX 模型

系统使用 YOLOv8 的 ONNX 格式模型进行推理。需要用 Python 一次性导出：

```bash
pip install ultralytics
yolo export model=yolov8n.pt format=onnx
```

将导出的 `yolov8n.onnx` 文件放入项目的 `models/` 目录，或放入可执行文件同级的 `models/` 目录。

## 编译与运行

```bash
# 创建构建目录
mkdir build && cd build

# 配置 (Windows 示例, 指定 Qt 和 OpenCV 路径)
cmake .. -DCMAKE_PREFIX_PATH="C:/Qt/5.15.2/msvc2019_64;C:/vcpkg/installed/x64-windows"

# 编译
cmake --build . --config Release

# 运行前确保 models/yolov8n.onnx 存在
# Windows: 将 yolov8n.onnx 放到 Release/models/ 下
./PetMonitorSystem
```

## 项目结构

```
PetMonitorSystem/
├── CMakeLists.txt              # CMake 构建配置
├── src/
│   ├── main.cpp                # 程序入口
│   ├── MainWindow.h/.cpp       # 主窗口布局与模块集成
│   ├── VideoPlayer.h/.cpp      # OpenCV 视频加载 + QTimer 逐帧播放
│   ├── VideoWidget.h/.cpp      # 视频渲染 + 检测框/标签叠加
│   ├── YoloDetector.h/.cpp     # YOLOv8 ONNX 推理 (cv::dnn)
│   ├── PetTracker.h/.cpp       # IoU 跨帧目标跟踪
│   ├── BehaviorAnalyzer.h/.cpp # 基于运动特征的行为分类
│   ├── TrajectoryWidget.h/.cpp # QPainter 轨迹可视化
│   ├── StatisticsWidget.h/.cpp # QtCharts 饼图/折线图
│   ├── AlertManager.h/.cpp     # 异常检测与告警
│   └── DetectionResult.h       # 核心数据结构定义
├── models/
│   └── yolov8n.onnx            # YOLOv8 模型 (需自行导出)
└── resources/
    └── resources.qrc           # Qt 资源文件
```

## 行为分类规则

| 行为 | 判定条件 |
|------|----------|
| 睡眠 | 平均速度 < 2px/帧 + bbox 宽高比 > 1.2 (横躺) |
| 静坐 | 平均速度 < 2px/帧 + bbox 高宽比 >= 1.0 (竖直) |
| 走动 | 平均速度 2~15 px/帧 |
| 奔跑 | 平均速度 15~30 px/帧 |
| 进食 | 低速 + 位于预设食盆区域内 |
| 拆家 | 平均速度 > 30 px/帧 (剧烈运动) |

## 告警规则

- 连续 150 帧 (~5秒@30fps) 未检测到宠物 → "宠物离开视野" 警告
- 连续 60 帧 (~2秒@30fps) 检测到拆家行为 → "疑似拆家" 紧急告警
