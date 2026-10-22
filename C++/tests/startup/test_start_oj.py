"""Standard-library startup tests. No WSL, browser, database, or real service is touched."""
import contextlib
import importlib.util
import io
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import unittest
from unittest import mock


MODULE_PATH = Path(__file__).resolve().parents[2] / 'tools/start_oj.py'
SPEC = importlib.util.spec_from_file_location('start_oj', MODULE_PATH)
launcher = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = launcher
SPEC.loader.exec_module(launcher)

CONFIG = '''server:
  host: 0.0.0.0
  port: 8080
  public_dir: "./public"
database:
  host: localhost
  port: 3306
  username: "root"
  password: ""
  database: oj_system
'''


class StartupTests(unittest.TestCase):
    def setUp(self):
        # Keep fixtures outside synchronized trees so a concurrent real launch cannot copy them.
        self.temporary = tempfile.TemporaryDirectory(prefix='.temp OJ ', dir=MODULE_PATH.parent.parent)
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.source = self.root / 'source with spaces'
        self.runtime = self.root / 'runtime with spaces'
        for directory in ('public/css', 'public/js', 'src', 'third_party', 'tools', 'tests', 'database', 'config'):
            (self.source / directory).mkdir(parents=True, exist_ok=True)
        self.runtime.mkdir()
        (self.source / 'config/config.yaml').write_text(CONFIG.replace('"root"', '"source-user"'), encoding='utf-8')
        (self.source / 'CMakeLists.txt').write_text('project(TestOJ)\n', encoding='utf-8')
        (self.source / 'src/main.cc').write_text('int main() {}\n', encoding='utf-8')
        (self.source / 'third_party/library.h').write_text('// library\n', encoding='utf-8')
        (self.source / 'tools/helper.py').write_text('pass\n', encoding='utf-8')
        (self.source / 'tests/test.txt').write_text('test\n', encoding='utf-8')
        (self.source / 'database/migrate.sh').write_text('#!/bin/bash\n', encoding='utf-8')
        (self.source / 'public/css/style.css').write_text('body { color: blue; }\n', encoding='utf-8')
        for name in ('app.js', 'api.js', 'auth.js', 'problem.js'):
            (self.source / 'public/js' / name).write_text('const answer = 42; // ' + name + '\n', encoding='utf-8')
        html = ('<html><head><title>CodeOJ</title><link href="css/style.css?theme=light"></head><body>'
                '<a href="register.html">Register</a><a href="login.html">Login</a>'
                '<script src="js/api.js"></script><script src="js/auth.js"></script>'
                '<script src="js/problem.js"></script><script src="js/app.js"></script></body></html>')
        for name in ('index.html', 'login.html', 'register.html', 'problem_list.html'):
            (self.source / 'public' / name).write_text(html, encoding='utf-8')
        (self.runtime / 'config').mkdir()
        (self.runtime / 'config/config.yaml').write_text(CONFIG, encoding='utf-8')
        self.settings = launcher.read_settings(self.runtime)

    def test_existing_config_is_preserved_and_only_missing_config_is_copied(self):
        original = (self.runtime / 'config/config.yaml').read_bytes()
        launcher.prepare_config(self.source, self.runtime)
        self.assertEqual((self.runtime / 'config/config.yaml').read_bytes(), original)
        self.assertEqual(launcher.read_settings(self.runtime).db_user, 'root')
        other = self.root / 'fresh runtime'
        launcher.prepare_config(self.source, other)
        self.assertEqual(launcher.read_settings(other).db_user, 'source-user')

    def test_sync_is_byte_based_keeps_extra_files_and_versions_html_idempotently(self):
        (self.runtime / 'src').mkdir()
        extra = self.runtime / 'src/keep-me.cc'
        extra.write_text('business file', encoding='utf-8')
        self.assertTrue(launcher.sync_source(self.source, self.settings))
        target_html = self.runtime / 'public/login.html'
        first_html = target_html.read_text(encoding='utf-8')
        self.assertIn('theme=light&amp;v=' + launcher.digest(self.source / 'public/css/style.css')[:16], first_html)
        self.assertIn('js/app.js?v=' + launcher.digest(self.source / 'public/js/app.js')[:16], first_html)
        self.assertNotIn('?v=', (self.source / 'public/login.html').read_text(encoding='utf-8'))
        html_mtime = target_html.stat().st_mtime_ns
        code_mtime = (self.runtime / 'src/main.cc').stat().st_mtime_ns
        self.assertFalse(launcher.sync_source(self.source, self.settings))
        self.assertEqual(target_html.stat().st_mtime_ns, html_mtime)
        self.assertEqual((self.runtime / 'src/main.cc').stat().st_mtime_ns, code_mtime)
        self.assertEqual(extra.read_text(encoding='utf-8'), 'business file')
        (self.source / 'public/css/style.css').write_text('body { color: red; }\n', encoding='utf-8')
        self.assertFalse(launcher.sync_source(self.source, self.settings))
        self.assertNotEqual(target_html.read_text(encoding='utf-8'), first_html)
        os.utime(self.runtime / 'src/main.cc', (1000, 1000))
        (self.source / 'src/main.cc').write_text('int main() { return 0; }\n', encoding='utf-8')
        self.assertTrue(launcher.sync_source(self.source, self.settings))
        self.assertGreater((self.runtime / 'src/main.cc').stat().st_mtime, 1000)

    def test_public_directory_is_respected_and_cannot_overlap_protected_or_external_paths(self):
        config = self.runtime / 'config/config.yaml'
        config.write_text(CONFIG.replace('./public', './web assets'), encoding='utf-8')
        settings = launcher.read_settings(self.runtime)
        launcher.sync_source(self.source, settings)
        self.assertTrue((self.runtime / 'web assets/login.html').is_file())
        self.assertFalse((self.runtime / 'public').exists())
        for directory in ('../outside', '.', 'src', 'tools/static', 'tests', 'database', 'third_party', '.launcher', 'build', 'config'):
            with self.subTest(directory=directory):
                config.write_text(CONFIG.replace('./public', directory), encoding='utf-8')
                with self.assertRaises(launcher.LaunchError):
                    launcher.read_settings(self.runtime)

    def test_dangerous_runtime_roots_are_rejected_before_any_mutation(self):
        for directory in (Path.home(), Path(self.runtime.anchor)):
            with self.subTest(directory=directory), mock.patch.object(launcher, 'prepare_config') as prepare:
                with self.assertRaises(launcher.LaunchError):
                    launcher.launch(self.source, directory)
                prepare.assert_not_called()

    def test_simple_yaml_handles_quotes_and_comments_but_rejects_ambiguous_formats(self):
        config = self.runtime / 'config/config.yaml'
        config.write_text(CONFIG.replace('password: ""', "password: 'a # b''c' # trailing comment"), encoding='utf-8')
        self.assertEqual(launcher.read_settings(self.runtime).db_password, "a # b'c")
        for text in ('server:\n  port: [8080]\n', 'server:\n    port: 8080\n', 'server:\n  port: 8080\n  port: 9090\n', 'server:\n  public_dir: |\n    public\n'):
            config.write_text(text, encoding='utf-8')
            with self.assertRaises(launcher.LaunchError):
                launcher.read_settings(self.runtime)

    def test_missing_local_asset_and_resource_path_escape_are_clear_failures(self):
        for content in (b'<script src="missing.js"></script>', b'<script src="../outside.js"></script>'):
            with self.assertRaises(launcher.LaunchError):
                launcher.versioned_html(content, Path('login.html'), self.source / 'public')
        external = b'<html><head></head><body><script src="https://example.com/a.js"></script></body></html>'
        stamped = launcher.versioned_html(external, Path('login.html'), self.source / 'public')
        self.assertIn('https://example.com/a.js', launcher.PageReferences(stamped).references)

    def test_no_build_for_unchanged_binary_and_incremental_build_has_no_delete(self):
        binary = self.runtime / 'build/oj_server'
        binary.parent.mkdir()
        binary.write_bytes(b'old binary')
        with mock.patch.object(launcher, 'run_command') as run:
            self.assertEqual(launcher.build_binary(self.settings, False), binary)
            run.assert_not_called()
            launcher.build_binary(self.settings, True)
        self.assertEqual(run.call_count, 2)
        self.assertEqual(run.call_args_list[0].args[0][0:2], ['cmake', '-S'])
        self.assertEqual(run.call_args_list[1].args[0][0:2], ['cmake', '--build'])
        self.assertTrue(binary.exists())

    def test_current_database_requires_no_backup_or_migration(self):
        with mock.patch.object(launcher, 'missing_columns', return_value=set()), mock.patch.object(launcher, 'run_command') as run:
            launcher.check_database(self.settings)
        run.assert_not_called()
        self.assertFalse((self.runtime / '.launcher').exists())

    def test_migration_requires_successful_private_backup_and_correct_dump_arguments(self):
        (self.runtime / 'database').mkdir()
        (self.runtime / 'database/migrate.sh').write_text('migration', encoding='utf-8')
        calls = []
        def run(args, **kwargs):
            calls.append((args, kwargs))
            if args[0] == 'mysqldump':
                kwargs['stdout'].write(b'-- complete SQL backup\n')
            return b''
        actual_open = os.open
        with mock.patch.object(launcher, 'missing_columns', side_effect=[{('problems', 'solution')}, set()]), mock.patch.object(launcher, 'run_command', side_effect=run), mock.patch.object(launcher.os, 'open', wraps=actual_open) as opened:
            launcher.check_database(self.settings)
        self.assertEqual([call[0][0] for call in calls], ['mysqldump', 'bash'])
        dump_args = calls[0][0]
        self.assertIn('oj_system', dump_args)
        self.assertFalse(any(arg.startswith('--database=') for arg in dump_args))
        self.assertIn('--single-transaction', dump_args)
        self.assertEqual(calls[0][1]['env']['MYSQL_USER'], 'root')
        self.assertEqual(calls[1][1]['env']['MYSQL_DATABASE'], 'oj_system')
        self.assertEqual(opened.call_args.args[2], 0o600)
        backup = list((self.runtime / '.launcher/backups').glob('*.sql'))
        self.assertEqual(len(backup), 1)
        self.assertEqual(backup[0].read_bytes(), b'-- complete SQL backup\n')

    def test_failed_or_empty_backup_never_invokes_migration(self):
        (self.runtime / 'database').mkdir()
        (self.runtime / 'database/migrate.sh').write_text('migration', encoding='utf-8')
        for error in (launcher.LaunchError('dump failed'), None):
            with self.subTest(error=error), mock.patch.object(launcher, 'missing_columns', return_value={('problems', 'spj_code')}), mock.patch.object(launcher, 'run_command', side_effect=error) as run:
                with self.assertRaisesRegex(launcher.LaunchError, '未执行迁移'):
                    launcher.check_database(self.settings)
                self.assertEqual(run.call_count, 1)
                self.assertEqual(run.call_args.args[0][0], 'mysqldump')

    def test_unsupported_schema_and_check_mode_do_not_backup_or_mutate_database(self):
        for missing, readonly in (({('users', 'id')}, False), ({('problems', 'solution')}, True)):
            with self.subTest(missing=missing), mock.patch.object(launcher, 'missing_columns', return_value=missing), mock.patch.object(launcher, 'run_command') as run:
                with self.assertRaises(launcher.LaunchError):
                    launcher.check_database(self.settings, readonly=readonly)
                run.assert_not_called()
        self.assertFalse((self.runtime / '.launcher').exists())

    def test_unknown_port_conflict_does_not_stop_any_process(self):
        process = launcher.Process(431, '123')
        with mock.patch.object(launcher, 'port_occupied', return_value=True), mock.patch.object(launcher, 'owns_port', return_value=False), mock.patch.object(launcher.os, 'kill') as kill:
            with self.assertRaisesRegex(launcher.LaunchError, '其他进程占用'):
                launcher.guard_port(self.settings, [process])
            kill.assert_not_called()

    def test_process_ownership_requires_matching_executable_cwd_and_starttime(self):
        proc = self.root / 'fake proc'
        entry = proc / '431'
        entry.mkdir(parents=True)
        (entry / 'stat').write_text('431 (oj_server) ' + ' '.join(['S'] + ['0'] * 18 + ['12345']), encoding='ascii')
        links = {str(entry / 'exe'): str(self.runtime / 'build/oj_server') + ' (deleted)', str(entry / 'cwd'): str(self.runtime)}
        def readlink(path):
            return links[str(path)]
        with mock.patch.object(launcher.os, 'readlink', side_effect=readlink):
            self.assertEqual(launcher.owned_process(431, self.runtime, proc), launcher.Process(431, '12345'))
            links[str(entry / 'cwd')] = str(self.source)
            self.assertIsNone(launcher.owned_process(431, self.runtime, proc))
            links[str(entry / 'cwd')] = str(self.runtime)
            links[str(entry / 'exe')] = str(self.source / 'build/oj_server')
            self.assertIsNone(launcher.owned_process(431, self.runtime, proc))

    def test_stop_only_signals_verified_process_with_sigterm_and_refuses_pid_reuse(self):
        process = launcher.Process(431, '123')
        with mock.patch.object(launcher, 'owned_process', side_effect=[process, None]), mock.patch.object(launcher.os, 'kill', create=True) as kill:
            launcher.stop_process(process, self.runtime)
            kill.assert_called_once_with(431, signal.SIGTERM)
        with mock.patch.object(launcher, 'owned_process', return_value=launcher.Process(431, 'different')), mock.patch.object(launcher.os, 'kill', create=True) as kill:
            with self.assertRaises(launcher.LaunchError):
                launcher.stop_process(process, self.runtime)
            kill.assert_not_called()

    def test_healthy_repeated_start_reuses_process_and_emits_exactly_one_ready_line(self):
        launcher.sync_source(self.source, self.settings)
        release = launcher.public_release(self.source / 'public')
        binary = self.runtime / 'build/oj_server'
        binary.parent.mkdir()
        binary.write_bytes(b'binary')
        process = launcher.Process(431, '123')
        output = io.StringIO()
        with mock.patch.object(launcher, 'launch_lock', return_value=contextlib.nullcontext()), mock.patch.object(launcher, 'project_processes', return_value=[process]), mock.patch.object(launcher, 'guard_port'), mock.patch.object(launcher, 'check_database'), mock.patch.object(launcher, 'build_binary', return_value=binary), mock.patch.object(launcher, 'owns_port', return_value=True), mock.patch.object(launcher, 'health', return_value=True), mock.patch.object(launcher, 'digest', return_value='1234567890abcdef'), mock.patch.object(launcher, 'start_server') as start, mock.patch.object(launcher, 'stop_process') as stop, contextlib.redirect_stdout(output):
            launcher.launch(self.source, self.runtime)
        start.assert_not_called()
        stop.assert_not_called()
        self.assertEqual(output.getvalue().splitlines(), ['OJ_READY=http://localhost:8080/login.html?v=' + release])

    def test_check_is_readonly_and_stop_does_not_run_mysql(self):
        launcher.sync_source(self.source, self.settings)
        before = {path: (path.read_bytes(), path.stat().st_mtime_ns) for path in self.runtime.rglob('*') if path.is_file()}
        process = launcher.Process(431, '123')
        with mock.patch.object(launcher, 'check_database') as database, mock.patch.object(launcher, 'project_processes', return_value=[process]), mock.patch.object(launcher, 'guard_port'), mock.patch.object(launcher, 'owns_port', return_value=True), mock.patch.object(launcher, 'health', return_value=True), mock.patch.object(launcher, 'launch_lock') as lock, contextlib.redirect_stdout(io.StringIO()):
            launcher.launch(self.source, self.runtime, check=True)
            database.assert_called_once_with(self.settings, readonly=True)
            lock.assert_not_called()
        after = {path: (path.read_bytes(), path.stat().st_mtime_ns) for path in self.runtime.rglob('*') if path.is_file()}
        self.assertEqual(before, after)
        with mock.patch.object(launcher, 'project_processes', return_value=[process]), mock.patch.object(launcher, 'stop_process') as stop, mock.patch.object(launcher, 'run_command') as run:
            launcher.launch(self.source, self.runtime, stop=True)
            stop.assert_called_once_with(process, self.runtime)
            run.assert_not_called()

    def test_unhealthy_new_child_is_cleaned_up_only_after_ownership_verification(self):
        process = launcher.Process(431, '123')
        child = mock.Mock(pid=431)
        binary = self.runtime / 'build/oj_server'
        with mock.patch.object(launcher.subprocess, 'Popen', return_value=child) as popen, mock.patch.object(launcher, 'wait_healthy', side_effect=launcher.LaunchError('unhealthy')), mock.patch.object(launcher, 'owned_process', return_value=process), mock.patch.object(launcher, 'stop_process') as stop:
            with self.assertRaisesRegex(launcher.LaunchError, 'unhealthy'):
                launcher.start_server(self.settings, binary)
            stop.assert_called_once_with(process, self.runtime)
            self.assertTrue(popen.call_args.kwargs['start_new_session'])
            self.assertEqual(popen.call_args.kwargs['cwd'], self.runtime)

    def serving_runtime(self, url):
        path = launcher.urlsplit(url).path
        if path == '/api/problems':
            return 401, b'{"error":"unauthorized"}'
        file = self.settings.public / (path.lstrip('/') or 'index.html')
        if file.is_file():
            return 200, file.read_bytes()
        return 404, b'not found'

    def test_health_requires_all_core_pages_scripts_unauthorized_api_and_exact_css_bytes(self):
        launcher.sync_source(self.source, self.settings)
        with mock.patch.object(launcher, 'http_get', side_effect=self.serving_runtime) as get:
            self.assertTrue(launcher.health(self.settings))
        paths = {launcher.urlsplit(call.args[0]).path for call in get.call_args_list}
        self.assertTrue({'/', '/index.html', '/login.html', '/register.html', '/problem_list.html', '/js/api.js', '/js/auth.js', '/js/problem.js', '/css/style.css', '/api/problems'} <= paths)
        for bad_path in ('/register.html', '/login.html', '/problem_list.html', '/js/api.js', '/js/auth.js', '/js/problem.js', '/css/style.css', '/api/problems'):
            def mixed_release(url):
                if launcher.urlsplit(url).path == bad_path:
                    return (200, b'old cached bytes')
                return self.serving_runtime(url)
            with self.subTest(path=bad_path), mock.patch.object(launcher, 'http_get', side_effect=mixed_release):
                self.assertFalse(launcher.health(self.settings))

    def test_release_covers_all_source_html_css_js_and_ignores_cache_directories(self):
        public = self.source / 'public'
        original = launcher.public_release(public)
        cache = public / '.cache'
        cache.mkdir()
        (cache / 'old.js').write_text('cached', encoding='utf-8')
        self.assertEqual(launcher.public_release(public), original)
        (public / 'unrelated.txt').write_text('not a web release asset', encoding='utf-8')
        self.assertEqual(launcher.public_release(public), original)
        for relative in ('register.html', 'css/style.css', 'js/auth.js'):
            with self.subTest(relative=relative):
                file = public / relative
                content = file.read_bytes()
                file.write_bytes(content + b'\n')
                self.assertNotEqual(launcher.public_release(public), original)
                file.write_bytes(content)
        launcher.sync_source(self.source, self.settings)
        for name in ('index.html', 'login.html', 'register.html', 'problem_list.html'):
            self.assertEqual(launcher.page_release((self.settings.public / name).read_bytes()), original)
        self.assertFalse((self.settings.public / '.cache').exists())

    def test_internal_page_links_and_actions_use_one_release_preserving_query_and_fragment(self):
        release = launcher.public_release(self.source / 'public')
        html = (b'<html><head><meta name="app-version" content="old"><meta content="duplicate" name="app-version"></head><body>'
                b'<a href="register.html?next=problem_list.html&amp;v=old#form">go</a>'
                b'<form action="/login.html?from=register#login"></form>'
                b'<a href="#features">anchor</a><a href="https://example.com/register.html">external</a></body></html>')
        stamped = launcher.versioned_html(html, Path('index.html'), self.source / 'public', release)
        parsed = launcher.PageReferences(stamped)
        self.assertEqual(parsed.versions, [release])
        self.assertIn('register.html?next=problem_list.html&v=' + release + '#form', parsed.references)
        self.assertIn('/login.html?from=register&v=' + release + '#login', parsed.references)
        self.assertIn('#features', parsed.references)
        self.assertIn('https://example.com/register.html', parsed.references)
        self.assertEqual(launcher.public_release(self.source / 'public'), release)

    def test_health_rejects_old_versions_unversioned_links_missing_scripts_and_wrong_source_release(self):
        launcher.sync_source(self.source, self.settings)
        register = self.settings.public / 'register.html'
        original = register.read_bytes()
        release = launcher.public_release(self.source / 'public')
        variants = [
            original.replace(release.encode('ascii'), b'0000000000000000'),
            original.replace(('login.html?v=' + release).encode('ascii'), b'login.html'),
            original.replace(b'js/auth.js?v=', b'https://example.com/js/auth.js?v='),
        ]
        for content in variants:
            register.write_bytes(content)
            with mock.patch.object(launcher, 'http_get', side_effect=self.serving_runtime):
                self.assertFalse(launcher.health(self.settings, release))
        register.write_bytes(original)
        with mock.patch.object(launcher, 'http_get', side_effect=self.serving_runtime):
            self.assertFalse(launcher.health(self.settings, '0000000000000000'))

    def test_cli_failure_is_nonzero_and_does_not_emit_ready(self):
        output, error = io.StringIO(), io.StringIO()
        with mock.patch.object(launcher.sys, 'platform', 'linux'), mock.patch.object(launcher, 'launch', side_effect=launcher.LaunchError('端口冲突')), contextlib.redirect_stdout(output), contextlib.redirect_stderr(error):
            self.assertEqual(launcher.main(['--source', str(self.source), '--runtime', str(self.runtime)]), 1)
        self.assertEqual(output.getvalue(), '')
        self.assertIn('端口冲突', error.getvalue())


if __name__ == '__main__':
    unittest.main()
