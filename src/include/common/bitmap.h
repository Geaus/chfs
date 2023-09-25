//===----------------------------------------------------------------------===//
//
//                         Chfs
//
// bitmap.h
//
// Identification: src/include/common/bitmap.h
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include <optional>
#include <string.h>

#include "./config.h"
#include "./macros.h"

namespace chfs {

const usize KBitsPerByte = 8;
const usize KBytesPerWord = sizeof(u64);

/**
 * A bitmap type over a block of data
 */
class Bitmap {
  u8 *data;
  usize payload;

public:
  /**
   * Constructor for the bitmap
   * Note that the data pointer is not checked
   *
   * By default, we don't zero the bitmap
   */
  Bitmap(u8 *data, usize payload) : data(data), payload(payload) {}

  /**
   * Zero the bitmap
   */
  auto zeroed() { memset(data, 0, payload); }

  /**
   * Set the bit at the index
   * @param index the index of the bit to set
   */
  auto set(usize index) {
    CHFS_ASSERT(index < payload * KBitsPerByte, "index out of range");
    data[index / KBitsPerByte] |= (1 << (index % KBitsPerByte));
  }

  /**
   * Clear the bit at the index
   * @param index the index of the bit to clear
   */
  auto clear(usize index) {
    CHFS_ASSERT(index < payload * KBitsPerByte, "index out of range");
    data[index / KBitsPerByte] &= ~(1 << (index % KBitsPerByte));
  }

  /**
   * Check the bit at the index
   * @param index the index of the bit to check
   */
  auto check(usize index) -> bool {
    CHFS_ASSERT(index < payload * KBitsPerByte, "index out of range");
    return (data[index / KBitsPerByte] & (1 << (index % KBitsPerByte))) != 0;
  }

  /**
   * Count the number of ones in the bitmap
   *
   * @return the number of ones in the bitmap
   */
  auto count_ones() -> usize {
    usize num_ones = 0;
    for (usize i = 0; i < payload * KBitsPerByte; ++i) {
      if (this->check(i)) {
        ++num_ones;
      }
    }
    return num_ones;
  }

  /**
   * Count the number of zeros in the bitmap
   *
   * @return the number of zeros in the bitmap
   */
  auto count_zeros() -> usize {
    auto total_bits = payload * KBitsPerByte;
    return total_bits - this->count_ones();
  }

  /**
   * Count the number of zeros in the bitmap up to a bound
   *
   * @param upbound the upper bound of the count
   */
  auto count_zeros_to_bound(usize upbound) -> usize {
    usize num_zeros = 0;
    for (usize i = 0; i < upbound; ++i) {
      if (!this->check(i)) {
        ++num_zeros;
      }
    }
    return num_zeros;
  }

  /**
   * Find the first free bit in the bitmap
   * @return the index of the first free bit, or std::nullopt if no free bit is
   * found
   */
  auto find_first_free() -> std::optional<usize> {
    return find_first_free_w_bound(payload * KBitsPerByte);
  }

  /**
   * Find the first free bit in the bitmap with an up bound
   * @param upbound the upper bound of the search (in bits!)
   *
   * @return the index of the first free bit, or std::nullopt if no free bit is
   * found
   */
  auto find_first_free_w_bound(usize bits) -> std::optional<usize> {
    auto refined_bits = std::min(this->payload * KBitsPerByte, bits);
    size_t num_words = refined_bits / (KBytesPerWord * KBitsPerByte);
    u64 *words = reinterpret_cast<u64 *>(data);

    // Check the words first
    for (usize i = 0; i < num_words; ++i) {
      if (words[i] !=
          ~u64(0)) { // if word is not all ones, at least one bit is free
        for (size_t j = 0; j < KBitsPerByte * KBytesPerWord; ++j) {
          size_t bit_index = i * KBytesPerWord * KBitsPerByte + j;
          if (!this->check(bit_index)) {
            return bit_index;
          }
        }
      }
    }

    // Check remaining bits
    size_t num_remaining_bits = bits % (KBytesPerWord * KBitsPerByte);
    for (size_t i = 0; i < num_remaining_bits; ++i) {
      auto bit_index = i + num_words * KBytesPerWord * KBitsPerByte;
      if (!this->check(bit_index)) {
        return bit_index;
      }
    }

    return std::nullopt; // No free bit found
  }
};

} // namespace chfs