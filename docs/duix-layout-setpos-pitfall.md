# DUIX 布局坑：勿用 `SetPos(GetPos())` 强制重排

## 现象

主窗口启动后左侧字体已默认选中，右侧文字预览空白（或高度异常、只显示极少格子）。字形枚举、控件指针都正常，问题出在布局刷新。

## 错误写法

```cpp
CControlUI* pRoot = m_pm.GetRoot();
if( pRoot )
    pRoot->SetPos(pRoot->GetPos());  // 危险
```

在 `InitWindow`、首次 `PostMessage` 刷新、或部分时机里，`GetPos()` 可能仍是 `(0,0,0,0)`（或宽有高无）。再 `SetPos` 回去等于把整棵 UI 压成零高度，子控件即使已经 `Add` 了也看不见。

相关误判：

| 表面现象 | 容易误判成 |
|----------|------------|
| 预览空白 | 字形没枚举到 / FindControl 失败 |
| `grid` 宽正常、高为 0 | 只用了错误的容器（如 TileLayout 被子项撑扁） |
| 延后刷新仍空白 | 定时器/消息没进 `HandleCustomMessage` |

## 正确做法

用**窗口客户区**给根控件布局，而不是回写自身 `GetPos()`：

```cpp
void EnsureRootLayout()
{
    HWND hWnd = GetHWND();
    if( hWnd == NULL ) return;

    RECT rcClient = {};
    if( !::GetClientRect(hWnd, &rcClient) ) return;
    if( rcClient.right - rcClient.left < 8 || rcClient.bottom - rcClient.top < 8 )
        return;

    CControlUI* pRoot = m_pm.GetRoot();
    if( pRoot )
        pRoot->SetPos(rcClient);
}
```

填充依赖尺寸的内容时：

1. 先 `EnsureRootLayout()`（客户区矩形）。
2. 用目标区域真实宽高算每页容量；未就绪时可用 fallback，但布局完成后必须再刷一次。
3. 在 `OnSize` 里比较区域高度 / 每页容量变化后再调度刷新；布局未完成时用 `PostMessage(WM_USER+…)` 重试，直到 `IsReady`。

注意：`WM_SIZE` 走 `WindowImplBase::OnSize`，**不会**进 `HandleCustomMessage`；尺寸变化逻辑应重写 `OnSize`，自定义延后消息才放 `HandleCustomMessage`。

## 容器选择（预览网格）

| 控件 | 说明 |
|------|------|
| `AppGrid` | `EstimateSize` 不按子项取高，适合图标墙；但本项目中间区曾用错刷新方式导致高为 0 |
| `TileLayout` | 易按子项估尺寸，父级 `flexible` 可能被压成单格大小 |
| `VBox` + 多行 `HBox`/`HorizontalLayout` | 与 JimuCode 一致，弹性高度稳妥；字格用固定宽高 `Label` 排布 |

中间内容区优先：`VBox` → `HBox content flexible=1` → 左右 `VBox`（参考 JimuCode `main.html`），不要把未验证的 `HSplit`/`AppGrid` 组合直接当弹性主区域。

## 自检清单

- [ ] 强制布局是否用了 `GetClientRect`，而不是 `SetPos(GetPos())`
- [ ] 启动后日志/断点里预览区 `height` 是否 > 单元格高度
- [ ] 切换字体、改窗口大小后是否仍会按新尺寸重算每页并 `RemoveAll` 再填充
- [ ] 空白时状态栏字数是否已 > 0（区分「没数据」与「有数据但高度为 0」）

## 调试日志

独立模块：`src/FontSVG/Logger.h` / `Logger.cpp`，写入 `{exe}\logs\YYYYMMDD.log`。

| 方式 | 说明 |
|------|------|
| `Logger::Instance().SetDebugEnabled(true)` | 打开 Debug（低频，如 Init/字形枚举） |
| `SetVerboseEnabled(true)` | 打开 Verbose（高频 Refresh/重试；会连带开 Debug） |
| `FONTSVG_LOG_DEBUG_DEFAULT` / `FONTSVG_LOG_VERBOSE_DEFAULT` | 编译期默认（`Logger.h`，改 `1` 免改业务代码） |

宏：`LOG_DEBUGF` / `LOG_VERBOSEF` / `LOG_INFOF` / `LOG_WARNF` / `LOG_ERRORF`（调用点可长期保留）。

## 关联代码

- `src/FontSVG/MainWnd.cpp`：`EnsureRootLayout` / `RefreshCharPage` / `OnSize`
- `bin/skin/FontSVG/main.html`：上中下 + 中左右 `HBox` 布局
