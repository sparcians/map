# Run a basic server to look at the client-side stuff on a different host (e.g. windows+chrome)

from http.server import HTTPServer, SimpleHTTPRequestHandler

# NOTE: Run this from within the UI directory beacuse that is where favicon.ico lives
PORT = 8003
with HTTPServer(('', PORT), SimpleHTTPRequestHandler) as httpd:
    print('serving ', PORT)
    print('now go to http://<this host>:{}/ui/viewer/viewer.html'.format(PORT))
    httpd.serve_forever()
