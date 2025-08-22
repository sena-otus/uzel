#pragma once

#include "enum.h"

#include <boost/property_tree/ptree.hpp>
#include <boost/optional.hpp>
#include <optional>
#include <iostream>
#include <charconv>


namespace uzel
{

    // uint8_t does not work well with boost::ptree...
  BETTER_ENUM(Priority, uint16_t, undefined=255, low = 0, high = 10, control); //NOLINT

}
  inline std::ostream& operator<<(std::ostream& os, const uzel::Priority pr)
  {
    return os << static_cast<int>(pr._to_integral());
  }

  inline std::istream& operator>>(std::istream& is, uzel::Priority& pr)
  {
    int iPriority{0};
    if (!(is >> iPriority)) return is;
    auto rez = uzel::Priority::_from_integral_nothrow(iPriority);
    if(!rez) {
      is.setstate(std::ios::failbit); // invalid token
    } else {
      pr = *rez;
    }
    return is;
  }

/** translator for property tree */

  template<typename EnumClass>
  struct BEnumIntTranslator {
    using internal_type = std::string;
    using external_type = EnumClass;

    boost::optional<external_type> get_value(const internal_type& str) {
      int val{};
      auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), val);
      if (ec != std::errc()) {
        return boost::optional<external_type>(boost::none);
      }
      auto maybe_val = EnumClass::_from_integral_nothrow(val);
      if(maybe_val) {
        return boost::optional<external_type>(*maybe_val);
      }
      return boost::optional<external_type>(boost::none);
    }

    boost::optional<internal_type> put_value(const external_type& value) {
      return boost::optional<internal_type>(std::to_string(value._to_integral()));
    }
  };


  using PriorityTranslator = BEnumIntTranslator<uzel::Priority>;

// Register translators
  namespace boost::property_tree {
    template<typename Ch, typename Traits, typename Alloc>
    struct translator_between<std::basic_string<Ch, Traits, Alloc>, uzel::Priority>
    {
      using type = PriorityTranslator;
    };
  }

  namespace boost::property_tree {
    template<>
    struct translator_between<std::string, uzel::Priority>
    {
      using type = PriorityTranslator;
    };

  } // namespace boost::property_tree
