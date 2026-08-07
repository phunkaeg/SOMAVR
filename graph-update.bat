@echo off
REM ---------------------------------------------------------------
REM  Refresh this project's knowledge graph (code only).
REM  FREE: AST-only, no LLM, no API key, no tokens.
REM  Docs are NOT re-extracted -- that needs the /graphify skill.
REM ---------------------------------------------------------------
cd /d "%~dp0"
echo Updating graphify graph for:
echo   %CD%
echo.
graphify update .
if errorlevel 1 (
  echo.
  echo ** FAILED **
  echo   - graphify not found?   uv tool install graphifyy
  echo   - asked for an LLM key? you want "graphify update ." ^(free^),
  echo     NOT "graphify . --update" ^(which demands a key and aborts^).
)
echo.
pause
