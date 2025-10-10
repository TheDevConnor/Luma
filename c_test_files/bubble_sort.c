#include <stdio.h>

// Function to perform Bubble Sort
void bubbleSort(int arr[], int n) {
  for (int i = 0; i < n - 1; i++) {
    // Flag to detect if any swapping happened
    int swapped = 0;

    // Last i elements are already in place
    for (int j = 0; j < n - i - 1; j++) {
      if (arr[j] > arr[j + 1]) {
        // Swap arr[j] and arr[j + 1]
        int temp = arr[j];
        arr[j] = arr[j + 1];
        arr[j + 1] = temp;

        swapped = 1;
      }
    }

    // If no elements were swapped, array is already sorted
    if (!swapped)
      break;
  }
}

// Function to print the array
void printArray(int arr[], int size) {
  for (int i = 0; i < size; i++)
    printf("%d ", arr[i]);
  printf("\n");
}

int main() {
  int arr[] = {64, 34, 25, 12, 22, 11, 90};
  int n = sizeof(arr) / sizeof(arr[0]);

  printf("Unsorted array: ");
  printArray(arr, n);

  bubbleSort(arr, n);

  printf("Sorted array: ");
  printArray(arr, n);

  return 0;
}
