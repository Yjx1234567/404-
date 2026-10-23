#!/usr/bin/env python3
"""Start an existing Linux OJ deployment without resetting its configuration or data."""
from __future__ import annotations

import argparse
import contextlib
import hashlib
import html
from html.parser import HTMLParser
import json
import os
from pathlib import Path
import re
import signal
import socket
import subprocess
import sys
import time
from dataclasses import dataclass
from urllib.error import HTTPError, URLError
from urllib.parse import parse_qsl, quote, unquote, urlencode, urlsplit, urlunsplit
from urllib.request import ProxyHandler, build_opener


class LaunchError(RuntimeError):
    pass


def log(message):
    print(message, file=sys.stderr, flush=True)


def digest(path):
    with Path(path).open('rb') as handle:
        return hashlib.sha256(handle.read()).hexdigest()


def within(path, root):
    path, root = Path(path).resolve(), Path(root).resolve()
    if path != root and root not in path.parents:
        raise LaunchError(f'路径超出运行目录，已停止操作：{path}')
    return path


def validate_runtime(runtime):
    runtime = Path(runtime).resolve()
    protected = {Path(runtime.anchor), Path.home().resolve()}
    if sys.platform == 'linux':
        protected.update(Path(path).resolve() for path in ('/home', '/root', '/tmp', '/var', '/usr', '/etc', '/opt', '/bin', '/sbin', '/dev', '/proc', '/sys', '/run'))
    if runtime in protected:
        raise LaunchError(f'运行目录不能是系统目录或用户主目录：{runtime}；请使用独立的 oj_platform 子目录。')
    return runtime


def yaml_scalar(raw, line):
    """Only the project's flat section/scalar YAML format is accepted."""
    raw = raw.strip()
    if not raw:
        raise LaunchError(f'配置第 {line} 行缺少标量值。')
    if raw.startswith('"'):
        try:
            value, end = json.JSONDecoder().raw_decode(raw)
        except ValueError as exc:
            raise LaunchError(f'配置第 {line} 行的双引号字符串无法解析。') from exc
        tail = raw[end:].strip()
        if not isinstance(value, str) or (tail and not tail.startswith('#')):
            raise LaunchError(f'配置第 {line} 行使用了不支持的 YAML 格式。')
        return value
    if raw.startswith("'"):
        match = re.fullmatch(r"'((?:[^']|'')*)'\s*(?:#.*)?", raw)
        if not match:
            raise LaunchError(f'配置第 {line} 行的单引号字符串无法解析。')
        return match.group(1).replace("''", "'")
    raw = re.split(r'\s+#', raw, maxsplit=1)[0].rstrip()
    if raw[0] in '[{&*!|>@`' or raw in ('null', '~') or ': ' in raw:
        raise LaunchError(f'配置第 {line} 行使用了不支持的 YAML 格式；请使用简单的分组和标量。')
    if re.fullmatch(r'[+-]?\d+', raw):
        return int(raw)
    if raw.lower() in ('true', 'false'):
        return raw.lower() == 'true'
    return raw


def parse_config(path):
    result, section = {}, None
    for number, line in enumerate(Path(path).read_text(encoding='utf-8-sig').splitlines(), 1):
        if not line.strip() or line.lstrip().startswith('#'):
            continue
        if '\t' in line[:len(line) - len(line.lstrip())]:
            raise LaunchError(f'配置第 {number} 行含制表符缩进，无法安全解析。')
        heading = re.fullmatch(r'([A-Za-z_]\w*):\s*(?:#.*)?', line)
        if heading:
            section = heading.group(1)
            if section in result:
                raise LaunchError(f'配置分组重复：{section}')
            result[section] = {}
            continue
        value = re.fullmatch(r'  ([A-Za-z_]\w*):\s*(.+)', line)
        if not value or section is None:
            raise LaunchError(f'配置第 {number} 行格式不受支持；请保留两空格的简单 YAML 标量格式。')
        key = value.group(1)
        if key in result[section]:
            raise LaunchError(f'配置项重复：{section}.{key}')
        result[section][key] = yaml_scalar(value.group(2), number)
    return result


