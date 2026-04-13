# 宠物居家场景可视化系统

基于 C++ Qt 和 YOLOv8 的宠物居家场景 AI 可视化监控系统。支持本地视频加载、实时宠物检测与行为识别、运动轨迹可视化、行为统计图表和异常告警。

## 功能模块

| 模块 | 说明 |
|------|------|
| 视频播放 | 支持 .mp4/.avi/.mkv/.mov/.flv 格式，定时器逐帧播放 |
| 实时摄像头 | 「打开摄像头」使用本机摄像头（OpenCV VideoCapture），与本地视频共用同一套 YOLO 推理与界面 |
| 宠物检测 | YOLOv8 ONNX 模型实时检测猫/狗，绘制检测框和标签 |
| 行为识别 | 基于运动特征规则分类：睡眠、静坐、走动、奔跑、进食、拆家 |
| 运动轨迹 | 跨帧 IoU 跟踪，实时绘制宠物活动轨迹 |
| 行为统计 | QtCharts 饼图展示行为分布，折线图展示活动量趋势 |
| 异常告警 | 长时间消失/拆家行为触发界面告警横幅 |
| 时段汇总 | **本地视频**：播放时为实时检测；点击 **「整段统计(时段表)」** 后台扫描全片生成「大段连续出现」数据；可导出 **UTF-8 CSV**。**摄像头**：在「开始分析」～「结束分析」区间内统计 |

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

## 使用摄像头实时推理

1. 点击 **「打开摄像头」**，在对话框中输入设备编号（一般为 **0**；外接摄像头可试 **1**、**2**）。
2. 打开成功后请点 **「播放」** 开始预览；点 **「暂停」** 停止取流。
3. 程序会预热若干帧并优先使用 Windows **MSMF** 后端，减轻首帧黑屏；推理链路与本地视频相同。
4. 实时模式下 **进度条不可用**（无总帧数）。
4. **Windows**：若打不开，请在「设置 → 隐私 → 相机」中允许桌面应用访问摄像头，并关闭占用摄像头的其它程序（如 Teams、浏览器）。

## 宠物出现时段导出（Excel）

### 本地视频

1. 使用 **「打开视频」** 加载本地文件；**播放**时即可看到**实时检测**（轨迹、图表随播放更新）。**实时推理步长**：视频仍按文件 FPS 全速显示，仅每 N 帧尝试提交一次推理；若推理慢于播放，会自动跳过提交以免卡顿，检测框可能略滞后。
2. 设置 **「最短连续出现(秒)」**（默认 0.5）：只有连续检测到宠物的时长 **≥ 该值** 的区段才会写入汇总（过滤短暂误检）。
3. 点击 **「整段统计(时段表)」**：后台对**整段视频**扫描一遍，**仅用于生成时段表数据**（与实时预览互不替代）；扫描期间会暂停向播放线程提交推理帧，完成后可导出。
   - **加速选项**：**步长** = 每 N 帧做一次 YOLO，中间帧用 OpenCV `grab()` 快速跳过解码，并沿用本段检测结果（默认 **3**）。推理输入边长与 YOLO ONNX 导出一致（一般为 **640**），不可随意改小，否则 OpenCV DNN 会在 reshape 层报错。
4. 点击 **「导出宠物出现时段(CSV)」**，保存为 `.csv` 文件。
5. 用 **Excel** 打开 CSV 即可；文件为 **UTF-8 带 BOM**，中文列名正常显示。

### 摄像头

1. 点击 **「开始分析」** 与 **「结束分析」** 采集区间后，再导出 CSV（与本地视频流程不同）。

导出列说明：`序号、开始时间(秒)、结束时间(秒)、持续时长(秒)、开始帧、结束帧、连续帧数`。

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
