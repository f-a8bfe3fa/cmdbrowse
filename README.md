# 📘 CMDBrowser v2.0 – Comprehensive Technical Introduction

## 1. Executive Summary
**CMDBrowser v2.0** is a high-performance, multi-engine command-line web search utility engineered in pure C11. Designed for developers, system administrators, researchers, and power users, it transforms the terminal into a fully-featured search interface capable of querying multiple search engines simultaneously, aggregating results, and outputting them in highly configurable formats. The application operates with minimal overhead, requiring no graphical dependencies, and leverages native Windows console APIs alongside a robust WinHTTP networking stack. Its architecture emphasizes modularity, persistent state management, and developer-friendly extensibility, making it suitable for both interactive research and automated scripting workflows.

---

## 2. System Architecture & Core Components
CMDBrowser follows a centralized state-machine architecture driven by a single `BrowserContext` struct that orchestrates all subsystems. The codebase is deliberately modular, delegating specialized responsibilities to external headers:

| Subsystem | Header Module | Responsibility |
|:---|:---|:---|
| **State Management** | `types.h` | `BrowserContext` lifecycle, configuration, UI mode, engine registry |
| **Utilities** | `utils.h` | String trimming, duplication, case conversion, whitespace handling |
| **Networking** | `http.h` | WinHTTP initialization, request building, header injection, proxy routing, response execution |
| **Engine Registry** | `engine.h` | Search engine abstraction, URL templating, rate-limit definitions, tier management |
| **Response Parsing** | `parser.h` | HTML/JSON extraction, result deduplication, pagination metadata parsing |
| **Configuration** | `config.h` | INI-based persistent settings (`config.ini`), section/key-value CRUD |
| **History** | `history.h` | Query logging, success/failure tracking, export to CSV |
| **Bookmarks** | `bookmarks.h` | Persistent result storage, HTML import/export, tag-based filtering |
| **Caching** | `cache.h` | Query-result LRU cache, TTL management, disk/memory persistence |
| **Terminal UI** | `ui.h` | ANSI color rendering, rich formatting, pagination, header/footer prompts |

The core orchestration logic resides in `main.c`, which handles initialization, command dispatch, execution routing, and graceful teardown.

---

## 3. Key Features & Capabilities

### 🔍 Multi-Engine Aggregation
- Queries Google, Bing, DuckDuckGo, Yahoo, and user-defined custom engines.
- Merges results across engines with relevance-based sorting and automatic deduplication.
- Supports pagination and configurable result limits (`1–200` per request).

### ⚙️ Advanced Search Control
- **Rate Limit Handling:** Automatically backs off on `403/429` HTTP responses using engine-specific `sleep_ms` multipliers.
- **Safe Search:** Three-tier filtering (`OFF`, `MODERATE`, `STRICT`) enforced per-engine.
- **Proxy Support:** Full HTTP/HTTPS proxy routing configurable at runtime.
- **Custom Engines:** Add/remove search providers via URL templates and query parameter mapping.

### 💾 Persistent State Management
- **Configuration:** `config.ini` stores display preferences, network limits, output formats, and proxy settings.
- **History:** Tracks all queries, engine targets, result counts, and success status. Supports search, stats, and CSV export.
- **Bookmarks:** Save, tag, search, and export results. Supports HTML bookmark file import for cross-tool compatibility.
- **Cache:** Stores parsed responses to accelerate repeated queries. Includes automated cleanup and expiration logic.

### 🎨 Rich Terminal Output
- ANSI escape code rendering for syntax highlighting and structural formatting.
- Six output formats: `rich` (default), `plain`, `json`, `csv`, `markdown`, `html`.
- Toggleable UI features: color output, line numbers, console clearing, and footer prompts.

---

## 4. Command Reference & Interactive Shell

CMDBrowser operates as a REPL (Read-Eval-Print Loop) with a comprehensive slash-command syntax:

| Command Category | Examples | Description |
|:---|:---|:---|
| **Search Execution** | `/search <query>`, `/s <q>`, `? <q>` | Multi-engine search with default settings |
| | `/mega <query>`, `/all <q>` | Force query across ALL registered engines |
| | `/google`, `/bing`, `/ddg`, `/yahoo`, `/engine <name> <q>` | Single-engine targeted search |
| **Engine Management** | `/engines` | List all engines with status/tier info |
| | `/engines on/off <name>` | Enable or disable specific engines |
| | `/engines add <name> <url> <parser>` | Register custom search provider |
| | `/engines remove <name>` | Remove custom engine |
| **Data Management** | `/history [search\|clear\|stats\|export <file>]` | Query log operations |
| | `/bookmark [add\|remove\|list\|search\|import\|export]` | Persistent result curation |
| | `/cache [stats\|clear\|cleanup]` | Cache inspection and maintenance |
| **Display & Format** | `/format [plain\|json\|csv\|md\|html\|rich]` | Switch output rendering mode |
| | `/color [on\|off\|toggle]` | Enable/disable ANSI colors |
| | `/lines [on\|off\|toggle]` | Toggle line numbering |
| | `/max <n>` | Set result limit per page (`1–200`) |
| **Network & Security** | `/proxy <url>` | Set HTTP/HTTPS proxy |
| | `/safe [off\|mod\|strict]` | Configure safe search level |
| **System & Utility** | `/config get\|set\|list\|keys` | Runtime INI configuration management |
| | `/open <url>` | Launch URL in default system browser (`start ""`) |
| | `/quit`, `/exit`, `/q`, `/save`, `/help`, `/about`, `/clear` | Session control & diagnostics |

