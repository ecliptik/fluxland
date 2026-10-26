#!/bin/sh
# SPDX-License-Identifier: MIT
# fluxbox-names.sh - The names Fluxbox knows, from its source, for the
# parity test (tests/compat.py): its commands, init resources, apps
# attributes and menu tags, one per line.
#
# Usage: tools/fluxbox-names.sh ~/git/fluxbox
#   writes tests/fixtures/fluxbox/{commands,init-keys,apps-keys,menu-tags}
#   (sorted names only: the examples next to them in commands, apps-keys
#   and menu-tags are kept, new names get none)

set -eu
src=${1:?usage: tools/fluxbox-names.sh FLUXBOX_SOURCE_DIR}/src
cd "$(dirname "$0")/.."
out=tests/fixtures/fluxbox

# Keep "name<TAB>example" lines for the names still there
merge() {
	file=$out/$1
	tmp=$(mktemp)
	sort -u > "$tmp.names"
	while read -r name; do
		line=$(awk -F'\t' -v n="$name" '$1 == n { print; exit }' \
			"$file" 2>/dev/null || true)
		echo "${line:-$name}"
	done < "$tmp.names" > "$tmp"
	{ grep '^#' "$file" 2>/dev/null || true; cat "$tmp"; } > "$file.new"
	mv "$file.new" "$file"
	rm -f "$tmp" "$tmp.names"
}

grep -rhoE 'REGISTER_COMMAND(_PARSER|_WITH_ARGS|_WITH_ARGS_NC|_PARSER_NC)?\([a-zA-Z0-9_]+' \
	"$src" | sed -E 's/.*\(//' | grep -vx name | merge commands

{
	grep -ohE '(scrname|name\(\)|screen\.name\(\)|screen\(\)\.name\(\)|screen->name\(\)|m_screen\.name\(\)) *\+ *"\.[A-Za-z0-9.]+"' \
		$(find "$src" -name '*.cc' -not -path '*/tests/*') |
		sed -E 's/.*"\.(.*)"/session.screen0.\1/'
	grep -ohE '"session\.[A-Za-z0-9.]+"' $(find "$src" -name '*.cc' -not -path '*/tests/*') |
		tr -d '"'
} | grep -v '\.[A-Z]' | grep -vE '^session\.(screen|Screen)$|\.$' |
	merge init-keys

grep -ohE '(str_key|lc_key) == "[a-z]+"' "$src/Remember.cc" |
	grep -oE '"[a-z]+"' | tr -d '"' | grep -vx end | merge apps-keys

grep -ohE '(str_key|type|str_label|key) == "[a-z]+"' "$src/MenuCreator.cc" |
	grep -oE '"[a-z]+"' | tr -d '"' |
	grep -vxE 'end|endencoding|iconmenu|workspacemenu' | merge menu-tags
# (iconmenu and workspacemenu: the menu types [icons] and [workspaces]
# make, createMenuType's, not tags)
