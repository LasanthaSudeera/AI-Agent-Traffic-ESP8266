#!/usr/bin/env python3
import argparse
import json
import time
import urllib.error
import urllib.request


def request(base_url, method, path, payload=None, raw_body=None):
    data = None
    headers = {}
    if payload is not None:
        data = json.dumps(payload).encode("utf-8")
        headers["Content-Type"] = "application/json"
    elif raw_body is not None:
        data = raw_body
        headers["Content-Type"] = "application/json"

    req = urllib.request.Request(
        f"{base_url.rstrip('/')}{path}", data=data, headers=headers, method=method
    )
    try:
        with urllib.request.urlopen(req, timeout=5) as response:
            body = response.read().decode("utf-8")
            return response.status, json.loads(body) if body else {}
    except urllib.error.HTTPError as error:
        body = error.read().decode("utf-8")
        return error.code, json.loads(body) if body else {}


def expect(condition, message):
    if not condition:
        raise AssertionError(message)


def expect_status(base_url, method, path, expected, payload=None, raw_body=None):
    status, body = request(base_url, method, path, payload, raw_body)
    expect(status == expected, f"{method} {path}: expected {expected}, got {status}: {body}")
    return body


def run(base_url, quick):
    body = expect_status(base_url, "POST", "/api/status", 200,
                         {"state": "working", "agent": "codex"})
    expect(body["state"] == "working", body)
    expect(body["agent"] == "codex", body)
    expect(1 <= body["expires_in_seconds"] <= 120, body)

    body = expect_status(base_url, "GET", "/api/status", 200)
    expect(body["state"] == "working" and body["agent"] == "codex", body)

    expect_status(base_url, "POST", "/api/status", 200,
                  {"state": "blocked", "agent": "agent-a"})
    expect_status(base_url, "POST", "/api/status", 200,
                  {"state": "permission", "agent": "agent-b"})
    body = expect_status(base_url, "POST", "/api/status", 200,
                         {"state": "ready", "agent": "agent-c"})
    expect(body["state"] == "ready" and body["agent"] == "agent-c", body)

    expect_status(base_url, "POST", "/api/status", 200,
                  {"state": "working", "agent": "preserved"})
    invalid_requests = [
        ({}, None),
        ({"state": 7}, None),
        ({"state": "unknown"}, None),
        ({"state": "ready", "agent": 7}, None),
        ({"state": "ready", "agent": "x" * 33}, None),
        (None, b"{not-json"),
        (None, json.dumps({"state": "ready", "padding": "x" * 300}).encode("utf-8")),
    ]
    for payload, raw_body in invalid_requests:
        expect_status(base_url, "POST", "/api/status", 400, payload, raw_body)

    body = expect_status(base_url, "GET", "/api/status", 200)
    expect(body["state"] == "working" and body["agent"] == "preserved", body)
    expect_status(base_url, "GET", "/missing", 404)
    expect_status(base_url, "PUT", "/api/status", 405, {"state": "ready"})

    if not quick:
        expect_status(base_url, "POST", "/api/status", 200,
                      {"state": "working", "agent": "timeout-test"})
        time.sleep(121)
        body = expect_status(base_url, "GET", "/api/status", 200)
        expect(body == {"state": "ready", "agent": "", "expires_in_seconds": 0}, body)

    print("API smoke test passed")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--base-url", help="Device URL, for example http://192.168.1.42")
    parser.add_argument("--quick", action="store_true", help="Skip the 121-second expiry test")
    args = parser.parse_args()
    base_url = args.base_url or input("Device URL printed in Serial Monitor: ").strip()
    run(base_url, args.quick)


if __name__ == "__main__":
    main()
