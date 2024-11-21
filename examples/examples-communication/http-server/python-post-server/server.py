
#!/usr/bin/env python3
  
from http.server import HTTPServer, SimpleHTTPRequestHandler

HOST = ""
PORT = 9988
path = "./audio.pcm"

class TestHTTPRequestHandler(SimpleHTTPRequestHandler):
    def do_POST(self):
        self.send_response(200)
        self.end_headers()

        if "Content-Length" in self.headers:
            content_length = int(self.headers["Content-Length"])
            body = self.rfile.read(content_length)
            with open(path, "wb") as out_file:
                print("writing:", content_length)
                out_file.write(body)
        elif "chunked" in self.headers.get("Transfer-Encoding", ""):
            with open(path, "wb") as out_file:
                while True:
                    line = self.rfile.readline().strip()
                    print(line)
                    chunk_length = int(line, 16)

                    if chunk_length != 0:
                        print("writing chunk:", chunk_length)
                        chunk = self.rfile.read(chunk_length)
                        out_file.write(chunk)

                    # Each chunk is followed by an additional empty newline
                    # that we have to consume.
                    self.rfile.readline()

                    # Finally, a chunk size of 0 is an end indication
                    if chunk_length == 0:
                        break

def main():
  httpd = HTTPServer((HOST, PORT), TestHTTPRequestHandler)
  print("Serving at port:", httpd.server_port)
  httpd.serve_forever()


if __name__ == "__main__":
    main()
