import argparse
import asyncio
import json
from configparser import ConfigParser


def require(parser, section, option):
    if not parser.has_section(section):
        raise ValueError(f"missing {section}.{option}")

    value = parser.get(section, option, fallback="").strip()
    if value == "":
        raise ValueError(f"missing {section}.{option}")

    return value


def optional(parser, section, option, default=""):
    if not parser.has_section(section):
        return default
    return parser.get(section, option, fallback=default).strip()


def load_probe_config(path):
    parser = ConfigParser()
    parser.read(path, encoding="utf-8")
    return {
        "url": require(parser, "server", "url"),
        "token": optional(parser, "server", "token"),
        "protocol_version": int(require(parser, "server", "protocol_version")),
        "device_id": require(parser, "device", "device_id"),
        "client_id": require(parser, "device", "client_id"),
        "input_sample_rate": int(require(parser, "audio", "input_sample_rate")),
    }


def build_request_headers(cfg):
    headers = {
        "Protocol-Version": str(cfg["protocol_version"]),
        "Device-Id": cfg["device_id"],
        "Client-Id": cfg["client_id"],
    }
    if cfg.get("token"):
        headers["Authorization"] = f"Bearer {cfg['token']}"
    return headers


def build_hello_payload(cfg):
    return {
        "type": "hello",
        "version": cfg["protocol_version"],
        "transport": "websocket",
        "audio_params": {
            "format": "opus",
            "sample_rate": cfg["input_sample_rate"],
            "channels": 1,
            "frame_duration": 60,
        },
    }


def mask_authorization(value):
    if not value:
        return ""
    return f"{value[:32]}..."


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--config", default="config/xiaozhi.ini")
    parser.add_argument("--timeout", type=float, default=5.0,
                        help="Seconds to wait for a message after hello; <=0 means wait forever")
    parser.add_argument("--no-headers", action="store_true")
    return parser.parse_args()


def print_config_summary(cfg):
    print("config summary")
    print(f"url: {cfg['url']}")
    print(f"protocol_version: {cfg['protocol_version']}")
    print(f"device_id: {cfg['device_id']}")
    print(f"client_id: {cfg['client_id']}")


def print_request_headers(headers):
    print("request headers")
    for key, value in headers.items():
        shown = mask_authorization(value) if key == "Authorization" else value
        print(f"{key}: {shown}")


async def run_probe(config_path, timeout_seconds, no_headers=False):
    import websockets
    from websockets.exceptions import ConnectionClosed

    cfg = load_probe_config(config_path)
    headers = build_request_headers(cfg)
    hello = build_hello_payload(cfg)

    print_config_summary(cfg)
    if no_headers:
        print("request headers")
        print("(disabled)")
    else:
        print_request_headers(headers)
    print("hello payload")
    print(json.dumps(hello, ensure_ascii=False, separators=(",", ":")))

    try:
        connect_kwargs = {}
        if not no_headers:
            connect_kwargs["additional_headers"] = headers

        async with websockets.connect(cfg["url"], **connect_kwargs) as ws:
            await ws.send(json.dumps(hello, ensure_ascii=False, separators=(",", ":")))
            while True:
                try:
                    if timeout_seconds <= 0:
                        message = await ws.recv()
                    else:
                        message = await asyncio.wait_for(ws.recv(), timeout=timeout_seconds)
                except TimeoutError:
                    print("timeout after hello")
                    return 0
                except ConnectionClosed as exc:
                    print(f"closed: code={exc.code} reason={exc.reason}")
                    return 0

                if isinstance(message, bytes):
                    print(f"recv binary: {len(message)} bytes")
                else:
                    print(f"recv text: {message}")
    except ConnectionClosed as exc:
        print(f"closed: code={exc.code} reason={exc.reason}")
        return 0
    except Exception as exc:
        print(f"error: {exc}")
        return 1


def main():
    args = parse_args()
    raise SystemExit(asyncio.run(run_probe(args.config, args.timeout, args.no_headers)))


if __name__ == "__main__":
    main()
