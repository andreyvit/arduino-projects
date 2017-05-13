#define WIFIESP8601_DEBUG 1

class WiFiViaESP8601;

typedef enum {
  WIFI_OK,
  WIFI_TIMEOUT,
  WIFI_ERROR,
} WiFiCompletionStatus;

class WiFiChannel {
public:
  typedef void (*Callback)(void *, WiFiChannel *chan);
  typedef void (*DataCallback)(void *, WiFiChannel *chan, unsigned int size);
  typedef void (*EndCallback)(void *, WiFiChannel *chan, WiFiCompletionStatus status);

  typedef enum {
    S_FREE = '.',
    S_CONNECTING = 'c',
    S_CONNECTING_CONFIRMED = 'd',
    S_CONNECTED = 'C',
    S_PREP_TO_SEND = 's',
    S_WAITING_TO_SEND = 'w',
    S_SENDING = 'S',
    S_FAILED_TO_CONNECT = 'F',
    S_FAILED_TO_SEND = 'f',
} State;

  WiFiChannel(WiFiViaESP8601 *wifi, int chnum);

  void connect(const char *host, int port, unsigned long timeout, void *context, Callback connectFunc, DataCallback dataFunc, EndCallback closeFunc);
  bool prepareToSend(unsigned int size, Callback callback);
  
  bool prepareHTTPRequest(const char *method, const char *path, const char *host, int bodySize, Callback callback);
  void sendHTTPRequest(const char *method, const char *path, const char *host, int bodySize);

  State state() { return _state; }
  int chnum() { return _chnum; }

public /*internal*/:
  void handleOK();
  void handleError();
  void handleConnect();
  void handleClosed();
  void handleData(int size);
  void handleReadyToSend();
private:
  WiFiViaESP8601 &_wifi;
  int _chnum;
  State _state;

  void *_context;

  Callback _connectFunc;
  DataCallback _dataFunc;
  EndCallback _closeFunc;
  Callback _sendCallback;
};

class WiFiViaESP8601 {
public:
  typedef enum {
    S_RESET_REQUIRED = '!',
    S_INITIALIZED = 'I',
    S_JOINING = 'J',
    S_READY = 'R',
    S_BUSY = 'B',
  } State;

  typedef enum {
    RS_FIRST,
    RS_IPD_CHNUM,
    RS_IPD_SIZE,
    RS_CHAN_STATUS,
    RS_UNKNOWN,
    RS_SKIPLN,
  } RecvState;

  WiFiViaESP8601(Stream &serial);
  bool reset();
  void join(const char *ap, const char *pwd, unsigned long timeout);
  void process();
  void wait();
  WiFiChannel *channel();

  void debug(Stream &stream);

  State state() {
    return _state;
  }

  Stream &serial;

  static const char *OK;
private:
  void send(const char *command, const char *terminator);
  bool expect(const char *expected);

  enum { NCHANNELS = 4 };
  enum { BUFSIZE = 64 };

  State _state;
  WiFiChannel _channels[NCHANNELS];

  RecvState _rs;
  char _inbuf[BUFSIZE];
  byte _insize;
  int _cmdchnum;
  int _cmdsize;

  int _busychnum;

#if WIFIESP8601_DEBUG
  Stream *debugStream;
#endif

  void handleError();

  bool markBusy(int chnum);
  void endBusy();

  friend WiFiChannel;
};

const char *WiFiViaESP8601::OK = "OK";

static unsigned int itoa_len(unsigned int n) {
  if (n == 0) {
    return 1;
  }

  unsigned int count = 0;
  while (n != 0) {
    ++count;
    n /= 10;
  }
  return count;
}

WiFiViaESP8601::WiFiViaESP8601(Stream &serial) :
  serial(serial),
  _state(S_RESET_REQUIRED),
  _rs(RS_FIRST),
  _channels{WiFiChannel(this, 0), WiFiChannel(this, 1), WiFiChannel(this, 2), WiFiChannel(this, 3)}
{
}

