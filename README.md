# CMDBrowser v2.0

一个基于 API 驱动的命令行网页搜索工具，纯 C11 编写，支持 Linux。通过调用 Tavily、Bing Search、Google Custom Search 三大搜索 API，在终端中返回结构化的搜索结果。

---

## 特性

- **多引擎 API 搜索**：支持 Tavily、Bing Search API、Google Custom Search，按优先级自动调度
- **智能配额管理**：本地持久化记录 API 调用次数，支持日/月限额自动清零，额度耗尽自动降级到下一引擎
- **多种输出格式**：rich（默认）、plain、json、csv、markdown、html
- **交互式 Shell**：REPL 模式支持命令补全、历史记录、书签管理
- **跨平台**：基于 libcurl 和 POSIX API，原生支持 Linux（Windows 支持已移除）
- **零外部依赖**：不依赖 Python、Node.js 或浏览器内核，单二进制文件即可运行

---

## 安装

### 依赖

- GCC 或 Clang（支持 C11）
- libcurl
- pthread

```bash
# Debian/Ubuntu
sudo apt-get install build-essential libcurl4-openssl-dev

# Fedora/RHEL
sudo dnf install gcc make libcurl-devel

# Arch
sudo pacman -S base-devel curl
```

### 编译

```bash
cd /workspace
make
```

编译产物为 `./cmdbrowser`。

---

## 配置 API 密钥

项目通过环境变量读取 API 密钥：

```bash
export TAVILY_API_KEY="your_tavily_key"
export BING_API_KEY="your_bing_key"
export GOOGLE_API_KEY="your_google_key"
export GOOGLE_SEARCH_CX="your_google_cx"
```

建议添加到 `~/.bashrc` 或 `~/.zshrc`。

| 引擎 | 环境变量 | 限额（默认） | 说明 |
|:---|:---|:---|:---|
| Tavily | `TAVILY_API_KEY` | 月限 1000 次 | 主引擎，优先级最高 |
| Bing Search | `BING_API_KEY` | 月限 1000 次 | 次选引擎 |
| Google Custom Search | `GOOGLE_API_KEY` + `GOOGLE_SEARCH_CX` | 日限 100 次 | 备用引擎 |

---

## 使用

### 交互模式

```bash
./cmdbrowser
```

启动后进入 REPL，提示符为 `>>>`，支持以下命令：

| 命令 | 说明 |
|:---|:---|
| `<query>` | 直接输入搜索词，使用所有可用引擎 |
| `/search <query>` | 同上 |
| `/engine <name> <query>` | 指定单个引擎搜索 |
| `/tavily <query>` | 使用 Tavily 搜索 |
| `/bing <query>` | 使用 Bing 搜索 |
| `/google <query>` | 使用 Google 搜索 |
| `/engines` | 列出所有引擎状态 |
| `/engines usage` | 查看 API 调用配额统计 |
| `/format <rich/plain/json/csv/md/html>` | 切换输出格式 |
| `/history` | 查看搜索历史 |
| `/bookmark add <id>` | 收藏结果 |
| `/quit` | 退出 |

### 命令行模式

```bash
# 单次搜索
./cmdbrowser "linux kernel"

# 指定引擎
./cmdbrowser "/engine tavily linux kernel"

# JSON 输出
./cmdbrowser "/format json linux kernel"
```

---

## 配额管理

用量数据持久化存储在 `~/.cmdbrowser/usage.dat`，自动管理：

- **Tavily / Bing**：每月 1 日清零月计数
- **Google**：每日 0 点清零日计数

搜索前自动检查配额，若当前引擎额度耗尽，自动跳过并尝试下一优先级的引擎。

---

## 项目结构

| 文件 | 说明 |
|:---|:---|
| `main.c` | 程序入口、命令解析、搜索调度 |
| `engine.c/h` | 搜索引擎注册、URL 构造、优先级管理 |
| `http.c/h` | libcurl HTTP 请求、API Key 认证 |
| `parser.c/h` | 响应解析路由 |
| `parser_api.c` | Tavily/Bing/Google JSON 响应解析 |
| `usage_tracker.c/h` | 本地用量统计、配额检查、持久化 |
| `ui.c/h` | 终端 UI 渲染、颜色、分页 |
| `config.c/h` | INI 配置文件管理 |
| `history.c/h` | 搜索历史记录 |
| `bookmarks.c/h` | 书签管理 |
| `cache.c/h` | 搜索结果缓存 |
| `utils.c/h` | 字符串、编码、文件工具函数 |
| `types.h` | 核心数据结构、常量、跨平台类型 |

---

## 技术细节

- **HTTP 层**：libcurl 替代 WinHTTP，支持 HTTPS、代理、自定义 Header
- **JSON 解析**：纯 C 标准库字符串操作（`strstr`、`strchr`），零第三方 JSON 库依赖
- **并发**：基于 pthread 的互斥锁保护全局状态
- **内存管理**：显式生命周期管理，无垃圾回收

---

## 许可

MIT License
