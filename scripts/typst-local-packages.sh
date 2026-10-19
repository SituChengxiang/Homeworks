#!/usr/bin/env bash
# 把本地维护的 Typst 模板仓库挂成 @local 包。
#
# 挂好之后，任何作业目录里的 .typ 都能直接写
#     #import "@local/zjut-phy-hwk:0.2.0": *
# 不需要再把 lib.typ / utils.typ / template/ 复制一份到作业目录里。
# 模板仓库改一行，所有作业下次编译就生效；版本升级时重跑本脚本即可。
#
# 用法：
#     bash scripts/typst-local-packages.sh          # 建立 / 刷新软链接
#     bash scripts/typst-local-packages.sh --check   # 只检查当前状态，不写任何东西
#
# 可选环境变量：
#     TEMPLATES_ROOT   模板仓库所在目录，默认 $HOME/Git
set -euo pipefail

TEMPLATES_ROOT="${TEMPLATES_ROOT:-$HOME/Git}"
PACKAGES=(zjut-phy-hwk ZJUT-Report-Typst)
LOCAL_DIR="${XDG_DATA_HOME:-$HOME/.local/share}/typst/packages/local"

check_only=0
if [[ "${1:-}" == "--check" ]]; then
  check_only=1
fi

# 取 typst.toml 里 [package] 的字段（name / version），只认第一次出现
pkg_field() {
  sed -n "s/^$2[[:space:]]*=[[:space:]]*\"\{0,1\}\([^\"[:space:]]*\)\"\{0,1\}[[:space:]]*\$/\1/p" "$1" | head -1
}

echo "本地包目录：$LOCAL_DIR"
echo "模板来源：  $TEMPLATES_ROOT"
echo

for repo in "${PACKAGES[@]}"; do
  src="$TEMPLATES_ROOT/$repo"
  toml="$src/typst.toml"

  if [[ ! -f "$toml" ]]; then
    echo "跳过 $repo：$toml 不存在" >&2
    continue
  fi

  name="$(pkg_field "$toml" name)"
  version="$(pkg_field "$toml" version)"
  if [[ -z "$name" || -z "$version" ]]; then
    echo "跳过 $repo：typst.toml 里读不到 name/version" >&2
    continue
  fi

  dst="$LOCAL_DIR/$name/$version"

  if [[ "$check_only" == 1 ]]; then
    if [[ -L "$dst" ]]; then
      echo "[ok]   @$name:$version -> $(readlink "$dst")"
    elif [[ -e "$dst" ]]; then
      echo "[warn] @$name:$version 已存在但不是软链接：$dst"
    else
      echo "[miss] @$name:$version 未挂载（源：$src）"
    fi
    continue
  fi

  mkdir -p "$LOCAL_DIR/$name"
  # -n：dst 已是指向目录的软链接时替换它，而不是钻进去建链接
  ln -sfn "$src" "$dst"
  echo "[link] @$name:$version -> $src"
done

if [[ "$check_only" == 1 ]]; then
  exit 0
fi

cat <<'EOF'

作业里这样引用（glob 导入即可拿到 hwk / problem / solution / 物理常数等全部定义）：

  #import "@local/zjut-phy-hwk:0.2.0": *          # 物理解答模板
  #import "@local/simple-zjut-report:0.1.1": experiment-report   # 实验报告模板

编译不需要额外的 --root，typst 会从本地包目录解析 @local。
EOF