bool WiFiViaESP8601::reset() {
#if WIFIESP8601_DEBUG
  if (debugStream) {
    debugStream->println(F("WiFi: reset..."));
  }
#endif
  _state = S_RESET_REQUIRED;
  serial.setTimeout(1000);

  serial.println(F("AT+RST"));
  if (!expect("ready")) {
#if WIFIESP8601_DEBUG
    if (debugStream) {
      debugStream->println(F("WiFi: reset: AT+RST failed"));
    }
#endif
    return false;
  }

  serial.println(F("ATE0"));
  if (!expect(OK)) {
#if WIFIESP8601_DEBUG
    if (debugStream) {
      debugStream->println(F("WiFi: reset: ATE0 failed"));
    }
#endif
    return false;
  }

  serial.println(F("AT+CWMODE=1"));
  if (!expect(OK)) {
#if WIFIESP8601_DEBUG
    if (debugStream) {
      debugStream->println(F("WiFi: reset: AT+CWMODE failed"));
    }
#endif
    return false;
  }

  serial.println(F("AT+CIPMUX=1"));
  if (!expect(OK)) {
#if WIFIESP8601_DEBUG
    if (debugStream) {
      debugStream->println(F("WiFi: reset: AT+CIPMIX failed"));
    }
#endif
    return false;
  }

#if WIFIESP8601_DEBUG
  if (debugStream) {
    debugStream->println(F("WiFi: reset finished"));
  }
#endif

  _state = S_INITIALIZED;
}

void WiFiViaESP8601::join(const char *ap, const char *pwd, unsigned long timeout) {
  if (_state != S_INITIALIZED) {
#if WIFIESP8601_DEBUG
    if (debugStream) {
      debugStream->println(F("WiFi: connect: wrong state"));
    }
#endif
    return false;
  }

#if WIFIESP8601_DEBUG
  if (debugStream) {
    debugStream->print(F("WiFi: joining "));
    debugStream->print(ap);
    debugStream->println(F("..."));
  }
#endif

  serial.print(F("AT+CWJAP=\""));
  serial.print(ap);
  serial.print(F("\",\""));
  serial.print(pwd);
  serial.println('"');
  _state = S_JOINING;
}

bool WiFiViaESP8601::expect(const char *expected) {
  return serial.find((char *) expected);
}

void WiFiViaESP8601::debug(Stream &stream) {
#if WIFIESP8601_DEBUG
  debugStream = &stream;
#endif
}

void WiFiViaESP8601::process() {
  for (int ch = serial.read(); ch >= 0; ch = serial.read()) {
//    if (debugStream) {
//      debugStream->print(F("WiFi: incoming char: "));
//      debugStream->print((int) ch);
//      debugStream->print(F(" which is "));
//      debugStream->print((char) ch);
//      debugStream->println();
//    }

    if (ch == '\r') {
      continue;
    }

    if (_rs == RS_FIRST && _insize == 0 && (ch == ' ' || ch == '\n')) {
      continue;
    }

    if (_rs == RS_FIRST && _insize == 0 && ch == '>' && _busychnum >= 0) {
      _channels[_busychnum].handleReadyToSend();
      continue;
    }

    if (ch == ',' || ch == '\n' || ch == ':') {
      _inbuf[_insize] = 0;

#if WIFIESP8601_DEBUG >= 2
      if (debugStream && _rs != RS_UNKNOWN) {
        debugStream->print(F("WiFi: incoming arg: "));
        debugStream->write(_inbuf, _insize);
        debugStream->println();
      }
#endif

      if (_rs == RS_FIRST) {
        if (_inbuf[0] >= '0' && _inbuf[0] <= '9') {
          _cmdchnum = atoi(_inbuf);
          _rs = RS_CHAN_STATUS;
        } else if (_insize == 2) {
          if (_inbuf[0] == 'O' && _inbuf[1] == 'K') {
            endBusy();
            _rs = RS_SKIPLN;
          } else {
            _rs = RS_UNKNOWN;
          }
        } else if (_insize == 4) {
          if (_inbuf[0] == '+' && _inbuf[1] == 'I' && _inbuf[2] == 'P' && _inbuf[3] == 'D') {
            _rs = RS_IPD_CHNUM;
          } else if (_inbuf[0] == 'F' && _inbuf[1] == 'A' && _inbuf[2] == 'I' && _inbuf[3] == 'L') {
            handleError();
            _rs = RS_SKIPLN;
          } else {
            _rs = RS_UNKNOWN;
          }
        } else if (_insize == 5) {
          if (_inbuf[0] == 'E' && _inbuf[1] == 'R' && _inbuf[2] == 'R' && _inbuf[3] == 'O' && _inbuf[4] == 'R') {
            handleError();
            _rs = RS_SKIPLN;
          } else {
            _rs = RS_UNKNOWN;
          }
        } else if (_insize == 7) {
          if (_inbuf[0] == 'S' && _inbuf[1] == 'E' && _inbuf[2] == 'N' && _inbuf[3] == 'D' && _inbuf[4] == ' ' && _inbuf[5] == 'O' && _inbuf[6] == 'K') {
            endBusy();
            _rs = RS_SKIPLN;
          } else {
            _rs = RS_UNKNOWN;
          }
        } else {
          _rs = RS_UNKNOWN;
        }
      } else if (_rs == RS_IPD_CHNUM) {
        _cmdchnum = atoi(_inbuf);
        _rs = RS_IPD_SIZE;
      } else if (_rs == RS_IPD_SIZE) {
        int size = atoi(_inbuf);
#if WIFIESP8601_DEBUG
        if (debugStream) {
          debugStream->print(F("WiFi: data on channel "));
          debugStream->print(_cmdchnum);
          debugStream->print(F(", size "));
          debugStream->println(size);
        }
#endif
        _rs = RS_SKIPLN;
        ch = '\n'; // simulate end of line to reset all state
        _channels[_cmdchnum].handleData(size);
      } else if (_rs == RS_CHAN_STATUS) {
        if (_insize == 7 && 0 == strcmp(_inbuf, "CONNECT")) {
#if WIFIESP8601_DEBUG
          if (debugStream) {
            debugStream->print(F("WiFi: channel "));
            debugStream->print(_cmdchnum);
            debugStream->println(F(" connected"));
          }
#endif
          _channels[_cmdchnum].handleConnect();
        } else if (_insize == 6 && 0 == strcmp(_inbuf, "CLOSED")) {
#if WIFIESP8601_DEBUG
          if (debugStream) {
            debugStream->print(F("WiFi: channel "));
            debugStream->print(_cmdchnum);
            debugStream->println(F(" closed"));
          }
#endif
          _channels[_cmdchnum].handleClosed();
        } else {
#if WIFIESP8601_DEBUG
          if (debugStream) {
            debugStream->print(F("WiFi: unknown event "));
            debugStream->write(_inbuf, _insize);
            debugStream->print(F(" on channel "));
            debugStream->println(_cmdchnum);
          }
#endif
        }
      }

      if (ch == '\n') {
#if WIFIESP8601_DEBUG
        if (_rs == RS_UNKNOWN) {
          if (debugStream) {
            debugStream->print(F("WiFi: recv unexpected line: "));
            debugStream->println(_inbuf);
          }
        }
#endif
        _rs = RS_FIRST;
        _insize = 0;
        continue;
      } else if (_rs != RS_UNKNOWN) {
        _insize = 0;
        continue;
      }
    }

    if (_insize < BUFSIZE - 1) {
      _inbuf[_insize++] = ch;
    }
  }
}

