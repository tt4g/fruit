/*
 * Copyright 2014 Google Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SEMISTATIC_MAP_H
#define SEMISTATIC_MAP_H

#include <vector>
#include <limits>
#include <climits>
#include <cstdint>

namespace fruit {
namespace impl {

/**
 * Provides a subset of the interface of std::map, and also has these additional assumptions:
 * - Key must be default constructible
 * - Value must be default constructible
 * 
 * Also, while insertion of elements after construction is supported, inserting more than O(1) elements
 * after construction will raise the cost of any further lookups to more than O(1).
 */
template <typename Key, typename Value>
class SemistaticMap {
private:
  using Unsigned = std::uintptr_t;
  using NumBits = unsigned char;
  using value_type = std::pair<Key, Value>;
  
  static const unsigned char beta = 4;
    
  static_assert(std::numeric_limits<NumBits>::max() >= sizeof(Unsigned)*CHAR_BIT,
                "An unsigned char is not enough to contain the number of bits in your platform. Please report this issue.");
    
  struct HashFunction {
    Unsigned a;
    NumBits shift; // shift==(sizeof(Unsigned)*CHAR_BIT - num_bits)
    
    inline Unsigned hash(Unsigned x) const {
      return (Unsigned)(a * x) >> shift;
    }
  };
  
  static NumBits pickNumBits(std::size_t n) {
    NumBits result = 1;
    while ((1U << result) < n) {
      ++result;
    }
    return result;
  }

  HashFunction hash_function;
  // Given a key x, if p=lookup_table[hash_function.hash(x)] the candidate places for x are [p.first, p.second). These pointers
  // point to the values[] vector, but it might be either the one of this object or the one of an object that was shallow-copied
  // into this one.
  std::vector<std::pair<value_type*, value_type*>> lookup_table;
  std::vector<value_type> values;
  
  inline Unsigned hash(const Key& key) const {
    return hash_function.hash(std::hash<typename std::remove_cv<Key>::type>()(key));
  }
  
  // Inserts a range [elemsBegin, elemsEnd) of new (key,value) pairs with hash h. The keys must not exist in the map.
  // Before calling this, ensure that the capacity of `values' is sufficient to contain the new values without re-allocating.
  void insert(std::size_t h, 
              typename std::vector<value_type>::const_iterator elemsBegin,
              typename std::vector<value_type>::const_iterator elemsEnd);
  
public:
  // Constructs an *invalid* map (as if this map was just moved from).
  SemistaticMap() = default;
  
  // Iter must be a forward iterator with value type std::pair<Key, Value>.
  template <typename Iter>
  SemistaticMap(Iter begin, std::size_t num_values);
  
  // Creates a shallow copy of `map' with the additional elements in newElements.
  // The keys in newElements must be unique and must not be present in `map'.
  // The new map will share data with `map', so must be destroyed before `map' is destroyed.
  // NOTE: If more than O(1) elements are added, calls to at() and find() on the result will *not* be O(1).
  // This is O(newElements.size()*log(newElements.size())).
  SemistaticMap(const SemistaticMap<Key, Value>& map, std::vector<value_type>&& newElements);
  
  SemistaticMap(SemistaticMap&&) = default;
  SemistaticMap(const SemistaticMap&) = delete;
  
  SemistaticMap& operator=(SemistaticMap&&) = default;
  SemistaticMap& operator=(const SemistaticMap&) = delete;
  
  // Precondition: `key' must exist in the map.
  // Unlike std::map::at(), this yields undefined behavior if the precondition isn't satisfied (instead of throwing).
  const Value& at(Key key) const;
  
  // Prefer using at() when possible, this is slightly slower.
  // Returns nullptr if the key was not found.
  const Value* find(Key key) const;
};

} // namespace impl
} // namespace fruit

// semistatic_map.templates.h is NOT included here to reduce the transitive includes. Include it when needed (in .cpp files).

#endif // SEMISTATIC_MAP_H
