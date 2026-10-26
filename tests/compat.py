#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""compat.py - Fluxbox parity: every name Fluxbox knows, checked by fluxland.

Each command, init resource, apps attribute and menu tag Fluxbox has
(tests/fixtures/fluxbox/, from its source by tools/fluxbox-names.sh) goes
into a file of that kind, and `fluxland --check-config` reads them: a name
it warns about is one fluxland doesn't support.

  tests/compat.py FLUXLAND               fail if any isn't supported
  tests/compat.py FLUXLAND --markdown F  write the compatibility matrix
"""

import os
import re
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
FIXTURES = os.path.join(HERE, 'fixtures', 'fluxbox')
SOURCE = os.path.dirname(HERE)


def names(fixture):
    """(name, example or None) for each line of a fixture"""
    out = []
    with open(os.path.join(FIXTURES, fixture)) as f:
        for line in f:
            line = line.rstrip('\n')
            if not line or line.startswith('#'):
                continue
            name, _, example = line.partition('\t')
            out.append((name, example or None))
    return out


def saved_init():
    """A saved Fluxbox init (fixtures/fluxbox/init): key -> value"""
    values = {}
    with open(os.path.join(FIXTURES, 'init')) as f:
        for line in f:
            key, sep, value = line.partition(':')
            if sep and key.startswith('session.'):
                values[key.strip()] = value.strip()
    return values


def ignored_init_keys():
    """Fluxbox's keys fluxland accepts and ignores (validate.c VT_IGNORED)"""
    with open(os.path.join(SOURCE, 'src', 'validate.c')) as f:
        return set(re.findall(r'\{"(session\.[^"]+)", VT_IGNORED', f.read()))


def check(fluxland, files):
    """Run fluxland --check-config on {name: text}: {(file, line): message}"""
    with tempfile.TemporaryDirectory() as d:
        for name, text in files.items():
            with open(os.path.join(d, name), 'w') as f:
                f.write(text)
        env = dict(os.environ, FLUXLAND_CONFIG_DIR=d, HOME=d)
        run = subprocess.run([fluxland, '--check-config'], env=env,
                             capture_output=True, text=True)
        out = run.stdout + run.stderr
        found = {}
        for m in re.finditer(r'^%s/(\w+):(\d+): (?:warning|error): (.*)$'
                             % re.escape(d), out, re.M):
            found[(m[1], int(m[2]))] = m[3]
        return found


MISSING = re.compile(r"unknown|isn't supported|not supported|malformed")


def verdict(bad, where, name):
    """'no' for a name fluxland doesn't know; another complaint is an
    example of ours that is wrong, which fails the test"""
    msg = bad.get(where)
    if msg is None:
        return 'yes', ''
    if MISSING.search(msg):
        return 'no', msg
    sys.exit('bad example for %s: %s' % (name, msg))


