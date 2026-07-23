#!/usr/bin/env bash
# minros dokümantasyonunu üretir (doxygen-awesome-css temasıyla).
#
# Kullanım:  bash docs/build.sh   (repo kökünden ya da her yerden)
# Çıktı:     docs/html/index.html
#
# Neden script? doxygen-awesome'ın JS eklentileri (dark-mode düğmesi, kod
# kopyalama, etkileşimli TOC) için header.html'in </head> öncesine <script>
# satırları enjekte edilmeli. header.html Doxygen SÜRÜMÜNE bağlı olduğundan
# elle yazmak yerine burada `doxygen -w` ile ÜRETİP yamalıyoruz → sürüm-güvenli.
set -euo pipefail

# Repo kökü = bu script'in bir üst dizini
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

command -v doxygen >/dev/null || { echo "HATA: doxygen kurulu değil → sudo apt-get install -y doxygen"; exit 1; }

HEADER="docs/header.html"

# 1) header.html'i bu Doxygen sürümü için üret (yoksa ya da yamasızsa)
if [ ! -f "$HEADER" ] || ! grep -q "DoxygenAwesomeInteractiveToc" "$HEADER"; then
  echo "→ header.html üretiliyor (doxygen -w)…"
  # -w html <header> <footer> <css> <config> : footer/css'i geçici üret, header'ı tut
  doxygen -w html "$HEADER" docs/.footer.tmp docs/.style.tmp docs/Doxyfile
  rm -f docs/.footer.tmp docs/.style.tmp

  # 2) awesome JS satırlarını </head> öncesine enjekte et
  echo "→ header.html yamalanıyor (awesome JS)…"
  python3 - "$HEADER" <<'PY'
import sys
p = sys.argv[1]
# NOT: Manuel dark-mode toggle'ı BİLEREK yüklemiyoruz. Tema, CSS'in
# @media (prefers-color-scheme) kuralıyla doğrudan sistemi izler → her sayfada
# tutarlı, localStorage yok. Toggle eklenirse file://'de localStorage sayfalar
# arası izole olduğundan sayfa-sayfa koyu/açık zıplaması oluşuyordu.
inject = '''<script type="text/javascript" src="$relpath^minros-theme-toggle.js"></script>
<script type="text/javascript" src="$relpath^doxygen-awesome-paragraph-link.js"></script>
<script type="text/javascript" src="$relpath^doxygen-awesome-interactive-toc.js"></script>
<script type="text/javascript">
    DoxygenAwesomeParagraphLink.init()
    DoxygenAwesomeInteractiveToc.init()
</script>
'''
s = open(p, encoding="utf-8").read()
s = s.replace("</head>", inject + "</head>", 1)
open(p, "w", encoding="utf-8").write(s)
PY
fi

# 3) siteyi üret
echo "→ doxygen çalışıyor…"
doxygen docs/Doxyfile
echo "✓ Bitti → docs/html/index.html"
