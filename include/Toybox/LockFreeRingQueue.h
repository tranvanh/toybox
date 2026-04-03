#pragma once

#include "Toybox/Common.h"
#include <atomic>
#include <new> // std::hardware_destructive_interference_size
#include <optional>

TOYBOX_NAMESPACE_BEGIN

template <typename T, size_t SIZE>
class LockFreeRingQueue {
    static constexpr size_t MASK = SIZE - 1;
    // if we happen to receive 2^64
    static_assert(SIZE > 0, "Capacity must be non-zero");
    static_assert((SIZE & (SIZE - 1)) == 0, "Capacity must be a power of two");
    static_assert(std::is_trivially_copyable<T>::value, "The type must be trivially copyable");
    // todo check that T and size_t will fit into the Slot
    // check that size of the queue is power of 2 and explain why it is better for optimization

    // Each cache line holds one slot. Prevents false sharing between
    // adjacent slots when multiple threads hammer different indices.
    static constexpr std::size_t CACHE_LINE_SIZE = 64; // \todo improve with something better

    struct alignas(CACHE_LINE_SIZE) Slot {
        T data;

        // seq == writerPos          -> empty, ready for producer
        // seq == writerPos + 1      -> full, ready for consumer
        // seq == writerPos+ SIZE -> empty again, next lap
        std::atomic<size_t> sequenceIndex;
    };

    Slot mSlots[SIZE];

    // written by different threads (producers vs consumers) and must not
    // share a line or they will bounce the line between cores constantly.
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> mWriterPos;
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> mReaderPos;

public:
    LockFreeRingQueue()
        : mWriterPos(0)
        , mReaderPos(0) {
        for (size_t i = 0; i < SIZE; ++i) {
            mSlots[i].sequenceIndex.store(i, std::memory_order_relaxed);
        }
    }

    bool try_push(T value) {
        size_t         writePos;
        size_t         seq;
        std::ptrdiff_t diff;
        const auto     fetchSlotMetaData = [this, &writePos, &seq, &diff]() {
            writePos = mWriterPos.load(std::memory_order_relaxed); // just like modulo
            seq      = mSlots[writePos & MASK].sequenceIndex.load(std::memory_order_acquire);

            // \todo improve the flagging
            diff = static_cast<std::ptrdiff_t>(seq) - static_cast<std::ptrdiff_t>(writePos);
        };
        const auto updateSlot = [this, &writePos, &seq, &value]() {
            // Flag as done
            mSlots[writePos & MASK].data = value;
            mSlots[writePos & MASK].sequenceIndex.store(seq + 1, std::memory_order_release);
        };

        fetchSlotMetaData();
        if (diff == 0) {
            if (mWriterPos.compare_exchange_weak(writePos, writePos + 1, std::memory_order_relaxed)) {
                updateSlot();
                return true;
            }
        }
        // Buffer is full, the writer position has wrapped full circle
        else if (diff < 0) {
            return false;
        }

        while (true) {
            // Some other thread could have raced up and changed the situation, fetch for new values
            fetchSlotMetaData();
            // Buffer is full, the writer position has wrapped full circle
            if (diff < 0) {
                return false;
            }
            if (diff == 0) {
                if (mWriterPos.compare_exchange_strong(writePos, writePos + 1, std::memory_order_relaxed)) {
                    updateSlot();
                    return true;
                }
            }

            // else someone has already written to the slot
        }
    }

    std::optional<T> try_pop() {
        size_t           readerPos;
        size_t           seq;
        std::ptrdiff_t   diff;
        std::optional<T> result;
        const auto       fetchSlotMetaData = [this, &readerPos, &seq, &diff]() {
            readerPos = mReaderPos.load(std::memory_order_relaxed); // just like modulo
            seq       = mSlots[readerPos & MASK].sequenceIndex.load(std::memory_order_acquire);
            // \todo improve the flagging
            diff = static_cast<std::ptrdiff_t>(seq) - static_cast<std::ptrdiff_t>(readerPos);
        };
        const auto readSlot = [this, &readerPos, &seq, &result]() {
            // Flag as done
            result = mSlots[readerPos & MASK].data;
            mSlots[readerPos & MASK].sequenceIndex.store(seq + (SIZE - 1), std::memory_order_release);
        };

        fetchSlotMetaData();
        if (diff == 1) {
            if (mReaderPos.compare_exchange_weak(readerPos, readerPos + 1, std::memory_order_relaxed)) {
                readSlot();
                return result;
            }
        } else if (diff < 1) {
            // slot is empty — nothing to read.
            return std::nullopt;
        }

        while (true) {
            fetchSlotMetaData();
            if (diff == 1) {
                if (mReaderPos.compare_exchange_strong(readerPos, readerPos + 1, std::memory_order_relaxed)) {
                    readSlot();
                    return result;
                }
            } else if (diff < 1) {
                // slot is empty — nothing to read.
                return std::nullopt;
            }
        }
    }
};

TOYBOX_NAMESPACE_END
