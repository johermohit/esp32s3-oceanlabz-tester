@echo off
cd /d "%~dp0docs"
echo Open http://localhost:8000 in Chrome or Edge. Ctrl+C to stop.
python -m http.server 8000
