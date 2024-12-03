#include <atomic>

class IOQueue {
 public:
  IOQueue() {
    front.store(0);
    end.store(0);
  }

  // Use this function to initialize the queue to
  // the size that you need.
  void init(int size) {
    buffer = new int[size];
  }

  // enqueue the element e into the queue
  void enq(int e) {
    // Reserve a spot in the buffer to write to.
    int reserved_index = end.fetch_add(1);
    buffer[reserved_index] = e;
  }

  // return a value from the queue.
  // return -1 if there is no more values
  // from the queue.
  int deq() {
    if (front.load() == end.load()) {
      // Queue is empty.
      // std::printf("queue is emtpy!");
      return -1;
    }

    // Reserve a spot to read from.
    int data_index = front.fetch_add(1);
    return buffer[data_index];
  }

  int deq_32(int ret[32]) {
    // Check if there's 32 items to dequeue.
    if (end.load() - front.load() < 32) {
      return -1;
    }

    int data_index = front.fetch_add(32);
    for (int i = 0; i < 32; i++) {
      ret[i] = buffer[data_index + i];
    } 

    return 0;
  }

 private:
  int *buffer;
  std::atomic<int> front; // Dequeue at front.
  std::atomic<int> end;   // Enqueue at end.
};