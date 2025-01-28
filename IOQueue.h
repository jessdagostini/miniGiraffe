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

  int deq_batch(int* ret, int batchsize) {
    int data_index;
    // Check if there's batchsize items to dequeue. If there's less than that, send what is missing. If there is 0 or negative number, return -1
    if (end.load() - front.load() <= 0) {
      return -1;
    } 
    
    if (end.load() - front.load() < batchsize) {
      int leftover = end.load() - front.load();
      data_index = front.fetch_add(leftover);
      for (int i = 0; i < leftover; i++) {
        ret[i] = buffer[data_index + i];
      }
      return 0;
    }

    data_index = front.fetch_add(batchsize);
    for (int i = 0; i < batchsize; i++) {
      ret[i] = buffer[data_index + i];
    } 

    return 0;
  }

 private:
  int *buffer;
  std::atomic<int> front; // Dequeue at front.
  std::atomic<int> end;   // Enqueue at end.
};