def results(fluxland):
    """{category: [(name, status, note)]}, status 'yes', 'ignored' or 'no'"""
    res = {}

    # Commands, each bound to a key of its own
    cmds = names('commands')
    lines = []
    for i, (name, example) in enumerate(cmds):
        lines.append('Mod4 Control Mod1 F%d F%d :%s'
                     % (i % 12 + 1, i // 12 + 1, example or name))
    # Every file, or fluxland reads ~/.fluxbox's
    bad = check(fluxland, {'keys': '\n'.join(lines) + '\n', 'init': ''})
    res['commands'] = [(n,) + verdict(bad, ('keys', i + 1), n)
                       for i, (n, _) in enumerate(cmds)]

    # init resources: Fluxbox's, and those of a saved init
    saved = saved_init()
    keys = sorted(set(n for n, _ in names('init-keys')) | set(saved),
                  key=str.lower)
    ignored = ignored_init_keys()
    # (the files a saved init names, as files that are there)
    def value(k):
        v = saved.get(k, 'true')
        return '/dev/null' if '/' in v else v
    text = ''.join('%s: %s\n' % (k, value(k)) for k in keys)
    bad = check(fluxland, {'init': text})
    res['init'] = []
    for i, k in enumerate(keys):
        status, note = verdict(bad, ('init', i + 1), k)
        if status == 'no':
            res['init'].append((k, status, note))
        elif k in ignored:
            res['init'].append((k, 'ignored', 'meaningless on Wayland'))
        else:
            res['init'].append((k, 'yes', ''))

    # apps: the attributes in an [app] block, the sections as blocks
    sections = ('app', 'transient', 'group', 'startup')
    lines, where = [], {}
    for name, example in names('apps-keys'):
        if name in sections:
            continue
        if not lines:
            lines.append('[app] (name=xterm)')
        lines.append('  ' + example)
        where[name] = len(lines)
    lines.append('[end]')
    for name, example in names('apps-keys'):
        if name not in sections:
            continue
        lines.append(example)
        where[name] = len(lines)
        if name != 'startup':
            lines += ['  [Sticky] {yes}', '[end]']
    bad = check(fluxland, {'apps': '\n'.join(lines) + '\n', 'init': ''})
    res['apps'] = [(n,) + verdict(bad, ('apps', where[n]), n)
                   for n, _ in names('apps-keys')]

    # menu tags, between [begin] and [end]
    lines, where = ['[begin] (Menu)'], {}
    for name, example in names('menu-tags'):
        lines.append(example)
        where[name] = len(lines)
        if name == 'submenu':
            lines += ['[exec] (Terminal) {xterm}', '[end]']
        elif name == 'encoding':
            lines.append('[endencoding]')
    lines.append('[end]')
    bad = check(fluxland, {'menu': '\n'.join(lines) + '\n', 'init': '',
                           'included': '[exec] (Terminal) {xterm}\n'})
    res['menu'] = [(n,) + verdict(bad, ('menu', where[n]), n)
                   for n, _ in names('menu-tags')]
    return res


TITLES = {
    'commands': ('Commands (keys, menus, fluxland-ctl)', 'command'),
    'init': ('init resources', 'resource'),
    'apps': ('apps attributes and sections', 'attribute'),
    'menu': ('menu tags', 'tag'),
}


def markdown(res):
    out = ['# Fluxbox compatibility', '',
           'What fluxland supports of each Fluxbox file, name by name.', '',
           'Generated by `tests/compat.py`, the parity test: it puts every',
           'name Fluxbox knows (`tests/fixtures/fluxbox/`, from Fluxbox\'s',
           'source by `tools/fluxbox-names.sh`) in a file of its kind and',
           'reads it with `fluxland --check-config`. Regenerate with',
           '`tests/compat.py build/fluxland --markdown docs/COMPATIBILITY.md`.',
           '', '| | Fluxbox has | fluxland supports | ignored | missing |',
           '|---|---|---|---|---|']
    for cat in ('commands', 'init', 'apps', 'menu'):
        rows = res[cat]
        count = {s: sum(1 for r in rows if r[1] == s)
                 for s in ('yes', 'ignored', 'no')}
        out.append('| %s | %d | %d | %d | %d |'
                   % (TITLES[cat][0], len(rows), count['yes'],
                      count['ignored'], count['no']))
    out += ['', '"Ignored" are accepted and have no effect: they mean nothing',
            'on Wayland (X11 colour depth, pixmap caches...).', '',
            'Styles: Fluxbox styles are read as Fluxbox reads them (its',
            'resource lookup, textures, fonts, pixmaps); fluxland ships',
            'Fluxbox\'s own styles. See [STYLES.md](STYLES.md).', '']
    for cat in ('commands', 'init', 'apps', 'menu'):
        title, what = TITLES[cat]
        out += ['## ' + title, '', '| %s | fluxland |' % what, '|---|---|']
        for name, status, note in res[cat]:
            mark = {'yes': 'yes', 'ignored': 'ignored', 'no': 'no'}[status]
            if note and status != 'yes':
                mark += ' (%s)' % note
            out.append('| `%s` | %s |' % (name, mark))
        out.append('')
    return '\n'.join(out)


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    res = results(sys.argv[1])
    if '--markdown' in sys.argv:
        path = sys.argv[sys.argv.index('--markdown') + 1]
        with open(path, 'w') as f:
            f.write(markdown(res))
    missing = [(c, n, note) for c, rows in res.items()
               for n, s, note in rows if s == 'no']
    total = sum(len(rows) for rows in res.values())
    for c, n, note in missing:
        print('missing %s %s: %s' % (c, n, note))
    print('%d of %d Fluxbox names supported (or ignored on Wayland)'
          % (total - len(missing), total))
    sys.exit(1 if missing else 0)


if __name__ == '__main__':
    main()
