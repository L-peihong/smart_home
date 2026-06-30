#!/usr/bin/env python3
import http.server
import socketserver
import json
import urllib.parse

PORT = 8000

def write_command(cmd):
    with open('e5_command.txt', 'w', encoding='utf-8') as f:
        f.write(cmd + '\n')

class Handler(http.server.SimpleHTTPRequestHandler):
    def do_POST(self):
        length = int(self.headers.get('Content-Length', 0))
        data = self.rfile.read(length) if length > 0 else b''
        path = urllib.parse.urlparse(self.path).path
        try:
            payload = json.loads(data.decode('utf-8')) if data else {}
            if path == '/api/fan':
                action = payload.get('action')
                speed = payload.get('speed')
                if action == 'on':
                    write_command(f'fan=on&value={speed if speed is not None else 20}')
                elif action == 'off':
                    write_command('fan=off')
                elif action == 'set':
                    write_command(f'fan=set&value={speed if speed is not None else 20}')
                else:
                    self.send_error(400)
                self.send_ok()
                return
            elif path == '/api/fanmode':
                mode = payload.get('mode', 0)
                write_command(f'fan_mode={mode}')   
                self.send_ok()
                return
            elif path == '/api/display':
                mode = payload.get('mode', 0)
                write_command(f'display={mode}')
                self.send_ok()
                return
            elif path == '/api/brightness':
                brightness = payload.get('brightness', 50)
                write_command(f'brightness={brightness}')
                self.send_ok()
                return
            elif path == '/api/lightmode':
                mode = payload.get('mode', 0)
                write_command(f'light_mode={mode}')
                self.send_ok()
                return
            elif path == '/api/scene':
                scene = int(payload.get('scene', 0))
                if scene in (0,1,2):
                    write_command(f'scene={scene}')
                self.send_ok()
                return
            elif path == '/api/voice':
                action = payload.get('action')
                if action == 'start':
                    write_command('voice=start')
                elif action in ('on','off'):
                    write_command(f'voice={action}')
                else:
                    self.send_error(400)
                self.send_ok()
                return
            elif path == '/api/speak_status':
                write_command('speak_status')
                self.send_ok()
                return
            elif path == '/api/threshold':
                ttype = payload.get('type')
                value = int(payload.get('value', 0))
                if ttype in ('temp','hum','light'):
                    write_command(f'threshold={ttype}&value={value}')
                self.send_ok()
                return
            elif path == '/api/sleep':
                minutes = int(payload.get('minutes', 1))
                write_command(f'sleep={minutes}')
                self.send_ok()
                return
            elif path == '/api/reboot':
                write_command('reboot')
                self.send_ok()
                return
            else:
                return http.server.SimpleHTTPRequestHandler.do_POST(self)
        except Exception as e:
            self.send_error(500, str(e))

    def send_ok(self):
        self.send_response(200)
        self.end_headers()
        self.wfile.write(b'ok')

    def do_GET(self):
        return http.server.SimpleHTTPRequestHandler.do_GET(self)

if __name__ == '__main__':
    import sys
    if len(sys.argv) > 1:
        try:
            PORT = int(sys.argv[1])
        except:
            pass
    socketserver.TCPServer.allow_reuse_address = True
    with socketserver.TCPServer(('0.0.0.0', PORT), Handler) as httpd:
        print(f"Serving at port {PORT}")
        httpd.serve_forever()