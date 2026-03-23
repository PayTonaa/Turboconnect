from __future__ import annotations
import hashlib
import re
import shutil
import subprocess
import os
from pathlib import Path

from flask import Flask, Response, abort, request, send_file, render_template_string
import yt_dlp

app = Flask(__name__)
application = app

# Folder na muzykę i ścieżka do Twojego FFmpeg
BASE_DIR = Path(__file__).resolve().parent
MUSIC_DIR = BASE_DIR / "music_library"
MUSIC_DIR.mkdir(exist_ok=True)

def _get_ffmpeg_path():
    """Szuka FFmpeg najpierw w folderze aplikacji, potem w systemie."""
    local_ffmpeg = BASE_DIR / "ffmpeg"
    if local_ffmpeg.exists() and os.access(local_ffmpeg, os.X_OK):
        return str(local_ffmpeg)
    return shutil.which("ffmpeg")

def _track_id_for_relpath(rel_posix: str) -> str:
    return hashlib.sha256(rel_posix.encode("utf-8")).hexdigest()[:16]

def _iter_audio():
    if not MUSIC_DIR.is_dir():
        return
    # Szukamy mp3 i m4a
    for ext in ("*.mp3", "*.m4a"):
        for path in sorted(MUSIC_DIR.rglob(ext)):
            if path.is_file():
                try:
                    rel = path.relative_to(MUSIC_DIR)
                    rel_posix = rel.as_posix()
                    yield path, rel_posix, _track_id_for_relpath(rel_posix)
                except ValueError:
                    continue

def _path_for_track_id(tid: str) -> Path | None:
    for path, _rel, i in _iter_audio():
        if i == tid:
            return path
    return None

def _sniff_audio_mimetype(path: Path) -> str:
    try:
        with path.open("rb") as f:
            head = f.read(16)
    except OSError:
        head = b""
    if len(head) >= 8 and head[4:8] == b"ftyp":
        return "audio/mp4"
    if head[:3] == b"ID3" or (len(head) >= 2 and head[0] == 0xFF and (head[1] & 0xE0) == 0xE0):
        return "audio/mpeg"
    if head[:4] == b"RIFF" and len(head) >= 12 and head[8:12] == b"WAVE":
        return "audio/wav"
    return "application/octet-stream"

def _stream_url_suffix(mimetype: str) -> str:
    if mimetype == "audio/mp4": return ".m4a"
    if mimetype == "audio/mpeg": return ".mp3"
    return ".bin"

def _normalize_stream_track_id(raw: str) -> str:
    for ext in (".mp3", ".m4a", ".webm", ".bin", ".MP3", ".M4A"):
        if raw.endswith(ext):
            return raw[: -len(ext)]
    return raw

def _ascii_download_name(path: Path) -> str:
    name = path.name.replace('"', "").replace("\\", "")[:120]
    return name if re.fullmatch(r"[\x20-\x7e]+", name) else f"track{path.suffix}"

def _convert_m4a_library_to_mp3(delete_source: bool = False) -> tuple[int, int, list[str]]:
    converted, failed, messages = 0, 0, []
    ffmpeg_bin = _get_ffmpeg_path()

    if not ffmpeg_bin:
        return 0, 0, ["Brak ffmpeg na serwerze (nawet lokalnego)."]

    for src in sorted(MUSIC_DIR.rglob("*.m4a")):
        dst = src.with_suffix(".mp3")
        if dst.exists(): continue

        cmd = [ffmpeg_bin, "-y", "-i", str(src), "-vn", "-acodec", "libmp3lame", "-b:a", "128k", str(dst)]
        result = subprocess.run(cmd, capture_output=True, text=True)
        if result.returncode == 0:
            converted += 1
            if delete_source: src.unlink()
        else:
            failed += 1
            messages.append(f"BLAD: {src.name}")
    return converted, failed, messages

def _clean_string(text: str) -> str:
    return re.sub(r'[^a-zA-Z0-9]', '', text).lower()

