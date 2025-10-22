#pragma once

#include <stddef.h>
#include <stdint.h>

template <typename T> class RingBuffer 
{
  private:
    T* ptr;
    uint32_t size;
    uint32_t headIdx;
    uint32_t tailIdx;
    bool full;

  public:
    RingBuffer(uint32_t size)
    {
      this->ptr = new T[size];
      this->size = size;
      this->headIdx = this->tailIdx = 0;
      this->full = false;
    }

    ~RingBuffer()
    {
      if (NULL != this->ptr)
        delete[] this->ptr;
    }

    void flush()
    {
      this->full = false;
      this->headIdx = this->tailIdx;
    }

    bool isEmpty()
    {
      if(this->headIdx == this->tailIdx)
        return true;
      return false;
    }

    bool isFull()
    {
      return this->full;
    }

    uint32_t available()
    {
      if (this->full)
        return this->size;
      return (this->headIdx - this->tailIdx) % this->size;
    }

    bool pop(T& retval)
    {
      if (this->isEmpty())
        return false;
      
      retval = this->ptr[this->tailIdx];
      if(++this->tailIdx >= this->size)
        this->tailIdx = 0;
      this->full = false;
      
      return true;
    }

    bool push(T& newItem)
    {
      if (this->full)
        return false;

      this->ptr[this->headIdx] = newItem;

      if(++this->headIdx >= this->size)
        this->headIdx = 0;

      if (this->headIdx == this->tailIdx)
        this->full = true;
      
      return true;
    }

    bool clear()
    {
      this->headIdx = this->tailIdx = 0;
      this->full = false;
      return true;
    }
};
