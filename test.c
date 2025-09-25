#include <assert.h>
#include <stdio.h>

size_t my_struct_align(size_t *member_alignments, size_t count) {
  size_t max = 1;
  for (size_t i = 0; i < count; i++) {
    if (member_alignments[i] > max)
      max = member_alignments[i];
  }
  return max;
}

int main(void) {
  {
    size_t arr[] = {1, 4, 8};
    assert(my_struct_align(arr, 3) == 8);
  }
  {
    size_t arr[] = {2, 2, 2};
    assert(my_struct_align(arr, 3) == 2);
  }
  {
    size_t arr[] = {16, 4, 8};
    assert(my_struct_align(arr, 3) == 16);
  }
  {
    size_t arr[] = {1}; // single element
    assert(my_struct_align(arr, 1) == 1);
  }
  {
    size_t arr[] = {0, 0, 0};             // edge case: all zeroes
    assert(my_struct_align(arr, 3) == 1); // starts with 1
  }

  printf("All tests passed! ✅\n");
  return 0;
}
