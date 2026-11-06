// REQUIRES: native-x86_64-linux
// RUN: %cxx --target=x86_64-unknown-linux-gnu -std=c++23 %s %S/Inputs/internal_template_argument_linkage_other.cxx -o %t
// RUN: %t

static constexpr unsigned input[2] = {7, 11};

template <const unsigned* pointer>
unsigned read_input() {
  return pointer[0];
}

unsigned first_lookup() { return read_input<input>(); }
unsigned second_lookup();

struct Descriptor {
  const unsigned* pointer;
};

template <Descriptor descriptor>
unsigned read_descriptor() {
  return descriptor.pointer[0];
}

unsigned first_descriptor() {
  return read_descriptor<Descriptor{&input[0]}>() +
         read_descriptor<Descriptor{&input[1]}>();
}
unsigned second_descriptor();

int main() {
  return first_lookup() != 7 || second_lookup() != 17 ||
         first_descriptor() != 18 || second_descriptor() != 36;
}
