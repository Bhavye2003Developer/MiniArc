#!/usr/bin/env bash
set -euo pipefail

MODEL_REPO="Qwen/Qwen2.5-0.5B-Instruct-GGUF"
MODEL_FILE="qwen2.5-0.5b-instruct-q4_k_m.gguf"
HF_URL="https://huggingface.co/${MODEL_REPO}/resolve/main/${MODEL_FILE}"
DEST="models/${MODEL_FILE}"

mkdir -p models

if [ -f "${DEST}" ]; then
    echo "Model already exists at ${DEST}"
    exit 0
fi

echo "Downloading ${MODEL_FILE} (~300 MB) from HuggingFace..."
echo "URL: ${HF_URL}"
echo ""

if command -v curl &>/dev/null; then
    curl -L --progress-bar -o "${DEST}" "${HF_URL}"
elif command -v wget &>/dev/null; then
    wget -q --show-progress -O "${DEST}" "${HF_URL}"
else
    echo "Error: neither curl nor wget is installed."
    exit 1
fi

echo ""
echo "Saved to ${DEST}"
echo "Run:  ./build/miniARC"