bool WiFiViaESP8601::markBusy(int chnum) {
  if (_state != S_READY) {
    return false;
  }
  _state = S_BUSY;
  _busychnum = chnum;
  return true;
}

void WiFiViaESP8601::endBusy() {
  if (_state == S_BUSY) {
    _state = S_READY;
#if WIFIESP8601_DEBUG
    if (debugStream) {
      debugStream->print(F("WiFi: OK received for channel "));
      debugStream->println(_busychnum);
    }
#endif    
    _channels[_busychnum].handleOK();
  } else if (_state == S_JOINING) {
#if WIFIESP8601_DEBUG
    if (debugStream) {
      debugStream->println(F("WiFi: joined"));
    }
#endif
    _state = S_READY;
  } else {
#if WIFIESP8601_DEBUG
    if (debugStream) {
      debugStream->println(F("WiFi: spurious OK received"));
    }
#endif    
  }
}

void WiFiViaESP8601::handleError() {
  if (_state == S_BUSY) {
    _state = S_READY;
#if WIFIESP8601_DEBUG
    if (debugStream) {
      debugStream->print(F("WiFi: error received for channel "));
      debugStream->println(_busychnum);
    }
#endif    
    _channels[_busychnum].handleError();
  } else if (_state == S_JOINING) {
#if WIFIESP8601_DEBUG
    if (debugStream) {
      debugStream->println(F("WiFi: failed to join"));
    }
#endif
    _state = S_INITIALIZED;
  } else {
#if WIFIESP8601_DEBUG
    if (debugStream) {
      debugStream->println(F("WiFi: spurious error received"));
    }
#endif    
  }
}

void WiFiViaESP8601::wait() {
  while (_state == S_BUSY || _state == S_JOINING) {
    process();
  }
}

WiFiChannel *WiFiViaESP8601::channel() {
  for (int chn = 0; chn < NCHANNELS; ++chn) {
    WiFiChannel *c = &_channels[chn];
    if (c->state() == WiFiChannel::S_FREE) {
      return c;
    }
  }

  return NULL;
}

