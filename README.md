# mousekey — 键盘控鼠标(Windows 鼠标键替代)

用数字键盘(或任意自定义键位)控制鼠标指针的小工具,替代 Windows
自带的"鼠标键"。核心是手感:可调加速曲线、亚像素平滑、斜向速度归一。

## 功能

- **运动引擎**:按住期间 250 Hz tick;速度按
  `v(t) = vStart + (vMax − vStart) · min(t/rampMs, 1)^gamma` 生长;
  位移内部浮点累加,每 tick 只发出整数部分(慢速不跳格);
  斜向分量 = v·diagScale(默认 0.7071,各向同性)。
- **键位**(默认沿用 Windows 鼠标键数字键盘,可在面板里任意改):
  - `8/2/4/6` 上/下/左/右,`7/9/1/3` 斜向
  - `5` **单击**——点哪个键由当前模式决定
  - `/` 把 5 切为**左键**,`*` 把 5 切为**右键**(与 Windows 鼠标键同一机制)
  - `.` **右键单击**(直接触发,不受模式影响)
  - `0` 按住(拖拽,跟随当前模式),松开自动抬起
  - `+`/`-` 滚轮上/下
  - 按扫描码识别,不受 NumLock 影响;面板顶部实时显示"5 → 左键/右键"
- **托盘**:图标随启用状态切换;左键开面板,右键菜单(启用/设置/退出)。
- **启动时自动关闭 NumLock**(默认开启,可在面板"杂项"里取消),避免
  数字键盘在 NumLock 开启时对外发出数字。
- **配置**:`mousekey.ini` 存在 exe 旁,可直接手改。
- **调试**:面板勾选 csvLog 后,下次启动把每 tick 的
  `t, dt, dir, v, dx, dy, wheel` 写入 `mousekey_ticks.csv`。

## 构建

要求:Visual Studio(MSVC)+ CMake。

```
vcvars64.bat
cmake -S . -B build-release -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --target mousekey
```

产物:`build-release/mousekey.exe`(约 0.56 MB,无 DLL 依赖)。

## 测试

```
cmake --build build-release --target mk_tests
build-release\mk_tests.exe
```

38 个用例 / 635 断言,覆盖加速曲线、慢速精度、斜向归一、滚轮累加、
键位路由、INI 读写与按键名称表(验收标准的客观部分)。
`injecthold.exe`(tools/)可注入按键做端到端冒烟。

## 目录

| 路径 | 内容 |
|---|---|
| `src/` | 主程序(config/engine/hook/inject/tray/ui/app/main) |
| `tests/` | doctest 单元测试 |
| `res/` | 托盘图标、资源、DPI 清单 |
| `tools/` | 图标生成脚本、按键注入工具 |
| `docs/DESIGN.md` | 设计方案与实测修正记录 |

## 已知边界

- 目标程序以管理员运行时,本工具也需以管理员运行(UIPI 限制)。
- UAC 安全桌面、全屏独占游戏内钩子不生效(系统行为)。
- 一个键只能绑一个动作;冲突时面板会提示。
