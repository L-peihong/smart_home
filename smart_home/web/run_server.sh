#!/bin/sh
#!/bin/sh
# 在 web 目录启动一个静态 + 简单 API 服务器（server.py）
PORT=${1:-8000}
cd "$(dirname "$0")" || exit 1
echo "Starting web server with API on http://0.0.0.0:$PORT"
python3 server.py $PORT
