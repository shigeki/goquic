package goquic

// #include <stddef.h>
// #include "src/adaptor.h"
import "C"
import (
	"net/http"
	"unsafe"
)

//   (For Quic(Server|Client)Stream)
type DataStreamProcessor interface {
	ProcessData(writer QuicStream, buffer []byte) int
	// Called when there's nothing to read. Called on server XXX(serialx): Not called on client
	OnFinRead(writer QuicStream)
	// Called when the connection is closed. Called on client XXX(serialx): Not called on server
	OnClose(writer QuicStream)
}

//   (For QuicServerSession)
type IncomingDataStreamCreator interface {
	CreateIncomingDynamicStream(streamId uint32) DataStreamProcessor
}

//   (For QuicClientSession)
type OutgoingDataStreamCreator interface {
	CreateOutgoingDynamicStream() DataStreamProcessor
}

type QuicStream interface {
	UserStream() DataStreamProcessor
	WriteHeader(header http.Header, is_body_empty bool)
	WriteOrBufferData(body []byte, fin bool)
	StopReading()
	AddHeader(key string, value string)
	GetHeader() http.Header
}

/*
             (Incoming/Outgoing)DataStreamCreator (a.k.a Session)
                                  |
                                  |   creates domain-specific stream (i.e. spdy, ...)
                                  v
   QuicStream -- owns -->  DataStreamProcessor

*/

//export CreateIncomingDynamicStream
func CreateIncomingDynamicStream(session_c unsafe.Pointer, stream_id uint32, wrapper_c unsafe.Pointer) unsafe.Pointer {
	session := (*QuicServerSession)(session_c)
	userStream := session.streamCreator.CreateIncomingDynamicStream(stream_id)
	header := make(http.Header)
	stream := &QuicServerStream{
		userStream: userStream,
		session:    session,
		wrapper:    wrapper_c,
		header :    header,
	}

	// This is to prevent garbage collection. This is cleaned up on QuicServerStream.OnClose()
	session.quicServerStreams[stream] = true

	return unsafe.Pointer(stream)
}

//export DataStreamProcessorAddHeader
func DataStreamProcessorAddHeader(go_data_stream_processor_c unsafe.Pointer, key_c unsafe.Pointer, key_sz_c C.size_t, value_c unsafe.Pointer, value_sz_c C.size_t, isServer int) {
	var stream QuicStream
	if isServer > 0 {
		stream = (*QuicServerStream)(go_data_stream_processor_c)
	} else {
		stream = (*QuicClientStream)(go_data_stream_processor_c)
	}

	key := C.GoBytes(key_c, C.int(key_sz_c))
	value := C.GoBytes(value_c, C.int(value_sz_c))
	stream.AddHeader(string(key), string(value))
}

//export DataStreamProcessorOnFinRead
func DataStreamProcessorOnFinRead(go_data_stream_processor_c unsafe.Pointer, isServer int) {
	var stream QuicStream
	if isServer > 0 {
		stream = (*QuicServerStream)(go_data_stream_processor_c)
	} else {
		stream = (*QuicClientStream)(go_data_stream_processor_c)
	}
	stream.UserStream().OnFinRead(stream)
}

//export DataStreamProcessorOnClose
func DataStreamProcessorOnClose(go_data_stream_processor_c unsafe.Pointer, isServer int) {
	var stream QuicStream
	if isServer > 0 {
		stream = (*QuicServerStream)(go_data_stream_processor_c)
	} else {
		stream = (*QuicClientStream)(go_data_stream_processor_c)
	}
	stream.UserStream().OnClose(stream)
}

//export UnregisterQuicServerStreamFromSession
func UnregisterQuicServerStreamFromSession(go_stream_c unsafe.Pointer) {
	stream := (*QuicServerStream)(go_stream_c)
	delete(stream.session.quicServerStreams, stream)
}

//export UnregisterQuicClientStreamFromSession
func UnregisterQuicClientStreamFromSession(go_stream_c unsafe.Pointer) {
	stream := (*QuicClientStream)(go_stream_c)
	delete(stream.session.quicClientStreams, stream)
}
