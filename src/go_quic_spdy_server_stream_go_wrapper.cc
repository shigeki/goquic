#include "go_quic_spdy_server_stream_go_wrapper.h"
#include "net/quic/quic_session.h"
#include "net/quic/quic_data_stream.h"
#include "net/quic/quic_ack_notifier.h"
#include "base/strings/string_piece.h"
#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "base/strings/string_number_conversions.h"
#include "net/spdy/spdy_protocol.h"
#include "go_functions.h"

GoQuicSpdyServerStreamGoWrapper::GoQuicSpdyServerStreamGoWrapper(net::QuicStreamId id, net::QuicSpdySession* session)
  : net::QuicDataStream(id, session) {
}

GoQuicSpdyServerStreamGoWrapper::~GoQuicSpdyServerStreamGoWrapper() {
  UnregisterQuicServerStreamFromSession_C(go_quic_spdy_server_stream_);
}

void GoQuicSpdyServerStreamGoWrapper::SetGoQuicSpdyServerStream(void* go_quic_spdy_server_stream) {
  go_quic_spdy_server_stream_ = go_quic_spdy_server_stream;
}

void GoQuicSpdyServerStreamGoWrapper::CloseReadSide_() {
  ReliableQuicStream::CloseReadSide();
}

void GoQuicSpdyServerStreamGoWrapper::WriteOrBufferData_(
    base::StringPiece buffer, bool fin, net::QuicAckNotifier::DelegateInterface* delegate) {
  WriteOrBufferData(buffer, fin, delegate);
}

void GoQuicSpdyServerStreamGoWrapper::OnStreamHeadersComplete(bool fin, size_t frame_len) {
  QuicDataStream::OnStreamHeadersComplete(fin, frame_len);
  if (!ParseRequestHeaders(decompressed_headers().data(),
                           decompressed_headers().length())) {
    // Headers were invalid.
    SendErrorResponse();
  }
  MarkHeadersConsumed(decompressed_headers().length());
}

void GoQuicSpdyServerStreamGoWrapper::OnClose() {
  net::QuicDataStream::OnClose();

  DataStreamProcessorOnClose_C(go_quic_spdy_server_stream_);
}

void GoQuicSpdyServerStreamGoWrapper::OnDataAvailable() {
  while (HasBytesToRead()) {
    struct iovec iov;
    if (GetReadableRegions(&iov, 1) == 0) {
      // No more data to read.
      break;
    }
    DVLOG(1) << "Processed " << iov.iov_len << " bytes for stream " << id();
    body_.append(static_cast<char*>(iov.iov_base), iov.iov_len);

    if (content_length_ >= 0 &&
        static_cast<int>(body_.size()) > content_length_) {
      SendErrorResponse();
      return;
    }
    MarkConsumed(iov.iov_len);
  }
  if (!sequencer()->IsClosed()) {
    sequencer()->SetUnblocked();
    return;
  }

  // If the sequencer is closed, then the all the body, including the fin,
  // has been consumed.

  net::SpdyHeaderBlock::const_iterator it;
  for (it = request_headers_.begin(); it != request_headers_.end(); ++it) {
    std::string key = it->first.as_string();
    std::string value = it->second.as_string();
    uint32_t key_len = key.length();
    uint32_t value_len = value.length();
    DataStreamProcessorAddHeader_C(go_quic_spdy_server_stream_, key.c_str(), key_len, value.c_str(), value_len);
  }

  OnFinRead();

  //  if (write_side_closed() || fin_buffered()) {
  //    return;
  //  }

  //  if (request_headers_.empty()) {
  //    SendErrorResponse();
  //     return;
  // }

  // if (content_length_ > 0 &&
  //    content_length_ != static_cast<int>(body_.size())) {
  //  SendErrorResponse();
  //  return;
  //}
  DataStreamProcessorOnFinRead_C(go_quic_spdy_server_stream_);
}

bool GoQuicSpdyServerStreamGoWrapper::ParseRequestHeaders(const char* data,
                                               uint32 data_len) {
  DCHECK(headers_decompressed());
  net::SpdyFramer framer(net::HTTP2);
  size_t len = framer.ParseHeaderBlockInBuffer(data,
                                               data_len,
                                               &request_headers_);
  DCHECK_LE(len, data_len);
  if (len == 0 || request_headers_.empty()) {
    return false;  // Headers were invalid.
  }

  if (data_len > len) {
    body_.append(data + len, data_len - len);
  }
  if (ContainsKey(request_headers_, "content-length")) {
    std::string delimiter;
    delimiter.push_back('\0');
    std::vector<std::string> values =
        base::SplitString(request_headers_["content-length"], delimiter,
                          base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);

    for (const std::string& value : values) {
      int new_value;
      if (!base::StringToInt(value, &new_value) || new_value < 0) {
        return false;
      }
      if (content_length_ < 0) {
        content_length_ = new_value;
        continue;
      }
      if (new_value != content_length_) {
        return false;
      }
    }
  }

  return true;
}

void GoQuicSpdyServerStreamGoWrapper::SendErrorResponse() {
  DVLOG(1) << "Sending error response for stream " << id();
  // TODO Implement
}
