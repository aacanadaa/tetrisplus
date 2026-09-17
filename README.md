<div align="center">

<a href="https://github.com/aacanadaa/tetrisplus">
  <img src="logos/logo-transparent.png" alt="tetrisplus" width="160">
</a>

<h1>Tetris+（tetrisplus）</h1>

<p><strong>终端里的完整俄罗斯方块 —— 用 C 编写，基于 ncurses / PDCurses。</strong></p>

<p>
  <a href="README.md"><strong>简体中文</strong></a> &nbsp;·&nbsp;
  <a href="README.en.md">English</a>
</p>

<p>
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-PolyForm%20Noncommercial%201.0.0-orange.svg" alt="License: PolyForm Noncommercial 1.0.0"></a>
  <a href="https://github.com/aacanadaa/tetrisplus/actions/workflows/ci.yml"><img src="https://github.com/aacanadaa/tetrisplus/actions/workflows/ci.yml/badge.svg" alt="CI status"></a>
  <a href="tetris.c"><img src="https://img.shields.io/badge/C-C99-00599C.svg?logo=c&logoColor=white" alt="Written in C99"></a>
  <img src="https://img.shields.io/badge/TUI-ncurses%20%2F%20PDCurses-3A7D44.svg?logo=gnu&logoColor=white" alt="Runs on ncurses and PDCurses">
  <img src="https://img.shields.io/badge/platform-Linux%20%7C%20macOS%20%7C%20Windows%20%7C%20BSD-2C2D72.svg" alt="Platform: Linux, macOS, Windows and BSD">
  <img src="https://img.shields.io/badge/packaging-.deb-A80030.svg?logo=debian&logoColor=white" alt="Ships a Debian package">
  <a href="https://snapcraft.io/tetrisplus"><img src="https://img.shields.io/badge/snap-tetrisplus-82BEA0.svg?logo=snapcraft&logoColor=white" alt="Available as a snap"></a>
  <a href="https://ko-fi.com/suoim"><img src="https://img.shields.io/badge/Ko--fi-Support%20me-ff5e5b?logo=kofi&logoColor=white" alt="Support me on Ko-fi"></a>
</p>

</div>

单文件、无构建系统：Unix 端只依赖 ncurses，Windows 端只依赖 PDCurses。内置游戏
菜单、可持久保存的街机式高分榜，以及一个便于继续扩展的游戏模式系统。

**菜单** —— 左右键选模式，上下键选操作：

```
                    T E T R I S
           ──────────────────────────────

               Mode    <  Marathon  >
 Classic endless tetris. Clear lines, survive, score.

           ──────────────────────────────

                > Play

                  High Scores

                  Quit

  Up/Dn move    L/R mode    Enter select    Q quit
```

**Marathon 游戏中** —— 下落中的方块、显示硬降落点的幽灵方块、下一块预览，以及
实时计数：

```
┌────────────────────┐  TETRIS
│. . . . . []. . . . │  ──────────────
│. . . [][][]. . . . │
│. . . . . . . . . . │  Score
│. . . . . . . . . . │  164
│. . . . . . . . . . │
│. . . . . . . . . . │  Level
│. . . . . . . . . . │  1
│. . . . . . . . . . │
│. . . . . . . . . . │  Lines
│. . . . . . . . . . │  0
│. . . . . . . . . . │
│. . . . . . . . . . │  Next
│. . . . . . . . . . │  . [][]
│. . . . . . . . . . │  [][].
│. . . . . []. . . . │  L/R   move
│. . . [][][]. . . . │  Up    rotate
│[][][][][][]. . . . │  Dn    soft drop
│[][]. . . []. . . . │  Space hard drop
│[][]. . . [][][][]. │  P     pause
│. [][]. . . . [][][]│  M     menu
└────────────────────┘  Q     quit
```

堆叠上方那个孤零零的 `[]` 就是**幽灵方块** —— 它标出按下空格后方块的落点。
在真实终端里它显示为暗色。

## 安装

所有方式最终都会得到一个 `tetrisplus` 命令，在任何目录下都能运行。

### 从 Snap 商店安装

```sh
sudo snap install tetrisplus
```

在任何一个装有 snapd 的 Linux 上都能用（不限于 Debian 系），而且会自动更新。
有两点值得先说明。

**这个名字来自商店，而不是游戏本身。** Snap 名称全局唯一，`terminal-tetris`
已被一个无关项目注册，所以本项目带上了 `plus`。仓库、`.deb` 和命令都沿用了
它。更短的 `tetris` 别名需要商店手工授予，因此默认没有。

