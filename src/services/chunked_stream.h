#ifndef CHUNKED_STREAM_H
#define CHUNKED_STREAM_H

#include <Stream.h>

class ChunkedStream : public Stream {
 public:
  explicit ChunkedStream(Stream* inner) : inner_(inner) {}

  int available() override {
    if (eof_) return 0;
    if (chunk_left_ == 0 && !readChunkHeader()) return 0;
    int avail = inner_->available();
    return avail < chunk_left_ ? avail : chunk_left_;
  }

  int read() override {
    if (eof_) return -1;
    if (chunk_left_ == 0 && !readChunkHeader()) return -1;
    int c = inner_->read();
    if (c >= 0) {
      chunk_left_--;
      if (chunk_left_ == 0) {
        // Read trailing \r\n
        inner_->read();
        inner_->read();
      }
    }
    return c;
  }

  int peek() override {
    if (eof_) return -1;
    if (chunk_left_ == 0 && !readChunkHeader()) return -1;
    return inner_->peek();
  }
  
  size_t write(uint8_t) override { return 0; }

 private:
  bool readChunkHeader() {
    if (eof_) return false;
    String header = inner_->readStringUntil('\n');
    header.trim();
    if (header.length() == 0) return false;
    chunk_left_ = strtol(header.c_str(), nullptr, 16);
    if (chunk_left_ == 0) {
      eof_ = true;
      return false;
    }
    return true;
  }

  Stream* inner_;
  long chunk_left_ = 0;
  bool eof_ = false;
};

#endif
