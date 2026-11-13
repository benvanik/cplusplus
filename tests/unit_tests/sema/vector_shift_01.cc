// RUN: %cxx -verify -fsyntax-only %s

using UChar8 = unsigned char __attribute__((ext_vector_type(8)));
using SChar8 = signed char __attribute__((ext_vector_type(8)));
using UShort8 = unsigned short __attribute__((ext_vector_type(8)));
using UInt8 = unsigned __attribute__((ext_vector_type(8)));
using UInt4 = unsigned __attribute__((ext_vector_type(4)));

void valid(UChar8 bytes, SChar8 signedBytes, unsigned count) {
  (void)(bytes << count);
  (void)(count >> bytes);
  (void)(bytes << signedBytes);
  bytes >>= signedBytes;
}

void elementWidths(UChar8 bytes, UShort8 halves, UInt8 words) {
  // expected-error@+1 {{invalid operands to binary expression ('unsigned char __attribute__((ext_vector_type(8)))' and 'unsigned short __attribute__((ext_vector_type(8)))')}}
  (void)(bytes << halves);
  // expected-error@+1 {{invalid operands to binary expression ('unsigned int __attribute__((ext_vector_type(8)))' and 'unsigned char __attribute__((ext_vector_type(8)))')}}
  (void)(words >> bytes);
  // expected-error@+1 {{invalid compound assignment operator '<<=' for types 'unsigned char __attribute__((ext_vector_type(8)))' and 'unsigned int __attribute__((ext_vector_type(8)))'}}
  bytes <<= words;
}

void laneCounts(UInt8 wide, UInt4 narrow) {
  // expected-error@+1 {{invalid operands to binary expression ('unsigned int __attribute__((ext_vector_type(8)))' and 'unsigned int __attribute__((ext_vector_type(4)))')}}
  (void)(wide >> narrow);
}