@dataclass(frozen=True)
class Settings:
    runtime: Path
    port: int
    public: Path
    db_host: str
    db_port: int
    db_user: str
    db_password: str
    db_name: str

    @property
    def base_url(self):
        return f'http://localhost:{self.port}'


def read_settings(runtime):
    runtime = Path(runtime).resolve()
    path = runtime / 'config/config.yaml'
    if not path.is_file():
        raise LaunchError(f'运行配置不存在：{path}；请先完成首次部署。')
    config = parse_config(path)
    server, database = config.get('server', {}), config.get('database', {})
    def number(group, key, default):
        value = group.get(key, default)
        if type(value) is not int or not 1 <= value <= 65535:
            raise LaunchError(f'配置端口 {key} 必须是 1 到 65535 的整数。')
        return value
    def text(group, key, default):
        value = group.get(key, default)
        if not isinstance(value, str) or '\x00' in value or '\n' in value:
            raise LaunchError(f'配置项 {key} 必须是单行字符串。')
        return value
    public_raw = text(server, 'public_dir', './public')
    if not public_raw:
        raise LaunchError('server.public_dir 不能为空。')
    public = within(runtime / public_raw, runtime)
    protected = {'build', 'config', 'src', 'third_party', 'tools', 'tests', 'database', '.launcher'}
    if public == runtime or public.relative_to(runtime).parts[0] in protected:
        raise LaunchError('静态目录不能覆盖运行根目录、配置、源码、构建或启动日志目录。')
    return Settings(runtime, number(server, 'port', 8080), public,
                    text(database, 'host', 'localhost'), number(database, 'port', 3306),
                    text(database, 'username', 'root'), text(database, 'password', ''),
                    text(database, 'database', 'oj_system'))


def write_changed(path, content, runtime):
    path = within(path, runtime)
    if path.exists() and path.read_bytes() == content:
        return False
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(content)
    # Set a current mtime even when the source was copied with an old timestamp.
    os.utime(path, None)
    return True


ASSET_REF = re.compile(r'''(?P<prefix>\b(?:src|href|action)\s*=\s*)(?P<quote>["'])(?P<url>[^"']+)(?P=quote)''', re.I)
VERSION_META = re.compile(r'''<meta\b(?=[^>]*\bname\s*=\s*["']app-version["'])[^>]*>\s*''', re.I)
CACHE_DIRECTORIES = {'__pycache__', '.cache', 'cache', '.pytest_cache', '.mypy_cache', '.ruff_cache', 'node_modules', '.git', '.launcher'}


def cached_path(relative):
    return any(part.lower() in CACHE_DIRECTORIES for part in Path(relative).parts)


def public_release(source_public):
    """Hash source bytes, never the HTML rewritten in the runtime deployment."""
    root = Path(source_public).resolve()
    if not root.is_dir():
        raise LaunchError(f'源静态目录不存在：{root}')
    release = hashlib.sha256()
    for path in sorted(root.rglob('*'), key=lambda item: item.relative_to(root).as_posix()):
        relative = path.relative_to(root)
        if cached_path(relative) or path.suffix.lower() not in ('.html', '.css', '.js'):
            continue
        if path.is_symlink():
            raise LaunchError(f'静态资源不能使用符号链接：{path}')
        if path.is_file():
            name, content = relative.as_posix().encode('utf-8'), path.read_bytes()
            release.update(str(len(name)).encode('ascii') + b':' + name)
            release.update(str(len(content)).encode('ascii') + b':' + content)
    return release.hexdigest()[:16]


def local_reference(root, html_relative, url):
    url = urlsplit(url)
    if url.scheme or url.netloc or not url.path:
        return None
    relative = unquote(url.path)
    path = root / relative.lstrip('/') if relative.startswith('/') else root / Path(html_relative).parent / relative
    return within(path, root)


