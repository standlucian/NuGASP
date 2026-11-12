#!/usr/bin/env python3
"""
NuTrackN Manual to Print-Ready HTML & PDF Generator
Converts docs/USER_MANUAL.md into docs/USER_MANUAL.html with:
- KaTeX LaTeX math support
- Forced page breaks before Table of Contents, after Table of Contents, and before every Chapter
- Professional typography, GitHub-style alerts, and print-optimized CSS
"""

import os
import re
import sys
import markdown

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
MD_PATH = os.path.join(SCRIPT_DIR, "USER_MANUAL.md")
HTML_PATH = os.path.join(SCRIPT_DIR, "USER_MANUAL.html")

def build_html():
    if not os.path.exists(MD_PATH):
        print(f"Error: {MD_PATH} not found.")
        sys.exit(1)

    with open(MD_PATH, "r", encoding="utf-8") as f:
        md_text = f.read()

    # Step 1: Protect LaTeX math formulas from Markdown formatting
    math_store = []
    def save_math(m):
        idx = len(math_store)
        math_store.append(m.group(0))
        return f"KZMATHPLACEHOLDER{idx}ENDKZ"

    # Display math ($$...$$) first, then inline math ($...$)
    protected = re.sub(r"\$\$[\s\S]*?\$\$", save_math, md_text)
    protected = re.sub(r"(?<!\\)\$([^\$\n]+?)(?<!\\)\$", save_math, protected)

    # Step 2: Extract Title Page, Table of Contents, and Chapter Bodies separately
    # This prevents Python-Markdown from failing to parse markdown inside <div> wrappers!
    toc_match = re.search(r"(## Table of Contents[\s\S]*?)(?=\n---\n\n## 1\. )", protected)
    if toc_match:
        title_raw = protected[:toc_match.start()].strip()
        # Clean trailing --- from title
        title_raw = re.sub(r"\n---\s*$", "", title_raw).strip()
        
        toc_raw = toc_match.group(1).strip()
        rest_raw = protected[toc_match.end():].strip()
        # Clean leading --- from rest
        rest_raw = re.sub(r"^---\s*", "", rest_raw).strip()
    else:
        title_raw = ""
        toc_raw = ""
        rest_raw = protected

    # Step 3: Split into individual chapters and wrap in semantic <section class="chapter-container">
    md_extensions = ["tables", "fenced_code"]
    title_html = markdown.markdown(title_raw, extensions=md_extensions)
    toc_html = markdown.markdown(toc_raw, extensions=md_extensions)

    chapter_splits = re.split(r'(?m)(?=^##\s+\d+\.\s+)', rest_raw)
    chapters_html_list = []
    for ch_raw in chapter_splits:
        ch_raw = ch_raw.strip()
        if not ch_raw:
            continue
        ch_m = re.match(r'^##\s+(\d+)\.\s+(.*)$', ch_raw, flags=re.MULTILINE)
        if ch_m:
            ch_num = ch_m.group(1)
            ch_title = ch_m.group(2)
            slug = re.sub(r"[^\w\- ]", "", f"{ch_num} {ch_title}").strip().lower().replace(" ", "-")
            ch_body_tagged = re.sub(
                r'^##\s+' + re.escape(ch_num) + r'\.\s+' + re.escape(ch_title) + r'$',
                f'<h2 id="{slug}" class="chapter-heading">{ch_num}. {ch_title}</h2>',
                ch_raw,
                count=1,
                flags=re.MULTILINE
            )
            ch_html = markdown.markdown(ch_body_tagged, extensions=md_extensions)
            chapters_html_list.append(f'<section class="chapter-container" id="chapter-{ch_num}">\n{ch_html}\n</section>')
        else:
            ch_html = markdown.markdown(ch_raw, extensions=md_extensions)
            chapters_html_list.append(f'<div class="extra-section">\n{ch_html}\n</div>')

    chapters_html = "\n\n".join(chapters_html_list)

    # Step 4: Inside Front Cover (Colophon / Publication Details)
    # Serves as Page 2 (Verso / Even page) in duplex printing, ensuring TOC starts on Page 3 (Recto / Odd page)
    colophon_html = """<div class="inside-cover-page">
  <div class="colophon">
    <div class="colophon-title">NuTrackN Official User Manual</div>
    <div class="colophon-meta">Release Version 2.0.1 (2026 Edition)</div>

    <div class="colophon-block">
      <strong>Nuclear Gamma-Ray &amp; Particle Spectroscopy Analysis Environment</strong><br>
      High-performance analysis software designed for experimental nuclear physics research, in-beam drift tracking, and coincidence matrix analysis.
    </div>

    <div class="colophon-block">
      <strong>Author &amp; Maintainer:</strong> Lucian Stan (<a href="mailto:lucian.stan@nipne.ro">lucian.stan@nipne.ro</a>)<br>
      <strong>Affiliation:</strong> Horia Hulubei National Institute for R&amp;D in Physics and Nuclear Engineering (IFIN-HH / NIPNE)<br>
      Department of Nuclear Physics • Tandem Accelerators Complex • Măgurele, Romania
    </div>

    <div class="colophon-block">
      <strong>Open-Source Software License:</strong> GNU General Public License v3.0 (GPLv3)<br>
      <strong>Official Repository:</strong> <a href="https://github.com/standlucian/NuGASP">https://github.com/standlucian/NuGASP</a><br>
      <strong>Algorithmic Heritage:</strong> GASPware &amp; Xtrackn spectrometer analysis packages (D. Bazzacco, INFN Legnaro / Padova).
    </div>

    <div class="colophon-block">
      <strong>Standard Academic Citation:</strong><br>
      L. Stan, <em>NuTrackN: Modern Nuclear Gamma-Ray and Particle Spectroscopy Analysis Environment</em>, Version 2.0.1, IFIN-HH / NIPNE, GitHub: <a href="https://github.com/standlucian/NuGASP">standlucian/NuGASP</a> (2026).
    </div>

    <div class="colophon-notice">
      <em>This page is intentionally placed on the verso (even-numbered) leaf for double-sided (duplex) book printing.</em>
    </div>
  </div>
</div>"""

    # Blank verso page after Table of Contents (Page 4), ensuring Chapter 1 begins strictly on Page 5 (Recto / Odd)
    toc_blank_verso_html = """<div class="toc-blank-verso">
  <p><em>This page intentionally left blank for double-sided (duplex) printing.</em></p>
</div>"""

    # Combine with appropriate wrapper divs
    combined_html = (
        f'<div class="title-page">\n{title_html}\n</div>\n\n'
        f'{colophon_html}\n\n'
        f'<div class="toc-section" id="table-of-contents">\n{toc_html}\n</div>\n\n'
        f'{toc_blank_verso_html}\n\n'
        f'{chapters_html}\n'
    )

    # Step 5: Restore LaTeX math
    def restore_math(m):
        idx = int(m.group(1))
        return math_store[idx]

    restored_body = re.sub(r"KZMATHPLACEHOLDER(\d+)ENDKZ", restore_math, combined_html)

    # Step 6: Convert GitHub style alert blockquotes
    def convert_alerts(html_content):
        def process_blockquote(bq_match):
            bq_inner = bq_match.group(1)
            if not re.search(r'\[!(NOTE|TIP|IMPORTANT|WARNING|CAUTION)\]', bq_inner):
                return f"<blockquote>{bq_inner}</blockquote>"
            
            parts = re.split(r'(?=<p>\s*\[!(?:NOTE|TIP|IMPORTANT|WARNING|CAUTION)\])', bq_inner)
            output = []
            for part in parts:
                part = part.strip()
                if not part:
                    continue
                m = re.match(r'^<p>\s*\[!(NOTE|TIP|IMPORTANT|WARNING|CAUTION)\]\s*([\s\S]*)$', part)
                if m:
                    alert_type = m.group(1).upper()
                    alert_class = f"alert alert-{alert_type.lower()}"
                    rest = m.group(2)
                    output.append(f'<div class="{alert_class}"><div class="alert-title">{alert_type}</div>\n<p>{rest}\n</div>')
                else:
                    output.append(f"<blockquote>\n{part}\n</blockquote>")
            return "\n\n".join(output)

        return re.sub(r'<blockquote>([\s\S]*?)</blockquote>', process_blockquote, html_content)

    restored_body = convert_alerts(restored_body)

    # Step 7: Construct complete print-ready HTML document
    full_html = f"""<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>NuTrackN User Manual (Version 2.0.1)</title>
  
  <!-- KaTeX for rendering formulas -->
  <link rel="stylesheet" href="https://cdn.jsdelivr.net/npm/katex@0.16.8/dist/katex.min.css">
  <script defer src="https://cdn.jsdelivr.net/npm/katex@0.16.8/dist/katex.min.js"></script>
  <script defer src="https://cdn.jsdelivr.net/npm/katex@0.16.8/dist/contrib/auto-render.min.js"
          onload="renderMathInElement(document.body, {{
            delimiters: [
              {{left: '$$', right: '$$', display: true}},
              {{left: '$', right: '$', display: false}}
            ],
            throwOnError: false
          }});"></script>

  <style>
    /* ----------------------------------------------------
       BASE TYPOGRAPHY & COLORS
       ---------------------------------------------------- */
    :root {{
      --text-color: #1e293b;
      --bg-color: #ffffff;
      --heading-color: #0f172a;
      --accent-color: #0284c7;
      --border-color: #cbd5e1;
      --table-stripe: #f8fafc;
      --code-bg: #f1f5f9;
    }}

    body {{
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Helvetica Neue", Arial, sans-serif;
      font-size: 11pt;
      line-height: 1.6;
      color: var(--text-color);
      background-color: var(--bg-color);
      margin: 0;
      padding: 0;
    }}

    .container {{
      max-width: 880px;
      margin: 0 auto;
      padding: 40px 30px;
    }}

    /* ----------------------------------------------------
       HEADINGS & SECTIONS
       ---------------------------------------------------- */
    h1, h2, h3, h4, h5, h6 {{
      color: var(--heading-color);
      font-weight: 700;
      margin-top: 1.5em;
      margin-bottom: 0.6em;
      line-height: 1.3;
    }}

    h1 {{
      font-size: 26pt;
      border-bottom: 2px solid var(--accent-color);
      padding-bottom: 0.3em;
      margin-top: 0;
    }}

    h2 {{
      font-size: 18pt;
      border-bottom: 1px solid var(--border-color);
      padding-bottom: 0.25em;
      margin-top: 2em;
    }}

    h3 {{
      font-size: 14pt;
      color: #0369a1;
      margin-top: 1.5em;
    }}

    h4 {{
      font-size: 12pt;
      color: #334155;
      margin-top: 1.2em;
    }}

    p {{
      margin: 0.8em 0;
    }}

    /* ----------------------------------------------------
       TITLE PAGE STYLING
       ---------------------------------------------------- */
    .title-page {{
      text-align: center;
      padding: 60px 20px 40px 20px;
      margin-bottom: 30px;
    }}

    .title-page h1 {{
      font-size: 28pt;
      color: #0f172a;
      border-bottom: 3px solid var(--accent-color);
      padding-bottom: 16px;
      margin-bottom: 24px;
    }}

    .title-page p {{
      font-size: 12.5pt;
      color: #475569;
      margin: 10px 0;
    }}

    /* ----------------------------------------------------
       INSIDE COVER / COLOPHON STYLING
       ---------------------------------------------------- */
    .inside-cover-page {{
      background-color: #f8fafc;
      border: 1px solid #e2e8f0;
      border-radius: 8px;
      padding: 30px 40px;
      margin: 30px 0 50px 0;
      color: #475569;
      font-size: 9.5pt;
      line-height: 1.6;
    }}

    .colophon-title {{
      font-size: 14pt;
      font-weight: 700;
      color: #0f172a;
      margin-bottom: 4px;
    }}

    .colophon-meta {{
      font-size: 10pt;
      font-weight: 600;
      color: #0284c7;
      margin-bottom: 16px;
    }}

    .colophon-block {{
      margin-bottom: 14px;
    }}

    .colophon-notice {{
      margin-top: 24px;
      font-style: italic;
      color: #94a3b8;
      font-size: 8.5pt;
      border-top: 1px dashed #cbd5e1;
      padding-top: 10px;
    }}

    .toc-blank-verso {{
      display: none;
    }}

    /* ----------------------------------------------------
       TABLE OF CONTENTS STYLING
       ---------------------------------------------------- */
    .toc-section {{
      background-color: #f8fafc;
      border: 1px solid #e2e8f0;
      border-radius: 8px;
      padding: 30px 40px;
      margin: 30px 0 50px 0;
    }}

    .toc-section h2 {{
      margin-top: 0;
      border-bottom: 2px solid var(--accent-color);
      color: #0f172a;
      padding-bottom: 10px;
    }}

    .toc-section ul {{
      list-style-type: none;
      padding-left: 20px;
      margin: 4px 0;
    }}

    .toc-section > ul {{
      padding-left: 0;
    }}

    .toc-section li {{
      margin: 5px 0;
      line-height: 1.45;
    }}

    .toc-section a {{
      color: #0284c7;
      text-decoration: none;
      font-weight: 500;
    }}

    .toc-section a:hover {{
      text-decoration: underline;
    }}

    /* ----------------------------------------------------
       TABLES
       ---------------------------------------------------- */
    table {{
      width: 100%;
      border-collapse: collapse;
      margin: 1.2em 0;
      font-size: 10pt;
    }}

    th, td {{
      padding: 8px 12px;
      border: 1px solid var(--border-color);
      text-align: left;
    }}

    th {{
      background-color: #f1f5f9;
      font-weight: 700;
      color: #1e293b;
    }}

    tr:nth-child(even) {{
      background-color: var(--table-stripe);
    }}

    /* ----------------------------------------------------
       CODE & PRE
       ---------------------------------------------------- */
    code {{
      font-family: "SFMono-Regular", Consolas, "Liberation Mono", Menlo, Courier, monospace;
      font-size: 9.5pt;
      background-color: var(--code-bg);
      padding: 2px 5px;
      border-radius: 4px;
      color: #0f172a;
      border: 1px solid #e2e8f0;
    }}

    pre {{
      background-color: #0f172a;
      color: #f8fafc;
      padding: 14px 18px;
      border-radius: 6px;
      overflow-x: auto;
      font-size: 9.5pt;
      line-height: 1.45;
    }}

    pre code {{
      background-color: transparent;
      color: inherit;
      border: none;
      padding: 0;
    }}

    /* ----------------------------------------------------
       ALERTS & CALLOUTS
       ---------------------------------------------------- */
    .alert {{
      border-left: 4px solid #94a3b8;
      background-color: #f8fafc;
      padding: 12px 18px;
      margin: 1.2em 0;
      border-radius: 0 6px 6px 0;
    }}

    .alert-title {{
      font-weight: bold;
      text-transform: uppercase;
      font-size: 9pt;
      letter-spacing: 0.5px;
      margin-bottom: 4px;
    }}

    .alert-note {{
      border-left-color: #3b82f6;
      background-color: #eff6ff;
    }}
    .alert-note .alert-title {{ color: #1d4ed8; }}

    .alert-tip {{
      border-left-color: #10b981;
      background-color: #ecfdf5;
    }}
    .alert-tip .alert-title {{ color: #047857; }}

    .alert-important {{
      border-left-color: #8b5cf6;
      background-color: #f5f3ff;
    }}
    .alert-important .alert-title {{ color: #6d28d9; }}

    .alert-warning {{
      border-left-color: #f59e0b;
      background-color: #fffbeb;
    }}
    .alert-warning .alert-title {{ color: #b45309; }}

    .alert-caution {{
      border-left-color: #ef4444;
      background-color: #fef2f2;
    }}
    .alert-caution .alert-title {{ color: #b91c1c; }}

    blockquote {{
      border-left: 4px solid var(--border-color);
      margin: 1em 0;
      padding: 8px 16px;
      color: #475569;
      background-color: #f8fafc;
    }}

    /* ----------------------------------------------------
       PRINT & PAGE BREAK RULES (RECTO / DUPLEX BOOK LAYOUT)
       ---------------------------------------------------- */
    @media print {{
      @page {{
        size: A4;
        margin: 18mm 15mm 20mm 15mm;
      }}

      /* Facing page margins & running footers for duplex printing */
      @page :left {{
        margin-left: 15mm;
        margin-right: 20mm;
        @bottom-left {{
          content: counter(page);
          font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
          font-size: 8.5pt;
          color: #64748b;
        }}
        @bottom-right {{
          content: "NuTrackN User Manual (v2.0.1)";
          font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
          font-size: 8pt;
          color: #94a3b8;
        }}
      }}

      @page :right {{
        margin-left: 20mm;
        margin-right: 15mm;
        @bottom-right {{
          content: counter(page);
          font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
          font-size: 8.5pt;
          color: #64748b;
        }}
        @bottom-left {{
          content: "NuTrackN User Manual (v2.0.1)";
          font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
          font-size: 8pt;
          color: #94a3b8;
        }}
      }}

      @page :blank {{
        @bottom-left {{ content: ""; }}
        @bottom-right {{ content: ""; }}
        @top-left {{ content: ""; }}
        @top-right {{ content: ""; }}
      }}

      body {{
        font-size: 10pt;
        line-height: 1.5;
        background-color: #ffffff;
      }}

      .container {{
        max-width: 100%;
        padding: 0;
      }}

      /* Cover / Title Page: Page 1 (Recto / Odd) */
      .title-page {{
        page-break-before: avoid;
        break-before: avoid;
        page-break-after: always;
        break-after: page;
        padding-top: 40mm;
        padding-bottom: 20mm;
        text-align: center;
        box-sizing: border-box;
      }}

      /* Inside Front Cover / Colophon: Page 2 (Verso / Even) */
      .inside-cover-page {{
        page-break-before: avoid;
        break-before: avoid;
        page-break-after: auto;
        break-after: auto;
        padding-top: 90mm;
        padding-bottom: 15mm;
        background: none;
        border: none;
        box-sizing: border-box;
      }}

      /* Table of Contents: Page 3 (Recto / Odd) formatted in 2 columns */
      .toc-section {{
        page-break-before: always;
        break-before: page;
        page-break-after: auto;
        break-after: auto;
        background-color: transparent;
        border: none;
        padding: 0;
        margin: 0;
      }}

      .toc-section h2 {{
        margin-top: 0;
        margin-bottom: 12px;
        font-size: 16pt;
        border-bottom: 2px solid var(--accent-color);
        padding-bottom: 6px;
      }}

      .toc-section > ul {{
        column-count: 2;
        column-gap: 10mm;
        column-rule: 1px solid #e2e8f0;
        padding-left: 0;
        margin: 0;
        list-style: none;
      }}

      .toc-section > ul > li {{
        break-inside: avoid;
        page-break-inside: avoid;
        margin-bottom: 7px;
        font-size: 8.5pt;
        line-height: 1.3;
        font-weight: 600;
      }}

      .toc-section ul ul {{
        padding-left: 12px;
        margin-top: 2px;
        margin-bottom: 4px;
        list-style: none;
      }}

      .toc-section ul ul li {{
        margin: 1.5px 0;
        font-size: 7.8pt;
        line-height: 1.25;
        font-weight: normal;
      }}

      /* Blank Verso Leaf after TOC: Page 4 (Verso / Even) */
      .toc-blank-verso {{
        display: block !important;
        page-break-before: always;
        break-before: page;
        page-break-after: auto;
        break-after: auto;
        text-align: center;
        color: #94a3b8;
        font-size: 8.5pt;
        font-style: italic;
        padding-top: 120mm;
        box-sizing: border-box;
      }}

      /* Every Chapter begins strictly on a new page (Page 5, Page 11, Page 19, Page 21) */
      .chapter-container {{
        page-break-before: always;
        break-before: page;
        page-break-before: right;
        break-before: right;
        break-before: recto;
      }}

      .chapter-heading {{
        padding-top: 10mm;
      }}

      /* Avoid awkward heading splits */
      h1, h2, h3, h4 {{
        page-break-after: avoid;
        break-after: avoid;
      }}

      /* Avoid breaking inside table rows, alerts, or code blocks */
      table, pre, blockquote, .alert, .katex-display {{
        page-break-inside: avoid;
        break-inside: avoid;
      }}

      /* Clean link styling for print */
      a {{
        color: #0f172a;
        text-decoration: none;
      }}
    }}
  </style>
</head>
<body>
  <div class="container">
    {restored_body}
  </div>
</body>
</html>
"""

    with open(HTML_PATH, "w", encoding="utf-8") as f:
        f.write(full_html)

    print(f"Successfully generated print-ready manual at:\n  {HTML_PATH}")

if __name__ == "__main__":
    build_html()