WiFiChannel::WiFiChannel(WiFiViaESP8601 *wifi, int chnum) :
  _wifi(*wifi), _chnum(chnum), _state(S_FREE)
{
}

void WiFiChannel::connect(const char *host, int port, unsigned long timeout, void *context, Callback connectFunc, DataCallback dataFunc, EndCallback closeFunc) {
  if (!_wifi.markBusy(_chnum)) {
#if WIFIESP8601_DEBUG
    if (_wifi.debugStream) {
      _wifi.debugStream->print(F("WiFi: cannot connect to "));
      _wifi.debugStream->print(host);
      _wifi.debugStream->println(F(" because the module is currently busy with another command."));
    }
#endif
    return;
  }

  _context = context;
  _connectFunc = connectFunc;
  _dataFunc = dataFunc;
  _closeFunc = closeFunc;

#if WIFIESP8601_DEBUG
  if (_wifi.debugStream) {
    _wifi.debugStream->print(F("WiFi: connecting to "));
    _wifi.debugStream->print(host);
    _wifi.debugStream->print(F(" on channel "));
    _wifi.debugStream->print(_chnum);
    _wifi.debugStream->println(F("..."));
  }
#endif

  _wifi.serial.print(F("AT+CIPSTART="));
  _wifi.serial.print(_chnum);
  _wifi.serial.print(F(",\"TCP\",\""));
  _wifi.serial.print(host);
  _wifi.serial.print(F("\","));
  _wifi.serial.println(port);

  _state = S_CONNECTING;
}

void WiFiChannel::handleOK() {
  if (_state == S_CONNECTING_CONFIRMED) {
#if WIFIESP8601_DEBUG
    if (_wifi.debugStream) {
      _wifi.debugStream->print(F("WiFi: channel "));
      _wifi.debugStream->print(_chnum);
      _wifi.debugStream->println(F(" connected."));
    }
#endif
    _state = S_CONNECTED;
    if (_connectFunc != NULL) {
      _connectFunc(_context, this);
    }
  } else if (_state == S_CONNECTING) {
#if WIFIESP8601_DEBUG
    if (_wifi.debugStream) {
      _wifi.debugStream->print(F("WiFi: channel "));
      _wifi.debugStream->print(_chnum);
      _wifi.debugStream->println(F(" connected, but didn't receive the connection event."));
    }
#endif
    // TODO: handle
    _state = S_CONNECTED;
  } else if (_state == S_PREP_TO_SEND) {
    _wifi.markBusy(_chnum);
    _state = S_WAITING_TO_SEND;
#if WIFIESP8601_DEBUG
    if (_wifi.debugStream) {
      _wifi.debugStream->print(F("WiFi: channel "));
      _wifi.debugStream->print(_chnum);
      _wifi.debugStream->println(F(" is readying to send data."));
    }
#endif
  } else if (_state == S_SENDING) {
#if WIFIESP8601_DEBUG
    if (_wifi.debugStream) {
      _wifi.debugStream->print(F("WiFi: channel "));
      _wifi.debugStream->print(_chnum);
      _wifi.debugStream->println(F(" finished sending."));
    }
#endif
    _state = S_CONNECTED;
  }
}

void WiFiChannel::handleError() {
  if (_state == S_CONNECTING || _state == S_CONNECTING_CONFIRMED) {
#if WIFIESP8601_DEBUG
    if (_wifi.debugStream) {
      _wifi.debugStream->print(F("WiFi: channel "));
      _wifi.debugStream->print(_chnum);
      _wifi.debugStream->println(F(" failed to connect."));
    }
#endif
    _state = S_FAILED_TO_CONNECT;
  } else if (_state == S_PREP_TO_SEND || _state == S_WAITING_TO_SEND || _state == S_SENDING) {
#if WIFIESP8601_DEBUG
    if (_wifi.debugStream) {
      _wifi.debugStream->print(F("WiFi: channel "));
      _wifi.debugStream->print(_chnum);
      _wifi.debugStream->println(F(" failed to send data."));
    }
#endif
    _state = S_FAILED_TO_SEND;
    // TODO: close connection
  }
}

void WiFiChannel::handleReadyToSend() {
#if WIFIESP8601_DEBUG
    if (_wifi.debugStream) {
      _wifi.debugStream->print(F("WiFi: channel "));
      _wifi.debugStream->print(_chnum);
      _wifi.debugStream->println(F(" is ready to send data."));
    }
#endif
  if (_state == S_WAITING_TO_SEND) {
    if (_sendCallback != NULL) {
      _sendCallback(_context, this);
      _sendCallback = NULL;
    }
  }
}

