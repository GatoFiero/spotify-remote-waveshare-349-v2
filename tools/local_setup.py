"""Loopback-only browser setup for the upstream PKCE firmware. Never logs credentials."""
import hashlib
import hmac
import html
import http.server
import json
import re
import secrets
import threading
import urllib.parse
from pathlib import Path
from spotify_auth import b64url, post_token, REDIRECT_URI, SCOPES

ROOT = Path(__file__).resolve().parents[1]
CSRF = secrets.token_urlsafe(32)
pending = None
complete = (ROOT/'src'/'Secrets.h').exists() and (ROOT/'local-state'/'setup-complete.json').exists()
STYLE = "body{background:#101513;color:#edf5ef;font:17px system-ui;max-width:700px;margin:50px auto;padding:24px}h1{color:#1ed760}label{display:block;margin:22px 0 8px}input{width:95%;padding:13px;font-size:17px;border-radius:8px;border:1px solid #53645a;background:#1e2923;color:white}button{background:#1ed760;border:0;border-radius:24px;padding:14px 24px;margin-top:28px;font-size:17px;font-weight:bold}a{color:#1ed760}small{color:#b5c4ba}code{background:#27352d;padding:4px}"

class Handler(http.server.BaseHTTPRequestHandler):
    def log_message(self, *_):
        pass

    def page(self, body, status=200):
        data = ('<!doctype html><meta charset="utf-8"><title>Spotify remote setup</title><style>'+STYLE+'</style>'+body).encode()
        self.send_response(status)
        self.send_header('Content-Type', 'text/html; charset=utf-8')
        self.send_header('Cache-Control', 'no-store')
        self.send_header('Referrer-Policy', 'same-origin')
        self.send_header('X-Frame-Options', 'DENY')
        self.send_header('Content-Length', str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def do_GET(self):
        global pending, complete
        parsed = urllib.parse.urlparse(self.path)
        if parsed.path == '/reauthorize':
            self.page('''<h1>Enable Like Song</h1><p>Approve the extra Spotify permission to save songs to your library. Your Wi-Fi details are retained.</p><form action="/reauthorize" method="post"><input type="hidden" name="csrf" value="'''+CSRF+'''"><button>Authorize Liked Songs</button></form>''')
            return
        if parsed.path == '/wifi':
            self.page('''<h1>Update the remote's Wi-Fi</h1><p>Use the exact network name shown in your router or phone. The board needs 2.4 GHz Wi-Fi.</p><form action="/wifi" method="post"><input type="hidden" name="csrf" value="'''+CSRF+'''"><label>Wi-Fi network name</label><input name="ssid" required maxlength="32" autocomplete="off"><label>Wi-Fi password</label><input name="password" type="password" autocomplete="off"><p>Your saved Spotify authorization will be retained.</p><button>Save Wi-Fi</button></form>''')
            return
        if parsed.path == '/callback':
            query = urllib.parse.parse_qs(parsed.query)
            if not pending or not hmac.compare_digest(query.get('state',[''])[0], pending['state']):
                self.page('<h1>Sign-in could not be verified</h1><p>Return to setup and try again.</p>',400)
                return
            if 'code' not in query:
                pending = None
                self.page('<h1>Sign-in was cancelled</h1><a href="/">Try again</a>',400)
                return
            config = pending
            pending = None
            try:
                tokens = post_token({'grant_type':'authorization_code','code':query['code'][0], 'redirect_uri':REDIRECT_URI,'client_id':config['client_id'],'code_verifier':config['verifier']})
                refresh = tokens['refresh_token']
                lines = ['// Local private configuration; do not commit.', '#pragma once', '']
                for name, value in [('WIFI_SSID',config['ssid']),('WIFI_PASSWORD',config['password']),('SPOTIFY_CLIENT_ID',config['client_id']),('SPOTIFY_REFRESH_TOKEN',refresh)]:
                    lines.append('#define '+name+' '+json.dumps(value,ensure_ascii=False))
                lines.append('#define SPOTIFY_AUTH_VERSION '+json.dumps(secrets.token_hex(8)))
                (ROOT/'src'/'Secrets.h').write_text('\n'.join(lines)+'\n',encoding='utf-8')
                complete = True
                (ROOT/'local-state').mkdir(exist_ok=True)
                (ROOT/'local-state'/'setup-complete.json').write_text(json.dumps({'configured':True,'library_modify':True}),encoding='utf-8')
                self.page('<h1>Spotify is connected</h1><p>Your Wi-Fi and Spotify sign-in are saved locally. Build and upload the firmware using the setup script or README instructions.</p><p>Keep the board plugged in. Start a song in Spotify so the remote has an active device to control.</p>')
            except (Exception,SystemExit):
                self.page('<h1>Spotify sign-in failed</h1><p>Check the app Client ID and redirect URI, then try again.</p><a href="/">Return to setup</a>',400)
            return
        if parsed.path != '/':
            self.page('Not found',404)
            return
        if complete:
            self.page('<h1>Setup saved</h1><p>Spotify and Wi-Fi setup is complete. Build and upload the firmware using the setup script or README instructions.</p>')
            return
        self.page('''<h1>Set up your Spotify remote</h1><p>Waveshare 3.49 V2</p><p>Create or open your app in the <a href="https://developer.spotify.com/dashboard" target="_blank" rel="noreferrer">Spotify developer dashboard</a>. Add this exact redirect URI:</p><p><code>http://127.0.0.1:8888/callback</code></p><p>The remote controls playback on your phone, PC, or speaker. Spotify Premium is required for playback controls.</p><form action="/authorize" method="post"><input type="hidden" name="csrf" value="'''+CSRF+'''"><label>Spotify app Client ID</label><input name="client_id" required pattern="[A-Fa-f0-9]{32}" autocomplete="off"><label>Wi-Fi network name (2.4 GHz)</label><input name="ssid" required maxlength="32" autocomplete="off"><label>Wi-Fi password</label><input name="password" type="password" autocomplete="off"><small>Enter credentials here on your own computer. They are saved only in this local project and the firmware; keep them out of public posts and repositories. No Spotify client secret is needed.</small><br><button>Connect Spotify</button></form>''')

    def do_POST(self):
        global pending
        # Embedded browsers and privacy policies can serialize a local form's
        # Origin as "null". The unpredictable form token below still verifies
        # that the submission came from this setup page.
        if self.path not in ('/authorize','/wifi','/reauthorize') or self.headers.get('Origin') not in (None,'null','http://127.0.0.1:8888'):
            self.page('<h1>Please reopen setup</h1><p>This browser submission could not be verified.</p><a href="/">Return to setup</a>',403)
            return
        length = int(self.headers.get('Content-Length','0'))
        if length > 4096:
            self.page('Request too large',400)
            return
        values = urllib.parse.parse_qs(self.rfile.read(length).decode(),keep_blank_values=True)
        get = lambda key: values.get(key,[''])[0]
        if self.path == '/reauthorize':
            if not hmac.compare_digest(get('csrf'),CSRF):
                self.page('<a href="/reauthorize">Reopen authorization</a>',400)
                return
            source = (ROOT/'src'/'Secrets.h').read_text(encoding='utf-8')
            config = {}
            for macro,key in [('WIFI_SSID','ssid'),('WIFI_PASSWORD','password'),('SPOTIFY_CLIENT_ID','client_id')]:
                match = re.search(r'^#define\s+'+macro+r'\s+(".*")$',source,re.M)
                if not match:
                    self.page('<a href="/">Complete setup first</a>',400)
                    return
                config[key] = json.loads(match.group(1))
            config['csrf'] = CSRF
            values = {key:[value] for key,value in config.items()}
        if self.path == '/wifi':
            if not hmac.compare_digest(get('csrf'),CSRF) or not 1 <= len(get('ssid').encode()) <= 32:
                self.page('Check the network name. <a href="/wifi">Try again</a>',400)
                return
            config_path = ROOT/'src'/'Secrets.h'
            source = config_path.read_text(encoding='utf-8')
            for name,value in [('WIFI_SSID',get('ssid')),('WIFI_PASSWORD',get('password'))]:
                source = re.sub(r'^#define\s+'+name+r'\s+.*$', lambda match:'#define '+name+' '+json.dumps(value,ensure_ascii=False),source,flags=re.M)
            config_path.write_text(source,encoding='utf-8')
            (ROOT/'local-state'/'wifi-updated.json').write_text(json.dumps({'updated':True}),encoding='utf-8')
            self.page('<h1>Wi-Fi saved</h1><p>Rebuild and upload the firmware to apply your new Wi-Fi settings. Spotify authorization has been retained.</p>')
            return
        if not hmac.compare_digest(get('csrf'),CSRF) or not re.fullmatch(r'[a-fA-F0-9]{32}',get('client_id')) or not 1 <= len(get('ssid').encode()) <= 32:
            self.page('Check the Client ID and Wi-Fi network name. <a href="/">Try again</a>',400)
            return
        pending = {'client_id':get('client_id'), 'ssid':get('ssid'),'password':get('password'),'state':secrets.token_urlsafe(24),'verifier':b64url(secrets.token_bytes(64))}
        challenge = b64url(hashlib.sha256(pending['verifier'].encode()).digest())
        url = 'https://accounts.spotify.com/authorize?'+urllib.parse.urlencode({'client_id':pending['client_id'],'response_type':'code','redirect_uri':REDIRECT_URI,'code_challenge_method':'S256','code_challenge':challenge,'state':pending['state'],'scope':SCOPES})
        self.send_response(303)
        self.send_header('Location',url)
        self.send_header('Content-Length','0')
        self.end_headers()

if __name__ == '__main__':
    http.server.HTTPServer(('127.0.0.1',8888),Handler).serve_forever()
