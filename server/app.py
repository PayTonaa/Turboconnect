from __future__ import annotations
import hashlib
import io
import json
import re
import shutil
import subprocess
import os
import time
import zipfile
from pathlib import Path

from flask import Flask, Response, abort, redirect, render_template_string, request, send_file, url_for
import yt_dlp

app = Flask(__name__)
application = app

# Folder na muzykę i ścieżka do Twojego FFmpeg
BASE_DIR = Path(__file__).resolve().parent
MUSIC_DIR = BASE_DIR / "music_library"
MUSIC_DIR.mkdir(exist_ok=True)
PLAYLISTS_FILE = BASE_DIR / "playlists.json"

def _get_ffmpeg_path():
    """Szuka FFmpeg najpierw w folderze aplikacji, potem w systemie."""
    local_ffmpeg = BASE_DIR / "ffmpeg"
    if local_ffmpeg.exists() and os.access(local_ffmpeg, os.X_OK):
        return str(local_ffmpeg)
    return shutil.which("ffmpeg")


def _load_playlists() -> dict:
    if not PLAYLISTS_FILE.exists():
        return {"playlists": []}
    try:
        return json.loads(PLAYLISTS_FILE.read_text(encoding="utf-8"))
    except Exception:
        return {"playlists": []}


def _save_playlists(data: dict) -> None:
    PLAYLISTS_FILE.write_text(
        json.dumps(data, ensure_ascii=False, indent=2),
        encoding="utf-8",
    )


def _find_playlist(data: dict, playlist_id: str) -> dict | None:
    for p in data.get("playlists", []):
        if p.get("id") == playlist_id:
            return p
    return None

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


