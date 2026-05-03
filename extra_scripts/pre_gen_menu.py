"""
Pre-build hook: автогенерация меню из menu.yaml.

Что делает:
  Запускает lib/idryer-core/tools/menu_gen.py с menu.yaml продукта,
  кладёт сгенерированные .h/.cpp в src/menu/. PlatformIO потом сам
  скомпилирует всё что в src/.

Когда срабатывает:
  Перед каждым `pio run` (PIO зовёт pre-extra_scripts автоматически).
  Делает mtime-check: если menu.yaml не новее src/menu/* — пропускает.
  Поэтому на повторных сборках без правки yaml — не тратит время.

Что нужно подключить:
  В platformio.ini:
      [env]
      extra_scripts =
        pre:extra_scripts/pre_gen_menu.py
        post:extra_scripts/copy_firmware.py    ; уже есть для prod

Ручное использование (без PIO):
  python3 lib/idryer-core/tools/menu_gen.py menu.yaml --out src/menu --num-units 1

Принудительная регенерация:
  rm -rf src/menu && pio run

Где что лежит:
  menu.yaml                            ← source of truth (правит разработчик)
  lib/idryer-core/tools/menu_gen.py    ← общий генератор (часть core)
  src/menu/                            ← сгенерированный C++ (НЕ редактировать)
"""

import os
import sys
import subprocess
from pathlib import Path

Import("env")

PROJECT_DIR = Path(env["PROJECT_DIR"])
YAML_PATH   = PROJECT_DIR / "menu.yaml"
OUT_DIR     = PROJECT_DIR / "src" / "menu"
GEN_PATH    = PROJECT_DIR / "lib" / "idryer-core" / "tools" / "menu_gen.py"

GREEN = "\033[92m"
YELLOW = "\033[93m"
RED = "\033[91m"
RESET = "\033[0m"


def _ensure_pyyaml() -> None:
    """Ставит pyyaml в PIO venv если его нет (один раз). Генератор требует yaml."""
    probe = subprocess.run(
        [sys.executable, "-c", "import yaml"],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    if probe.returncode == 0:
        return
    print(f"  {YELLOW}[menu_gen] installing pyyaml into PIO venv (one-time){RESET}")
    subprocess.check_call(
        [sys.executable, "-m", "pip", "install", "--quiet", "pyyaml"]
    )


def _needs_regen() -> bool:
    """True если menu.yaml новее любого файла в src/menu/, или src/menu пустой."""
    if not OUT_DIR.exists():
        return True
    cpp_files = list(OUT_DIR.glob("*.cpp"))
    if not cpp_files:
        return True
    yaml_mtime = YAML_PATH.stat().st_mtime
    oldest_gen = min(f.stat().st_mtime for f in OUT_DIR.glob("*") if f.is_file())
    return yaml_mtime > oldest_gen


def _run() -> None:
    if not YAML_PATH.exists():
        print(f"  {YELLOW}[menu_gen] no menu.yaml in project root — skipped{RESET}")
        return
    if not GEN_PATH.exists():
        print(f"  {RED}[menu_gen] generator not found: {GEN_PATH}{RESET}")
        print(f"  {RED}[menu_gen] check that lib/idryer-core points to a valid checkout{RESET}")
        return

    if not _needs_regen():
        print(f"  {GREEN}[menu_gen] up-to-date — generation skipped{RESET}")
        return

    _ensure_pyyaml()
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    print(f"  {GREEN}[menu_gen] regenerating from {YAML_PATH.name} → src/menu/{RESET}")
    subprocess.check_call([
        sys.executable, str(GEN_PATH),
        str(YAML_PATH),
        "--out", str(OUT_DIR),
        "--num-units", "1",
    ])


_run()
