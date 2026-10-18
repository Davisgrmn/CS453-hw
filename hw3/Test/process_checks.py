#!/usr/bin/env python3
"""Linux/Onyx interactive checks using a real pseudo-terminal (stdlib only)."""
import errno
import os
from pathlib import Path
import pty
import re
import select
import signal
import tempfile
import time
import unittest

ROOT = Path(__file__).resolve().parents[1]
SHELL = str(ROOT / "shell")


def eventually(predicate, timeout=5):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if predicate():
            return
        time.sleep(0.02)
    raise AssertionError("timed out waiting for process state")


class TerminalShell:
    def __init__(self, directory):
        self.pid, self.fd = pty.fork()
        if self.pid == 0:
            os.chdir(directory)
            os.execv(SHELL, [SHELL])
        self.status = None
        try:
            self.prompt()
        except BaseException:
            self.close()
            raise

    def send(self, data):
        os.write(self.fd, data)

    def prompt(self):
        output = b""
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            ready, _, _ = select.select([self.fd], [], [], 0.1)
            if ready:
                try:
                    data = os.read(self.fd, 65536)
                except OSError as error:
                    if error.errno == errno.EIO:
                        break
                    raise
                if not data:
                    break
                output += data
                if output.endswith(b"$ "):
                    return output
        raise AssertionError("no shell prompt: " + repr(output))

    def command(self, text):
        self.send(text.encode() + b"\n")
        return self.prompt()

    def children(self):
        return (Path(f"/proc/{self.pid}/task/{self.pid}/children")
                .read_text().split())

    def exited(self):
        if self.status is None:
            pid, status = os.waitpid(self.pid, os.WNOHANG)
            if pid:
                self.status = status
        return self.status is not None

    def finish(self):
        eventually(self.exited)
        if os.waitstatus_to_exitcode(self.status) != 0:
            raise AssertionError(f"shell exit status: {self.status}")

    def close(self):
        if not self.exited():
            # pty.fork created this session/process group specifically for us.
            os.killpg(self.pid, signal.SIGKILL)
            os.waitpid(self.pid, 0)
        os.close(self.fd)


class ProcessChecks(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="hw3-terminal-")
        self.work = Path(self.temp.name)
        self.shell = TerminalShell(self.work)

    def tearDown(self):
        self.shell.close()
        self.temp.cleanup()

    def test_reaps_background_jobs_while_idle(self):
        self.shell.command("sleep 0.2 &")
        self.assertTrue(self.shell.children(), "background child was not launched")
        eventually(lambda: not self.shell.children())
        self.assertFalse(self.shell.exited())

    def test_interrupts_foreground_child_and_keeps_shell(self):
        self.shell.send(b"sleep 10\n")
        eventually(lambda: bool(self.shell.children()))
        self.shell.send(b"\x03")
        self.shell.prompt()
        output = self.shell.command("echo signal-recovered")
        output = re.sub(rb"\x1b\[[0-?]*[ -/]*[@-~]", b"", output)
        self.assertIn(b"signal-recovered", output.replace(b"\r", b"").splitlines())
        self.assertFalse(self.shell.children())

    def test_history_is_written_in_starting_directory(self):
        (self.work / "sub").mkdir()
        self.shell.command("cd sub")
        self.shell.command("pwd")
        output = self.shell.command("history")
        self.assertIn(b"1 cd sub", output)
        self.assertIn(b"2 pwd", output)
        self.shell.send(b"exit\n")
        self.shell.finish()
        self.assertEqual((self.work / ".history").read_text(),
                         "cd sub\npwd\nhistory\nexit\n")
        self.assertFalse((self.work / "sub/.history").exists())

    def test_ctrl_d_waits_for_background_jobs(self):
        helper = ROOT / "Test/helpers/after-marker"
        self.shell.command(f"sh {helper} ready > result &")
        self.shell.command("touch ready")
        self.shell.send(b"\x04")
        self.shell.finish()
        self.assertEqual((self.work / "result").read_text(), "finished\n")

    def test_pipeline_and_builtin_redirection_close_descriptors(self):
        fd_dir = Path(f"/proc/{self.shell.pid}/fd")
        before = len(list(fd_dir.iterdir()))
        commands = "pwd > file ; pwd | cat > file ; pwd < file > other ; "
        self.shell.command(commands * 20)
        after = len(list(fd_dir.iterdir()))
        self.assertEqual(before, after)
        self.assertEqual((self.work / "other").read_text(), str(self.work) + "\n")


if __name__ == "__main__":
    unittest.main(verbosity=2)
