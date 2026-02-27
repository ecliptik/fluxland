"""HTML and JSON test report generator for fluxland UI tests.

Generates human-readable HTML reports and machine-readable JSON summaries
from pytest test results. Integrated via conftest.py hooks.
"""

import json
import os
import time
from dataclasses import dataclass, field
from datetime import datetime
from pathlib import Path


@dataclass
class TestResult:
    """Result of a single test."""
    nodeid: str
    outcome: str  # "passed", "failed", "skipped"
    duration: float = 0.0
    message: str = ""


@dataclass
class TestReport:
    """Collection of test results for a run."""
    results: list = field(default_factory=list)
    start_time: float = 0.0
    end_time: float = 0.0

    @property
    def passed(self):
        return sum(1 for r in self.results if r.outcome == "passed")

    @property
    def failed(self):
        return sum(1 for r in self.results if r.outcome == "failed")

    @property
    def skipped(self):
        return sum(1 for r in self.results if r.outcome == "skipped")

    @property
    def total(self):
        return len(self.results)

    @property
    def duration(self):
        return self.end_time - self.start_time


class ReportGenerator:
    """Generate HTML and JSON reports from test results."""

    def __init__(self, results_dir):
        self.results_dir = Path(results_dir)
        self.results_dir.mkdir(parents=True, exist_ok=True)

    def write_json(self, report):
        """Write machine-readable JSON report."""
        data = {
            "timestamp": datetime.fromtimestamp(report.start_time).isoformat(),
            "duration_seconds": round(report.duration, 2),
            "summary": {
                "total": report.total,
                "passed": report.passed,
                "failed": report.failed,
                "skipped": report.skipped,
            },
            "tests": [
                {
                    "nodeid": r.nodeid,
                    "outcome": r.outcome,
                    "duration": round(r.duration, 3),
                    "message": r.message,
                }
                for r in report.results
            ],
        }
        path = self.results_dir / "report.json"
        with open(path, "w") as f:
            json.dump(data, f, indent=2)
        return path

    def write_html(self, report):
        """Write human-readable HTML report."""
        timestamp = datetime.fromtimestamp(report.start_time).strftime(
            "%Y-%m-%d %H:%M:%S"
        )
        duration = f"{report.duration:.1f}s"

        # Group results by file
        by_file = {}
        for r in report.results:
            parts = r.nodeid.split("::", 1)
            filename = parts[0]
            testname = parts[1] if len(parts) > 1 else r.nodeid
            by_file.setdefault(filename, []).append((testname, r))

        rows = []
        for filename in sorted(by_file):
            rows.append(
                f'<tr class="file-header"><td colspan="4">{_esc(filename)}</td></tr>'
            )
            for testname, r in by_file[filename]:
                icon = {"passed": "pass", "failed": "fail", "skipped": "skip"}.get(
                    r.outcome, "?"
                )
                msg_html = ""
                if r.message:
                    msg_html = f'<pre class="msg">{_esc(r.message)}</pre>'
                rows.append(
                    f'<tr class="{r.outcome}">'
                    f"<td>{_esc(testname)}</td>"
                    f'<td class="status">{icon}</td>'
                    f"<td>{r.duration:.2f}s</td>"
                    f"<td>{msg_html}</td>"
                    f"</tr>"
                )

        html = _HTML_TEMPLATE.format(
            timestamp=timestamp,
            duration=duration,
            total=report.total,
            passed=report.passed,
            failed=report.failed,
            skipped=report.skipped,
            rows="\n".join(rows),
        )

        path = self.results_dir / "report.html"
        with open(path, "w") as f:
            f.write(html)
        return path


def _esc(text):
    """Escape HTML special characters."""
    return (
        str(text)
        .replace("&", "&amp;")
        .replace("<", "&lt;")
        .replace(">", "&gt;")
        .replace('"', "&quot;")
    )


_HTML_TEMPLATE = """\
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<title>Fluxland UI Test Report</title>
<style>
body {{ font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Helvetica, Arial, sans-serif; margin: 2em; background: #fafafa; color: #333; }}
h1 {{ margin-bottom: 0.2em; }}
.summary {{ font-size: 1.1em; margin-bottom: 1.5em; color: #555; }}
.summary .pass {{ color: #22863a; font-weight: bold; }}
.summary .fail {{ color: #cb2431; font-weight: bold; }}
.summary .skip {{ color: #b08800; font-weight: bold; }}
table {{ border-collapse: collapse; width: 100%; }}
th, td {{ text-align: left; padding: 6px 12px; border-bottom: 1px solid #e1e4e8; }}
th {{ background: #f6f8fa; font-weight: 600; }}
tr.file-header td {{ background: #f1f3f5; font-weight: 600; font-size: 0.95em; }}
tr.passed td {{ }}
tr.failed td {{ background: #ffeef0; }}
tr.skipped td {{ background: #fffbdd; }}
td.status {{ text-align: center; font-weight: bold; }}
tr.passed td.status {{ color: #22863a; }}
tr.failed td.status {{ color: #cb2431; }}
tr.skipped td.status {{ color: #b08800; }}
pre.msg {{ margin: 0.3em 0 0; font-size: 0.85em; white-space: pre-wrap; max-width: 600px; }}
</style>
</head>
<body>
<h1>Fluxland UI Test Report</h1>
<p class="summary">
  {timestamp} &mdash; {duration}<br>
  <span class="pass">{passed} passed</span> &middot;
  <span class="fail">{failed} failed</span> &middot;
  <span class="skip">{skipped} skipped</span> &middot;
  {total} total
</p>
<table>
<thead><tr><th>Test</th><th>Status</th><th>Duration</th><th>Details</th></tr></thead>
<tbody>
{rows}
</tbody>
</table>
</body>
</html>
"""
