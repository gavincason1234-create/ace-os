"""Download the latest ACE OS ISO from the project's GitHub releases.

Release assets are published by the "Build ACE OS ISO" workflow. Because a
GitHub release asset tops out at 2 GiB, the workflow may split the ISO into
`.part` files; this module reassembles them and verifies the SHA-256 checksum.
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
ISO_NAME = "ace-os-amd64.iso"
CHUNK = 1024 * 1024


class DownloadError(RuntimeError):
    pass


def _token():
    return os.environ.get("ACEOS_GITHUB_TOKEN") or os.environ.get("GITHUB_TOKEN")


def _open(url, accept="application/vnd.github+json"):
    req = urllib.request.Request(
        url, headers={"Accept": accept, "User-Agent": "aceos-installer"}
    )
    token = _token()
    if token:
        req.add_header("Authorization", "Bearer " + token)
    return urllib.request.urlopen(req)


def _latest_release():
    try:
        with _open(API + "/releases/latest") as resp:
            return json.load(resp)
    except urllib.error.HTTPError as err:
        if err.code in (401, 403, 404):
            raise DownloadError(
                "Could not find a published ACE OS ISO.\n"
                "  - If no ISO has been built yet: run the 'Build ACE OS ISO' workflow\n"
                "    in the repo's Actions tab; it publishes the ISO as a release.\n"
                "  - If the GitHub repo is private: make it public, or set the\n"
                "    ACEOS_GITHUB_TOKEN environment variable to a GitHub token that\n"
                "    can read the repo."
            )
        raise


def _progress(name, done, total):
    if total:
        pct = 100 * done // total
        bar = "#" * (pct // 4)
        sys.stdout.write(f"\r  {name}  [{bar:<25}] {pct:3d}%  ({done // (1024*1024)} MB)")
    else:
        sys.stdout.write(f"\r  {name}  {done // (1024*1024)} MB")
    sys.stdout.flush()
    if total and done >= total:
        sys.stdout.write("\n")


def _fetch_asset(asset, dest: Path):
    # The API asset URL works for both public and private repos
    # (browser_download_url does not work on private ones).
    with _open(asset["url"], accept="application/octet-stream") as resp, open(
        dest, "wb"
    ) as out:
        total = int(asset.get("size") or resp.headers.get("Content-Length") or 0)
        done = 0
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


def _expected_hash(assets, tmp_dir: Path):
    asset = assets.get("SHA256SUMS")
    if not asset:
        return None
    sums = tmp_dir / "SHA256SUMS"
    _fetch_asset(asset, sums)
    for line in sums.read_text().splitlines():
        parts = line.split()
        if len(parts) >= 2 and parts[-1].lstrip("*") == ISO_NAME:
            return parts[0]
    return None


def download_iso(dest_dir: Path) -> Path:
    """Download (or reuse) the latest ISO; returns the path to it."""
    dest_dir = Path(dest_dir).expanduser()
    dest_dir.mkdir(parents=True, exist_ok=True)
    iso_path = dest_dir / ISO_NAME

    print("Checking for the latest ACE OS release...")
    release = _latest_release()
    assets = {a["name"]: a for a in release.get("assets", [])}
    expected = _expected_hash(assets, dest_dir)

    if iso_path.exists() and expected and _sha256(iso_path) == expected:
        print(f"Already downloaded and verified: {iso_path}")
        return iso_path

    parts = sorted(
        name
        for name in assets
        if name.startswith(ISO_NAME + ".") and name.endswith(".part")
    )

    tmp = iso_path.with_suffix(".iso.download")
    if ISO_NAME in assets:
        print("Downloading the ACE OS ISO...")
        _fetch_asset(assets[ISO_NAME], tmp)
    elif parts:
        print(f"Downloading the ACE OS ISO ({len(parts)} parts)...")
        with open(tmp, "wb") as out:
            for name in parts:
                part_path = dest_dir / name
                _fetch_asset(assets[name], part_path)
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
        print("Verifying checksum...")
        actual = _sha256(tmp)
        if actual != expected:
            tmp.unlink()
            raise DownloadError(
                "Checksum mismatch — the download is corrupt. Please try again."
            )
        print("Checksum OK.")

    tmp.replace(iso_path)
    print(f"ISO ready: {iso_path}")
    return iso_path
