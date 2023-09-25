#pragma once

#include <iostream>
#include <stdexcept>
#include <variant>

#include "./error_code.h"

namespace chfs {

/*
 A result class to handle error inspired by rust
*/
template <typename T, typename E> class Result {
  std::variant<T, E> data;
  bool hasValue;

public:
  Result(const T &value) : data(value), hasValue(true) {}

  Result(const E &error) : data(error), hasValue(false) {}

  auto is_ok() const -> bool { return hasValue; }

  auto is_err() const -> bool { return !hasValue; }

  auto unwrap() const -> T {
    if (hasValue) {
      return std::get<T>(data);
    } else {
      throw std::runtime_error("Tried to unwrap error");
    }
  }

  auto unwrap_error() const -> E {
    if (!hasValue) {
      return std::get<E>(data);
    } else {
      throw std::runtime_error("Tried to unwrap value");
    }
  }
};

template <typename T> using ChfsResult = Result<T, ErrorType>;

using ChfsNullResult = Result<std::monostate, ErrorType>;

const ChfsNullResult KNullOk = ChfsNullResult(std::monostate());

#define CHFS_HANDLE(RESULT_VAR, FUNCTION_CALL, ERROR_TYPE)                     \
  {                                                                            \
    auto temp_res = FUNCTION_CALL;                                             \
    if (temp_res.is_err()) {                                                   \
      return ERROR_TYPE(temp_res.unwrap_error());                              \
    } else {                                                                   \
      RESULT_VAR = temp_res.unwrap();                                          \
    }                                                                          \
  }

#define CHFS_HANDLE_NO_RETURN(FUNCTION_CALL, ERROR_TYPE)                       \
  {                                                                            \
    auto temp_res = FUNCTION_CALL;                                             \
    if (temp_res.is_err()) {                                                   \
      return ERROR_TYPE(temp_res.unwrap_error());                              \
    } else {                                                                   \
    }                                                                          \
  }

} // namespace chfs