#ifndef __GO_QUIC_SPDY_SERVER_STREAM_GO_WRAPPER_H__
#define __GO_QUIC_SPDY_SERVER_STREAM_GO_WRAPPER_H__

#include "net/quic/quic_data_stream.h"
#include "net/quic/quic_ack_notifier.h"
#include "base/strings/string_piece.h"
#include "net/spdy/spdy_framer.h"

class GoQuicSpdyServerStreamGoWrapper : public net::QuicDataStream {
 public:

  GoQuicSpdyServerStreamGoWrapper(net::QuicStreamId id, net::QuicSpdySession* session);
  ~GoQuicSpdyServerStreamGoWrapper() override;

  void SetGoQuicSpdyServerStream(void* go_quic_spdy_server_stream);

  void OnStreamHeadersComplete(bool fin, size_t frame_len) override;
  void OnDataAvailable() override;
  void OnClose() override;

  // we need a proxy because ReliableQuicStream::WriteOrBufferData & StopReading() is protected.
  // we could access this function from C (go) side.
  void WriteOrBufferData_(base::StringPiece buffer, bool fin);
  void StopReading_();
 protected:
  net::SpdyHeaderBlock* request_headers() { return &request_headers_; }

  const std::string& body() { return body_; }

 private:
  void* go_quic_spdy_server_stream_;

  // Parses the request headers from |data| to |request_headers_|.
  // Returns false if there was an error parsing the headers.
  bool ParseRequestHeaders(const char* data, uint32 data_len);

  void SendErrorResponse();

  net::SpdyHeaderBlock request_headers_;
  int content_length_;
  std::string body_;
  DISALLOW_COPY_AND_ASSIGN(GoQuicSpdyServerStreamGoWrapper);
};

#endif
