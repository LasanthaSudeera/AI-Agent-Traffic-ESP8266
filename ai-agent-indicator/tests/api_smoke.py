#!/usr/bin/env python3
import argparse
import json
import math
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


def run_validation(base_url):
    # Each case catches an invalid update changing state or restarting expiry.
    invalid_requests = [
        ("missing state", {}, None),
        ("non-string state", {"state": 7}, None),
        ("unknown state", {"state": "unknown"}, None),
        ("null agent", {"state": "ready", "agent": None}, None),
        ("non-string agent", {"state": "ready", "agent": 7}, None),
        ("long agent", {"state": "ready", "agent": "x" * 33}, None),
        ("NUL state suffix", {"state": "ready\0invalid"}, None),
        ("long NUL agent", {"state": "ready", "agent": "x\0" + "y" * 31}, None),
        ("long UTF-8 agent", {"state": "ready", "agent": "é" * 17}, None),
        ("broken object", None, b"{not-json"),
        ("trailing garbage", None, b'{"state":"ready"}garbage'),
        ("second object", None, b'{"state":"ready"} {}'),
        ("unquoted key", None, b'{state:"ready"}'),
        ("single quotes", None, b"{\"state\":'ready'}"),
        ("relaxed syntax", None, b"{state:'ready'}"),
        ("trailing comma", None, b'{"state":"ready",}'),
        ("raw control", None, b'{"state":"ready","agent":"a\nb"}'),
        ("invalid escape", None, br'{"state":"ready","agent":"\x41"}'),
        ("leading zero", None, b'{"state":"ready","extra":01}'),
        ("leading plus", None, b'{"state":"ready","extra":+1}'),
        ("missing fraction", None, b'{"state":"ready","extra":1.}'),
        ("comment", None, b'{"state":"ready"/*comment*/}'),
        ("non-JSON whitespace", None, b'{"state":"ready"}\v'),
        ("trailing NUL", None, b'{"state":"ready"}\0'),
        ("array root", None, b'[{"state":"ready"}]'),
        ("invalid UTF-8", None, b'{"state":"ready","extra":"\xff"}'),
        ("overlong UTF-8", None, b'{"state":"ready","extra":"\xc0\x80"}'),
        ("oversized body", None,
         json.dumps({"state": "ready", "padding": "x" * 300}).encode("utf-8")),
    ]
    failures = []
    for name, payload, raw_body in invalid_requests:
        seed_start = time.monotonic()
        expect_status(base_url, "POST", "/api/status", 200,
                      {"state": "working", "agent": "preserved"})
        seed_end = time.monotonic()
        time.sleep(1.1)  # Make a timer refresh observable at whole-second resolution.
        before = expect_status(base_url, "GET", "/api/status", 200)
        expect(0 < before["expires_in_seconds"] < 120, before)
        status, response = request(base_url, "POST", "/api/status", payload, raw_body)
        after_start = time.monotonic()
        after = expect_status(base_url, "GET", "/api/status", 200)
        after_end = time.monotonic()
        errors = []
        if status != 400:
            errors.append(f"expected HTTP 400, got {status}: {response}")
        if after["state"] != "working" or after["agent"] != "preserved":
            errors.append(f"state/agent changed: {after}")
        remaining = after["expires_in_seconds"]
        # Bound the original deadline by the seed POST's request/response times.
        # This accounts for network delay and the device's rounded-up seconds.
        minimum = max(0, math.ceil(seed_start + 120 - after_end))
        maximum = min(before["expires_in_seconds"],
                      max(0, math.ceil(seed_end + 120 - after_start)))
        if not minimum <= remaining <= maximum:
            errors.append(f"deadline changed: before={before}, after={after}")
        if errors:
            failures.append(f"{name}: {'; '.join(errors)}")

    # Decoded strings must survive storage and both POST/GET serialization.
    valid_requests = [
        (b'{"state":"ready"}', ""),
        (br'{"state":"r\u0065ady","agent":"a\u0000b\n\"\\"}', 'a\0b\n"\\'),
        (json.dumps({"state": "ready", "agent": "x" * 32}).encode(), "x" * 32),
        (json.dumps({"state": "ready", "agent": "é" * 16}).encode(), "é" * 16),
        (json.dumps({"state": "ready", "agent": "é😀"}, ensure_ascii=False).encode(), "é😀"),
        (b' \t\r\n{"state":"ready","extra":[true,false,null,-1.25e+2,{}]} \t\r\n', ""),
    ]
    for raw_body, agent in valid_requests:
        try:
            body = expect_status(base_url, "POST", "/api/status", 200, raw_body=raw_body)
            expect(body["state"] == "ready" and body["agent"] == agent, body)
            body = expect_status(base_url, "GET", "/api/status", 200)
            expect(body["state"] == "ready" and body["agent"] == agent, body)
        except AssertionError as error:
            failures.append(f"valid string/body {raw_body!r}: {error}")
    expect(not failures, "Validation failures:\n" + "\n".join(failures))
    print(f"API validation passed ({len(invalid_requests)} invalid, {len(valid_requests)} valid cases)")


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

    run_validation(base_url)
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
    parser.add_argument("--validation-only", action="store_true", help="Run focused input-validation regressions")
    args = parser.parse_args()
    base_url = args.base_url or input("Device URL printed in Serial Monitor: ").strip()
    if args.validation_only:
        run_validation(base_url)
    else:
        run(base_url, args.quick)


if __name__ == "__main__":
    main()
