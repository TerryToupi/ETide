# String

The `ETide::String` namespace provides non-owning byte spans and arena-backed string
utilities. String values carry an explicit length, so embedded null bytes are valid and
do not truncate operations.

## String spans

`String8` is a mutable byte span:

```cpp
String::String8 text = {
    .str  = bytes,
    .size = byte_count,
};
```

`String8View` is the read-only equivalent:

```cpp
String::String8View view = String::str8_literal("hello");
```

Neither type owns its memory. The caller must keep the referenced bytes alive.

## Constructing spans

```cpp
String::String8 explicit_span = String::str8(bytes, byte_count);
String::String8 c_string      = String::str8_cstr(bytes);
String::String8 mutable_view  = String::str8_mut(String::str8_literal("text"));
```

- `str8` uses the supplied pointer and length without scanning.
- `str8(array)` uses the complete array size, including a trailing null when present.
- `str8_cstr` scans until the first null byte.
- `str8_literal` excludes the source literal's trailing null.
- `str8_mut` converts a read-only span to a mutable span without copying.
- `str8_empty` contains a zero pointer and zero length.

The caller must only use `str8_mut` when the referenced storage may legally be treated
as mutable.

## Arena allocation

```cpp
String::String8 raw  = String::str8_alloc(arena, 64);
String::String8 cstr = String::str8_cstr_alloc(arena, 64);
String::String8 copy = String::str8_copy(arena, source);
```

- `str8_alloc` allocates exactly the requested byte count.
- `str8_cstr_alloc` allocates one extra byte and writes a trailing null.
- `str8_copy` copies the explicit source range and adds a trailing null.

The returned span does not include the trailing null in its `size`. Its lifetime is
controlled by the supplied arena.

## Exact comparison

```cpp
if (String::str8_match_exact(a, b)) {
    // Length and every byte match.
}
```

Empty spans compare equal regardless of their pointers. Non-empty spans require valid
byte ranges.

## Intrusive span lists

`String8List` links `String8Node` entries without copying their referenced bytes:

```cpp
String::String8List list = {};
String::str8_list_push(arena, &list, first);
String::str8_list_push(arena, &list, second);

String::String8 joined = String::str8_list_join(arena, &list);
```

`str8_list_join` allocates one null-terminated result in the destination arena and
copies every span in list order.

Caller-owned nodes can be linked without allocation:

```cpp
String::String8Node node = {};
String::str8_list_push_node_set_string(&list, &node, text);
```

The node must outlive its membership in the list.

## Incremental serialization

Serialization builds an intrusive list of spans and joins it once at the end:

```cpp
String::String8List output = {};
String::str8_serial_begin(arena, &output);
String::str8_serial_push_str8(arena, &output, prefix);
String::str8_serial_push_char(arena, &output, ':');
String::str8_serial_push_str8(arena, &output, value);

String::String8 result = String::str8_serial_end(arena, output);
```

The intermediate nodes and any pushed characters belong to the supplied arena.

## Failure behavior

- Allocation functions return an empty span when allocation fails.
- List insertion returns `0` when its arguments are invalid or allocation fails.
- Joining rejects total-size overflow.
- Functions never take ownership of caller-provided bytes.
