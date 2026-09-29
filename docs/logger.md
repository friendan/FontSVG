# 日志模块（Logger）

## 位置

- `src/FontSVG/Logger.h`
- `src/FontSVG/Logger.cpp`

## 行为

- 默认关闭；开启后写入 `{exe}\logs\YYYYMMDD.log`（UTF-8）
- 级别：`Verbose`（高频）/ `Debug` / `Info` / `Warn` / `Error`
- 宏：`LOG_VERBOSEF` / `LOG_DEBUGF` / `LOG_INFOF` / `LOG_WARNF` / `LOG_ERRORF`

## 如何打开

```cpp
Logger::Instance().SetDebugEnabled(true);    // 低频诊断
Logger::Instance().SetVerboseEnabled(true);  // 高频（Refresh 等）
Logger::Instance().SetEnabled(true);         // 仅 Info+ 写文件时
```

或在 `Logger.h` 把 `FONTSVG_LOG_DEBUG_DEFAULT` / `FONTSVG_LOG_VERBOSE_DEFAULT` 改为 `1` 后重编。

## 约定

业务代码保留日志调用点，用开关控制输出，避免删了下次再加。