void WiFiChannel::handleConnect() {
  if (_state == S_CONNECTING) {
    _state = S_CONNECTING_CONFIRMED;
  }
}

void WiFiChannel::handleClosed() {
  if (_closeFunc != NULL) {
    WiFiCompletionStatus cstatus = WIFI_OK;
    if (_state == S_FAILED_TO_CONNECT || _state == S_FAILED_TO_SEND) {
      cstatus = WIFI_ERROR;
    }
    _closeFunc(_context, this, cstatus);
    _context = NULL;
    _connectFunc = NULL;
    _dataFunc = NULL;
    _closeFunc = NULL;
  }
  _state = S_FREE;
}

void WiFiChannel::handleData(int size) {
  if (_dataFunc != NULL) {
    _dataFunc(_context, this, size);
  } else {
    for (int i = 0; i < size; ++i) {
      _wifi.serial.read();
    }
  }
}

bool WiFiChannel::prepareToSend(unsigned int size, Callback callback) {
  if (_state != S_CONNECTED) {
    return false;
  }
  if (!_wifi.markBusy(_chnum)) {
#if WIFIESP8601_DEBUG
    if (_wifi.debugStream) {
      _wifi.debugStream->println(F("WiFi: cannot send because the module is currently busy with another command."));
    }
#endif
    return false;
  }

  _sendCallback = callback;

  _wifi.serial.print(F("AT+CIPSEND="));
  _wifi.serial.print(_chnum);
  _wifi.serial.print(',');
  _wifi.serial.println(size);
  _state = S_PREP_TO_SEND;
  return true;
}

bool WiFiChannel::prepareHTTPRequest(const char *method, const char *path, const char *host, int bodySize, Callback callback) {
  unsigned int mlen = strlen(method);
  unsigned int plen = strlen(path);
  unsigned int hlen = strlen(host);

  unsigned int blen = 0;
  if (bodySize >= 0) {
    blen = 18 + itoa_len(bodySize);
  }
  
  return prepareToSend(mlen + 1 + plen + 17 + hlen + blen + 23, callback);
}

void WiFiChannel::sendHTTPRequest(const char *method, const char *path, const char *host, int bodySize) {
  _wifi.serial.write(method);
  _wifi.serial.write(' ');
  _wifi.serial.write(path);
  _wifi.serial.print(F(" HTTP/1.0\r\nHost: "));
  _wifi.serial.write(host);
  if (bodySize >= 0) {
    _wifi.serial.print(F("\r\nContent-Length: "));
    _wifi.serial.print(bodySize);
  }
  _wifi.serial.print(F("\r\nConnection: close\r\n\r\n"));
}

//void WiFiViaESP8601::dumpTo(Stream &stream) {
//  stream.print(F("WiFi: state "));
//  stream.print((char) _state);
//  stream.println();
//}

WiFiViaESP8601 wifi(Serial3);
WiFiChannel *chan;

enum { GBUFSIZE = 256 };
char gbuf[GBUFSIZE];
int gsize;

void google_send_rq(void *, WiFiChannel *chan) {
  chan->sendHTTPRequest("GET", "/", "google.com", -1);
  Serial.println(F("Google request sent."));
}

void google_connected(void *, WiFiChannel *chan) {
  Serial.println(F("Google connected!"));
  if (!chan->prepareHTTPRequest("GET", "/", "google.com", -1, google_send_rq)) {
    Serial.println(F("Failed to send request to Google."));
  }
}

void google_data(void *, WiFiChannel *chan, unsigned int size) {
  for (int i = 0; i < size; ++i) {
    int ch = wifi.serial.read();
    if (gsize < GBUFSIZE - 1) {
      gbuf[gsize++] = ch;
    }
  }

  gbuf[gsize] = 0;
  Serial.print(F("Received from Google: "));
  Serial.println(gbuf);
  Serial.println(F("-----------------------------"));
}

void google_end(void *, WiFiChannel *chan, WiFiCompletionStatus status) {
  if (status == WIFI_OK) {
    Serial.println("Google disconnected.");
  } else {
    Serial.println("Google connection failed.");
  }
}

void setup() {
  Serial.begin(9600);
  Serial3.begin(115200);

  wifi.debug(Serial);
  wifi.reset();
  wifi.join("Tarantsov-ASUS", "**PWHERE**", 10000);
  wifi.wait();
  chan = wifi.channel();

  gsize = 0;
  chan->connect("192.168.1.102", 5000, 10000, NULL, google_connected, google_data, google_end);
}

void loop() {
  wifi.process();
}

