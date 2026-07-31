"""Download the latest ACE OS ISO from the project's GitHub releases.

Release assets are published by the "Build ACE OS ISO" workflow under the
rolling "latest" release with stable asset names. The primary download path
uses GitHub's direct release-download URLs, which need no authentication on
a public repo and are not subject to the anonymous API rate limit. The
GitHub API is only used as a fallback for private repos with a token.

Because a GitHub release asset tops out at 2 GiB, the workflow may split
the ISO into `.part` files; SHA256SUMS lists every file, so it doubles as
the manifest. This module reassembles the parts and verifies checksums.
"""

import hashlib
import json
import os
import sys
import urllib.error
import urllib.request
from pathlib import Path

OWNER = "gavincason1234-create"
REPO = "ace-os"
API = f"https://api.github.com/repos/{OWNER}/{REPO}"
DOWNLOAD_BASE = f"https://github.com/{OWNER}/{REPO}/releases/latest/download"
ISO_NAME = "ace-os-amd64.iso"
CHUNK = 1024 * 1024


class DownloadError(RuntimeError):
    pass


def _token():
    return os.environ.get("ACEOS_GITHUB_TOKEN") or os.environ.get("GITHUB_TOKEN")


def _open(url, accept=None, auth=False):
    headers = {"User-Agent": "aceos-installer"}
    if accept:
        headers["Accept"] = accept
    if auth and _token():
        headers["Authorization"] = "Bearer " + _token()
    return urllib.request.urlopen(urllib.request.Request(url, headers=headers))


