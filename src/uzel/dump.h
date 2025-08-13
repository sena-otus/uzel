#pragma once

#include <boost/log/trivial.hpp>
#include <stdexcept>
#include <vector>

namespace uzel
{

  struct dump_chars {
    const std::vector<char>& data;
    const size_t maxsize;

    template<class OStream>
    OStream& print(OStream &os) const
      {
        if(maxsize < 5){
          throw std::runtime_error("maxsize must be greater than 5");
        }
        os << "[";
        for(size_t ii = 0; ii < data.size(); ++ii) {
          if(ii >= (maxsize-3)) {
            os << "...]";
            return os;
          }
          char ch = data[ii];
          if (ch == '\n') {
            os << "\\n";
          } else {
            os << ch;
          }
        }
        os << "]";
        return os;
      }

      // Optional: stream to std::ostream by using the explicit conversion
    template <class CharT, class Traits>
    friend std::basic_ostream<CharT, Traits>&
    operator<<(std::basic_ostream<CharT, Traits>& os, const dump_chars& self) {
      return os << self.print(os);
    }

      // Optional: stream to Boost.Log's formatting_ostream
    friend boost::log::formatting_ostream&
    operator<<(boost::log::formatting_ostream& os, const dump_chars& self) {
      return self.print(os);
    }
  };

    // helper
  inline dump_chars dump(const std::vector<char>& v, size_t maxsize = 200) { return dump_chars{.data=v, .maxsize=maxsize}; }

}
