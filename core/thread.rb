#--
# Be very careful about calling raise in here! Thread has its own
# raise which, if you're calling raise, you probably don't want. Use
# Kernel.raise to call the proper raise.
#++
class Thread
  attr_reader :recursive_objects
  attr_reader :pid
  attr_reader :exception

  def self.new(*args, &block)
    thr = Rubinius.invoke_primitive :thread_s_new, args, block, self

    Rubinius::VariableScope.of_sender.locked!

    unless thr.send :initialized?
      raise ThreadError, "Thread#initialize not called"
    end

    return thr
  end

  def self.start(*args, &block)
    raise ArgumentError.new("no block passed to Thread.start") unless block

    Rubinius.invoke_primitive :thread_s_start, args, block, self
  end

  class << self
    alias_method :fork, :start
  end

  def initialize(*args, &block)
    unless block
      Kernel.raise ThreadError, "no block passed to Thread#initialize"
    end

    if @initialized
      Kernel.raise ThreadError, "already initialized thread"
    end

    @args = args
    @block = block
    @initialized = true

    Thread.current.group.add self
  end

  alias_method :__thread_initialize__, :initialize

  def initialized?
    @initialized
  end

  private :initialized?

  def self.current
    Rubinius.primitive :thread_current
    Kernel.raise PrimitiveFailure, "Thread.current primitive failed"
  end

  def self.allocate
    raise TypeError, "allocator undefined for Thread"
  end

  def self.pass
    Rubinius.primitive :thread_pass
    Kernel.raise PrimitiveFailure, "Thread.pass primitive failed"
  end

  def self.list
    Rubinius.primitive :thread_list
    Kernel.raise PrimitiveFailure, "Thread.list primitive failed"
  end

  def self.stop
    sleep
    nil
  end

  def wakeup
    Rubinius.primitive :thread_wakeup
    Kernel.raise ThreadError, "Thread#wakeup primitive failed, thread may be dead"
  end

  def priority
    Rubinius.primitive :thread_get_priority
    Kernel.raise ThreadError, "Thread#priority primitive failed"
  end

  def priority=(val)
    Rubinius.primitive :thread_set_priority
    Kernel.raise TypeError, "priority must be a Fixnum" unless val.kind_of? Fixnum
    Kernel.raise ThreadError, "Thread#priority= primitive failed"
  end

  def __context__
    Rubinius.primitive :thread_context
    Kernel.raise PrimitiveFailure, "Thread#__context__ primitive failed"
  end

  def mri_backtrace
    Rubinius.primitive :thread_mri_backtrace
    Kernel.raise PrimitiveFailure, "Thread#mri_backtrace primitive failed"
  end

  @abort_on_exception = false

  def self.abort_on_exception
    @abort_on_exception
  end

  def self.abort_on_exception=(val)
    @abort_on_exception = val
  end

  # It's also an instance method...
  def abort_on_exception=(val)
    @abort_on_exception = val
  end

  def abort_on_exception
    @abort_on_exception || false
  end

  def inspect
    stat = status()
    stat = "dead" unless stat

    "#<#{self.class}:0x#{object_id.to_s(16)} id=#{@thread_id} #{stat}>"
  end

  alias_method :to_s, :inspect

  def alive?
    Rubinius.synchronize(self) do
      @alive
    end
  end

  def stop?
    !alive? || sleeping?
  end

  def sleeping?
    Rubinius.synchronize(self) do
      @sleep
    end
  end

  def status
    if @alive
      if @sleep
        "sleep"
      else
        "run"
      end
    elsif @exception
      nil
    else
      false
    end
  end

  def join(timeout=undefined)
    if undefined.equal? timeout or nil.equal? timeout
      timeout = nil
    else
      timeout = Rubinius::Type.coerce_to_float timeout
    end

    value = Rubinius.invoke_primitive :thread_join, self, timeout

    if @exception
      Kernel.raise @exception
    else
      value
    end
  end

  def group
    @group
  end

  def raise(exc=undefined, msg=nil, trace=nil)
    Rubinius.synchronize(self) do
      return self unless @alive

      if undefined.equal? exc
        no_argument = true
        exc = active_exception
      end

      if exc.respond_to? :exception
        exc = exc.exception msg
        Kernel.raise TypeError, 'exception class/object expected' unless Exception === exc
        exc.set_backtrace trace if trace
      elsif no_argument
        exc = RuntimeError.exception nil
      elsif exc.kind_of? String
        exc = RuntimeError.exception exc
      else
        Kernel.raise TypeError, 'exception class/object expected'
      end

      if $DEBUG
        STDERR.puts "Exception: #{exc.message} (#{exc.class})"
      end

      if self == Thread.current
        Kernel.raise exc
      else
        Rubinius.invoke_primitive :thread_raise, self, exc
      end
    end
  end

  def [](key)
    locals_aref(Rubinius::Type.coerce_to_symbol(key))
  end

  def locals_aref(key)
    Rubinius.primitive :thread_locals_aref
    raise PrimitiveFailure, "Thread#locals_aref primitive failed"
  end
  private :locals_aref

  def []=(key, value)
    locals_store(Rubinius::Type.coerce_to_symbol(key), value)
  end

  def locals_store(key, value)
    Rubinius.primitive :thread_locals_store
    raise PrimitiveFailure, "Thread#locals_store primitive failed"
  end
  private :locals_store

  def keys
    Rubinius.primitive :thread_locals_keys
    raise PrimitiveFailure, "Thread#keys primitive failed"
  end

  def key?(key)
    locals_key?(Rubinius::Type.coerce_to_symbol(key))
  end

  def locals_key?(key)
    Rubinius.primitive :thread_locals_has_key
    raise PrimitiveFailure, "Thread#locals_key? primitive failed"
  end
  private :locals_key?

  # Register another Thread object +thr+ as the Thread where the debugger
  # is running. When the current thread hits a breakpoint, it uses this
  # field to figure out who to send variable/scope information to.
  #
  def set_debugger_thread(thr)
    raise TypeError, "Must be another Thread" unless thr.kind_of?(Thread)

    @debugger_thread = thr
  end

  # Called by a debugger thread (a thread where the debugger lives) to
  # setup the @control_channel to a Channel object. A debuggee thread
  # will send data to this Channel when it hits a breakpoint, allowing
  # an easy place for the debugger to wait.
  #
  # This channel is sent a Tuple containing:
  #  * The object registered in the breakpoint (can be any object)
  #  * The Thread object where the breakpoint was hit
  #  * A Channel object to use to wake up the debuggee thread
  #  * An Array of Rubinius::Location objects for the stack. These Location
  #    objects contain references to Rubinius::VariableScope objects
  #    which contain info like local variables.
  #
  def setup_control!(chan=nil)
    chan ||= Rubinius::Channel.new
    @control_channel = chan
    return chan
  end

  def self.main
    @main_thread
  end

  def self.initialize_main_thread(thread)
    @main_thread = thread
  end

  def self.exit
    Thread.current.kill
  end

  def self.kill(thread)
    thread.kill
  end

  alias_method :run, :wakeup

  def kill
    @sleep = false
    Rubinius.synchronize(self) do
      Rubinius.invoke_primitive :thread_kill, self
    end
    self
  end

  alias_method :exit, :kill
  alias_method :terminate, :kill

  def value
    join
    @value
  end

  def active_exception
    nil
  end

  def self.exclusive
    MUTEX_FOR_THREAD_EXCLUSIVE.synchronize { yield }
  end

  # Implementation note: ideally, the recursive_objects
  # lookup table would be different per method call.
  # Currently it doesn't cause problems, but if ever
  # a method :foo calls a method :bar which could
  # recurse back to :foo, it could require making
  # the tables independant.

  def self.recursion_guard(obj)
    id = obj.object_id
    objects = current.recursive_objects
    objects[id] = true

    begin
      yield
    ensure
      objects.delete id
    end
  end

  def self.guarding?(obj)
    current.recursive_objects[obj.object_id]
  end

  # detect_recursion will return if there's a recursion
  # on obj (or the pair obj+paired_obj).
  # If there is one, it returns true.
  # Otherwise, it will yield once and return false.

  def self.detect_recursion(obj, paired_obj=undefined)
    id = obj.object_id
    pair_id = paired_obj.object_id
    objects = current.recursive_objects

    case objects[id]

      # Default case, we haven't seen +obj+ yet, so we add it and run the block.
    when nil
      objects[id] = pair_id
      begin
        yield
      ensure
        objects.delete id
      end

      # We've seen +obj+ before and it's got multiple paired objects associated
      # with it, so check the pair and yield if there is no recursion.
    when Rubinius::LookupTable
      return true if objects[id][pair_id]
      objects[id][pair_id] = true

      begin
        yield
      ensure
        objects[id].delete pair_id
      end

      # We've seen +obj+ with one paired object, so check the stored one for
      # recursion.
      #
      # This promotes the value to a LookupTable since there is another new paired
      # object.
    else
      previous = objects[id]
      return true if previous == pair_id

      objects[id] = Rubinius::LookupTable.new(previous => true, pair_id => true)

      begin
        yield
      ensure
        objects[id] = previous
      end
    end

    false
  end

  # Similar to detect_recursion, but will short circuit all inner recursion
  # levels (using a throw)

  class InnerRecursionDetected < Exception; end

  def self.detect_outermost_recursion(obj, paired_obj=undefined, &block)
    rec = current.recursive_objects

    if rec[:__detect_outermost_recursion__]
      if detect_recursion(obj, paired_obj, &block)
        raise InnerRecursionDetected
      end
      false
    else
      begin
        rec[:__detect_outermost_recursion__] = true

        begin
          detect_recursion(obj, paired_obj, &block)
        rescue InnerRecursionDetected
          return true
        end

        return nil
      ensure
        rec.delete :__detect_outermost_recursion__
      end
    end
  end

  def randomizer
    @randomizer ||= Rubinius::Randomizer.new
  end

  def backtrace
    mri_backtrace.map do |tup|
      code = tup[0]
      line = tup[1]
      is_block = tup[2]
      name = tup[3]

      "#{code.active_path}:#{line}:in `#{name}'"
    end
  end

  # This is a class in MRI, although you can't initialize it.
  class Backtrace
    # Stores location of a single call frame, available since Ruby 2.0.
    class Location
      attr_reader :label
      attr_reader :path
      attr_reader :absolute_path
      attr_reader :lineno

      def initialize(label, absolute_path, path, lineno)
        @label         = label
        @absolute_path = absolute_path
        @path          = path
        @lineno        = lineno
      end

      alias_method :base_label, :label

      def to_s
        "#{absolute_path}:#{lineno}:in `#{label}'"
      end

      def inspect
        to_s
      end
    end
  end
end