def versioned_html(content, html_relative, source_public, release=None):
    text = content.decode('utf-8')
    root = Path(source_public).resolve()
    release = release or public_release(root)
    def replace(match):
        url = urlsplit(html.unescape(match.group('url')))
        if url.scheme or url.netloc or not url.path.lower().endswith(('.css', '.js', '.html')):
            return match.group(0)
        asset = local_reference(root, html_relative, urlunsplit(url))
        if not asset.is_file():
            raise LaunchError(f'页面引用的本地资源不存在：{asset}')
        query = [(key, value) for key, value in parse_qsl(url.query, keep_blank_values=True) if key != 'v']
        query.append(('v', release if url.path.lower().endswith('.html') else digest(asset)[:16]))
        result = urlunsplit(('', '', url.path, urlencode(query), url.fragment))
        return match.group('prefix') + match.group('quote') + html.escape(result, quote=True) + match.group('quote')
    text = ASSET_REF.sub(replace, text)
    text = VERSION_META.sub('', text)
    marker = f'<meta name="app-version" content="{release}">'
    heading = re.search(r'<head\b[^>]*>', text, re.I)
    if heading:
        text = text[:heading.end()] + '\n    ' + marker + text[heading.end():]
    else:
        raise LaunchError(f'页面缺少 head，无法注入版本标记：{html_relative}')
    return text.encode('utf-8')


def sync_source(source, settings, release=None):
    source = Path(source).resolve()
    release = release or public_release(source / 'public')
    build_changed = False
    changed = 0
    for directory in ('public', 'src', 'third_party', 'tools', 'tests', 'database'):
        source_dir = source / directory
        if not source_dir.is_dir():
            raise LaunchError(f'源代码目录缺少 {directory}，请检查 --source。')
        destination = settings.public if directory == 'public' else settings.runtime / directory
        for path in sorted(source_dir.rglob('*')):
            if cached_path(path.relative_to(source_dir)):
                continue
            if path.is_symlink():
                raise LaunchError(f'为避免写入其他目录，不同步符号链接：{path}')
            if not path.is_file() or '__pycache__' in path.parts or path.suffix == '.pyc':
                continue
            relative = path.relative_to(source_dir)
            content = path.read_bytes()
            if directory == 'public' and path.suffix.lower() == '.html':
                content = versioned_html(content, relative, source_dir, release)
            if write_changed(destination / relative, content, settings.runtime):
                changed += 1
                build_changed |= directory in ('src', 'third_party')
    cmake = source / 'CMakeLists.txt'
    if not cmake.is_file():
        raise LaunchError('源代码目录缺少 CMakeLists.txt。')
    if write_changed(settings.runtime / 'CMakeLists.txt', cmake.read_bytes(), settings.runtime):
        changed += 1
        build_changed = True
    log(f'已同步 {changed} 个变化文件；保留配置、数据库和其他运行文件。')
    return build_changed


def prepare_config(source, runtime):
    target = within(Path(runtime) / 'config/config.yaml', runtime)
    if not target.exists():
        template = Path(source) / 'config/config.yaml'
        if not template.is_file():
            raise LaunchError('没有可用配置，请先完成首次部署。')
        write_changed(target, template.read_bytes(), runtime)
        log('首次复制配置文件；以后启动将保留该运行配置。')


def run_command(args, *, cwd=None, env=None, stdout=None, timeout=120):
    try:
        completed = subprocess.run([str(arg) for arg in args], cwd=cwd, env=env,
                                   stdout=stdout if stdout is not None else subprocess.PIPE,
                                   stderr=subprocess.PIPE, timeout=timeout, check=False)
    except FileNotFoundError as exc:
        raise LaunchError(f'缺少命令 {args[0]}；请先完成首次部署和依赖安装。') from exc
    except subprocess.TimeoutExpired as exc:
        raise LaunchError(f'命令 {args[0]} 执行超时，已停止本次启动。') from exc
    if completed.returncode:
        detail = (completed.stderr or b'').decode('utf-8', errors='replace').strip()[-3000:]
        raise LaunchError(f'{Path(str(args[0])).name} 执行失败（{completed.returncode}）：{detail}')
    return completed.stdout or b''


