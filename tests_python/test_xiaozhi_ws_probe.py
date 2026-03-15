import tempfile
import textwrap
import unittest
from pathlib import Path
from unittest import mock

from tools.xiaozhi_ws_probe import (
    build_hello_payload,
    build_request_headers,
    load_probe_config,
    mask_authorization,
    parse_args,
)


class ProbeConfigTest(unittest.TestCase):
    def test_loads_ini_and_builds_same_headers_and_hello_as_c_client(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            config_path = Path(tmpdir) / "xiaozhi.ini"
            config_path.write_text(
                textwrap.dedent(
                    """
                    [server]
                    url = wss://api.tenclass.net/xiaozhi/v1/
                    token = test
                    protocol_version = 1

                    [device]
                    device_id = desktop-test
                    client_id = client-test

                    [audio]
                    input_sample_rate = 16000
                    """
                ).strip()
                + "\n",
                encoding="utf-8",
            )

            cfg = load_probe_config(config_path)
            headers = build_request_headers(cfg)
            hello = build_hello_payload(cfg)

        self.assertEqual(cfg["url"], "wss://api.tenclass.net/xiaozhi/v1/")
        self.assertEqual(headers["Authorization"], "Bearer test")
        self.assertEqual(headers["Protocol-Version"], "1")
        self.assertEqual(headers["Device-Id"], "desktop-test")
        self.assertEqual(headers["Client-Id"], "client-test")
        self.assertEqual(
            hello,
            {
                "type": "hello",
                "version": 1,
                "transport": "websocket",
                "audio_params": {
                    "format": "opus",
                    "sample_rate": 16000,
                    "channels": 1,
                    "frame_duration": 60,
                },
            },
        )


class ProbeValidationTest(unittest.TestCase):
    def test_missing_server_token_is_allowed_and_skips_authorization_header(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            config_path = Path(tmpdir) / "xiaozhi.ini"
            config_path.write_text(
                textwrap.dedent(
                    """
                    [server]
                    url = wss://api.tenclass.net/xiaozhi/v1/
                    protocol_version = 1

                    [device]
                    device_id = desktop-test
                    client_id = client-test

                    [audio]
                    input_sample_rate = 16000
                    """
                ).strip()
                + "\n",
                encoding="utf-8",
            )

            cfg = load_probe_config(config_path)
            headers = build_request_headers(cfg)

        self.assertEqual(cfg["token"], "")
        self.assertNotIn("Authorization", headers)


class ProbeFormatTest(unittest.TestCase):
    def test_mask_authorization_value(self):
        self.assertEqual(mask_authorization("Bearer test"), "Bearer test...")
        self.assertEqual(mask_authorization(""), "")

    def test_parse_args_supports_no_headers(self):
        with mock.patch("sys.argv", ["xiaozhi_ws_probe.py", "--no-headers"]):
            args = parse_args()

        self.assertTrue(args.no_headers)
