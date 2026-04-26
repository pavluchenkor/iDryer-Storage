# Генератор меню

## Установка

```bash
python3 -m venv .venv
source .venv/bin/activate
pip install pyyaml
```

## Запуск

Из папки `lib/idryer-menu/`:

```bash
python3 generator/gen_menu_v3_nvs.py menu_v2.yaml --out src
```
```bash
python3 lib/idryer-menu/generator/gen_menu_v3_nvs.py lib/idryer-menu/menu_v2.yaml --out lib/idryer-menu/src
```