PAGE = """
<!DOCTYPE html>
<html>
<head><meta charset="utf-8"><title>Turboconnect E75</title></head>
<body style="background:#000;color:#fff;font-family:sans-serif;">
    <h2>TURBOCONNECT</h2>
    <form method="get" action="/music">
        Szukaj: <input type="text" name="q" value="{{ q_e }}">
        <input type="submit" value="OK">
    </form>
    <hr>
    {% for t in tracks %}
    <div style="border:1px solid #444;padding:10px;margin:10px 0;">
        {{ t.title }}<br>
        <a href="{{ t.stream_url }}" style="color:#0f0;">[ GRAJ ]</a> | 
        <a href="{{ t.download_url }}" style="color:#0af;">[ POBIERZ ]</a>
    </div>
    {% endfor %}
    <div style="background:#222;padding:10px;margin-top:20px;">
        <b>POBIERZ NOWY:</b>
        <form method="get" action="/api/fetch">
            <input type="text" name="q">
            <input type="submit" value="POBIERZ">
        </form>
    </div>
    <p style="font-size:10px;color:#444;">FFmpeg: {{ ffmpeg_status }}</p>
</body>
</html>
"""

@app.route("/")
def index(): return "OK. <a href='/music'>/music</a>"

@app.route("/music")
def music_page():
    q = request.args.get("q", "")
    qn = _clean_string(q)
    tracks = []
    ffmpeg_bin = _get_ffmpeg_path()
    status = "AKTYWNY" if ffmpeg_bin else "BRAK"

    for path, rel_posix, tid in _iter_audio():
        if not qn or qn in _clean_string(path.stem):
            mime = _sniff_audio_mimetype(path)
            suf = _stream_url_suffix(mime)
            tracks.append({
                "title": path.stem.replace('_', ' '),
                "stream_url": f"/library/stream/{tid}{suf}",
                "download_url": f"/library/file/{tid}{suf}",
            })
    return render_template_string(PAGE, q_e=q, tracks=tracks, count=len(tracks), ffmpeg_status=status)

@app.route("/api/fetch")
def api_fetch():
    query = request.args.get("q", "").strip()
    if not query: return "Brak nazwy!", 400

    ffmpeg_bin = _get_ffmpeg_path()
    ydl_opts = {
        "outtmpl": str(MUSIC_DIR / "%(title)s.%(ext)s"),
        "noplaylist": True,
        "quiet": True,
    }

    if ffmpeg_bin:
        ydl_opts["ffmpeg_location"] = ffmpeg_bin
        ydl_opts["postprocessors"] = [{
            "key": "FFmpegExtractAudio",
            "preferredcodec": "mp3",
            "preferredquality": "128",
        }]
    else:
        ydl_opts["format"] = "bestaudio[ext=m4a]/best"

    try:
        with yt_dlp.YoutubeDL(ydl_opts) as ydl:
            ydl.extract_info(f"ytsearch1:{query}", download=True)
        return "Pobrano! Odswiez /music"
    except Exception as e:
        return str(e), 500

@app.route("/library/stream/<track_id>")
def library_stream(track_id: str):
    tid = _normalize_stream_track_id(track_id)
    path = _path_for_track_id(tid)
    if not path: abort(404)
    return send_file(path, mimetype=_sniff_audio_mimetype(path), conditional=False)

@app.route("/library/file/<track_id>")
def library_file_download(track_id: str):
    tid = _normalize_stream_track_id(track_id)
    path = _path_for_track_id(tid)
    if not path: abort(404)
    return send_file(path, mimetype=_sniff_audio_mimetype(path), as_attachment=True, download_name=_ascii_download_name(path))

@app.route("/api/convert_m4a")
def api_convert_m4a():
    converted, failed, details = _convert_m4a_library_to_mp3(delete_source=True)
    return f"Konwersja: {converted} OK, {failed} bledow. Szczegoly: {details}"

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000)
