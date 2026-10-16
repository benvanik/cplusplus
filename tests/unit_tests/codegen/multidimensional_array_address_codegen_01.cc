// RUN: %cxx -toolchain macos -emit-llvm %s -o - | %filecheck %s

struct Record {
  int padding;
  int value;
};

struct Bucket {
  int prefix;
  Record values[2][3];
};

struct Left {
  int left;
};

struct Right {
  int right;
};

struct Combined : Left, Right {};

int scalar_table[2][3];
Record record_table[2][2];
Bucket buckets[2];
Combined combined;

int* scalar_element = &scalar_table[1][2];
int (*row)[3] = &scalar_table[1];
int* row_end = scalar_table[1] + 3;
int* record_member = &record_table[1][0].value;
int* nested_member = &buckets[1].values[1][2].value;
int& scalar_reference = scalar_table[1][1];
const char* string_element = "abc" + 2;
const char16_t* wide_string_element = u"abc" + 2;
int* inherited_member = &combined.right;

// CHECK-DAG: @scalar_element = global ptr getelementptr inbounds nuw (i8, ptr @scalar_table, i64 20)
// CHECK-DAG: @row = global ptr getelementptr inbounds nuw (i8, ptr @scalar_table, i64 12)
// CHECK-DAG: @row_end = global ptr getelementptr inbounds nuw (i8, ptr @scalar_table, i64 24)
// CHECK-DAG: @record_member = global ptr getelementptr inbounds nuw (i8, ptr @record_table, i64 20)
// CHECK-DAG: @nested_member = global ptr getelementptr inbounds nuw (i8, ptr @buckets, i64 100)
// CHECK-DAG: @scalar_reference = constant ptr getelementptr inbounds nuw (i8, ptr @scalar_table, i64 16)
// CHECK-DAG: @string_element = global ptr getelementptr inbounds nuw (i8, ptr @.str{{[0-9]+}}, i64 2)
// CHECK-DAG: @wide_string_element = global ptr getelementptr inbounds nuw (i8, ptr @.str{{[0-9]+}}, i64 4)
// CHECK-DAG: @inherited_member = global ptr getelementptr inbounds nuw (i8, ptr @combined, i64 4)
