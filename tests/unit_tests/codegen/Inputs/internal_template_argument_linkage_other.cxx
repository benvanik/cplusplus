static constexpr unsigned input[2] = {17, 19};

template <const unsigned* pointer>
unsigned read_input() {
  return pointer[0];
}

unsigned second_lookup() { return read_input<input>(); }

struct Descriptor {
  const unsigned* pointer;
};

template <Descriptor descriptor>
unsigned read_descriptor() {
  return descriptor.pointer[0];
}

unsigned second_descriptor() {
  return read_descriptor<Descriptor{&input[0]}>() +
         read_descriptor<Descriptor{&input[1]}>();
}