def _progress(name, done, total):
    if total:
        pct = 100 * done // total
        bar = "#" * (pct // 4)
        sys.stdout.write(
            f"\r  {name}  [{bar:<25}] {pct:3d}%  ({done // (1024*1024)} MB)"
        )
    else:
        sys.stdout.write(f"\r  {name}  {done // (1024*1024)} MB")
    sys.stdout.flush()


def _stream_to(resp, dest: Path, size_hint=0):
    total = int(resp.headers.get("Content-Length") or size_hint or 0)
    done = 0
    with open(dest, "wb") as out:
        while True:
            chunk = resp.read(CHUNK)
            if not chunk:
                break
            out.write(chunk)
            done += len(chunk)
            _progress(dest.name, done, total)
    sys.stdout.write("\n")


def _sha256(path: Path):
    digest = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(CHUNK), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _parse_sums(text):
    """Parse `sha256sum` output into {filename: hash}."""
    sums = {}
    for line in text.splitlines():
        parts = line.split()
        if len(parts) >= 2:
            sums[parts[-1].lstrip("*")] = parts[0]
    return sums


def _no_release_error():
    return DownloadError(
        "Could not find a published ACE OS ISO.\n"
        "  - If no ISO has been built yet: run the 'Build ACE OS ISO' workflow\n"
        "    in the repo's Actions tab; it publishes the ISO as a release.\n"
        "  - If the GitHub repo is private: make it public, or set the\n"
        "    ACEOS_GITHUB_TOKEN environment variable to a GitHub token that\n"
        "    can read the repo."
    )


def _verify(path: Path, expected, label):
    print(f"Verifying {label}...")
    if _sha256(path) != expected:
        path.unlink()
        raise DownloadError(
            f"Checksum mismatch on {label} — the download is corrupt. Try again."
        )


def _download_direct(dest_dir: Path, iso_path: Path) -> Path:
    """Primary path: stable public URLs, no API, no auth, no rate limits."""
    try:
        with _open(f"{DOWNLOAD_BASE}/SHA256SUMS") as resp:
            sums = _parse_sums(resp.read().decode())
    except urllib.error.HTTPError as err:
        if err.code in (401, 403, 404):
            raise _no_release_error()
        raise

    expected = sums.get(ISO_NAME)
    if not expected:
        raise DownloadError(
            "The latest release's SHA256SUMS does not list the ISO. Re-run the "
            "'Build ACE OS ISO' workflow in the repo's Actions tab."
        )

    if iso_path.exists() and _sha256(iso_path) == expected:
        print(f"Already downloaded and verified: {iso_path}")
        return iso_path

    parts = sorted(name for name in sums if name.endswith(".part"))
    tmp = iso_path.with_suffix(".iso.download")

    if parts:
        print(f"Downloading the ACE OS ISO ({len(parts)} parts)...")
        with open(tmp, "wb") as out:
            for name in parts:
                part_path = dest_dir / name
                with _open(f"{DOWNLOAD_BASE}/{name}") as resp:
                    _stream_to(resp, part_path)
                _verify(part_path, sums[name], name)
                with open(part_path, "rb") as pf:
                    while True:
                        chunk = pf.read(CHUNK)
                        if not chunk:
                            break
                        out.write(chunk)
                part_path.unlink()
    else:
        print("Downloading the ACE OS ISO...")
        with _open(f"{DOWNLOAD_BASE}/{ISO_NAME}") as resp:
            _stream_to(resp, tmp)

    _verify(tmp, expected, ISO_NAME)
    print("Checksum OK.")
    tmp.replace(iso_path)
    return iso_path


def _download_via_api(dest_dir: Path, iso_path: Path) -> Path:
    """Fallback for private repos: needs ACEOS_GITHUB_TOKEN/GITHUB_TOKEN."""
    try:
        with _open(f"{API}/releases/latest", accept="application/vnd.github+json",
                   auth=True) as resp:
            release = json.load(resp)
    except urllib.error.HTTPError as err:
        if err.code in (401, 403, 404):
            raise _no_release_error()
        raise

    assets = {a["name"]: a for a in release.get("assets", [])}
    sums = {}
    if "SHA256SUMS" in assets:
        with _open(assets["SHA256SUMS"]["url"],
                   accept="application/octet-stream", auth=True) as resp:
            sums = _parse_sums(resp.read().decode())

    expected = sums.get(ISO_NAME)
    if iso_path.exists() and expected and _sha256(iso_path) == expected:
        print(f"Already downloaded and verified: {iso_path}")
        return iso_path

    parts = sorted(n for n in assets if n.endswith(".part"))
    tmp = iso_path.with_suffix(".iso.download")

    if ISO_NAME in assets:
        print("Downloading the ACE OS ISO...")
        asset = assets[ISO_NAME]
        with _open(asset["url"], accept="application/octet-stream", auth=True) as resp:
            _stream_to(resp, tmp, asset.get("size", 0))
    elif parts:
        print(f"Downloading the ACE OS ISO ({len(parts)} parts)...")
        with open(tmp, "wb") as out:
            for name in parts:
                part_path = dest_dir / name
                asset = assets[name]
                with _open(asset["url"], accept="application/octet-stream",
                           auth=True) as resp:
                    _stream_to(resp, part_path, asset.get("size", 0))
                if name in sums:
                    _verify(part_path, sums[name], name)
                with open(part_path, "rb") as pf:
                    while True:
                        chunk = pf.read(CHUNK)
                        if not chunk:
                            break
                        out.write(chunk)
                part_path.unlink()
    else:
        raise DownloadError(
            "The latest release has no ISO attached. Re-run the "
            "'Build ACE OS ISO' workflow in the repo's Actions tab."
        )

    if expected:
        _verify(tmp, expected, ISO_NAME)
        print("Checksum OK.")
    tmp.replace(iso_path)
    return iso_path


def download_iso(dest_dir: Path) -> Path:
    """Download (or reuse) the latest ISO; returns the path to it."""
    dest_dir = Path(dest_dir).expanduser()
    dest_dir.mkdir(parents=True, exist_ok=True)
    iso_path = dest_dir / ISO_NAME

    print("Checking for the latest ACE OS release...")
    try:
        iso_path = _download_direct(dest_dir, iso_path)
    except DownloadError:
        # Direct URLs fail on private repos; retry through the API if the
        # user supplied a token, otherwise surface the friendly error.
        if not _token():
            raise
        iso_path = _download_via_api(dest_dir, iso_path)

    print(f"ISO ready: {iso_path}")
    return iso_path
