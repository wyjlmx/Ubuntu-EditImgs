#!/bin/bash
# Linux 默认终端通常已是 UTF-8 编码

echo "======================================================="
echo "            🚀 正在将代码及文档打包给 AI..."
echo "======================================================="
echo ""

# 检查是否安装了 Node.js / npx
if ! command -v npx &> /dev/null; then
    echo "[错误] 未找到 npx 命令！请确保已安装 Node.js。"
    echo "请前往 https://nodejs.org/ 下载安装，或使用你的包管理器 (如 apt/yum) 安装。"
    exit 1
fi

# 需要【排除】的文件和文件夹（黑名单）
IGNORES="gver,build/**/*"
OUTPUT_FILE="ExportCode.xml"

echo "[状态] 正在扫描和压缩代码与文档，请稍候..."
npx repomix --ignore "$IGNORES" --remove-comments --compress --output "$OUTPUT_FILE"

if [ $? -eq 0 ]; then
    echo "======================================================="
    echo "    ✅ 打包成功！文件已保存为: $OUTPUT_FILE"
    echo "    ✨ 请将生成的文件直接拖拽到 AI 对话框中即可！"
    echo "======================================================="
else
    echo "======================================================="
    echo "    ❌ 打包过程中出现错误，请检查上方的报错信息。"
    echo "======================================================="
fi