REQUIRED_COLUMNS = {
    'problems': {'id', 'title', 'difficulty', 'content', 'template', 'created_at', 'solution', 'spj_code'},
    'test_cases': {'id', 'problem_id', 'input', 'expected', 'position'},
    'users': {'id', 'username', 'password', 'role', 'created_at'},
    'sessions': {'id', 'user_id', 'token', 'expires_at', 'created_at'},
    'submissions': {'id', 'user_id', 'problem_id', 'code', 'status', 'passed', 'total', 'time_ms', 'error_info', 'result', 'created_at'},
    'tags': {'id', 'name'}, 'problem_tags': {'problem_id', 'tag_id'},
    'classes': {'id', 'name', 'join_code', 'teacher_id', 'created_at'},
    'class_members': {'class_id', 'user_id', 'joined_at'},
    'assignments': {'id', 'class_id', 'title', 'description', 'due_at', 'created_at'},
    'assignment_problems': {'assignment_id', 'problem_id', 'position'},
}


def mysql_args(settings, program='mysql'):
    args = [program, '--host=' + settings.db_host, '--port=' + str(settings.db_port),
            '--user=' + settings.db_user]
    return args + (['--database=' + settings.db_name] if program == 'mysql' else [settings.db_name])


def mysql_env(settings):
    env = os.environ.copy()
    env.update(MYSQL_HOST=settings.db_host, MYSQL_TCP_PORT=str(settings.db_port),
               MYSQL_USER=settings.db_user, MYSQL_DATABASE=settings.db_name,
               MYSQL_PWD=settings.db_password)
    return env


def query(settings, sql):
    return run_command(mysql_args(settings) + ['--batch', '--skip-column-names', '--execute=' + sql],
                       env=mysql_env(settings), timeout=30).decode('utf-8').strip()


def missing_columns(settings):
    if query(settings, 'SELECT 1') != '1':
        raise LaunchError('数据库连通检查没有返回预期结果。')
    rows = query(settings, 'SELECT TABLE_NAME,COLUMN_NAME FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE()')
    existing = set(tuple(line.split('\t', 1)) for line in rows.splitlines() if '\t' in line)
    return {(table, column) for table, columns in REQUIRED_COLUMNS.items() for column in columns if (table, column) not in existing}


