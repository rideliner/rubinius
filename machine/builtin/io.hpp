#ifndef RBX_BUILTIN_IO_HPP
#define RBX_BUILTIN_IO_HPP

#include "object_utils.hpp"

#include "builtin/byte_array.hpp"
#include "builtin/encoding.hpp"
#include "builtin/fixnum.hpp"
#include "builtin/object.hpp"
#include "builtin/string.hpp"

namespace rubinius {
  class ByteArray;
  class String;
  class Encoding;

  class IO : public Object {
    static int max_descriptors_;

  public:
    const static object_type type = IOType;

    attr_accessor(descriptor, Fixnum);
    attr_accessor(path, String);
    attr_accessor(ibuffer, Object);
    attr_accessor(mode, Fixnum);
    attr_accessor(eof, Object);
    attr_accessor(lineno, Fixnum);
    attr_accessor(sync, Object);
    attr_accessor(external, Encoding);
    attr_accessor(internal, Encoding);
    attr_accessor(autoclose, Object);

    static void bootstrap(STATE);
    static void initialize(STATE, IO* obj) {
      obj->descriptor(nil<Fixnum>());
      obj->path(nil<String>());
      obj->ibuffer(nil<Object>());
      obj->mode(nil<Fixnum>());
      obj->eof(cFalse);
      obj->lineno(Fixnum::from(0));
      obj->sync(nil<Object>());
      obj->external(nil<Encoding>());
      obj->internal(nil<Encoding>());
      obj->autoclose(nil<Object>());
    }

    static IO* create(STATE, int fd);

    static int max_descriptors() {
      return max_descriptors_;
    }

    native_int to_fd();
    void set_mode(STATE);
    void force_read_only(STATE);
    void force_write_only(STATE);
    static void finalize(STATE, IO* io);

  /* Class primitives */

    // Rubinius.primitive :io_allocate
    static IO*      allocate(STATE, Object* self);

    // Rubinius.primitive :io_connect_pipe
    static Object*  connect_pipe(STATE, IO* lhs, IO* rhs);

    // Rubinius.primitive :io_open
    static Fixnum*  open(STATE, String* path, Fixnum* mode, Fixnum* perm);

    static native_int open_with_cloexec(STATE, const char* path, int mode, int permissions);
    static void new_open_fd(STATE, native_int fd);
    static void update_max_fd(STATE, native_int fd);

    /**
     *  Perform select() on descriptors.
     *
     *  @todo Replace with an evented version when redoing events. --rue
     */
    // Rubinius.primitive :io_select
    static Object*  select(STATE, Object* readables, Object* writables, Object* errorables, Object* timeout);

    // Rubinius.primitive :io_fnmatch
    static Object* fnmatch(STATE, String* pattern, String* path, Fixnum* flags);

  /* Instance primitives */

    // Rubinius.primitive :io_ensure_open
    Object* ensure_open(STATE);

    /**
     *  Directly read up to number of bytes from descriptor.
     *
     *  Returns cNil at EOF.
     */
    // Rubinius.primitive :io_sysread
    Object* sysread(STATE, Fixnum* number_of_bytes);

    // Rubinius.primitive :io_read_if_available
    Object* read_if_available(STATE, Fixnum* number_of_bytes);

    // Rubinius.primitive :io_socket_read
    Object* socket_read(STATE, Fixnum* bytes, Fixnum* flags, Fixnum* type);

    // Rubinius.primitive :io_seek
    Integer* seek(STATE, Integer* amount, Fixnum* whence);

    // Rubinius.primitive :io_truncate
    static Integer* truncate(STATE, String* name, Fixnum* off);

    // Rubinius.primitive :io_ftruncate
    Integer* ftruncate(STATE, Fixnum* off);

    // Rubinius.primitive :io_write
    Object* write(STATE, String* buf);

    // Rubinius.primitive :io_reopen
    Object* reopen(STATE, IO* other);

    // Rubinius.primitive :io_reopen_path
    Object* reopen_path(STATE, String* other, Fixnum * mode);

    // Rubinius.primitive :io_close
    Object* close(STATE);

    // Rubinius.primitive :io_send_io
    Object* send_io(STATE, IO* io);

    // Rubinius.primitive :io_recv_fd
    Object* recv_fd(STATE);

    /**
     *  Shutdown a full-duplex descriptor's read and/or write stream.
     *
     *  Careful with this, it applies to full-duplex only.
     *  It also shuts the stream *in all processes*, not
     *  just the current one.
     */
    // Rubinius.primitive :io_shutdown
    Object* shutdown(STATE, Fixnum* how);

    // Rubinius.primitive :io_query
    Object* query(STATE, Symbol* op);

    // Rubinius.primitive :io_write_nonblock
    Object* write_nonblock(STATE, String* buf);

    // Rubinius.primitive :io_advise
    Object* advise(STATE, Symbol* advice_name, Integer* offset, Integer* len);

    void set_nonblock(STATE);

    class Info : public TypeInfo {
    public:
      BASIC_TYPEINFO(TypeInfo)
    };

  };

#define IOBUFFER_SIZE 32768U

  class IOBuffer : public Object {
  public:
    const static size_t fields = 7;
    const static object_type type = IOBufferType;

    attr_accessor(storage, ByteArray);
    attr_accessor(total, Fixnum);
    attr_accessor(used, Fixnum);
    attr_accessor(start, Fixnum);
    attr_accessor(eof, Object);
    attr_accessor(write_synced, Object);

    static void initialize(STATE, IOBuffer* obj) {
      obj->storage(nil<ByteArray>());
      obj->total(Fixnum::from(0));
      obj->used(Fixnum::from(0));
      obj->start(Fixnum::from(0));
      obj->eof(cFalse);
      obj->write_synced(cTrue);
    }

    static IOBuffer* create(STATE, size_t bytes = IOBUFFER_SIZE);
    // Rubinius.primitive :iobuffer_allocate
    static IOBuffer* allocate(STATE);

    // Rubinius.primitive :iobuffer_unshift
    Object* unshift(STATE, String* str, Fixnum* start_pos);

    // Rubinius.primitive :iobuffer_fill
    Object* fill(STATE, IO* io);

    void reset(STATE);
    String* drain(STATE);
    char* byte_address();
    size_t left();
    char* at_unused();
    void read_bytes(STATE, size_t bytes);

    class Info : public TypeInfo {
    public:
      BASIC_TYPEINFO(TypeInfo)
    };
  };

}

#endif
