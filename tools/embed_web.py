"""Embed the self-contained UI in firmware."""
from pathlib import Path
Import("env")
root = Path(env.subst("$PROJECT_DIR"))
html = (root / "web/index.html").read_text()
(root / "include/web_page.h").write_text('#pragma once\nstatic const char WEB_PAGE[] PROGMEM = R"CYDWEB(' + html + ')CYDWEB";\n')
