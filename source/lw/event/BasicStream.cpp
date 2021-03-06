
#include <cstdlib>
#include <uv.h>

#include "lw/event/BasicStream.hpp"
#include "lw/event/Promise.impl.hpp"

namespace lw {
namespace event {

namespace _details {
    struct WriteRequest {
        WriteRequest( void ){
            request.data = (void*)this;
        }

        uv_write_t request;
        Promise< std::size_t > promise;
        std::size_t size;
    };
}

// ---------------------------------------------------------------------------------------------- //

BasicStream::BasicStream( uv_stream_s* handle ):
    m_state( nullptr )
{
    auto state_ptr = std::make_shared< _State >();
    state_ptr->handle = handle;
    state( state_ptr );
}

// ---------------------------------------------------------------------------------------------- //

BasicStream::_State::~_State(void){
    if (handle) {
        uv_close((uv_handle_t*)handle, [](uv_handle_t* handle){ std::free(handle); });
    }
}

// ---------------------------------------------------------------------------------------------- //

void BasicStream::state(const std::shared_ptr<_State>& state){
    m_state = state;
    m_state->handle->data   = (void*)m_state.get();
    m_state->read_count     = 0;
    m_state->read_callback  = nullptr;
}

// ---------------------------------------------------------------------------------------------- //

void BasicStream::stop_read( void ){
    int res = uv_read_stop( m_state->handle );
    m_state->read_callback = nullptr;
    if( res < 0 ){
        throw LW_UV_ERROR( StreamError, res );
    }
    _stop_read();
}

// ---------------------------------------------------------------------------------------------- //

Future< std::size_t > BasicStream::write( buffer_ptr_t buffer ){
    auto write_req = std::make_shared< _details::WriteRequest >();
    write_req->size = buffer->size();
    uv_buf_t buffers[ 1 ];
    *buffers = uv_buf_init( (char*)buffer->data(), buffer->size() );
    int res = uv_write(
        &write_req->request,
        m_state->handle,
        buffers, 1,
        []( uv_write_t* req, int status ){
            auto* write_req = (_details::WriteRequest*)req->data;
            auto& promise   = write_req->promise;
            if( status < 0 ){
                promise.reject( LW_UV_ERROR( StreamError, status ) );
            }
            else {
                promise.resolve( write_req->size );
            }
        }
    );

    if( res < 0 ){
        throw LW_UV_ERROR( StreamError, res );
    }

    auto state = m_state;
    return write_req->promise.future()
        .then([ write_req, state, buffer ]( const std::size_t bytes_written ){
            return bytes_written;
        })
    ;
}

// ---------------------------------------------------------------------------------------------- //

Future<std::size_t> BasicStream::_read(void){
    int res = uv_read_start(
        m_state->handle,
        [](uv_handle_t* handle, std::size_t size, uv_buf_t* out_buffer){
            // Allocate a buffer for libuv to read into.
            auto state = ((_State*)handle->data)->shared_from_this();
            memory::Buffer& buffer = BasicStream( state )._next_read_buffer();
            *out_buffer = uv_buf_init((char*)buffer.data(), buffer.size());
        },
        [](uv_stream_t* handle, long int size, const uv_buf_t* buffer){
            // Data has been read, or an error encountered.
            auto state  = ((_State*)handle->data)->shared_from_this();
            auto stream = BasicStream(state);

            if (size == UV_EOF) {
                // End of file, trigger a stop.
                stream._stop_read();
                state.reset();
            }
            else {
                // More data is available, update our state and call back.
                state->read_count += size;
                state->read_callback(
                    buffer_ptr_t(
                        new memory::Buffer((memory::byte*)buffer->base, size),
                        [state](memory::Buffer* buffer){
                            BasicStream stream = state;
                            stream._release_read_buffer(buffer->data());
                            delete buffer;
                        }
                    )
                );
            }
        }
    );

    if (res < 0) {
        m_state->read_callback = nullptr;
        throw LW_UV_ERROR(StreamError, res);
    }

    return m_state->read_promise.future();
}

// ---------------------------------------------------------------------------------------------- //

void BasicStream::_stop_read( void ){
    m_state->read_promise.resolve( m_state->read_count );
    m_state->read_promise.reset();
    m_state->read_count = 0;
}

// ---------------------------------------------------------------------------------------------- //

memory::Buffer& BasicStream::_next_read_buffer( void ){
    if( m_state->idle_read_buffers.size() == 0 ){
        m_state->idle_read_buffers.emplace_back( memory::Buffer( 1024 ) );
    }
    m_state->active_read_buffers.splice(
        m_state->active_read_buffers.end(),
        m_state->idle_read_buffers,
        m_state->idle_read_buffers.begin()
    );
    return m_state->active_read_buffers.back();
}

// ---------------------------------------------------------------------------------------------- //

void BasicStream::_release_read_buffer( const void* base ){
    for(
        auto it = m_state->active_read_buffers.begin();
        it != m_state->active_read_buffers.end();
        ++it
    ){
        if( it->data() == base ){
            m_state->idle_read_buffers.splice(
                m_state->idle_read_buffers.end(),
                m_state->active_read_buffers,
                it
            );
            break;
        }
    }
}

}
}
