#!/bin/sh
# ACE OS: optional local AI assistant setup.
#
# Run this AFTER installing ACE OS, with the laptop online:
#   sh tools/setup-ai.sh
#
# What it does:
#   1. Installs Ollama (local AI model runner).
#   2. Pulls a small model that actually fits this laptop's 4 GB of RAM.
#
# Reality check for the L510M (Celeron N4020, 4 GB RAM):
#   - Small models (~0.5-1.5B parameters) work, but replies are slow —
#     expect a few words per second. Fine for questions and short help.
#   - Large models will NOT run on this hardware. For heavier AI work,
#     use a cloud assistant in Firefox (e.g. claude.ai) instead.
set -e

MODEL="${1:-qwen2.5:1.5b}"

echo "==> Installing Ollama..."
if ! command -v ollama >/dev/null 2>&1; then
    curl -fsSL https://ollama.com/install.sh | sh
else
    echo "    Ollama is already installed."
fi

echo "==> Downloading the AI model ($MODEL — about 1 GB)..."
ollama pull "$MODEL"

echo ""
echo "All set. To chat with your local AI, open a terminal and run:"
echo "    ollama run $MODEL"
echo ""
echo "Tip: close other apps first — the model needs most of the 4 GB of RAM."
