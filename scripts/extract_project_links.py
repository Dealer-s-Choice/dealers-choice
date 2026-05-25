#!/usr/bin/env python3
"""Extract the "Project Links" section of README.md into a YAML data file
for the Jekyll site's sidebar (`_data/project_links.yml`).

Run from the repo root:

    python3 scripts/extract_project_links.py [readme_path] [output_path]

Defaults: README.md -> _data/project_links.yml. Exit code is non-zero if
the README has no "## Project Links" section (treated as an error so a
typo doesn't silently empty the sidebar).
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

LINK_RE = re.compile(r"\*\s+\[([^\]]+)\]\(([^)]+)\)\s*$")
HEADING_RE = re.compile(r"^#{1,6}\s")
PROJECT_LINKS_HEADING_RE = re.compile(r"^##\s+Project\s+Links\s*$", re.IGNORECASE)


def rewrite_url_for_site(url: str) -> str:
    """Convert relative `.md` paths to `.html` for the rendered Jekyll site.
    Absolute URLs (http(s)://, mailto:, etc.) and fragment links pass through.
    The README on github.com still resolves the `.md` paths correctly because
    only the YAML output is rewritten, not the README itself."""
    if "://" in url or url.startswith(("#", "mailto:")):
        return url
    if url.endswith(".md"):
        return url[:-3] + ".html"
    return url


def extract(readme: str) -> list[dict]:
    lines = readme.splitlines()
    start = next(
        (i for i, ln in enumerate(lines) if PROJECT_LINKS_HEADING_RE.match(ln)),
        None,
    )
    if start is None:
        raise SystemExit("error: README has no '## Project Links' section")

    items: list[dict] = []
    for raw in lines[start + 1 :]:
        if HEADING_RE.match(raw):
            break
        stripped = raw.lstrip(" ")
        if not stripped.startswith("*"):
            continue
        indent = len(raw) - len(stripped)
        m = LINK_RE.match(stripped)
        if not m:
            continue
        entry = {"title": m.group(1), "url": rewrite_url_for_site(m.group(2))}
        if indent == 0:
            items.append(entry)
        elif items:
            items[-1].setdefault("children", []).append(entry)
    return items


def to_yaml(items: list[dict]) -> str:
    out: list[str] = []
    for it in items:
        out.append(f"- title: {yaml_str(it['title'])}")
        out.append(f"  url: {yaml_str(it['url'])}")
        if children := it.get("children"):
            out.append("  children:")
            for ch in children:
                out.append(f"    - title: {yaml_str(ch['title'])}")
                out.append(f"      url: {yaml_str(ch['url'])}")
    return "\n".join(out) + "\n"


def yaml_str(s: str) -> str:
    # Quote anything that could confuse YAML; plain scalars are fine for
    # the rest.
    needs_quote = s == "" or s[0] in "-?:,[]{}#&*!|>'\"%@`" or ": " in s
    if needs_quote:
        return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'
    return s


def main(argv: list[str]) -> int:
    readme_path = Path(argv[1]) if len(argv) > 1 else Path("README.md")
    out_path = Path(argv[2]) if len(argv) > 2 else Path("_data/project_links.yml")
    items = extract(readme_path.read_text(encoding="utf-8"))
    if not items:
        raise SystemExit("error: 'Project Links' section parsed empty")
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(to_yaml(items), encoding="utf-8")
    print(f"wrote {len(items)} top-level link(s) to {out_path}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
