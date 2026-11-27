#!/bin/sh
cd "$(dirname "$0")/docs" && echo "Open http://localhost:8000 in Chrome or Edge" && python3 -m http.server 8000