**Snap 使用自己独立的高分表**，因为严格沙箱把文件系统的其余部分都遮住了。
它保存在哪里、如何删除，见[高分榜](#高分榜)。

### 从 Release 安装

从[最新 Release](https://github.com/aacanadaa/tetrisplus/releases/latest) 下载
`.deb` 并安装。通配符可以省去手输版本号：

```sh
sudo apt install ./tetrisplus_*_amd64.deb
```

这是适合直接发给别人的方式。它会把 `tetrisplus` 装进 `/usr/bin`，对机器上所有
用户可用，自动拉取 `libncurses6`，并安装 man 手册，因此 `man tetrisplus` 可用。
用 `sudo apt remove tetrisplus` 卸载。

### 从压缩包安装（macOS 与 Windows）

同一个[最新 Release](https://github.com/aacanadaa/tetrisplus/releases/latest) 也
为另外两个平台提供了预编译二进制，二者都无需安装编译器、ncurses 或任何其他
东西：

- **macOS** —— `tetrisplus_<version>_macos_<arch>.tar.gz`。解压后运行
  `./tetrisplus`。它只是由编译器做了 ad-hoc 签名，并未经过公证；如果下载后被
  Gatekeeper 隔离，用 `xattr -d com.apple.quarantine ./tetrisplus` 解除即可。
- **Windows** —— `tetrisplus_<version>_windows_<arch>.zip`。解压后运行
  `tetrisplus.exe`。它是静态链接的，单个 `.exe` 就是完整的游戏。

### 从克隆的源码安装（本机）

```sh
./install.sh
```

它会编译游戏并安装到 `/usr/local/bin/tetrisplus`，只有在该目录不可写时才请求
sudo。若想完全不写入系统目录：

```sh
./install.sh --user     # 安装到 ~/.local/bin，从不使用 sudo
```

安装脚本在开始前会检查编译器和 curses 头文件，缺少时会打印对应的安装命令。同
一个脚本在 Windows 的 MSYS2 MinGW shell 里也能用：此时它改为链接 PDCurses，并
把 `tetrisplus.exe` 装进 `$MINGW_PREFIX/bin`（该目录已在 Windows 的 `PATH` 中）。

### 自行构建软件包

```sh
./build-deb.sh              # -> dist/tetrisplus_<version>_<arch>.deb
sudo apt install ./dist/tetrisplus_*_amd64.deb
```

在 MSYS2 MinGW shell 里，同样的思路会为 Windows 生成一个自包含的发布压缩包：

```sh
./build-windows.sh          # -> dist/tetrisplus_<version>_windows_<arch>.zip
```

### 卸载

请与安装时使用相同的参数。前缀必须一致，否则脚本会去错误的位置查找，什么也找
不到：

| 安装方式                                    | 卸载方式                          |
| ------------------------------------------- | --------------------------------- |
| `sudo snap install tetrisplus`              | `sudo snap remove tetrisplus`     |
| `sudo apt install ./tetrisplus_*.deb`       | `sudo apt remove tetrisplus`      |
| `./install.sh`（系统级）                    | `./install.sh --uninstall`        |
| `./install.sh --user`                       | `./install.sh --user --uninstall` |

两种 `install.sh` 方式都需要克隆仍在磁盘上。如果你已经删除了它，安装其实只有
一个文件：

```sh
rm ~/.local/bin/tetrisplus              # --user 安装
sudo rm /usr/local/bin/tetrisplus       # 系统级安装
rm "$MINGW_PREFIX/bin/tetrisplus.exe"   # Windows（MSYS2）
```

**你的高分不属于安装的一部分**，所以上面任何一种方式都不会删除它们 —— 它们保
存在仓库之外，重装后依然存在。Snap 同样属于这个例外：`snap remove` 会一并删除
它的分数表，因为表保存在 snap 自己的数据目录里，而不是你的目录。

清空其余内容：

```sh
rm -rf "${XDG_DATA_HOME:-$HOME/.local/share}/tetrisplus"   # .deb 和 install.sh
rm -rf ~/snap/tetrisplus/current/.local/share/tetrisplus   # snap，如需单独删除
```

而这一切都不是 Makefile：本项目刻意不提供 Makefile。

## 系统要求

仅在从源码构建时需要 —— Release 压缩包和安装脚本会处理其余的事情。

游戏是 C99，只有一层很薄的平台适配。在 Unix 上是纯 POSIX，除 ncurses 外无需任
何东西；在 Windows 上则针对 PDCurses 构建，它提供相同的 API。使用
`-std=c99 -D_POSIX_C_SOURCE=200809L` 可以干净编译，不含 GNU 或 glibc 扩展；只
使用历史悠久的 curses 调用（`initscr`、`napms` 等基础接口），因此不绑定某个库版
本或某种 Unix 变体。

- **Ubuntu / Debian** —— `sudo apt install -y build-essential libncurses-dev`
- **macOS** —— `xcode-select --install`；ncurses 随系统提供
- **Windows** —— 安装 [MSYS2](https://www.msys2.org)，在 UCRT64 shell 中执行
  `pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-pdcurses`
- **FreeBSD / OpenBSD / NetBSD** —— ncurses 在基础系统中，无需安装

然后按下方说明构建。如果你只是想玩，上面的 Release 压缩包已经包含二进制，完全
不需要这些。

> Linux、macOS 和 Windows 都由 CI 构建。pty 测试套件在 Linux 和 macOS 上运行；
> Windows 只做编译检查，因为无头运行器没有可供它使用的控制台。

## 构建

```sh
gcc tetris.c -o tetrisplus -lncurses                  # Linux、macOS、BSD
gcc tetris.c -o tetrisplus.exe -lpdcurses -lwinmm     # Windows（MSYS2）
```

在 Windows 上加 `-static`，可以把 PDCurses、libgcc 和 libwinpthreads 一并打进
`.exe`，从而在没有安装 MinGW 的机器上也能运行 —— `build-windows.sh` 和 Release
压缩包就是这么做的。

## 运行

```sh
./tetrisplus        # Linux、macOS、BSD
tetrisplus.exe      # Windows
```

终端至少需要 **50 列 × 22 行**。大多数默认的 80x24 终端都没问题。可随时调整窗
口大小 —— 棋盘和菜单会自动重新居中。

## 菜单

游戏从菜单开始。上下键选择操作，左右键切换模式，回车确认。

- **Play** —— 开始游戏（或在游戏结束后再来一局）
- **High Scores** —— 当前模式的前 10 名
- **Quit** —— 退出并恢复终端

## 操作

### 菜单中

| 按键             | 操作           |
| ---------------- | -------------- |
| `Up` / `Down`    | 在操作之间移动 |
| `Left` / `Right` | 切换游戏模式   |
| `Enter`          | 确认           |
| `Q`              | 退出           |

### 游戏中

| 按键             | 操作                          |
| ---------------- | ----------------------------- |
| `Left` / `Right` | 左右移动方块                  |
| `Up`             | 顺时针旋转                    |
| `Down`           | 软降（每格 +1 分）            |
| `Space`          | 硬降（每格 +2 分）            |
| `P`              | 暂停 / 继续                   |
| `M`              | 返回主菜单（放弃本局）        |
| `Q`              | 退出到 shell                  |

### 本局结束时

当方块堆到顶部，或达成模式目标时，本局结束 —— 面板会相应显示 `GAME OVER`、
`CLEARED` 或 `TIME UP`。

| 按键 | 操作               |
| ---- | ------------------ |
| `R`  | 以相同模式再来一局 |
| `M`  | 返回主菜单         |
| `Q`  | 退出               |

### 输入名字缩写

如果你的分数进入前 10，会出现街机风格的名字输入提示：

| 按键              | 操作             |
| ----------------- | ---------------- |
| `A`-`Z`、`0`-`9`  | 输入当前字符     |
| `Backspace`       | 删除上一个字符   |
| `Enter`           | 保存分数         |
| `Esc`             | 跳过 —— 不保存   |

## 游戏模式

| 模式                     | 目标                                                         | 排名依据 |
| ------------------------ | ------------------------------------------------------------ | -------- |
| **Marathon（马拉松）**   | 无尽模式。活下去并拿高分。                                   | 分数     |
| **Sprint（冲刺）**       | 尽快消除 40 行。                                             | **时间** |
| **Ultra（极限）**        | 两分钟倒计时，尽可能多拿分。                                 | 分数     |
| **Expert（专家）**       | 无尽模式，但从 10 级开始 —— 重力初始为每步 170 毫秒而非 800。 | 分数     |

重力从每步 800 毫秒逐步加快到最低 80 毫秒，每消除 10 行升一级；Expert 则直接从
这条曲线的中段开始。

Sprint 按**最快时间**排名，而不是最高分 —— 它的排行榜显示 `TIME` 列而非
`SCORE`。只有真正消完 40 行才会记录时间；中途在 30 行放弃不会留下一个无法打破
的短时间。任何带目标的模式都会把等级显示换成时钟：Sprint 正计时，Ultra 倒计时。

新增一个模式就是在 `tetris.c` 顶部的 `MODES[]` 表里加一行 —— 规则引擎、菜单、
HUD 和分数表都从那里读取行为。每个模式各自维护高分表。

## 高分榜

分数会在会话之间保存，位置是应用数据的标准目录。在 Unix 上：

```
$XDG_DATA_HOME/tetrisplus/scores
# 如果未设置 XDG_DATA_HOME：
~/.local/share/tetrisplus/scores
```

在 Windows 上，同一个文件位于 `%LOCALAPPDATA%` 下：

```
%LOCALAPPDATA%\tetrisplus\scores
```

Snap 是例外。严格沙箱对它隐藏了你的主目录，因此它把分数表保存在自己的沙箱内，
两者互不可见 —— 在 snap 下取得的分数不会出现在 `.deb` 安装里，反之亦然：

```
~/snap/tetrisplus/current/.local/share/tetrisplus/scores
```

文件是纯文本，方便阅读或备份：

```
# tetrisplus high scores
# <mode> <initials> <score> <level> <lines> <date> [<elapsed_ms>]
marathon SUO 12400 5 42 2026-09-09 0
sprint BOT 13782 5 40 2026-09-09 13061
```

末尾的时间字段是可选的，且只对按时间排名的模式有意义 —— 在 Sprint 出现之前写下
的文件没有它也能正常读取，而缺少时间的行在按时间排名的榜上会排在最后。

每个模式只保留前 10 名。读取逻辑刻意宽容：忽略注释、空行、格式错误的行，以及
已不存在模式的行。名字会被强制为三个可安全显示的字符，因此手工编辑永远不会把终
端转义序列注入到界面里。

**要清空分数**，删除该文件即可：

```sh
rm ~/.local/share/tetrisplus/scores          # Unix
del "%LOCALAPPDATA%\tetrisplus\scores"       # Windows（cmd）
```

如果文件因任何原因无法读取或写入，游戏依然可以玩 —— 只是不会记住分数。

## 计分

| 一次消除行数       | 得分          |
| ------------------ | ------------- |
| 1                  | 100 × 等级    |
| 2                  | 300 × 等级    |
| 3                  | 500 × 等级    |
| 4（即 "tetris"）   | 800 × 等级    |

软降每格加 1 分；硬降每格加 2 分。

## 功能特性

- 全部七种方块（I、J、L、O、S、T、Z），各有自己的颜色
- 踢墙（wall kick），贴墙旋转也能成功
- 幽灵方块，显示硬降的落点
- 7-bag 随机算法，不会长时间不出 I 块
- 下一块预览，以及分数、等级、行数计数
- 按模式持久保存的前 10 名高分，带街机式名字输入
- 菜单界面与排行榜界面
- 固定约 60 fps 循环上的非阻塞输入，重力平滑且与按键无关
- 退出时始终恢复终端，包括 Ctrl-C
- 由单个源文件原生支持 Linux、macOS、Windows 和 BSD

## 测试

游戏是全屏 TUI，无法在普通 shell 里运行 —— 它需要一个 pty。`tests/run_tests.sh`
会准备好 pty、编译游戏，并对它运行三个测试套件：

```sh
tests/run_tests.sh
```

它覆盖四种模式的 HUD、下落计分、暂停、终端尺寸门限、分数文件解析，以及两种结束
条件的端到端流程（Ultra 超时到 `TIME UP`，Sprint 到达 `CLEARED` 并在榜上留下时
间）。它构建的所有内容都放到 `$TETRIS_WORK`（默认 `/tmp/tetrisplus-test`），不会
动你的工作区 —— 运行结束时会校验这一点。

需要带 `venv` 的 `python3`。`pyte` 会自动装进 venv。在 Linux 上，运行器会把
ncurses 头文件解包到 `$TETRIS_WORK`，无需 root；在 macOS 上则使用系统自带的
ncurses。

那些本来要跑上几分钟才能触达的结束条件，用缩短常数临时生成的变体来测试 —— 3 秒
的 Ultra 时钟、单行的 Sprint 目标。同一套代码路径，只是数字不同。

CI 在 Linux 和 macOS 上运行这套测试，并用 MSYS2 + PDCurses 对 Windows 构建做编译
检查；见 `.github/workflows/ci.yml`。发布时推送一个 tag，
`.github/workflows/release.yml` 随后会构建 `.deb`、macOS tar 包和 Windows zip，
并作为同一个 GitHub Release 发布。

## 目录结构

```
tetris.c           整个游戏
install.sh         构建并安装为 `tetrisplus`
build-deb.sh       打包为 .deb
build-windows.sh   打包为 Windows .zip
snap/              打包为 snap
packaging/         man 手册
logos/             项目 logo
LICENSE            PolyForm Noncommercial 许可证
tests/             pty 测试套件及其运行器
CLAUDE.md          项目记忆与约定
README.md          简体中文（默认）
README.en.md       English
```

## 许可证

[PolyForm Noncommercial 1.0.0](LICENSE) —— 源码可见，但不是开源。

你可以在任何**非商业**目的下阅读、运行、修改和分享它：个人使用、学习、业余项
目，以及慈善机构、学校、公共研究机构、健康与安全组织和政府机构的使用。不允许商
业使用。

在本许可证启用之前发布的副本以 MIT 许可证发布，该授权无法撤回 —— 那些版本仍为
MIT，任何已经从其 fork 出来的项目也一样。