def _all_tracks() -> list[dict]:
    tracks: list[dict] = []
    for path, rel_posix, tid in _iter_audio():
        mime = _sniff_audio_mimetype(path)
        suf = _stream_url_suffix(mime)
        tracks.append(
            {
                "id": tid,
                "title": path.stem.replace("_", " "),
                "filename": path.name,
                "rel": rel_posix,
                "path": path,
                "stream_url": f"/library/stream/{tid}{suf}",
                "download_url": f"/library/file/{tid}{suf}",
            }
        )
    return tracks

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
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Turboconnect E75</title>
  <style>
    body { background:#121212; color:#fff; font-family:sans-serif; margin:10px; }
    .card { background:#181818; border:1px solid #2a2a2a; border-radius:8px; padding:10px; margin:8px 0; }
    .title { color:#1db954; font-weight:bold; }
    .tiny { color:#9a9a9a; font-size:11px; }
    a { color:#1db954; text-decoration:none; }
    input, select, button { background:#0f0f0f; color:#fff; border:1px solid #333; padding:6px; width:100%; margin:4px 0; }
    button { background:#1db954; color:#000; font-weight:bold; }
    .row { display:flex; gap:6px; }
    .row > * { flex:1; }
  </style>
</head>
<body>
  <div class="card">
    <div class="title">TURBOCONNECT (NO-LOGIN)</div>
    <div class="tiny">Spotify-like, low-power UI for Nokia E75. FFmpeg: {{ ffmpeg_status }}</div>
    <form method="get" action="/music">
      <input type="text" name="q" value="{{ q_e }}" placeholder="Szukaj utworu...">
      <button type="submit">Szukaj</button>
    </form>
  </div>

  <div class="card">
    <form method="post" action="/playlists/create">
      <div class="title">Nowa playlista</div>
      <input type="text" name="name" placeholder="Nazwa playlisty">
      <button type="submit">Utworz</button>
    </form>
  </div>

  <div class="card">
    <div class="title">Import publicznej playlisty Spotify</div>
    <form method="get" action="/api/import_spotify">
      <input type="text" name="url" placeholder="https://open.spotify.com/playlist/...">
      <button type="submit">Importuj (moze potrwac)</button>
    </form>
  </div>

  <form method="post" action="/playlists/add_tracks">
    <div class="card">
      <div class="title">Playlisty</div>
      {% if playlists %}
        {% for p in playlists %}
          <div class="tiny">{{ p.name }} ({{ p.track_count }}) -
            <a href="/playlist/{{ p.id }}">otworz</a> |
            <a href="/playlists/delete/{{ p.id }}">usun</a>
          </div>
        {% endfor %}
      {% else %}
        <div class="tiny">Brak playlist.</div>
      {% endif %}
      <select name="playlist_id">
        <option value="">-- wybierz playliste --</option>
        {% for p in playlists %}
          <option value="{{ p.id }}">{{ p.name }}</option>
        {% endfor %}
      </select>
      <button type="submit">Dodaj zaznaczone do playlisty</button>
    </div>

    <div class="card">
      <div class="title">Biblioteka</div>
      {% for t in tracks %}
      <div style="border-bottom:1px solid #222; padding:6px 0;">
        <label><input type="checkbox" name="track_ids" value="{{ t.id }}"> {{ t.title }}</label>
        <div class="tiny">{{ t.filename }}</div>
        <a href="/player/{{ t.id }}">[GRAJ]</a> |
        <a href="{{ t.download_url }}">[POBIERZ]</a>
      </div>
      {% endfor %}
      <button formaction="/api/download_selected" formmethod="post">Pobierz zaznaczone (ZIP)</button>
    </div>
  </form>

  <div class="card">
    <div class="title">Pobierz nowy utwor</div>
    <form method="get" action="/api/fetch">
      <input type="text" name="q" placeholder="Artysta - Tytul">
      <button type="submit">Pobierz do biblioteki</button>
    </form>
    <div class="tiny">Bez ffmpeg pobierane sa tylko natywne MP3 (gdy dostepne).</div>
  </div>
</body>
</html>
"""

PLAYER_PAGE = """
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>{{ title }}</title>
  {% if prefetch_1 %}<link rel="prefetch" href="{{ prefetch_1 }}">{% endif %}
  {% if prefetch_2 %}<link rel="prefetch" href="{{ prefetch_2 }}">{% endif %}
  <style>
    body { background:#121212; color:#fff; font-family:sans-serif; margin:10px; }
    .card { background:#181818; border:1px solid #2a2a2a; border-radius:8px; padding:10px; margin:8px 0; }
    a { color:#1db954; text-decoration:none; }
    .title { color:#1db954; font-weight:bold; margin-bottom:4px; }
    .tiny { color:#9a9a9a; font-size:11px; }
  </style>
</head>
<body>
  <div class="card">
    <div class="title">{{ title }}</div>
    <audio controls preload="metadata" src="{{ stream_url }}" style="width:100%;"></audio>
    <div class="tiny">Tryb oszczedny baterii: preload=metadata + lekki HTML + brak pollingu.</div>
  </div>
  <div class="card">
    <a href="/music">[Biblioteka]</a>
    {% if prev_url %}| <a href="{{ prev_url }}">[Poprzedni]</a>{% endif %}
    {% if next_url %}| <a href="{{ next_url }}">[Nastepny]</a>{% endif %}
    | <a href="{{ download_url }}">[Pobierz]</a>
  </div>
</body>
</html>
"""

@app.route("/")
def index():
    return redirect(url_for("music_page"))

@app.route("/music")
def music_page():
    q = request.args.get("q", "")
    qn = _clean_string(q)
    tracks = []
    ffmpeg_bin = _get_ffmpeg_path()
    status = "AKTYWNY" if ffmpeg_bin else "BRAK"
    playlists_data = _load_playlists()

    for t in _all_tracks():
        if not qn or qn in _clean_string(t["title"]):
            tracks.append(t)

    playlists = []
    for p in playlists_data.get("playlists", []):
        playlists.append(
            {"id": p.get("id"), "name": p.get("name", "Playlista"), "track_count": len(p.get("track_ids", []))}
        )
    return render_template_string(PAGE, q_e=q, tracks=tracks, playlists=playlists, ffmpeg_status=status)


@app.route("/player/<track_id>")
def player(track_id: str):
    tid = _normalize_stream_track_id(track_id)
    tracks = _all_tracks()
    idx = -1
    for i, t in enumerate(tracks):
        if t["id"] == tid:
            idx = i
            break
    if idx < 0:
        abort(404)
    current = tracks[idx]
    prev_url = f"/player/{tracks[idx - 1]['id']}" if idx > 0 else None
    next_url = f"/player/{tracks[idx + 1]['id']}" if idx + 1 < len(tracks) else None
    prefetch_1 = tracks[idx + 1]["stream_url"] if idx + 1 < len(tracks) else None
    prefetch_2 = tracks[idx + 2]["stream_url"] if idx + 2 < len(tracks) else None
    return render_template_string(
        PLAYER_PAGE,
        title=current["title"],
        stream_url=current["stream_url"],
        download_url=current["download_url"],
        prev_url=prev_url,
        next_url=next_url,
        prefetch_1=prefetch_1,
        prefetch_2=prefetch_2,
    )


@app.route("/playlists/create", methods=["POST"])
def playlists_create():
    name = request.form.get("name", "").strip()
    if not name:
        return "Podaj nazwe playlisty.", 400
    data = _load_playlists()
    playlist_id = hashlib.sha1(f"{name}-{time.time()}".encode("utf-8")).hexdigest()[:12]
    data.setdefault("playlists", []).append(
        {"id": playlist_id, "name": name[:60], "track_ids": [], "created": int(time.time())}
    )
    _save_playlists(data)
    return redirect(url_for("music_page"))


@app.route("/playlists/delete/<playlist_id>")
def playlists_delete(playlist_id: str):
    data = _load_playlists()
    before = len(data.get("playlists", []))
    data["playlists"] = [p for p in data.get("playlists", []) if p.get("id") != playlist_id]
    if len(data["playlists"]) != before:
        _save_playlists(data)
    return redirect(url_for("music_page"))


@app.route("/playlists/add_tracks", methods=["POST"])
def playlists_add_tracks():
    playlist_id = request.form.get("playlist_id", "").strip()
    track_ids = request.form.getlist("track_ids")
    if not playlist_id:
        return "Wybierz playliste.", 400
    if not track_ids:
        return "Zaznacz przynajmniej jeden utwor.", 400

    available_ids = {t["id"] for t in _all_tracks()}
    data = _load_playlists()
    playlist = _find_playlist(data, playlist_id)
    if not playlist:
        return "Playlista nie istnieje.", 404
    existing = set(playlist.get("track_ids", []))
    for tid in track_ids:
        if tid in available_ids and tid not in existing:
            playlist.setdefault("track_ids", []).append(tid)
    _save_playlists(data)
    return redirect(url_for("playlist_view", playlist_id=playlist_id))


@app.route("/playlist/<playlist_id>")
def playlist_view(playlist_id: str):
    data = _load_playlists()
    playlist = _find_playlist(data, playlist_id)
    if not playlist:
        return "Brak playlisty.", 404
    all_tracks = {t["id"]: t for t in _all_tracks()}
    tracks = [all_tracks[tid] for tid in playlist.get("track_ids", []) if tid in all_tracks]
    html = """
    <html><body style='background:#121212;color:#fff;font-family:sans-serif;margin:10px;'>
      <h3 style='color:#1db954;'>Playlista: {{ name }}</h3>
      <p><a style='color:#1db954;' href='/music'>[Powrot]</a></p>
      {% for t in tracks %}
        <div style='border:1px solid #2a2a2a;padding:8px;margin:6px 0;background:#181818;'>
          {{ t.title }}<br>
          <a style='color:#1db954;' href='/player/{{ t.id }}'>[GRAJ]</a> |
          <a style='color:#1db954;' href='{{ t.download_url }}'>[POBIERZ]</a>
        </div>
      {% endfor %}
      {% if not tracks %}<p>Brak utworow w playliscie.</p>{% endif %}
    </body></html>
    """
    return render_template_string(html, name=playlist.get("name", "Playlista"), tracks=tracks)


@app.route("/api/download_selected", methods=["POST"])
def api_download_selected():
    track_ids = request.form.getlist("track_ids")
    if not track_ids:
        return "Nie zaznaczono utworow.", 400
    all_tracks = {t["id"]: t for t in _all_tracks()}
    selected = [all_tracks[tid] for tid in track_ids if tid in all_tracks]
    if not selected:
        return "Brak poprawnych utworow.", 400

    mem = io.BytesIO()
    with zipfile.ZipFile(mem, mode="w", compression=zipfile.ZIP_STORED) as zf:
        for t in selected:
            zf.write(t["path"], arcname=t["filename"])
    mem.seek(0)
    ts = int(time.time())
    return send_file(
        mem,
        mimetype="application/zip",
        as_attachment=True,
        download_name=f"turboconnect-selected-{ts}.zip",
    )

@app.route("/api/fetch")
def api_fetch():
    query = request.args.get("q", "").strip()
    if not query:
        return "Brak nazwy!", 400

    ffmpeg_bin = _get_ffmpeg_path()
    ydl_opts = {
        "outtmpl": str(MUSIC_DIR / "%(title)s.%(ext)s"),
        "noplaylist": True,
        "quiet": True,
    }

    if ffmpeg_bin:
        ydl_opts["ffmpeg_location"] = ffmpeg_bin
        ydl_opts["format"] = "bestaudio/best"
        ydl_opts["postprocessors"] = [{
            "key": "FFmpegExtractAudio",
            "preferredcodec": "mp3",
            "preferredquality": "128",
        }]
    else:
        # Bez ffmpeg unikamy m4a, bo Nokia E75 zwykle ich nie odtwarza.
        ydl_opts["format"] = "bestaudio[ext=mp3]/best[ext=mp3]/bestaudio[acodec*=mp3]/best[acodec*=mp3]"

    try:
        with yt_dlp.YoutubeDL(ydl_opts) as ydl:
            ydl.extract_info(f"ytsearch1:{query}", download=True)
        return "Pobrano! Odswiez /music"
    except Exception as e:
        if not ffmpeg_bin:
            return f"Brak ffmpeg i nie znaleziono natywnego MP3 dla: {query}. Szczegoly: {e}", 500
        return str(e), 500


@app.route("/api/search_plain")
def api_search_plain():
    q = request.args.get("q", "")
    qn = _clean_string(q)
    lines: list[str] = []
    for t in _all_tracks():
        if qn and qn not in _clean_string(t["title"]):
            continue
        title = t["title"].replace("|", " ").replace("\r", " ").replace("\n", " ")
        filename = _ascii_download_name(t["path"]).replace("|", "_")
        lines.append(f"{title}|{filename}|{t['download_url']}")
        if len(lines) >= 12:
            break
    return Response("\n".join(lines), mimetype="text/plain; charset=utf-8")


@app.route("/api/import_spotify")
def api_import_spotify():
    spotify_url = request.args.get("url", "").strip()
    if "open.spotify.com/playlist/" not in spotify_url:
        return "Podaj publiczny link do playlisty Spotify.", 400

    info_opts = {"quiet": True, "extract_flat": True}
    try:
        with yt_dlp.YoutubeDL(info_opts) as ydl:
            info = ydl.extract_info(spotify_url, download=False)
    except Exception as e:
        return f"Nie udalo sie odczytac playlisty Spotify: {e}", 500

    entries = info.get("entries") or []
    if not entries:
        return "Playlista pusta lub extractor nie zwrocil utworow.", 400

    ffmpeg_bin = _get_ffmpeg_path()
    ydl_opts = {
        "outtmpl": str(MUSIC_DIR / "%(title)s.%(ext)s"),
        "noplaylist": True,
        "quiet": True,
    }
    if ffmpeg_bin:
        ydl_opts["ffmpeg_location"] = ffmpeg_bin
        ydl_opts["format"] = "bestaudio/best"
        ydl_opts["postprocessors"] = [{
            "key": "FFmpegExtractAudio",
            "preferredcodec": "mp3",
            "preferredquality": "128",
        }]
    else:
        ydl_opts["format"] = "bestaudio[ext=mp3]/best[ext=mp3]/bestaudio[acodec*=mp3]/best[acodec*=mp3]"

    ok = 0
    failed = 0
    details: list[str] = []
    for entry in entries[:40]:
        title = (entry.get("title") or "").strip()
        if not title:
            continue
        query = f"ytsearch1:{title}"
        try:
            with yt_dlp.YoutubeDL(ydl_opts) as ydl:
                ydl.extract_info(query, download=True)
            ok += 1
        except Exception as e:
            failed += 1
            details.append(f"{title}: {e}")

    summary = [f"Import Spotify zakonczony. OK={ok}, BLAD={failed}."]
    if details:
        summary.append("Pierwsze bledy:")
        summary.extend(details[:5])
    return "<br>".join(summary)

@app.route("/library/stream/<track_id>")
def library_stream(track_id: str):
    tid = _normalize_stream_track_id(track_id)
    path = _path_for_track_id(tid)
    if not path:
        abort(404)
    response = send_file(path, mimetype=_sniff_audio_mimetype(path), conditional=False)
    response.headers.set("Cache-Control", "public, max-age=86400")
    return response

@app.route("/library/file/<track_id>")
def library_file_download(track_id: str):
    tid = _normalize_stream_track_id(track_id)
    path = _path_for_track_id(tid)
    if not path:
        abort(404)
    return send_file(path, mimetype=_sniff_audio_mimetype(path), as_attachment=True, download_name=_ascii_download_name(path))

@app.route("/api/convert_m4a")
def api_convert_m4a():
    converted, failed, details = _convert_m4a_library_to_mp3(delete_source=True)
    return f"Konwersja: {converted} OK, {failed} bledow. Szczegoly: {details}"

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000)