def check_database(settings, readonly=False):
    missing = missing_columns(settings)
    if not missing:
        log('数据库连接及必需表列检查通过。')
        return
    migratable = {('problems', 'solution'), ('problems', 'spj_code')}
    if readonly or not missing <= migratable:
        description = '、'.join(table + '.' + column for table, column in sorted(missing))
        raise LaunchError('数据库缺少必需结构：' + description + '；请先完成首次部署或升级。')
    migration = settings.runtime / 'database/migrate.sh'
    if not migration.is_file():
        raise LaunchError('缺少数据库迁移脚本，未修改数据库。')
    backup_dir = within(settings.runtime / '.launcher/backups', settings.runtime)
    backup_dir.mkdir(parents=True, exist_ok=True)
    backup = backup_dir / ('database-before-migration-' + str(time.time_ns()) + '.sql')
    log('检测到新增列缺失，先完整备份现有数据库。')
    try:
        descriptor = os.open(backup, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
        with os.fdopen(descriptor, 'wb') as handle:
            run_command(mysql_args(settings, 'mysqldump') + ['--single-transaction', '--routines', '--triggers', '--hex-blob', '--no-tablespaces'],
                        env=mysql_env(settings), stdout=handle, timeout=180)
        if backup.stat().st_size == 0:
            raise LaunchError('数据库备份为空，未执行迁移。')
    except (OSError, LaunchError) as exc:
        raise LaunchError(f'数据库备份未成功，未执行迁移。请检查 {backup}：{exc}') from exc
    log(f'数据库备份已保存：{backup}')
    run_command(['bash', migration], cwd=settings.runtime, env=mysql_env(settings), timeout=120)
    if missing_columns(settings):
        raise LaunchError('迁移后表列检查仍未通过；请检查备份和迁移日志。')
    log('仅补齐缺失列，保留所有题目、提交记录和账号。')


def build_binary(settings, changed):
    binary = within(settings.runtime / 'build/oj_server', settings.runtime)
    if not changed and binary.is_file():
        return binary
    log('检测到源码变化或首次构建，正在增量编译。')
    run_command(['cmake', '-S', settings.runtime, '-B', settings.runtime / 'build'], timeout=180)
    run_command(['cmake', '--build', settings.runtime / 'build', '--target', 'oj_server', '-j', str(min(4, os.cpu_count() or 1))], timeout=600)
    if not binary.is_file():
        raise LaunchError('编译未生成 build/oj_server，请检查构建日志。')
    return binary


@dataclass(frozen=True)
class Process:
    pid: int
    start: str


def owned_process(pid, runtime, proc_root=Path('/proc')):
    """Do not trust a pid file, process name, or command-line substring."""
    path = Path(proc_root) / str(pid)
    try:
        exe = os.readlink(path / 'exe').removesuffix(' (deleted)')
        cwd = Path(os.readlink(path / 'cwd')).resolve()
        start = (path / 'stat').read_text().rpartition(') ')[2].split()[19]
        if Path(exe) != Path(runtime).resolve() / 'build/oj_server' or cwd != Path(runtime).resolve():
            return None
        return Process(int(pid), start)
    except (OSError, ValueError, IndexError):
        return None


def project_processes(runtime, proc_root=Path('/proc')):
    try:
        entries = Path(proc_root).iterdir()
        return [process for path in entries if path.name.isdigit()
                for process in [owned_process(path.name, runtime, proc_root)] if process]
    except OSError as exc:
        raise LaunchError('无法读取 /proc，不能安全核实服务进程。') from exc


def stop_process(process, runtime, timeout=35):
    if owned_process(process.pid, runtime) != process:
        raise LaunchError('服务进程已变化，未发送停止信号，请重试。')
    os.kill(process.pid, signal.SIGTERM)
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if owned_process(process.pid, runtime) != process:
            log(f'已正常停止本项目服务（PID {process.pid}）。')
            return
        time.sleep(0.2)
    raise LaunchError('服务正在等待判题任务结束，未强制终止。请稍后再试。')


def listening_inodes(port, proc_root=Path('/proc')):
    inodes = set()
    for table in ('tcp', 'tcp6'):
        try:
            lines = (Path(proc_root) / 'net' / table).read_text().splitlines()[1:]
        except OSError:
            continue
        for line in lines:
            fields = line.split()
            if len(fields) > 9 and fields[3] == '0A' and int(fields[1].rsplit(':', 1)[1], 16) == port:
                inodes.add(fields[9])
    return inodes


def owns_port(process, port, proc_root=Path('/proc')):
    sockets = {'socket:[' + inode + ']' for inode in listening_inodes(port, proc_root)}
    try:
        for fd in (Path(proc_root) / str(process.pid) / 'fd').iterdir():
            try:
                if os.readlink(fd) in sockets:
                    return True
            except OSError:
                pass
    except OSError:
        pass
    return False


def port_occupied(port):
    if listening_inodes(port):
        return True
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as probe:
            probe.bind(('0.0.0.0', port))
        return False
    except OSError:
        return True


def guard_port(settings, processes):
    if port_occupied(settings.port) and not any(owns_port(process, settings.port) for process in processes):
        raise LaunchError(f'端口 {settings.port} 被其他进程占用，未停止任何未知进程。请更换端口或自行关闭占用程序。')


def http_get(url):
    # Local startup must not be redirected through an inherited system proxy.
    opener = build_opener(ProxyHandler({}))
    try:
        with opener.open(url, timeout=2) as response:
            return response.status, response.read()
    except HTTPError as exc:
        return exc.code, exc.read()


class PageReferences(HTMLParser):
    def __init__(self, content):
        super().__init__(convert_charrefs=True)
        self.versions, self.references = [], []
        self.feed(content.decode('utf-8'))

    def handle_starttag(self, tag, attributes):
        attributes = dict(attributes)
        if tag == 'meta' and (attributes.get('name') or '').lower() == 'app-version':
            self.versions.append(attributes.get('content', ''))
        for name in ('href', 'src', 'action'):
            if attributes.get(name):
                self.references.append(attributes[name])

    handle_startendtag = handle_starttag


def page_release(content):
    versions = PageReferences(content).versions
    if len(versions) != 1 or not re.fullmatch(r'[0-9a-f]{16}', versions[0]):
        raise LaunchError('页面缺少唯一有效的 app-version 版本标记，请重新执行启动同步。')
    return versions[0]


def health(settings, expected_release=None):
    try:
        release = expected_release or page_release((settings.public / 'login.html').read_bytes())
        if not re.fullmatch(r'[0-9a-f]{16}', release):
            return False
        # A successful root response alone cannot establish that navigation serves this release.
        status, home = http_get(settings.base_url + '/')
        if status != 200 or home != (settings.public / 'index.html').read_bytes():
            return False
        resources = {}
        required_scripts = {
            'login.html': {'js/api.js', 'js/auth.js'},
            'register.html': {'js/api.js', 'js/auth.js'},
            'problem_list.html': {'js/api.js', 'js/auth.js', 'js/problem.js'},
        }
        for name in ('index.html', 'login.html', 'register.html', 'problem_list.html'):
            local = (settings.public / name).read_bytes()
            status, served = http_get(settings.base_url + '/' + name + '?v=' + release)
            if status != 200 or served != local or page_release(served) != release:
                return False
            page_assets = set()
            for reference in PageReferences(served).references:
                url = urlsplit(reference)
                if url.scheme or url.netloc or not url.path.lower().endswith(('.html', '.css', '.js')):
                    continue
                path = local_reference(settings.public, Path(name), reference)
                if not path.is_file():
                    return False
                values = [value for key, value in parse_qsl(url.query, keep_blank_values=True) if key == 'v']
                expected = release if url.path.lower().endswith('.html') else digest(path)[:16]
                if values != [expected]:
                    return False
                if path.suffix.lower() in ('.css', '.js'):
                    relative = path.relative_to(settings.public).as_posix()
                    resources[relative] = (path, expected)
                    page_assets.add(relative)
            if not required_scripts.get(name, set()) <= page_assets:
                return False
        for relative in ('css/style.css', 'js/api.js', 'js/auth.js', 'js/problem.js'):
            path = settings.public / relative
            resources[relative] = (path, digest(path)[:16])
        for relative, (path, version) in resources.items():
            status, content = http_get(settings.base_url + '/' + quote(relative) + '?v=' + version)
            if status != 200 or content != path.read_bytes():
                return False
        status, body = http_get(settings.base_url + '/api/problems')
        response = json.loads(body)
        if status != 401 or not isinstance(response, dict) or response.get('error') != 'unauthorized':
            return False
        return True
    except (OSError, URLError, ValueError, KeyError, LaunchError):
        return False


def wait_healthy(settings, child, timeout=35, expected_release=None):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if child.poll() is not None:
            raise LaunchError(f'服务提前退出（{child.returncode}），请查看 {settings.runtime / ".launcher/server.log"}。')
        if health(settings, expected_release):
            return
        time.sleep(0.3)
    raise LaunchError(f'服务未通过页面版本、注册导航、脚本、接口权限和样式检查；请查看 {settings.runtime / ".launcher/server.log"}。')


def start_server(settings, binary, release=None):
    launcher = within(settings.runtime / '.launcher', settings.runtime)
    launcher.mkdir(parents=True, exist_ok=True)
    with (launcher / 'server.log').open('ab') as output:
        child = subprocess.Popen([str(binary)], cwd=settings.runtime, stdout=output, stderr=subprocess.STDOUT,
                                 stdin=subprocess.DEVNULL, start_new_session=True, close_fds=True)
    (launcher / 'server.pid').write_text(str(child.pid) + '\n', encoding='ascii')
    try:
        wait_healthy(settings, child, expected_release=release)
        process = owned_process(child.pid, settings.runtime)
        if not process or not owns_port(process, settings.port):
            raise LaunchError('服务端口归属检查未通过，未宣告启动成功。')
        return process
    except (LaunchError, OSError):
        # Only clean up the child we launched, after rechecking its executable/cwd.
        process = owned_process(child.pid, settings.runtime)
        if process:
            try:
                stop_process(process, settings.runtime)
            except (LaunchError, OSError) as exc:
                log('本次服务未能自动退出，请使用 --stop 检查：' + str(exc))
        raise


def ready(settings, expected_release=None):
    login = settings.public / 'login.html'
    if not login.is_file():
        raise LaunchError('缺少登录页，未宣告启动成功。')
    release = page_release(login.read_bytes())
    if expected_release is not None and release != expected_release:
        raise LaunchError('登录页与本次发布版本不一致，未宣告启动成功。')
    print(f'OJ_READY={settings.base_url}/login.html?v={release}', flush=True)


@contextlib.contextmanager
def launch_lock(runtime):
    import fcntl  # Linux/WSL only; import lazily so the pure helpers can be tested elsewhere.
    directory = within(Path(runtime) / '.launcher', runtime)
    directory.mkdir(parents=True, exist_ok=True)
    with (directory / 'launch.lock').open('a') as handle:
        try:
            fcntl.flock(handle, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError as exc:
            raise LaunchError('另一个启动任务正在运行，请等待它完成。') from exc
        yield


def launch(source, runtime, *, stop=False, check=False):
    source, runtime = Path(source).resolve(), validate_runtime(runtime)
    if stop:
        processes = project_processes(runtime)
        for process in processes:
            stop_process(process, runtime)
        if not processes:
            log('本项目服务当前未运行。')
        return
    if check:
        settings = read_settings(runtime)
        release = public_release(source / 'public')
        check_database(settings, readonly=True)
        processes = project_processes(runtime)
        guard_port(settings, processes)
        if not any(owns_port(process, settings.port) for process in processes) or not health(settings, release):
            raise LaunchError('本项目服务尚未运行，或未通过健康检查。')
        ready(settings, release)
        return
    with launch_lock(runtime):
        prepare_config(source, runtime)
        settings = read_settings(runtime)
        release = public_release(source / 'public')
        processes = project_processes(runtime)
        guard_port(settings, processes)
        changed = sync_source(source, settings, release)
        check_database(settings)
        binary = build_binary(settings, changed)
        current_hash = digest(binary)
        reuse = len(processes) == 1 and owns_port(processes[0], settings.port)
        if reuse:
            try:
                reuse = digest(Path('/proc') / str(processes[0].pid) / 'exe') == current_hash and health(settings, release)
            except OSError:
                reuse = False
        if reuse:
            log(f'复用已通过健康检查的服务（PID {processes[0].pid}）。')
            ready(settings, release)
            return
        for process in processes:
            stop_process(process, runtime)
        guard_port(settings, [])
        start_server(settings, binary, release)
        log('服务已启动，页面版本、注册导航、脚本、接口权限和样式内容检查通过。')
        ready(settings, release)


def main(argv=None):
    parser = argparse.ArgumentParser(description='保留配置和数据的一键启动引擎（WSL/Linux）')
    parser.add_argument('--source', required=True, type=Path)
    parser.add_argument('--runtime', type=Path, default=Path.home() / 'oj_platform')
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--stop', action='store_true')
    mode.add_argument('--check', action='store_true')
    args = parser.parse_args(argv)
    try:
        if sys.platform != 'linux':
            raise LaunchError('请通过 Windows 启动脚本在 WSL 中运行此引擎。')
        launch(args.source, args.runtime, stop=args.stop, check=args.check)
        return 0
    except (LaunchError, OSError) as exc:
        log('启动失败：' + str(exc))
        return 1


if __name__ == '__main__':
    sys.exit(main())
