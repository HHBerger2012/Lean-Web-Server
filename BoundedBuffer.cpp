/**
 * Implementation of the BoundedBuffer class.
 * See the associated header file (BoundedBuffer.hpp) for the declaration of
 * this class.
 */
#include <cstdio>

#include "BoundedBuffer.hpp"

/**
 * Constructor that sets capacity to the given value. The buffer itself is
 * initialized to en empty queue.
 *
 * @param max_size The desired capacity for the buffer.
 */
BoundedBuffer::BoundedBuffer(int max_size) {
	capacity = max_size;
	// buffer field implicitly has its default (no-arg) constructor called.
	// This means we have a new buffer with no items in it.
}

/**
 * Gets the first item from the buffer then removes it.
 */
int BoundedBuffer::getItem() {
	std::unique_lock<std::mutex> getLock(this->mutex);

	while(this->buffer.empty()) {
		data_available.wait(getLock);
	}
	
	int item = this->buffer.front(); // "this" refers to the calling object...
	buffer.pop(); // ... but like Java it is optional (no this in front of buffer on this line)

	space_available.notify_one();

	getLock.unlock();

	return item;
}

/**
 * Adds a new item to the back of the buffer.
 *
 * @param new_item The item to put in the buffer.
 */
void BoundedBuffer::putItem(int new_item) {	
	std::unique_lock<std::mutex> putLock(this->mutex);
	
	while (this->buffer.size() == (unsigned long int)this->capacity) {
		space_available.wait(putLock);
	}
	
	buffer.push(new_item);
	data_available.notify_one();	
	
	putLock.unlock();
}