Any input not prefixed with `/`, `?`, or `.` is automatically treated as a search query and routed to `do_multi_search()`.

---

## 5. Technical Implementation Details

### 🧠 State & Memory Management
- Centralized `BrowserContext` prevents global variable pollution.
- Explicit lifecycle management: `browser_init()` allocates and configures; `main()` ensures `free()` is called for config, history, bookmarks, and HTTP subsystems before exit.
- Bounds-safe string operations via `snprintf`, `_dup` wrappers, and fixed-size stack buffers (`MAX_QUERY_LENGTH`, `MAX_PATH_LENGTH`).

### 🌐 HTTP & Networking Layer
- Leverages Windows HTTP Services (WinHTTP) for robust TLS, proxy auto-detection, and asynchronous readiness.
- Headers are dynamically injected (`User-Agent`, `Accept`, `Accept-Language`).
- Graceful degradation on network failures: returns structured error responses with status codes instead of crashing.

### 🔀 Result Processing Pipeline
1. **Cache Lookup:** Checks for valid cached response matching query + engine + page.
2. **Request Construction:** Engine-specific URL builder injects query, pagination, and safe-search parameters.
3. **Execution & Backoff:** `http_execute()` handles I/O. `403/429` triggers exponential backoff via `sleep_ms`.
4. **Parsing:** HTML/JSON stripped into `SearchResult` structs.
5. **Deduplication:** URL/title normalization removes cross-engine duplicates.
6. **Merging & Sorting:** Results ranked by relevance score, merged into unified response object.
7. **Storage:** Cached, logged to history, and formatted for UI output.

### 🖥️ Console & Encoding
- Forces UTF-8 console output (`SetConsoleOutputCP(CP_UTF8)`) for international character support.
- ANSI escape sequences managed via `enable_ansi_escapes()` / `disable_ansi_escapes()` for cross-terminal compatibility.
- Interactive prompt uses colored input markers (`COLOR_BRIGHT_GREEN " >>> "`).

---

## 6. Execution Modes & Usage Patterns

### 🟢 Interactive Shell Mode
```text
cmdbrowser.exe
```
Launches the REPL loop. Ideal for exploratory research, iterative querying, and manual bookmark/history management. Maintains state across commands until `/quit` or `EOF`.

### 🔵 Command-Line / Scripting Mode
```text
cmdbrowser.exe "quantum computing advances 2025"
cmdbrowser.exe /search machine learning
cmdbrowser.exe /format json /proxy http://127.0.0.1:8080 "AI ethics"
```
Executes a single query or command chain, outputs to `stdout`, and exits. Perfect for piping into `jq`, `grep`, `awk`, or automation scripts.

---

## 7. System Requirements & Build Considerations
- **OS:** Windows 7+ (x64 recommended)
- **Compiler:** MSVC 2019+, GCC 9+, or Clang-cl with C11 standard support
- **Dependencies:** Windows SDK (WinHTTP, Console API), standard C runtime (`msvcrt`/`ucrtbase`)
- **Build Flags:** `-std=c11`, `-DUNICODE` (if applicable), link `winhttp.lib`, `kernel32.lib`
- **Architecture:** Single-threaded execution in current snapshot. Concurrency limit (`ctx->concurrent_limit`) is reserved for future async I/O integration.

---

## 8. Extensibility & Future Roadmap
The modular header structure enables straightforward extension:
- **Threading:** Replace sequential `do_multi_search()` loop with thread pool (`_beginthread`/`std::thread`) to honor `ctx->concurrent_limit`.
- **Plugin API:** Expose engine parser hooks for community-maintained regex/JSONPath extractors.
- **Cross-Platform Port:** Swap WinHTTP for `libcurl` or `neon`, replace `SetConsoleCP` with POSIX termios, and implement `~/.cmdbrowser/` config paths.
- **Advanced Caching:** Integrate SQLite for indexed query/result storage and full-text search over history.
- **OAuth / API Keys:** Support authenticated engine endpoints (Google Custom Search API, Bing Web Search API).

---

## 9. Conclusion
CMDBrowser v2.0 is a meticulously engineered terminal-native search orchestrator that bridges the gap between lightweight CLI utilities and full-featured web aggregation platforms. By decoupling networking, parsing, state management, and UI rendering into discrete modules, it achieves exceptional maintainability and performance. Its rich command taxonomy, persistent data layer, and format flexibility make it an indispensable tool for developers, data analysts, and privacy-conscious users who demand precision, speed, and scriptability without leaving the command line.
